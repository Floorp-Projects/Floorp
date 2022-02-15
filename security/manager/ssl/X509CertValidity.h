/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef X509CertValidity_h
#define X509CertValidity_h

#include "mozpkix/Input.h"
#include "nsIX509CertValidity.h"
#include "prtime.h"

class X509CertValidity : public nsIX509CertValidity {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERTVALIDITY

  explicit X509CertValidity(mozilla::pkix::Input certDER);

  X509CertValidity(const X509CertValidity& x) = delete;
  X509CertValidity& operator=(const X509CertValidity& x) = delete;

 protected:
  virtual ~X509CertValidity() = default;

 private:
  PRTime mNotBefore;
  PRTime mNotAfter;
  bool mTimesInitialized;
};

#endif  // X509CertValidity_h
