/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
        object = nullptr;
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
  explicit nsstype##Cleaner(nsstype *&a_object)    \
    :object(a_object) {}                           \
  ~nsstype##Cleaner() {                            \
    if (object) {                                  \
      cleanfunc(object);                           \
      object = nullptr;                             \
    }                                              \
  }                                                \
  void detach() {object=nullptr;}                   \
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
  explicit nsstype##Cleaner##namesuffix(nsstype *&a_object)           \
    :object(a_object) {}                           \
  ~nsstype##Cleaner##namesuffix() {                \
    if (object) {                                  \
      cleanfunc(object, paramvalue);               \
      object = nullptr;                             \
    }                                              \
  }                                                \
  void detach() {object=nullptr;}                   \
};

#include "certt.h"

class CERTVerifyLogContentsCleaner
{
public:
  explicit CERTVerifyLogContentsCleaner(CERTVerifyLog *&cvl);
  ~CERTVerifyLogContentsCleaner();
private:
  CERTVerifyLog *&m_cvl;
};

#endif
