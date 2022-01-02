// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INLINE_CTOR_H_
#define INLINE_CTOR_H_

#include <string>
#include <vector>

class InlineCtorsArentOKInHeader {
 public:
  InlineCtorsArentOKInHeader() {}
  ~InlineCtorsArentOKInHeader() {}

 private:
  std::vector<int> one_;
  std::vector<std::string> two_;
};

#endif  // INLINE_CTOR_H_
