/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5Module_h
#define nsHtml5Module_h

#include "nsIParser.h"
#include "nsIThread.h"

class nsHtml5Module
{
  public:
    static void InitializeStatics();
    static void ReleaseStatics();
    static already_AddRefed<nsIParser> NewHtml5Parser();
    static nsresult Initialize(nsIParser* aParser, nsIDocument* aDoc, nsIURI* aURI, nsISupports* aContainer, nsIChannel* aChannel);
    static nsIThread* GetStreamParserThread();
    static bool sOffMainThread;
  private:
#ifdef DEBUG
    static bool sNsHtml5ModuleInitialized;
#endif
    static nsIThread* sStreamParserThread;
    static nsIThread* sMainThread;
};

#endif // nsHtml5Module_h
