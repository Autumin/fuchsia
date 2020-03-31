// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <fuchsia/boot/llcpp/fidl.h>
#include <fuchsia/io/llcpp/fidl.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/fd.h>
#include <lib/fdio/io.h>
#include <lib/zx/resource.h>
#include <unistd.h>
#include <zircon/errors.h>
#include <zircon/rights.h>

#include <blobfs/mount.h>
#include <block-client/cpp/block-device.h>
#include <block-client/cpp/remote-block-device.h>
#include <fbl/auto_call.h>
#include <fbl/unique_fd.h>
#include <fs-management/mount.h>
#include <ramdevice-client/ramdisk.h>
#include <zxtest/zxtest.h>

#include "blobfs.h"
#include "runner.h"
#include "test/blob_utils.h"

namespace {

namespace fio = ::llcpp::fuchsia::io;

zx_rights_t get_rights(const zx::object_base& handle) {
  zx_info_handle_basic_t info;
  zx_status_t status = handle.get_info(ZX_INFO_HANDLE_BASIC, &info, sizeof(info), nullptr, nullptr);
  return status == ZX_OK ? info.rights : ZX_RIGHT_NONE;
}

class MountTest : public zxtest::Test {
 public:
  explicit MountTest(blobfs::ServeLayout layout) : layout_(layout) {}

  void SetUp() final {
    ASSERT_OK(ramdisk_create(512, 1 << 16, &ramdisk_));
    ASSERT_OK(mkfs(ramdisk_get_path(ramdisk_), DISK_FORMAT_BLOBFS, launch_stdio_sync,
                   &default_mkfs_options));

    fbl::unique_fd block_fd(ramdisk_get_block_fd(ramdisk_));
    zx::channel block_channel;
    ASSERT_OK(fdio_fd_clone(block_fd.get(), block_channel.reset_and_get_address()));
    std::unique_ptr<block_client::RemoteBlockDevice> device;
    ASSERT_OK(block_client::RemoteBlockDevice::Create(std::move(block_channel), &device));

    blobfs::MountOptions options;
    zx::channel root_client, root_server;
    ASSERT_OK(zx::channel::create(0, &root_client, &root_server));

    loop_ = std::make_unique<async::Loop>(&kAsyncLoopConfigNoAttachToCurrentThread);

    std::unique_ptr<blobfs::Runner> runner;
    ASSERT_OK(blobfs::Runner::Create(loop_.get(), std::move(device), &options,
                                     std::move(vmex_resource_), &runner));
    ASSERT_OK(runner->ServeRoot(std::move(root_server), layout_));
    ASSERT_OK(loop_->StartThread("blobfs test dispatcher"));
    runner_ = std::move(runner);

    ASSERT_OK(fdio_fd_create(root_client.release(), root_fd_.reset_and_get_address()));
    ASSERT_TRUE(root_fd_.is_valid());
  }

  void TearDown() final {
    zx::channel root_client;
    ASSERT_OK(fdio_fd_transfer(root_fd_.release(), root_client.reset_and_get_address()));
    ASSERT_OK(fio::DirectoryAdmin::Call::Unmount(zx::unowned_channel(root_client)).status());
    ASSERT_OK(ramdisk_destroy(ramdisk_));
  }

 protected:
  int root_fd() const { return root_fd_.get(); }
  void set_vmex_resource(zx::resource resource) { vmex_resource_ = std::move(resource); }

 private:
  blobfs::ServeLayout layout_;
  zx::resource vmex_resource_;
  ramdisk_client_t* ramdisk_ = nullptr;
  std::unique_ptr<async::Loop> loop_;
  std::unique_ptr<blobfs::Runner> runner_;
  fbl::unique_fd root_fd_;
};

class DataMountTest : public MountTest {
 public:
  DataMountTest() : MountTest(blobfs::ServeLayout::kDataRootOnly) {}
};

class OutgoingMountTest : public MountTest {
 public:
  OutgoingMountTest() : MountTest(blobfs::ServeLayout::kExportDirectory) {}
};

class ExecutableMountTest : public DataMountTest {
 public:
  ExecutableMountTest() : DataMountTest() {
    zx::channel local, remote;
    zx_status_t status = zx::channel::create(0, &local, &remote);
    ASSERT_OK(status);

    // It would be more appropriate to use fuchsia.security.resource.Vmex here, but that would
    // require adding that service to svchost since this is still a non-packaged Zircon test.
    // fuchsia.boot.RootResource is already available so we just use that instead of expanding
    // svchost further.
    status = fdio_service_connect("/svc/fuchsia.boot.RootResource", remote.release());
    ASSERT_OK(status, "Failed to connect to fuchsia.boot.RootResource: %d\n", status);

    auto client = llcpp::fuchsia::boot::RootResource::SyncClient{std::move(local)};
    auto result = client.Get();
    ASSERT_TRUE(result.ok(), "fuchsia.boot.RootResource.Get() failed: %d\n", result.status());

    set_vmex_resource(std::move(result.Unwrap()->resource));
  }
};

// merkle root for a file. in order to create a file on blobfs we need the filename to be a valid
// merkle root whether or not we ever write the content.
//
// This is valid enough to create files but it is unknown what content this was generated
// from. Previously this comment said it was "test content" but that seems to be incorrect.
constexpr std::string_view kFileName =
    "be901a14ec42ee0a8ee220eb119294cdd40d26d573139ee3d51e4430e7d08c28";

TEST_F(DataMountTest, DataRootHasNoRootDirectoryInIt) {
  errno = 0;
  fbl::unique_fd no_fd(openat(root_fd(), blobfs::kOutgoingDataRoot, O_RDONLY));
  ASSERT_FALSE(no_fd.is_valid());
  ASSERT_EQ(errno, EINVAL);
}

TEST_F(DataMountTest, DataRootCanHaveBlobsCreated) {
  fbl::unique_fd foo_fd(openat(root_fd(), kFileName.data(), O_CREAT));
  ASSERT_TRUE(foo_fd.is_valid());
}

TEST_F(OutgoingMountTest, OutgoingDirectoryHasRootDirectoryInIt) {
  fbl::unique_fd no_fd(openat(root_fd(), blobfs::kOutgoingDataRoot, O_DIRECTORY));
  ASSERT_TRUE(no_fd.is_valid());
}

TEST_F(OutgoingMountTest, OutgoingDirectoryIsReadOnly) {
  fbl::unique_fd foo_fd(openat(root_fd(), kFileName.data(), O_CREAT));
  ASSERT_FALSE(foo_fd.is_valid());
}

TEST_F(OutgoingMountTest, OutgoingDirectoryDataRootCanHaveBlobsCreated) {
  std::string path = std::string("root/") + kFileName.data();
  fbl::unique_fd foo_fd(openat(root_fd(), path.c_str(), O_CREAT));
  ASSERT_TRUE(foo_fd.is_valid());
}

// Verify that if no valid Resource of at least KIND_VMEX is provided to the filesystem during
// creation then it does not support VMO_FLAG_EXEC to obtain executable VMOs from blobs.
TEST_F(DataMountTest, CannotLoadBlobsExecutable) {
  // Create a new blob with random contents on the mounted filesystem.
  std::unique_ptr<blobfs::BlobInfo> info;
  ASSERT_NO_FAILURES(GenerateRandomBlob(".", 1 << 16, &info));

  fbl::unique_fd fd(openat(root_fd(), info->path, O_CREAT | O_RDWR));
  ASSERT_TRUE(fd.is_valid());

  ASSERT_EQ(ftruncate(fd.get(), info->size_data), 0);
  ASSERT_EQ(blobfs::StreamAll(write, fd.get(), info->data.get(), info->size_data), 0,
            "Failed to write Data");
  ASSERT_NO_FAILURES(blobfs::VerifyContents(fd.get(), info->data.get(), info->size_data));
  fd.reset();

  // Open the new blob again but with READABLE | EXECUTABLE rights, then confirm that we can get the
  // blob contents as a VMO but not as an executable VMO.
  ASSERT_OK(fdio_open_fd_at(root_fd(), info->path,
                            fio::OPEN_RIGHT_READABLE | fio::OPEN_RIGHT_EXECUTABLE,
                            fd.reset_and_get_address()));
  ASSERT_TRUE(fd.is_valid());

  zx::vmo vmo;
  ASSERT_OK(fdio_get_vmo_clone(fd.get(), vmo.reset_and_get_address()));
  ASSERT_TRUE(vmo.is_valid());

  vmo.reset();
  ASSERT_EQ(ZX_ERR_NOT_SUPPORTED, fdio_get_vmo_exec(fd.get(), vmo.reset_and_get_address()));
  ASSERT_FALSE(vmo.is_valid());
}

// The test fixture for this test provides a valid Resource object to the filesystem when it is
// created, which means it should support fuchsia.io.File.GetBuffer with VMO_FLAG_EXEC which
// fdio_get_vmo_exec exercises.
TEST_F(ExecutableMountTest, CanLoadBlobsExecutable) {
  // Create a new blob with random contents on the mounted filesystem.
  std::unique_ptr<blobfs::BlobInfo> info;
  ASSERT_NO_FAILURES(GenerateRandomBlob(".", 1 << 16, &info));

  fbl::unique_fd fd(openat(root_fd(), info->path, O_CREAT | O_RDWR));
  ASSERT_TRUE(fd.is_valid());

  ASSERT_EQ(ftruncate(fd.get(), info->size_data), 0);
  ASSERT_EQ(blobfs::StreamAll(write, fd.get(), info->data.get(), info->size_data), 0,
            "Failed to write Data");
  ASSERT_NO_FAILURES(blobfs::VerifyContents(fd.get(), info->data.get(), info->size_data));
  fd.reset();

  // Open the new blob again but with READABLE | EXECUTABLE rights, then confirm that we can get the
  // blob contents as a normal and executable VMO.
  ASSERT_OK(fdio_open_fd_at(root_fd(), info->path,
                            fio::OPEN_RIGHT_READABLE | fio::OPEN_RIGHT_EXECUTABLE,
                            fd.reset_and_get_address()));
  ASSERT_TRUE(fd.is_valid());

  zx::vmo vmo;
  ASSERT_OK(fdio_get_vmo_clone(fd.get(), vmo.reset_and_get_address()));
  ASSERT_TRUE(vmo.is_valid());

  vmo.reset();
  ASSERT_OK(fdio_get_vmo_exec(fd.get(), vmo.reset_and_get_address()));
  ASSERT_TRUE(vmo.is_valid());
  zx_rights_t rights = get_rights(vmo);
  ASSERT_TRUE((rights & ZX_RIGHT_EXECUTE) == ZX_RIGHT_EXECUTE);
}

}  // namespace
