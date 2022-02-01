/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nss_policy_h_
#define nss_policy_h_

#include "prtypes.h"
#include "secoid.h"
#include "nss.h"

namespace nss_test {

// container class to hold all a temp policy
class NssPolicy {
 public:
  NssPolicy() : oid_(SEC_OID_UNKNOWN), set_(0), clear_(0) {}
  NssPolicy(SECOidTag _oid, PRUint32 _set, PRUint32 _clear)
      : oid_(_oid), set_(_set), clear_(_clear) {}
  NssPolicy(const NssPolicy &p)
      : oid_(p.oid_), set_(p.set_), clear_(p.clear_) {}
  // clone the current policy for this oid
  NssPolicy(SECOidTag _oid) : oid_(_oid), set_(0), clear_(0) {
    NSS_GetAlgorithmPolicy(_oid, &set_);
    clear_ = ~set_;
  }
  SECOidTag oid(void) const { return oid_; }
  PRUint32 set(void) const { return set_; }
  PRUint32 clear(void) const { return clear_; }
  operator bool() const { return oid_ != SEC_OID_UNKNOWN; }

 private:
  SECOidTag oid_;
  PRUint32 set_;
  PRUint32 clear_;
};

// container class to hold a temp option
class NssOption {
 public:
  NssOption() : id_(-1), value_(0) {}
  NssOption(PRInt32 _id, PRInt32 _value) : id_(_id), value_(_value) {}
  NssOption(const NssOption &o) : id_(o.id_), value_(o.value_) {}
  // clone the current option for this id
  NssOption(PRInt32 _id) : id_(_id), value_(0) { NSS_OptionGet(id_, &value_); }
  PRInt32 id(void) const { return id_; }
  PRInt32 value(void) const { return value_; }
  operator bool() const { return id_ != -1; }

 private:
  PRInt32 id_;
  PRInt32 value_;
};

// set the policy indicated in NssPolicy and restor the old policy
// when we go out of scope
class NssManagePolicy {
 public:
  NssManagePolicy(const NssPolicy &p, const NssOption &o)
      : policy_(p), save_policy_(~(PRUint32)0), option_(o), save_option_(0) {
    if (p) {
      (void)NSS_GetAlgorithmPolicy(p.oid(), &save_policy_);
      (void)NSS_SetAlgorithmPolicy(p.oid(), p.set(), p.clear());
    }
    if (o) {
      (void)NSS_OptionGet(o.id(), &save_option_);
      (void)NSS_OptionSet(o.id(), o.value());
    }
  }
  ~NssManagePolicy() {
    if (policy_) {
      (void)NSS_SetAlgorithmPolicy(policy_.oid(), save_policy_, ~save_policy_);
    }
    if (option_) {
      (void)NSS_OptionSet(option_.id(), save_option_);
    }
  }

 private:
  NssPolicy policy_;
  PRUint32 save_policy_;
  NssOption option_;
  PRInt32 save_option_;
};

// wrapping PRFileDesc this way ensures that tests that attempt to access
// PRFileDesc always correctly apply
// the policy that was bound to that socket with TlsAgent::SetPolicy().
class NssManagedFileDesc {
 public:
  NssManagedFileDesc(PRFileDesc *fd, const NssPolicy &policy,
                     const NssOption &option)
      : fd_(fd), managed_policy_(policy, option) {}
  PRFileDesc *get(void) const { return fd_; }
  operator PRFileDesc *() const { return fd_; }
  bool operator==(PRFileDesc *fd) const { return fd_ == fd; }

 private:
  PRFileDesc *fd_;
  NssManagePolicy managed_policy_;
};

}  // namespace nss_test

#endif
