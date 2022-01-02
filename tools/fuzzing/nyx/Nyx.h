/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_fuzzing_Nyx_h
#define mozilla_fuzzing_Nyx_h

#include <stdint.h>

#ifndef NYX_DISALLOW_COPY_AND_ASSIGN
#  define NYX_DISALLOW_COPY_AND_ASSIGN(T) \
    T(const T&);                          \
    void operator=(const T&)
#endif

namespace mozilla {
namespace fuzzing {

class Nyx {
 public:
  static Nyx& instance();

  void start(void);
  bool is_enabled(const char* identifier);
  uint32_t get_data(uint8_t* data, uint32_t size);
  void release(void);
  void handle_event(const char* type, const char* file, int line,
                    const char* reason);

 private:
  bool mInited = false;

  Nyx();
  NYX_DISALLOW_COPY_AND_ASSIGN(Nyx);
};

}  // namespace fuzzing
}  // namespace mozilla

#endif /* mozilla_fuzzing_Nyx_h */
