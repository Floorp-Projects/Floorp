/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSX509CERTVALIDITY_H_
#define _NSX509CERTVALIDITY_H_

#include "nsIX509CertValidity.h"

#include "certt.h"

class nsX509CertValidity : public nsIX509CertValidity
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIX509CERTVALIDITY

  nsX509CertValidity();
  explicit nsX509CertValidity(CERTCertificate *cert);

protected:
  virtual ~nsX509CertValidity();
  /* additional members */

private:
  PRTime mNotBefore, mNotAfter;
  bool mTimesInitialized;
};

#endif
