// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/developer/feedback/feedback_data/attachments/screenshot_ptr.h"

#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/async/cpp/executor.h>
#include <lib/zx/time.h>
#include <zircon/errors.h>

#include <memory>
#include <vector>

#include "src/developer/feedback/testing/stubs/scenic.h"
#include "src/developer/feedback/testing/unit_test_fixture.h"
#include "src/lib/fxl/logging.h"
#include "third_party/googletest/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace feedback {
namespace {

using fuchsia::ui::scenic::ScreenshotData;
using testing::UnorderedElementsAreArray;

constexpr bool kSuccess = true;

class TakeScreenshotTest : public UnitTestFixture {
 public:
  TakeScreenshotTest() : executor_(dispatcher()) {}

 protected:
  void SetUpScenicServer(std::unique_ptr<stubs::ScenicBase> server) {
    scenic_server_ = std::move(server);
    if (scenic_server_) {
      InjectServiceProvider(scenic_server_.get());
    }
  }

  ::fit::result<ScreenshotData> TakeScreenshot(const zx::duration timeout = zx::sec(1)) {
    ::fit::result<ScreenshotData> result;
    executor_.schedule_task(
        feedback::TakeScreenshot(dispatcher(), services(),
                                 fit::Timeout(timeout, /*actions=*/[this] { did_timeout_ = true; }))
            .then([&result](::fit::result<ScreenshotData>& res) { result = std::move(res); }));
    RunLoopFor(timeout);
    return result;
  }

  async::Executor executor_;
  bool did_timeout_ = false;

 private:
  std::unique_ptr<stubs::ScenicBase> scenic_server_;
};

TEST_F(TakeScreenshotTest, Succeed_CheckerboardScreenshot) {
  const size_t image_dim_in_px = 100;
  std::vector<stubs::TakeScreenshotResponse> scenic_server_responses;
  scenic_server_responses.emplace_back(stubs::CreateCheckerboardScreenshot(image_dim_in_px),
                                       kSuccess);
  std::unique_ptr<stubs::Scenic> scenic = std::make_unique<stubs::Scenic>();
  scenic->set_take_screenshot_responses(std::move(scenic_server_responses));
  SetUpScenicServer(std::move(scenic));

  ::fit::result<ScreenshotData> result = TakeScreenshot();

  ASSERT_TRUE(result.is_ok());
  ScreenshotData screenshot = result.take_value();
  EXPECT_TRUE(screenshot.data.vmo.is_valid());
  EXPECT_EQ(static_cast<size_t>(screenshot.info.height), image_dim_in_px);
  EXPECT_EQ(static_cast<size_t>(screenshot.info.width), image_dim_in_px);
  EXPECT_EQ(screenshot.info.stride, image_dim_in_px * 4u);
  EXPECT_EQ(screenshot.info.pixel_format, fuchsia::images::PixelFormat::BGRA_8);
}

TEST_F(TakeScreenshotTest, Fail_ScenicReturningFalse) {
  SetUpScenicServer(std::make_unique<stubs::ScenicAlwaysReturnsFalse>());

  ::fit::result<ScreenshotData> result = TakeScreenshot();

  ASSERT_TRUE(result.is_error());
}

TEST_F(TakeScreenshotTest, Check_Timeout) {
  SetUpScenicServer(std::make_unique<stubs::ScenicNeverReturns>());
  ASSERT_TRUE(TakeScreenshot().is_error());
  EXPECT_TRUE(did_timeout_);
}

}  // namespace
}  // namespace feedback
