// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVELOPER_FEEDBACK_UTILS_FIDL_ONESHOT_PTR_H_
#define SRC_DEVELOPER_FEEDBACK_UTILS_FIDL_ONESHOT_PTR_H_

#include <lib/async/dispatcher.h>
#include <lib/fidl/cpp/interface_ptr.h>
#include <lib/fit/function.h>
#include <lib/fit/promise.h>
#include <lib/sys/cpp/service_directory.h>

#include "src/developer/feedback/utils/fit/bridge.h"
#include "src/developer/feedback/utils/fit/timeout.h"
#include "src/lib/fxl/logging.h"
#include "src/lib/fxl/strings/string_printf.h"

namespace feedback {
namespace fidl {

// Wrapper around InterfacePtr<Interface> that provides 'one shot' to make a call to the interface.
//
// For example, if we wished to fetch a device's update channel from
// fuchsia::update::channel::Provider then we would use OneShotPtr as follows:
//
//  ::fit::promise<std::string> GetChannel() {
//    OneShotPtr<fuchsia::update::channel::Provider, std::string> channel_ptr(dispatcher,
//    services);
//
//    channel_ptr->GetCurrent([&](std::string channel) {
//      if (channel_ptr.IsAlreadyDone()) {
//        return;
//      }
//
//      channel_ptr.CompleteOk(channel);
//    });
//
//    return channel_ptr.WaitForDone()
//  }
//
// Any additional attempts to dereference |channel_ptr| in GetChannel() would cause the program to
// check-fail.
template <typename Interface, typename V = void, typename E = void>
class OneShotPtr {
 public:
  OneShotPtr(async_dispatcher_t* dispatcher, std::shared_ptr<sys::ServiceDirectory> services)
      : services_(services),
        bridge_(dispatcher, fxl::StringPrintf("call on %s", Interface::Name_)) {}

  bool IsAlreadyDone() const { return bridge_.IsAlreadyDone(); }

  template <typename VV = V, typename = std::enable_if_t<std::is_void_v<VV>>>
  void CompleteOk() {
    bridge_.CompleteOk();
  }
  template <typename VV = V, typename = std::enable_if_t<!std::is_void_v<VV>>>
  void CompleteOk(VV value) {
    bridge_.CompleteOk(std::move(value));
  }

  void CompleteError() { bridge_.CompleteError(); }

  ::fit::promise<V, E> WaitForDone() { return bridge_.WaitForDone(); }

  ::fit::promise<V, E> WaitForDone(fit::Timeout timeout) {
    return bridge_.WaitForDone(std::move(timeout));
  }

  Interface* operator->() {
    // We use FXL_ and not FX_ so the messages goes to stderr and can be intercepted by
    // ASSERT_DEATH.
    FXL_CHECK(!oneshot_used_) << fxl::StringPrintf(
        "You've only got one shot to use ->  on a OneShotPtr<%s>", Interface::Name_);
    oneshot_used_ = true;

    connection_ = services_->Connect<Interface>();

    connection_.set_error_handler([this](zx_status_t status) {
      if (bridge_.IsAlreadyDone()) {
        return;
      }

      FX_PLOGS(ERROR, status) << "Lost connection to " << Interface::Name_;

      bridge_.CompleteError();
    });

    return connection_.get();
  }

 protected:
  ::fidl::InterfacePtr<Interface> connection_;

 private:
  const std::shared_ptr<sys::ServiceDirectory> services_;
  fit::Bridge<V, E> bridge_;

  bool oneshot_used_ = false;
};

}  // namespace fidl
}  // namespace feedback

#endif  // SRC_DEVELOPER_FEEDBACK_UTILS_FIDL_ONESHOT_PTR_H_
