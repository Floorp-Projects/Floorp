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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tony Chang <tc@google.com>
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

#ifndef _NS_KEYMODULE_H_
#define _NS_KEYMODULE_H_

#include "nsIKeyModule.h"
#include "pk11pub.h"

#define NS_KEYMODULEOBJECT_CLASSNAME "Key Object Component"
/* eae599aa-ecef-49c6-a8af-6ddcc6feb484 */
#define NS_KEYMODULEOBJECT_CID   \
{ 0xeae599aa, 0xecef, 0x49c6, {0xa8, 0xaf, 0x6d, 0xdc, 0xc6, 0xfe, 0xb4, 0x84} }
#define NS_KEYMODULEOBJECT_CONTRACTID "@mozilla.org/security/keyobject;1"

#define NS_KEYMODULEOBJECTFACTORY_CLASSNAME "Key Object Factory Component"
/* a39e0e9d-e567-41e3-b12c-5df67f18174d */
#define NS_KEYMODULEOBJECTFACTORY_CID   \
{ 0xa39e0e9d, 0xe567, 0x41e3, {0xb1, 0x2c, 0x5d, 0xf6, 0x7f, 0x18, 0x17, 0x4d} }
#define NS_KEYMODULEOBJECTFACTORY_CONTRACTID \
"@mozilla.org/security/keyobjectfactory;1"

class nsKeyObject : public nsIKeyObject
{
public:
  nsKeyObject();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIKEYOBJECT

private:
  ~nsKeyObject();
  
  // Disallow copy constructor
  nsKeyObject(nsKeyObject&);

  // 0 if not yet set, otherwise one of the nsIKeyObject::*KEY values
  PRUint32 mKeyType;
  
  // A union of our possible key types
  PK11SymKey* mSymKey;
  SECKEYPrivateKey* mPrivateKey;
  SECKEYPublicKey* mPublicKey;

  // Helper method to free memory used by keys.
  void CleanUp();
};


class nsKeyObjectFactory : public nsIKeyObjectFactory
{
public:
  nsKeyObjectFactory();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIKEYOBJECTFACTORY

private:
  ~nsKeyObjectFactory() {}

  // Disallow copy constructor
  nsKeyObjectFactory(nsKeyObjectFactory&);
};

#endif // _NS_KEYMODULE_H_
