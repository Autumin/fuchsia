// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERIDOT_BIN_CLOUD_PROVIDER_FIRESTORE_FIRESTORE_FIRESTORE_SERVICE_H_
#define PERIDOT_BIN_CLOUD_PROVIDER_FIRESTORE_FIRESTORE_FIRESTORE_SERVICE_H_

#include <functional>

#include <google/firestore/v1beta1/document.pb.h>
#include <google/firestore/v1beta1/firestore.grpc.pb.h>
#include <grpc++/grpc++.h>

#include "lib/fxl/macros.h"
#include "lib/fxl/tasks/task_runner.h"
#include "peridot/bin/cloud_provider_firestore/firestore/listen_call_client.h"

namespace cloud_provider_firestore {

// Client library for Firestore.
//
// Requests methods are assumed to be called on the |main_runner| thread. All
// client callbacks are called on the |main_runner|.
class FirestoreService {
 public:
  FirestoreService() {}
  virtual ~FirestoreService() {}

  virtual void CreateDocument(
      google::firestore::v1beta1::CreateDocumentRequest request,
      std::function<void(grpc::Status status,
                         google::firestore::v1beta1::Document document)>
          callback) = 0;

  virtual std::unique_ptr<ListenCallHandler> Listen(
      ListenCallClient* client) = 0;

 private:
  FXL_DISALLOW_COPY_AND_ASSIGN(FirestoreService);
};

}  // namespace cloud_provider_firestore

#endif  // PERIDOT_BIN_CLOUD_PROVIDER_FIRESTORE_FIRESTORE_FIRESTORE_SERVICE_H_
