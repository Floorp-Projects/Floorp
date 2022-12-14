/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5Module_h
#define nsHtml5Module_h

#include "nsIThread.h"

class nsHtml5Parser;

class nsHtml5Module {
  friend class nsHtml5ParserThreadTerminator;

 public:
  static void InitializeStatics();
  static void ReleaseStatics();
  static already_AddRefed<nsHtml5Parser> NewHtml5Parser();
  static already_AddRefed<nsISerialEventTarget> GetStreamParserEventTarget();

 private:
#ifdef DEBUG
  static bool sNsHtml5ModuleInitialized;
#endif
  static nsIThread* sStreamParserThread;
};

#endif  // nsHtml5Module_h
