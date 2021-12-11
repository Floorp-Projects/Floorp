/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "argparse.h"

ArgParser::ArgParser(const std::vector<std::string>& arguments) {
  for (size_t i = 0; i < arguments.size(); i++) {
    std::string arg = arguments.at(i);
    if (arg.find("--") == 0) {
      // look for an option argument
      if (i + 1 < arguments.size() && arguments.at(i + 1).find("--") != 0) {
        programArgs_[arg] = arguments.at(i + 1);
        i++;
      } else {
        programArgs_[arg] = "";
      }
    } else {
      // positional argument (e.g. required argument)
      positionalArgs_.push_back(arg);
    }
  }
}
