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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *      Gagan Saksena <gagan@netscape.com>
 */
#include "nsHTTPAtoms.h"

// define storage for all atoms
#define HTTP_ATOM(_name, _value) nsIAtom* nsHTTPAtoms::_name;
#include "nsHTTPAtomList.h"
#undef HTTP_ATOM


static nsrefcnt gRefCnt;

void nsHTTPAtoms::AddRefAtoms()
{
  if (0 == gRefCnt++) {
    // create atoms
#define HTTP_ATOM(_name, _value) _name = NS_NewAtom(_value);
#include "nsHTTPAtomList.h"
#undef HTTP_ATOM
  }
}

void nsHTTPAtoms::ReleaseAtoms()
{
  NS_PRECONDITION(gRefCnt != 0, "bad release atoms");
  if (--gRefCnt == 0) {
    // release atoms
#define HTTP_ATOM(_name, _value) NS_RELEASE(_name);
#include "nsHTTPAtomList.h"
#undef HTTP_ATOM
  }
}

