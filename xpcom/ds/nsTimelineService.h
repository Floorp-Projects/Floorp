/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsITimelineService.h"

#ifdef MOZ_TIMELINE

#define NS_TIMELINESERVICE_CID \
{ /* a335edf0-3daf-11d5-b67d-000064657374 */ \
    0xa335edf0, \
        0x3daf, \
        0x11d5, \
        {0xb6, 0x7d, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74}}

#define NS_TIMELINESERVICE_CONTRACTID "@mozilla.org;timeline-service;1"
#define NS_TIMELINESERVICE_CLASSNAME "Timeline Service"

class nsTimelineService : public nsITimelineService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMELINESERVICE

  nsTimelineService();
  virtual ~nsTimelineService();
  /* additional members */
};

#endif
