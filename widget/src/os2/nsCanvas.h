/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/12/2000       IBM Corp.      Changed params on DispatchMouseEvent to match Windows..
 *
 */

#ifndef _nscanvas_h
#define _nscanvas_h

// nscanvas - this is the NS_CHILD_CID class which contains content.
//            The main code added in here is to do painting.

#include "nswindow.h"

class nsCanvas : public nsWindow
{
 public:
   nsCanvas() {};

 protected:
   virtual PRBool DispatchMouseEvent( PRUint32 aEventType, MPARAM mp1, MPARAM mp2);
   virtual ULONG WindowStyle();
};

#endif
