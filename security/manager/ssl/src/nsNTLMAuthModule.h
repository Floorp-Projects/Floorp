/* vim:set ts=2 sw=2 et cindent: */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#ifndef nsNTLMAuthModule_h__
#define nsNTLMAuthModule_h__

#include "nsIAuthModule.h"
#include "nsString.h"

class nsNTLMAuthModule : public nsIAuthModule
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTHMODULE

  nsNTLMAuthModule() {}
  virtual ~nsNTLMAuthModule();

  nsresult InitTest();

private:
  nsString mDomain;
  nsString mUsername;
  nsString mPassword;
};

#define NS_NTLMAUTHMODULE_CLASSNAME \
  "nsNTLMAuthModule"
#define NS_NTLMAUTHMODULE_CONTRACTID \
  NS_AUTH_MODULE_CONTRACTID_PREFIX "ntlm"
#define NS_NTLMAUTHMODULE_CID \
{ /* a4e5888f-4fe4-4632-8e7e-745196ea7c70 */       \
  0xa4e5888f,                                      \
  0x4fe4,                                          \
  0x4632,                                          \
  {0x8e, 0x7e, 0x74, 0x51, 0x96, 0xea, 0x7c, 0x70} \
}

#endif // nsNTLMAuthModule_h__
