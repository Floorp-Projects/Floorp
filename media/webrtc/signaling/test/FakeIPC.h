/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FAKE_IPC_H_
#define FAKE_IPC_H_
#include <unistd.h>

class PlatformThread {
public:
  static void YieldCurrentThread();
};

namespace base {
class AtExitManager {
public:
  typedef void (*AtExitCallbackType)(void*);

  static void RegisterCallback(AtExitCallbackType func, void* param);
};
}
#endif
