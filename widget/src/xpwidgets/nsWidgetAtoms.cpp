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
 * Original Author: Mike Pinkerton (pinkerton@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsWidgetAtoms.h"

// define storage for all atoms
#define WIDGET_ATOM(_name, _value) nsIAtom* nsWidgetAtoms::_name;
#include "nsWidgetAtomList.h"
#undef WIDGET_ATOM


static nsrefcnt gRefCnt = 0;

void nsWidgetAtoms::AddRefAtoms() {

  if (gRefCnt == 0) {
    // now register the atoms
#define WIDGET_ATOM(_name, _value) _name = NS_NewAtom(_value);
#include "nsWidgetAtomList.h"
#undef WIDGET_ATOM
  }
  ++gRefCnt;
}

void nsWidgetAtoms::ReleaseAtoms() {

  NS_PRECONDITION(gRefCnt != 0, "bad release of xul atoms");
  if (--gRefCnt == 0) {
#define WIDGET_ATOM(_name, _value) NS_RELEASE(_name);
#include "nsWidgetAtomList.h"
#undef WIDGET_ATOM
  }
}
