/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCertVerificationThread.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"

using namespace mozilla;

nsCertVerificationThread *nsCertVerificationThread::verification_thread_singleton;

NS_IMPL_ISUPPORTS(nsCertVerificationResult, nsICertVerificationResult)

namespace {
class DispatchCertVerificationResult : public nsRunnable
{
public:
  DispatchCertVerificationResult(const nsMainThreadPtrHandle<nsICertVerificationListener>& aListener,
                                 nsIX509Cert* aCert,
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
  nsMainThreadPtrHandle<nsICertVerificationListener> mListener;
  nsCOMPtr<nsIX509Cert> mCert;
  nsCOMPtr<nsICertVerificationResult> mResult;
};
} // anonymous namespace

void nsCertVerificationJob::Run()
{
  if (!mListener || !mCert)
    return;

  uint32_t verified;
  uint32_t count;
  char16_t **usages;

  nsCOMPtr<nsICertVerificationResult> ires;
  RefPtr<nsCertVerificationResult> vres(new nsCertVerificationResult);
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

  nsCOMPtr<nsIRunnable> r = new DispatchCertVerificationResult(mListener, mCert, ires);
  NS_DispatchToMainThread(r);
}

nsCertVerificationThread::nsCertVerificationThread()
: mJobQ(nullptr)
{
  NS_ASSERTION(!verification_thread_singleton, 
               "nsCertVerificationThread is a singleton, caller attempts"
               " to create another instance!");
  
  verification_thread_singleton = this;
}

nsCertVerificationThread::~nsCertVerificationThread()
{
  verification_thread_singleton = nullptr;
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

    nsBaseVerificationJob *job = nullptr;

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
: mRV(NS_OK),
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
nsCertVerificationResult::GetUsagesArrayResult(uint32_t *aVerified,
                                               uint32_t *aCount,
                                               char16_t ***aUsages)
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
