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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kaie@netscape.com>
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

#ifndef _INC_NSSCleaner_H
#define _INC_NSSCleaner_H

/*
  NSSCleanupAutoPtrClass(CERTCertificate, CERT_DestroyCertificate)

  will produce:

  class CERTCertificateCleaner
  {
  private:
    CERTCertificateCleaner(const CERTCertificateCleaner&);
    CERTCertificateCleaner();
    void operator=(const CERTCertificateCleaner&);
    CERTCertificate *&object;
  public:
    CERTCertificateCleaner(CERTCertificate *&a_object)
      :object(a_object) {}
    ~CERTCertificateCleaner() {
      if (object) {
        CERT_DestroyCertificate(object);
        object = nsnull;
      }
    }
  };
  
  By making default and copy constructor, and assignment operator
  private, we will make sure nobody will be able to use it.
  Not defining bodies for them is an additional safeguard.
  
  This class is not designed to allow being passed around.
  It's just for automatic cleanup of a local variable.
  
  
  By storing a reference to the underlying pointer,
  we will zero out the given pointer variable,
  making sure it will not be used after it has been freed.
  
  Even better, in case the underlying pointer variable gets
  assigned another value, this will be recognized, and
  the latest value stored in the pointer will be freed.
  
  
  In order to not require everybody to have all the NSS
  includes in their implementation files,
  we don't declare the classes here.
  
*/

#define NSSCleanupAutoPtrClass(nsstype, cleanfunc) \
class nsstype##Cleaner                             \
{                                                  \
private:                                           \
  nsstype##Cleaner(const nsstype##Cleaner&);       \
  nsstype##Cleaner();                              \
  void operator=(const nsstype##Cleaner&);         \
  nsstype *&object;                                \
public:                                            \
  nsstype##Cleaner(nsstype *&a_object)             \
    :object(a_object) {}                           \
  ~nsstype##Cleaner() {                            \
    if (object) {                                  \
      cleanfunc(object);                           \
      object = nsnull;                             \
    }                                              \
  }                                                \
  void detach() {object=nsnull;}                   \
};

#define NSSCleanupAutoPtrClass_WithParam(nsstype, cleanfunc, namesuffix, paramvalue) \
class nsstype##Cleaner##namesuffix                 \
{                                                  \
private:                                           \
  nsstype##Cleaner##namesuffix(const nsstype##Cleaner##namesuffix &); \
  nsstype##Cleaner##namesuffix();                                     \
  void operator=(const nsstype##Cleaner##namesuffix &);               \
  nsstype *&object;                                \
public:                                            \
  nsstype##Cleaner##namesuffix(nsstype *&a_object) \
    :object(a_object) {}                           \
  ~nsstype##Cleaner##namesuffix() {                \
    if (object) {                                  \
      cleanfunc(object, paramvalue);               \
      object = nsnull;                             \
    }                                              \
  }                                                \
  void detach() {object=nsnull;}                   \
};

#endif
