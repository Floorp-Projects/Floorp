/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifdef __NSSTACKBASEDTIMER_H
#define __NSSTACKBASEDTIMER_H
#include "stopwatch.h"

class nsStackBasedTimer {
public:
  nsStackBasedTimer(Stopwatch* aStopwatch) { sw = aStopwatch; }
  ~nsStackBasedTimer() { if (sw) sw->Stop(); }

  void Start(void) { if (sw) sw->Start(PR_FALSE); }
  void Stop(void) { if (sw) sw->Stop(); }
  void Reset() { if (sw) sw->Reset(); }
  void SaveState() { if (sw) sw->SaveState(); }
  void RestoreState() { if (sw) sw->RestoreState(); }
  
private:
  Stopwatch* sw;
};
#endif  // __NSSTACKBASEDTIMER_H