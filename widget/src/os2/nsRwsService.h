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
 * The Original Code is OS/2 Remote Workplace Server interface.
 *
 * The Initial Developer of the Original Code is
 * Richard L Walsh.
 * Portions created by the Initial Developer are Copyright (C) 2005
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

//------------------------------------------------------------------------

#ifndef nsRwsService_h__
#define nsRwsService_h__

#include "nsIRwsService.h"
#include "rws.h"

// e314efd1-f4ef-49e0-bd98-12d4e87a63a7
#define NS_RWSSERVICE_CID \
{ 0xe314efd1, 0xf4ef,0x49e0, { 0xbd, 0x98, 0x12, 0xd4, 0xe8, 0x7a, 0x63, 0xa7 } }

#define NS_RWSSERVICE_CONTRACTID "@mozilla.org/rwsos2;1"

//------------------------------------------------------------------------

NS_IMETHODIMP nsRwsServiceConstructor(nsISupports *aOuter, REFNSIID aIID,
                                      void **aResult);

//------------------------------------------------------------------------

class ExtCache;

class nsRwsService : public nsIRwsService, public nsIObserver
{
public:
  NS_DECL_NSIRWSSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_ISUPPORTS

  nsRwsService();

private:
  ~nsRwsService();

protected:
  static nsresult RwsConvert(PRUint32 type, PRUint32 value, PRUint32 *result);
  static nsresult RwsConvert(PRUint32 type, PRUint32 value, nsAString& result);

  ExtCache *mExtCache;
};

//------------------------------------------------------------------------

#endif // nsRwsService_h__
