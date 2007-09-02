/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIDeviceContextSpec_h___
#define nsIDeviceContextSpec_h___

#include "nsIDeviceContext.h"
#include "prtypes.h"

class nsIWidget;
class nsIPrintSettings;

class gfxASurface;

#define NS_IDEVICE_CONTEXT_SPEC_IID   \
{ 0x205c614f, 0x39f8, 0x42e1, \
{ 0x92, 0x53, 0x04, 0x9b, 0x48, 0xc3, 0xcb, 0xd8 } }

class nsIDeviceContextSpec : public nsISupports
{
public:
   NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDEVICE_CONTEXT_SPEC_IID)

   /**
    * Initialize the device context spec.
    * @param aWidget         A widget a dialog can be hosted in
    * @param aPrintSettings  Print settings for the print operation
    * @param aIsPrintPreview True if creating Spec for PrintPreview
    * @return NS_OK or a suitable error code.
    */
   NS_IMETHOD Init(nsIWidget *aWidget,
                   nsIPrintSettings* aPrintSettings,
                   PRBool aIsPrintPreview) = 0;

   NS_IMETHOD GetSurfaceForPrinter(gfxASurface **nativeSurface) = 0;

   NS_IMETHOD BeginDocument(PRUnichar*  aTitle,
                            PRUnichar*  aPrintToFileName,
                            PRInt32     aStartPage, 
                            PRInt32     aEndPage) = 0;

   NS_IMETHOD EndDocument() = 0;
   NS_IMETHOD BeginPage() = 0;
   NS_IMETHOD EndPage() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDeviceContextSpec,
                              NS_IDEVICE_CONTEXT_SPEC_IID)

#endif
