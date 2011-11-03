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
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#include "nsMemory.h"
#include "nsAutoPtr.h"
#include "nsCertVerificationThread.h"
#include "nsThreadUtils.h"

using namespace mozilla;

nsCertVerificationThread *nsCertVerificationThread::verification_thread_singleton;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCertVerificationResult, nsICertVerificationResult)

namespace {
class DispatchCertVerificationResult : public nsRunnable
{
public:
  DispatchCertVerificationResult(nsICertVerificationListener* aListener,
                                 nsIX509Cert3* aCert,
                                 nsICertVerificationResult* aResult)
    : mListener(aListener)
    , mCert(aCert)
    , mResult(aResult)
  { }

  NS_IMETHOD Run() {
    mListener->Notify(mCert, mResult);
    return NS_OK;
  }

private:
  nsCOMPtr<nsICertVerificationListener> mListener;
  nsCOMPtr<nsIX509Cert3> mCert;
  nsCOMPtr<nsICertVerificationResult> mResult;
};
} // anonymous namespace

void nsCertVerificationJob::Run()
{
  if (!mListener || !mCert)
    return;

  PRUint32 verified;
  PRUint32 count;
  PRUnichar **usages;

  nsCOMPtr<nsICertVerificationResult> ires;
  nsRefPtr<nsCertVerificationResult> vres = new nsCertVerificationResult;
  if (vres)
  {
    nsresult rv = mCert->GetUsagesArray(false, // do not ignore OCSP
                                        &verified,
                                        &count,
                                        &usages);
    vres->mRV = rv;
    if (NS_SUCCEEDED(rv))
    {
      vres->mVerified = verified;
      vres->mCount = count;
      vres->mUsages = usages;
    }

    ires = vres;
  }
  
  nsCOMPtr<nsIX509Cert3> c3 = do_QueryInterface(mCert);
  nsCOMPtr<nsIRunnable> r = new DispatchCertVerificationResult(mListener, c3, ires);
  NS_DispatchToMainThread(r);
}

void nsSMimeVerificationJob::Run()
{
  if (!mMessage || !mListener)
    return;
  
  nsresult rv;
  
  if (digest_data)
    rv = mMessage->VerifyDetachedSignature(digest_data, digest_len);
  else
    rv = mMessage->VerifySignature();
  
  nsCOMPtr<nsICMSMessage2> m2 = do_QueryInterface(mMessage);
  mListener->Notify(m2, rv);
}

nsCertVerificationThread::nsCertVerificationThread()
: mJobQ(nsnull)
{
  NS_ASSERTION(!verification_thread_singleton, 
               "nsCertVerificationThread is a singleton, caller attempts"
               " to create another instance!");
  
  verification_thread_singleton = this;
}

nsCertVerificationThread::~nsCertVerificationThread()
{
  verification_thread_singleton = nsnull;
}

nsresult nsCertVerificationThread::addJob(nsBaseVerificationJob *aJob)
{
  if (!aJob || !verification_thread_singleton)
    return NS_ERROR_FAILURE;
  
  if (!verification_thread_singleton->mThreadHandle)
    return NS_ERROR_OUT_OF_MEMORY;

  MutexAutoLock threadLock(verification_thread_singleton->mMutex);

  verification_thread_singleton->mJobQ.Push(aJob);
  verification_thread_singleton->mCond.NotifyAll();
  
  return NS_OK;
}

void nsCertVerificationThread::Run(void)
{
  while (true) {

    nsBaseVerificationJob *job = nsnull;

    {
      MutexAutoLock threadLock(verification_thread_singleton->mMutex);

      while (!exitRequested(threadLock) &&
             0 == verification_thread_singleton->mJobQ.GetSize()) {
        // no work to do ? let's wait a moment

        mCond.Wait();
      }
      
      if (exitRequested(threadLock))
        break;
      
      job = static_cast<nsBaseVerificationJob*>(mJobQ.PopFront());
    }

    if (job)
    {
      job->Run();
      delete job;
    }
  }
  
  {
    MutexAutoLock threadLock(verification_thread_singleton->mMutex);

    while (verification_thread_singleton->mJobQ.GetSize()) {
      nsCertVerificationJob *job = 
        static_cast<nsCertVerificationJob*>(mJobQ.PopFront());
      delete job;
    }
    postStoppedEventToMainThread(threadLock);
  }
}

nsCertVerificationResult::nsCertVerificationResult()
: mRV(0),
  mVerified(0),
  mCount(0),
  mUsages(0)
{
}

nsCertVerificationResult::~nsCertVerificationResult()
{
  if (mUsages)
  {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mCount, mUsages);
  }
}

NS_IMETHODIMP
nsCertVerificationResult::GetUsagesArrayResult(PRUint32 *aVerified,
                                               PRUint32 *aCount,
                                               PRUnichar ***aUsages)
{
  if (NS_FAILED(mRV))
    return mRV;
  
  // transfer ownership
  
  *aVerified = mVerified;
  *aCount = mCount;
  *aUsages = mUsages;
  
  mVerified = 0;
  mCount = 0;
  mUsages = 0;
  
  nsresult rv = mRV;
  
  mRV = NS_ERROR_FAILURE; // this object works only once...
  
  return rv;
}
