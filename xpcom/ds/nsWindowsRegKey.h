/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsWindowsRegKey_h__
#define nsWindowsRegKey_h__

//-----------------------------------------------------------------------------

#include "nsIWindowsRegKey.h"

/**
 * This ContractID may be used to instantiate a windows registry key object
 * via the XPCOM component manager.
 */
#define NS_WINDOWSREGKEY_CONTRACTID "@mozilla.org/windows-registry-key;1"

/**
 * This function may be used to instantiate a windows registry key object prior
 * to XPCOM being initialized.
 */
extern "C" nsresult
NS_NewWindowsRegKey(nsIWindowsRegKey **result);

//-----------------------------------------------------------------------------

#ifdef _IMPL_NS_COM

#define NS_WINDOWSREGKEY_CLASSNAME "nsWindowsRegKey"

// a53bc624-d577-4839-b8ec-bb5040a52ff4
#define NS_WINDOWSREGKEY_CID \
  { 0xa53bc624, 0xd577, 0x4839, \
    { 0xb8, 0xec, 0xbb, 0x50, 0x40, 0xa5, 0x2f, 0xf4 } }

extern nsresult
nsWindowsRegKeyConstructor(nsISupports *outer, const nsIID &iid, void **result);

#endif  // _IMPL_NS_COM

//-----------------------------------------------------------------------------

#endif  // nsWindowsRegKey_h__
