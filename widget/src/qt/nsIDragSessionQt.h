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
 *      Denis Issoupov <denis@macadamian.com>
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
#ifndef nsIDragSessionQt_h__
#define nsIDragSessionQt_h__

#include "nsISupports.h"
#include <qdragobject.h> 

#define NS_IDRAGSESSIONQT_IID      \
{ 0x36c4c381, 0x09e3, 0x11d4, { 0xb0, 0x33, 0xa4, 0x20, 0xf4, 0x2c, 0xfd, 0x7c } };

class nsIDragSessionQt : public nsISupports
{
  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDRAGSESSIONQT_IID)

  /**
    * Since the drag may originate in an external application, we need some
    * way of communicating the QDragObject to the session so it can use it
    * when filling in data requests.
    *
    */
   NS_IMETHOD SetDragReference(QMimeSource* aDragRef) = 0; 
};

#endif
