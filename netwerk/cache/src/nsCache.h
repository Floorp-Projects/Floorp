/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is nsCache.h, released March 18, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan  <gordon@netscape.com>
 *    Patrick C. Beard <beard@netscape.com>
 *    Darin Fisher     <darin@netscape.com>
 */

/**
 * Cache Service Utility Functions
 */

#ifndef _nsCache_h_
#define _nsCache_h_

#include "nsAReadableString.h"

#if 0
          // Convert PRTime to unix-style time_t, i.e. seconds since the epoch
PRUint32  ConvertPRTimeToSeconds(PRTime time64);

          // Convert unix-style time_t, i.e. seconds since the epoch, to PRTime
PRTime    ConvertSecondsToPRTime(PRUint32 seconds);
#endif


extern PRUint32  SecondsFromPRTime(PRTime prTime);
extern PRTime    PRTimeFromSeconds(PRUint32 seconds);


extern nsresult  ClientIDFromCacheKey(const nsAReadableCString&  key, char ** result);
extern nsresult  ClientKeyFromCacheKey(const nsAReadableCString& key, char ** result);

#endif // _nsCache_h
