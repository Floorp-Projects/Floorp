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
#ifndef nsWidgetAtoms_h___
#define nsWidgetAtoms_h___

#include "prtypes.h"
#include "nsIAtom.h"

class nsINameSpaceManager;

/**
 * This class wraps up the creation and destruction of the standard
 * set of xul atoms used during normal xul handling. This object
 * is created when the first xul content object is created, and
 * destroyed when the last such content object is destroyed.
 *
 * It's here because we cannot use nsHTMLAtoms or nsXULAtoms from
 * the Widget shlb. They are components are we're not.
 */
class nsWidgetAtoms {
public:

  static void AddRefAtoms();
  static void ReleaseAtoms();

  // XUL namespace ID, good for the life of the nsXULAtoms object
  static PRInt32  nameSpaceID;

  /* Declare all atoms

     The atom names and values are stored in nsWidgetAtomList.h and
     are brought to you by the magic of C preprocessing

     Add new atoms to nsWidgetAtomList and all support logic will be auto-generated
   */
#define WIDGET_ATOM(_name, _value) static nsIAtom* _name;
#include "nsWidgetAtomList.h"
#undef WIDGET_ATOM

};

#endif /* nsXULAtoms_h___ */
