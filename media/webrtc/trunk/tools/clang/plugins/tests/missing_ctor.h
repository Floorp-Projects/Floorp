// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MISSING_CTOR_H_
#define MISSING_CTOR_H_

#include <string>
#include <vector>

class MissingCtorsArentOKInHeader {
 public:

 private:
  std::vector<int> one_;
  std::vector<std::string> two_;
};

#endif  // MISSING_CTOR_H_
