/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include <algorithm>
#include <iostream>
#include <vector>

#include "assert.h"

extern const uint16_t DEFAULT_MAX_LENGTH;

const uint16_t MERGE_MAX_LENGTH = 50000U;

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
  std::vector<std::string> args(*argv, *argv + *argc);

  auto hasMaxLenArg = [](std::string &a) { return a.find("-max_len=") == 0; };

  // Nothing to do if a max_len argument is given.
  if (any_of(args.begin(), args.end(), hasMaxLenArg)) {
    return 0;
  }

  auto hasMergeArg = [](std::string &a) { return a.find("-merge=1") == 0; };

  uint16_t max_length = DEFAULT_MAX_LENGTH;

  // Set specific max_len when merging.
  if (any_of(args.begin(), args.end(), hasMergeArg)) {
    max_length = MERGE_MAX_LENGTH;
  }

  std::cerr << "INFO: MaxLen: " << max_length << std::endl;
  std::string param = "-max_len=" + std::to_string(max_length);

  // Copy original arguments.
  char **new_args = new char *[*argc + 1];
  for (int i = 0; i < *argc; i++) {
    new_args[i] = (*argv)[i];
  }

  // Append corpus max length.
  size_t param_len = param.size() + 1;
  new_args[*argc] = new char[param_len];
  memcpy(new_args[*argc], param.c_str(), param_len);

  // Update arguments.
  (*argc)++;
  *argv = new_args;

  return 0;
}
