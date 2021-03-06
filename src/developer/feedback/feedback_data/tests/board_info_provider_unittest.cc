// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/developer/feedback/feedback_data/annotations/board_info_provider.h"

#include <fuchsia/hwinfo/cpp/fidl.h>
#include <lib/async/cpp/executor.h>
#include <lib/zx/time.h>
#include <zircon/errors.h>

#include <map>
#include <memory>
#include <string>

#include "src/developer/feedback/feedback_data/annotations/aliases.h"
#include "src/developer/feedback/feedback_data/constants.h"
#include "src/developer/feedback/testing/cobalt_test_fixture.h"
#include "src/developer/feedback/testing/stubs/board_info_provider.h"
#include "src/developer/feedback/testing/stubs/cobalt_logger_factory.h"
#include "src/developer/feedback/testing/unit_test_fixture.h"
#include "src/developer/feedback/utils/cobalt.h"
#include "src/developer/feedback/utils/cobalt_event.h"
#include "src/lib/fxl/logging.h"
#include "src/lib/fxl/strings/split_string.h"
#include "third_party/googletest/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace feedback {
namespace {

using fuchsia::hwinfo::BoardInfo;
using fxl::SplitResult::kSplitWantNonEmpty;
using fxl::WhiteSpaceHandling::kTrimWhitespace;
using sys::testing::ServiceDirectoryProvider;
using testing::ElementsAreArray;
using testing::Pair;

class BoardInfoProviderTest : public UnitTestFixture, public CobaltTestFixture {
 public:
  BoardInfoProviderTest()
      : CobaltTestFixture(/*unit_test_fixture=*/this), executor_(dispatcher()) {}

 protected:
  void SetUpBoardProviderServer(std::unique_ptr<stubs::BoardInfoProviderBase> server) {
    board_provider_server_ = std::move(server);
    if (board_provider_server_) {
      InjectServiceProvider(board_provider_server_.get());
    }
  }

  Annotations GetBoardInfo(const AnnotationKeys& annotations_to_get = {},
                           const zx::duration timeout = zx::sec(1)) {
    SetUpCobaltServer(std::make_unique<stubs::CobaltLoggerFactory>());
    Cobalt cobalt(dispatcher(), services());

    BoardInfoProvider provider(annotations_to_get, dispatcher(), services(), timeout, &cobalt);
    auto promise = provider.GetAnnotations();

    Annotations annotations;
    executor_.schedule_task(
        std::move(promise).then([&annotations](::fit::result<Annotations>& res) {
          if (res.is_ok()) {
            annotations = res.take_value();
          }
        }));
    RunLoopFor(timeout);

    if (annotations.empty()) {
      return {};
    }

    Annotations board_info;
    for (auto& [key, value] : annotations) {
      board_info[key] = std::move(value);
    }

    return board_info;
  }

  async::Executor executor_;

 private:
  std::unique_ptr<stubs::BoardInfoProviderBase> board_provider_server_;
};

BoardInfo CreateBoardInfo(const Annotations& annotations) {
  BoardInfo info;

  for (const auto& [key, value] : annotations) {
    if (key == kAnnotationHardwareBoardName) {
      info.set_name(value);
    } else if (key == kAnnotationHardwareBoardRevision) {
      info.set_revision(value);
    }
  }

  return info;
}

TEST_F(BoardInfoProviderTest, Succeed_AllAnnotationsRequested) {
  auto board_provider = std::make_unique<stubs::BoardInfoProvider>(CreateBoardInfo({
      {kAnnotationHardwareBoardName, "some-name"},
      {kAnnotationHardwareBoardRevision, "some-revision"},
  }));
  SetUpBoardProviderServer(std::move(board_provider));

  auto board_info = GetBoardInfo({
      kAnnotationHardwareBoardName,
      kAnnotationHardwareBoardRevision,
  });
  EXPECT_THAT(board_info, ElementsAreArray({
                              Pair(kAnnotationHardwareBoardName, "some-name"),
                              Pair(kAnnotationHardwareBoardRevision, "some-revision"),
                          }));
}
TEST_F(BoardInfoProviderTest, Succeed_SingleAnnotationRequested) {
  auto board_provider = std::make_unique<stubs::BoardInfoProvider>(CreateBoardInfo({
      {kAnnotationHardwareBoardName, "some-name"},
  }));
  SetUpBoardProviderServer(std::move(board_provider));

  auto board_info = GetBoardInfo({
      kAnnotationHardwareBoardName,
      kAnnotationHardwareBoardRevision,
  });
  EXPECT_THAT(board_info, ElementsAreArray({
                              Pair(kAnnotationHardwareBoardName, "some-name"),
                          }));
}

TEST_F(BoardInfoProviderTest, Succeed_SpuriousAnnotationRequested) {
  auto board_provider = std::make_unique<stubs::BoardInfoProvider>(CreateBoardInfo({
      {kAnnotationHardwareBoardName, "some-name"},
      {kAnnotationHardwareBoardRevision, "some-revision"},
  }));
  SetUpBoardProviderServer(std::move(board_provider));

  auto board_info = GetBoardInfo({
      kAnnotationHardwareBoardName,
      kAnnotationHardwareBoardRevision,
      "bad-key",
  });
  EXPECT_THAT(board_info, ElementsAreArray({
                              Pair(kAnnotationHardwareBoardName, "some-name"),
                              Pair(kAnnotationHardwareBoardRevision, "some-revision"),
                          }));
}

TEST_F(BoardInfoProviderTest, Succeed_MissingAnnotationReturned) {
  auto board_provider = std::make_unique<stubs::BoardInfoProvider>(CreateBoardInfo({
      {kAnnotationHardwareBoardName, "some-name"},
  }));
  SetUpBoardProviderServer(std::move(board_provider));

  auto board_info = GetBoardInfo({
      kAnnotationHardwareBoardName,
      kAnnotationHardwareBoardRevision,
  });
  EXPECT_THAT(board_info, ElementsAreArray({
                              Pair(kAnnotationHardwareBoardName, "some-name"),
                          }));
}

TEST_F(BoardInfoProviderTest, Check_CobaltLogsTimeout) {
  SetUpBoardProviderServer(std::make_unique<stubs::BoardInfoProviderNeverReturns>());

  auto board_info = GetBoardInfo();

  ASSERT_TRUE(board_info.empty());
  EXPECT_THAT(ReceivedCobaltEvents(), ElementsAreArray({
                                          CobaltEvent(TimedOutData::kBoardInfo),
                                      }));
}

}  // namespace
}  // namespace feedback
