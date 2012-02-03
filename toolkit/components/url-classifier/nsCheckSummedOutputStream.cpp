//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Url Classifier code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
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

#include "nsILocalFile.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nsISupportsImpl.h"
#include "nsCheckSummedOutputStream.h"

////////////////////////////////////////////////////////////////////////////////
// nsCheckSummedOutputStream

NS_IMPL_ISUPPORTS_INHERITED3(nsCheckSummedOutputStream,
                             nsSafeFileOutputStream,
                             nsISafeOutputStream,
                             nsIOutputStream,
                             nsIFileOutputStream)

NS_IMETHODIMP
nsCheckSummedOutputStream::Init(nsIFile* file, PRInt32 ioFlags, PRInt32 perm,
                                PRInt32 behaviorFlags)
{
  nsresult rv;
  mHash = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mHash->Init(nsICryptoHash::MD5);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsSafeFileOutputStream::Init(file, ioFlags, perm, behaviorFlags);
}

NS_IMETHODIMP
nsCheckSummedOutputStream::Finish()
{
  nsresult rv = mHash->Finish(false, mCheckSum);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 written;
  rv = nsSafeFileOutputStream::Write(reinterpret_cast<const char*>(mCheckSum.BeginReading()),
                                     mCheckSum.Length(), &written);
  NS_ASSERTION(written == mCheckSum.Length(), "Error writing stream checksum");
  NS_ENSURE_SUCCESS(rv, rv);

  return nsSafeFileOutputStream::Finish();
}

NS_IMETHODIMP
nsCheckSummedOutputStream::Write(const char *buf, PRUint32 count, PRUint32 *result)
{
  nsresult rv = mHash->Update(reinterpret_cast<const uint8*>(buf), count);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsSafeFileOutputStream::Write(buf, count, result);
}

////////////////////////////////////////////////////////////////////////////////
