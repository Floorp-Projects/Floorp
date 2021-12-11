/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef argparse_h__
#define argparse_h__

#include <string>
#include <unordered_map>
#include <vector>

class ArgParser {
 public:
  ArgParser(const std::vector<std::string>& arguments);

  bool Has(std::string arg) const { return programArgs_.count(arg) > 0; }

  std::string Get(std::string arg) const { return programArgs_.at(arg); }

  size_t GetPositionalArgumentCount() const { return positionalArgs_.size(); }
  std::string GetPositionalArgument(size_t pos) const {
    return positionalArgs_.at(pos);
  }

 private:
  std::unordered_map<std::string, std::string> programArgs_;
  std::vector<std::string> positionalArgs_;
};

#endif  // argparse_h__
