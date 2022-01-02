/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "shared.h"

size_t CustomMutate(Mutators mutators, uint8_t *data, size_t size,
                    size_t max_size, unsigned int seed) {
  std::mt19937 rng(seed);
  static std::bernoulli_distribution bdist;

  if (bdist(rng)) {
    std::uniform_int_distribution<size_t> idist(0, mutators.size() - 1);
    return mutators.at(idist(rng))(data, size, max_size, seed);
  }

  return LLVMFuzzerMutate(data, size, max_size);
}
