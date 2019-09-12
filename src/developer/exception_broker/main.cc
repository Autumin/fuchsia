// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/syslog/cpp/logger.h>

#include "src/developer/exception_broker/exception_broker.h"

int main() {
  syslog::InitLogger({"exception-broker"});

  async::Loop loop(&kAsyncLoopConfigAttachToCurrentThread);
  auto context = sys::ComponentContext::Create();

  auto broker = fuchsia::exception::ExceptionBroker::Create(loop.dispatcher(), context->svc());
  if (!broker)
    return EXIT_FAILURE;

  // Create the bindings for the protocols.
  fidl::BindingSet<fuchsia::exception::Handler> handler_bindings;
  context->outgoing()->AddPublicService(handler_bindings.GetHandler(broker.get()));

  fidl::BindingSet<fuchsia::exception::ProcessLimbo> limbo_bindings;
  context->outgoing()->AddPublicService(limbo_bindings.GetHandler(broker.get()));

  loop.Run();

  return EXIT_SUCCESS;
}
