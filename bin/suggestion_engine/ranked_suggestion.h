// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERIDOT_BIN_SUGGESTION_ENGINE_RANKED_SUGGESTION_H_
#define PERIDOT_BIN_SUGGESTION_ENGINE_RANKED_SUGGESTION_H_

#include "lib/suggestion/fidl/suggestion_provider.fidl.h"
#include "peridot/bin/suggestion_engine/suggestion_prototype.h"

namespace maxwell {

// |rank| and |adjusted_confidence| should satisfy the invariant that for any
// sorted set of ranked suggestions, |rank| is increasing and
// |adjusted_confidence| is nonincreasing.
struct RankedSuggestion {
  SuggestionPrototype* prototype;
  double confidence;
};

SuggestionPtr CreateSuggestion(const RankedSuggestion& suggestion_data);

}  // namespace maxwell

#endif  // PERIDOT_BIN_SUGGESTION_ENGINE_RANKED_SUGGESTION_H_
