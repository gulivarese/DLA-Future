//
// Distributed Linear Algebra with Future (DLAF)
//
// Copyright (c) 2018-2022, ETH Zurich
// All rights reserved.
//
// Please, refer to the LICENSE file in the root directory.
// SPDX-License-Identifier: BSD-3-Clause
//

#include <dlaf/init.h>
#include <dlaf/tune.h>

namespace dlaf {

TuneParameters& getTuneParameters() {
  static TuneParameters params;
  return params;
}

}