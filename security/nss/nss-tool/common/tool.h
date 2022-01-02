/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tool_h__
#define tool_h__

#include <string>
#include <vector>

class Tool {
 public:
  virtual bool Run(const std::vector<std::string>& arguments) = 0;
  virtual ~Tool() {}

 private:
  virtual void Usage() = 0;
};

#endif  // tool_h__
