/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AutoSQLiteLifetime_h
#define mozilla_AutoSQLiteLifetime_h

namespace mozilla {

class AutoSQLiteLifetime final {
 private:
  static int sSingletonEnforcer;
  static int sResult;

 public:
  AutoSQLiteLifetime();
  ~AutoSQLiteLifetime();
  static int getInitResult() { return AutoSQLiteLifetime::sResult; }
};

}  // namespace mozilla

#endif
