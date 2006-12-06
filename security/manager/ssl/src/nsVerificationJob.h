/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kengert@redhat.com>
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

#ifndef _INC_NSVERIFICATIONJOB_H
#define _INC_NSVERIFICATIONJOB_H

#include "nspr.h"

#include "nsIX509Cert.h"
#include "nsIX509Cert3.h"
#include "nsICMSMessage.h"
#include "nsICMSMessage2.h"

class nsBaseVerificationJob
{
public:
  virtual ~nsBaseVerificationJob() {}
  virtual void Run() = 0;
};

class nsCertVerificationJob : public nsBaseVerificationJob
{
public:
  nsCOMPtr<nsIX509Cert> mCert;
  nsCOMPtr<nsICertVerificationListener> mListener;

  void Run();
};

class nsCertVerificationResult : public nsICertVerificationResult
{
public:
  nsCertVerificationResult();
  virtual ~nsCertVerificationResult();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICERTVERIFICATIONRESULT

private:
  nsresult mRV;
  PRUint32 mVerified;
  PRUint32 mCount;
  PRUnichar **mUsages;

friend class nsCertVerificationJob;
};

class nsSMimeVerificationJob : public nsBaseVerificationJob
{
public:
  nsSMimeVerificationJob() { digest_data = nsnull; digest_len = 0; }
  ~nsSMimeVerificationJob() { delete [] digest_data; }

  nsCOMPtr<nsICMSMessage> mMessage;
  nsCOMPtr<nsISMimeVerificationListener> mListener;

  unsigned char *digest_data;
  PRUint32 digest_len;

  void Run();
};



#endif
