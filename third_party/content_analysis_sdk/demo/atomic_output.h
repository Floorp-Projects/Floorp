// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <sstream>
#include <string>

// Utility class to atomically write outout to std::cout.  All data streamed
// the class is automatically sent to std::cout in the dtor.  This is useful
// to keep the output of multiple threads writing to std::Cout from
// interleaving.

class AtomicCout {
 public:
  ~AtomicCout() {
    flush();
  }

  std::stringstream& stream() { return stream_; }

  void flush() {
    std::cout << stream_.str();
    stream_.str(std::string());
  }

 private:
  std::stringstream stream_;
};