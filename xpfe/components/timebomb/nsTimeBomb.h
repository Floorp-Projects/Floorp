/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Doug Turner <dougt@netscape.com>
 */
#ifndef nstimebomb_h___
#define nstimebomb_h___

#include "nsITimeBomb.h"
#include "nsCOMPtr.h"
#include "nsIPref.h"

#define NS_TIMEBOMB_CID { 0x141917dc, 0xe1c3, 0x11d3, {0xac, 0x71, 0x00, 0xc0, 0x4f, 0xa0, 0xd2, 0x6b}}

class nsTimeBomb : public nsITimeBomb
{
public:

	nsTimeBomb();
	virtual ~nsTimeBomb();

	NS_DECL_ISUPPORTS    
    NS_DECL_NSITIMEBOMB

protected:
    nsCOMPtr<nsIPref> mPrefs;
    nsresult GetInt64ForPref(const char* pref, PRInt64* time);
};

#endif // nstimebomb_h___
