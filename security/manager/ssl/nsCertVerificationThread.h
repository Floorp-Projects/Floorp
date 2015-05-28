/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSCERTVERIFICATIONTHREAD_H_
#define _NSCERTVERIFICATIONTHREAD_H_

#include "nsCOMPtr.h"
#include "nsDeque.h"
#include "nsPSMBackgroundThread.h"
#include "nsVerificationJob.h"

class nsCertVerificationThread : public nsPSMBackgroundThread
{
private:
  nsDeque mJobQ;

  virtual void Run(void);

public:
  nsCertVerificationThread();
  ~nsCertVerificationThread();

  static nsCertVerificationThread *verification_thread_singleton;
  
  static nsresult addJob(nsBaseVerificationJob *aJob);
};

#endif
