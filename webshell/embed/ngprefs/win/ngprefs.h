/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

