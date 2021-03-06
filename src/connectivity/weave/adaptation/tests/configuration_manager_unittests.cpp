// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
#include <Weave/DeviceLayer/internal/WeaveDeviceLayerInternal.h>
#include <Weave/DeviceLayer/ConfigurationManager.h>
#include <Weave/Core/WeaveKeyIds.h>
#include "src/connectivity/weave/adaptation/configuration_manager_impl.h"
#include <Weave/Profiles/security/WeaveApplicationKeys.h>

#include <Weave/Core/WeaveVendorIdentifiers.hpp>
#include <Weave/DeviceLayer/internal/GenericConfigurationManagerImpl.ipp>
// clang-format on

#include <fuchsia/hwinfo/cpp/fidl.h>
#include <fuchsia/hwinfo/cpp/fidl_test_base.h>
#include <fuchsia/weave/cpp/fidl.h>
#include <fuchsia/weave/cpp/fidl_test_base.h>
#include <fuchsia/wlan/device/service/cpp/fidl.h>
#include <fuchsia/wlan/device/service/cpp/fidl_test_base.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/gtest/real_loop_fixture.h>
#include <lib/sys/cpp/testing/component_context_provider.h>
#include <net/ethernet.h>
#include <thread>
#include <fuchsia/factory/cpp/fidl.h>
#include <fuchsia/factory/cpp/fidl_test_base.h>
#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/io/cpp/fidl_test_base.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>
#include <lib/syslog/cpp/logger.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/vmo_file.h>

#include "configuration_manager_impl.h"
#include "gtest/gtest.h"
#include <memory>
#include "src/lib/fsl/vmo/strings.h"

namespace adaptation {
namespace testing {
namespace {

using nl::Weave::DeviceLayer::ConfigurationManager;
using nl::Weave::DeviceLayer::ConfigurationMgr;
using nl::Weave::DeviceLayer::ConfigurationMgrImpl;
using nl::Weave::Profiles::DeviceDescription::WeaveDeviceDescriptor;

constexpr uint8_t kExpectedMac[] = {124, 46, 119, 21, 27, 102};
constexpr uint16_t kExpectedPhyId = 0;
constexpr char kExpectedPath[] = "/dev/wifi/wlanphy";

// Below expected values are from testdata JSON files and should be
// consistent with the file for the related tests to pass.
constexpr uint16_t kExpectedVendorId = 5050;
constexpr uint16_t kExpectedProductId = 60209;
constexpr uint64_t kExpectedDeviceId = 65535;
constexpr char kExpectedFirmwareRevision[] = "prerelease-1";
constexpr char kExpectedSerialNumber[] = "dummy_serial_number";
constexpr char kExpectedPairingCode[] = "PAIRDUMMY123";
constexpr uint16_t kMaxFirmwareRevisionSize = ConfigurationManager::kMaxFirmwareRevisionLength + 1;
constexpr uint16_t kMaxSerialNumberSize = ConfigurationManager::kMaxSerialNumberLength + 1;
constexpr uint16_t kMaxPairingCodeSize = ConfigurationManager::kMaxPairingCodeLength + 1;

}  // namespace

class FakeWlanStack : public fuchsia::wlan::device::service::testing::DeviceService_TestBase {
 public:
  void NotImplemented_(const std::string& name) override { FAIL() << __func__; }

  void ListPhys(ListPhysCallback callback) override {
    fuchsia::wlan::device::service::ListPhysResponse resp;
    fuchsia::wlan::device::service::PhyListItem item;
    item.phy_id = kExpectedPhyId;
    item.path = kExpectedPath;
    resp.phys.push_back(item);
    callback(resp);
  }

  void QueryPhy(fuchsia::wlan::device::service::QueryPhyRequest req,
                QueryPhyCallback callback) override {
    std::unique_ptr<fuchsia::wlan::device::service::QueryPhyResponse> resp =
        std::make_unique<fuchsia::wlan::device::service::QueryPhyResponse>();
    resp->info.id = 0;
    std::copy(std::begin(kExpectedMac), std::end(kExpectedMac), resp->info.hw_mac_address.begin());
    resp->info.mac_roles = {fuchsia::wlan::device::MacRole::CLIENT,
                            fuchsia::wlan::device::MacRole::AP};
    callback(0, std::move(resp));
  }

  fidl::InterfaceRequestHandler<fuchsia::wlan::device::service::DeviceService> GetHandler(
      async_dispatcher_t* dispatcher = nullptr) {
    dispatcher_ = dispatcher;
    return [this, dispatcher](
               fidl::InterfaceRequest<fuchsia::wlan::device::service::DeviceService> request) {
      binding_.Bind(std::move(request), dispatcher);
    };
  }

 private:
  fidl::Binding<fuchsia::wlan::device::service::DeviceService> binding_{this};
  async_dispatcher_t* dispatcher_;
};

// This fake class hosts device protocol in the backgroud thread
class FakeHwinfo : public fuchsia::hwinfo::testing::Device_TestBase {
 public:
  void NotImplemented_(const std::string& name) override { FAIL() << __func__; }

  void GetInfo(fuchsia::hwinfo::Device::GetInfoCallback callback) override {
    fuchsia::hwinfo::DeviceInfo device_info;
    device_info.set_serial_number(kExpectedSerialNumber);
    callback(std::move(device_info));
  }

  fidl::InterfaceRequestHandler<fuchsia::hwinfo::Device> GetHandler(
      async_dispatcher_t* dispatcher = nullptr) {
    dispatcher_ = dispatcher;
    return [this, dispatcher](fidl::InterfaceRequest<fuchsia::hwinfo::Device> request) {
      binding_.Bind(std::move(request), dispatcher);
    };
  }

 private:
  fidl::Binding<fuchsia::hwinfo::Device> binding_{this};
  async_dispatcher_t* dispatcher_;
};

class FakeWeaveFactoryDataManager : public fuchsia::weave::testing::FactoryDataManager_TestBase {
 public:
  void NotImplemented_(const std::string& name) override { FAIL() << __func__; }

  void GetPairingCode(GetPairingCodeCallback callback) override {
    constexpr char device_pairing_code[] = "PAIRCODE123";
    fuchsia::weave::FactoryDataManager_GetPairingCode_Result result;
    fuchsia::weave::FactoryDataManager_GetPairingCode_Response response((::std::vector<uint8_t>(
        std::begin(device_pairing_code), std::end(device_pairing_code) - 1)));
    result.set_response(response);
    callback(std::move(result));
  }

  fidl::InterfaceRequestHandler<fuchsia::weave::FactoryDataManager> GetHandler(
      async_dispatcher_t* dispatcher = nullptr) {
    dispatcher_ = dispatcher;
    return [this, dispatcher](fidl::InterfaceRequest<fuchsia::weave::FactoryDataManager> request) {
      binding_.Bind(std::move(request), dispatcher);
    };
  }

 private:
  fidl::Binding<fuchsia::weave::FactoryDataManager> binding_{this};
  async_dispatcher_t* dispatcher_;
};

class FakeDirectory {
 public:
  FakeDirectory() { root_ = std::make_unique<vfs::PseudoDir>(); }
  zx_status_t AddResource(std::string filename, const std::string& data) {
    return root_->AddEntry(filename, CreateVmoFile(data));
  }
  void Serve(fidl::InterfaceRequest<fuchsia::io::Directory> channel, async_dispatcher_t* dispatcher) {
    root_->Serve(
        fuchsia::io::OPEN_FLAG_DIRECTORY | fuchsia::io::OPEN_RIGHT_READABLE |
        fuchsia::io::OPEN_FLAG_DESCRIBE | fuchsia::io::OPEN_RIGHT_WRITABLE,
        channel.TakeChannel(), dispatcher);
  }

 private:
  std::unique_ptr<vfs::VmoFile> CreateVmoFile(const std::string& data) {
    fsl::SizedVmo test_vmo;
    if (!VmoFromString(data, &test_vmo)) {
      return nullptr;
    }
    return std::make_unique<vfs::VmoFile>(zx::unowned_vmo(test_vmo.vmo()), 0, data.size(),
                            vfs::VmoFile::WriteOption::WRITABLE,
                            vfs::VmoFile::Sharing::CLONE_COW);
  }
 protected:
  std::unique_ptr<vfs::PseudoDir> root_;
};

class FakeWeaveFactoryStoreProvider
    : public fuchsia::factory::testing::WeaveFactoryStoreProvider_TestBase {
 public:
  FakeWeaveFactoryStoreProvider() = default;
  fidl::InterfaceRequestHandler<fuchsia::factory::WeaveFactoryStoreProvider> GetHandler(
      async_dispatcher_t* dispatcher = nullptr) {
    dispatcher_ = dispatcher;
    return [this, dispatcher](fidl::InterfaceRequest<fuchsia::factory::WeaveFactoryStoreProvider> request) {
      binding_.Bind(std::move(request), dispatcher);
    };
  }
  ~FakeWeaveFactoryStoreProvider() override = default;
  FakeWeaveFactoryStoreProvider(const FakeWeaveFactoryStoreProvider&) = delete;
  FakeWeaveFactoryStoreProvider& operator=(
      const FakeWeaveFactoryStoreProvider&) = delete;

  void AttachDir(std::unique_ptr<FakeDirectory> fake_dir) {
    fake_dir_ = std::move(fake_dir);
  }

  void GetFactoryStore(::fidl::InterfaceRequest<::fuchsia::io::Directory> dir) override {
    if (!fake_dir_) {
      ADD_FAILURE();
    }

    fake_dir_->Serve(std::move(dir), dispatcher_);
  }

  void NotImplemented_(const std::string& name) final { ADD_FAILURE(); };
 private:
  fidl::Binding<fuchsia::factory::WeaveFactoryStoreProvider> binding_{this};
  std::unique_ptr<FakeDirectory> fake_dir_;
  async_dispatcher_t* dispatcher_;
};

class ConfigurationManagerTest : public ::gtest::RealLoopFixture {
 public:
  ConfigurationManagerTest() {
    context_provider_.service_directory_provider()->AddService(
        fake_wlan_stack_.GetHandler(dispatcher()));
    context_provider_.service_directory_provider()->AddService(
        fake_hwinfo_.GetHandler(dispatcher()));
    context_provider_.service_directory_provider()->AddService(
        fake_weave_factory_data_manager_.GetHandler(dispatcher()));
    context_provider_.service_directory_provider()->AddService(
        fake_weave_factory_store_provider_.GetHandler(dispatcher()));
  }
  ~ConfigurationManagerTest() { QuitLoop(); }

  void SetUp() {
    RealLoopFixture::SetUp();
    RunLoopAsync();
    cfg_mgr_ = std::make_unique<nl::Weave::DeviceLayer::ConfigurationManagerImpl>(
        context_provider_.TakeContext());
    WaitOnLoop(true);
  }

  void RunLoopAsync() {
    if (test_loop_bg_thread_) {
      FAIL() << "Already running the background thread.";
    }
    test_loop_bg_trigger_.store(false);
    test_loop_bg_thread_ = std::make_unique<std::thread>(
        [this] { RunLoopUntil([this] { return test_loop_bg_trigger_.load(); }); });
  }

  void WaitOnLoop(bool trigger = false) {
    if (!test_loop_bg_thread_) {
      FAIL() << "Background thread was not started, nothing to wait on.";
    }
    if (trigger) {
      TriggerCondition();
    }
    test_loop_bg_thread_->join();
    test_loop_bg_thread_.reset();
  }

  void TriggerCondition() { test_loop_bg_trigger_.store(true); }

 protected:
  std::unique_ptr<nl::Weave::DeviceLayer::ConfigurationManagerImpl> cfg_mgr_;

 private:
  std::unique_ptr<std::thread> test_loop_bg_thread_;
  std::atomic_bool test_loop_bg_trigger_;

  sys::testing::ComponentContextProvider context_provider_;

  FakeHwinfo fake_hwinfo_;
  FakeWlanStack fake_wlan_stack_;
  FakeWeaveFactoryDataManager fake_weave_factory_data_manager_;
  FakeWeaveFactoryStoreProvider fake_weave_factory_store_provider_;

 public:
  FakeWeaveFactoryStoreProvider* GetFactoryProvider(void) {
    return &fake_weave_factory_store_provider_;
  }
};

TEST_F(ConfigurationManagerTest, SetAndGetFabricId) {
  const uint64_t fabric_id = 123456789U;
  uint64_t stored_fabric_id = 0U;
  EXPECT_EQ(cfg_mgr_->StoreFabricId(fabric_id), WEAVE_NO_ERROR);
  EXPECT_EQ(cfg_mgr_->GetFabricId(stored_fabric_id), WEAVE_NO_ERROR);
  EXPECT_EQ(stored_fabric_id, fabric_id);
}

TEST_F(ConfigurationManagerTest, GetDeviceId) {
  uint64_t device_id = 0U;
  EXPECT_EQ(cfg_mgr_->GetDeviceId(device_id), WEAVE_NO_ERROR);
  EXPECT_EQ(device_id, kExpectedDeviceId);
}

TEST_F(ConfigurationManagerTest, GetPrimaryWiFiMacAddress) {
  uint8_t mac[ETH_ALEN];
  RunLoopAsync();
  EXPECT_EQ(cfg_mgr_->GetPrimaryWiFiMACAddress(mac), WEAVE_NO_ERROR);
  WaitOnLoop(true);
  EXPECT_TRUE(std::equal(std::begin(kExpectedMac), std::end(kExpectedMac), std::begin(mac)));
}

TEST_F(ConfigurationManagerTest, GetVendorId) {
  uint16_t vendor_id;
  EXPECT_EQ(cfg_mgr_->GetVendorId(vendor_id), WEAVE_NO_ERROR);
  EXPECT_EQ(vendor_id, kExpectedVendorId);
}

TEST_F(ConfigurationManagerTest, GetProductId) {
  uint16_t product_id;
  EXPECT_EQ(cfg_mgr_->GetProductId(product_id), WEAVE_NO_ERROR);
  EXPECT_EQ(product_id, kExpectedProductId);
}

TEST_F(ConfigurationManagerTest, GetFirmwareRevision) {
  char firmware_revision[kMaxFirmwareRevisionSize];
  size_t out_len;
  EXPECT_EQ(cfg_mgr_->GetFirmwareRevision(firmware_revision, sizeof(firmware_revision), out_len),
            WEAVE_NO_ERROR);
  EXPECT_EQ(strncmp(firmware_revision, kExpectedFirmwareRevision, out_len), 0);
}

TEST_F(ConfigurationManagerTest, GetSerialNumber) {
  char serial_num[kMaxSerialNumberSize];
  size_t serial_num_len;
  EXPECT_EQ(cfg_mgr_->GetSerialNumber(serial_num, sizeof(serial_num), serial_num_len),
            WEAVE_NO_ERROR);
  EXPECT_STREQ(serial_num, kExpectedSerialNumber);
}

TEST_F(ConfigurationManagerTest, GetDeviceDescriptor) {
  ::nl::Weave::Profiles::DeviceDescription::WeaveDeviceDescriptor device_desc;
  RunLoopAsync();
  EXPECT_EQ(cfg_mgr_->GetDeviceDescriptor(device_desc), WEAVE_NO_ERROR);
  WaitOnLoop(true);

  EXPECT_STREQ(device_desc.SerialNumber, kExpectedSerialNumber);
  EXPECT_EQ(device_desc.ProductId, kExpectedProductId);
  EXPECT_EQ(device_desc.VendorId, kExpectedVendorId);
}

TEST_F(ConfigurationManagerTest, GetPairingCode) {
  char pairing_code[kMaxPairingCodeSize];
  size_t pairing_code_len;
  EXPECT_EQ(cfg_mgr_->GetPairingCode(pairing_code, sizeof(pairing_code), pairing_code_len),
            WEAVE_NO_ERROR);
  EXPECT_EQ(pairing_code_len,
            strnlen(kExpectedPairingCode, WeaveDeviceDescriptor::kMaxPairingCodeLength) + 1);
  EXPECT_STREQ(pairing_code, kExpectedPairingCode);
}

TEST_F(ConfigurationManagerTest, SetAndGetDeviceId) {
  const std::string test_device_id_file("test_device_id");
  const std::string test_device_id_data("1234ABCD");
  uint64_t stored_weave_device_id = 0;

  EXPECT_EQ(nl::Weave::DeviceLayer::Internal::EnvironmentConfig::FactoryResetConfig(), WEAVE_NO_ERROR);
  auto fake_dir = std::make_unique<FakeDirectory>();
  EXPECT_EQ(ZX_OK, fake_dir->AddResource(test_device_id_file, test_device_id_data));
  GetFactoryProvider()->AttachDir(std::move(fake_dir));
  RunLoopAsync();
  EXPECT_EQ(cfg_mgr_->GetDeviceId(stored_weave_device_id), WEAVE_NO_ERROR);
  WaitOnLoop(true);
  EXPECT_EQ(stored_weave_device_id, strtoull(test_device_id_data.c_str(), NULL, 16));

  // Show that even if the file is modified, it doesn't affect us as we read from
  // factory only once.
  stored_weave_device_id = 0;
  auto fake_dir2 = std::make_unique<FakeDirectory>();
  GetFactoryProvider()->AttachDir(std::move(fake_dir2));
  RunLoopAsync();
  EXPECT_EQ(cfg_mgr_->GetDeviceId(stored_weave_device_id), WEAVE_NO_ERROR);
  WaitOnLoop(true);
  EXPECT_EQ(stored_weave_device_id, strtoull(test_device_id_data.c_str(), NULL, 16));
}

}  // namespace testing
}  // namespace adaptation
