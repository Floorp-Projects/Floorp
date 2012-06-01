/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NSOCSPRESPONDER_H__
#define __NSOCSPRESPONDER_H__

#include "nsIOCSPResponder.h"
#include "nsString.h"

#include "certt.h"

class nsOCSPResponder : public nsIOCSPResponder
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOCSPRESPONDER

  nsOCSPResponder();
  nsOCSPResponder(const PRUnichar*, const PRUnichar*);
  virtual ~nsOCSPResponder();
  /* additional members */
  static PRInt32 CmpCAName(nsIOCSPResponder *a, nsIOCSPResponder *b);
  static PRInt32 CompareEntries(nsIOCSPResponder *a, nsIOCSPResponder *b);
  static bool IncludeCert(CERTCertificate *aCert);
private:
  nsString mCA;
  nsString mURL;
};

#endif
