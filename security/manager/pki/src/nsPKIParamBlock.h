/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 *   Javier Delgadillo <javi@netscape.com>
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
#ifndef _NSPKIPARAMBLOCK_
#define _NSPKIPARAMBLOCK_
#include "nsCOMPtr.h"
#include "nsIPKIParamBlock.h"
#include "nsIDialogParamBlock.h"
#include "nsISupportsArray.h"

#define NS_PKIPARAMBLOCK_CID \
  { 0x0bec75a8, 0x1dd2, 0x11b2, \
    { 0x86, 0x3a, 0xf6, 0x9f, 0x77, 0xc3, 0x13, 0x71 }}

#define NS_PKIPARAMBLOCK_CONTRACTID "@mozilla.org/security/pkiparamblock;1"

class nsPKIParamBlock : public nsIPKIParamBlock,
                        public nsIDialogParamBlock
{
public:
 
  nsPKIParamBlock();
  virtual ~nsPKIParamBlock();
  nsresult Init();

  NS_DECL_NSIPKIPARAMBLOCK
  NS_DECL_NSIDIALOGPARAMBLOCK
  NS_DECL_ISUPPORTS
private:
  nsCOMPtr<nsIDialogParamBlock> mDialogParamBlock;
  nsCOMPtr<nsISupportsArray>    mSupports;
};

#endif //_NSPKIPARAMBLOCK_
