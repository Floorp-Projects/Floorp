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

#ifndef _PREFS_H
#define _PREFS_H
#include <ole2.h>

// {5F541370-4DAE-11d2-B70A-00805F8A2676}
DEFINE_GUID(CLSID_NGLayoutPrefs, 
0x5f541370, 0x4dae, 0x11d2, 0xb7, 0xa, 0x0, 0x80, 0x5f, 0x8a, 0x26, 0x76);

// {4E97BE30-4DB1-11d2-B70A-00805F8A2676}
DEFINE_GUID(IID_INGLayoutPrefs, 
0x4e97be30, 0x4db1, 0x11d2, 0xb7, 0xa, 0x0, 0x80, 0x5f, 0x8a, 0x26, 0x76);

interface INGLayoutPrefs: public IUnknown {
    STDMETHOD(Show)(HWND hwnd) = 0;
};

extern "C" void
DisplayPreferences(HWND pFrame);

#endif

