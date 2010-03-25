/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (original author)
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

#include "AsyncFaviconHelpers.h"

#include "mozilla/storage.h"

#define TO_CHARBUFFER(_buffer) \
  reinterpret_cast<char*>(const_cast<PRUint8*>(_buffer))
#define TO_INTBUFFER(_string) \
  reinterpret_cast<PRUint8*>(const_cast<char*>(_string.get()))

namespace mozilla {
namespace places {


////////////////////////////////////////////////////////////////////////////////
//// AsyncFaviconStep

NS_IMPL_ISUPPORTS0(
  AsyncFaviconStep
)


////////////////////////////////////////////////////////////////////////////////
//// AsyncFaviconStepper

NS_IMPL_ISUPPORTS0(
  AsyncFaviconStepper
)


AsyncFaviconStepper::AsyncFaviconStepper(nsIFaviconDataCallback* aCallback)
  : mStepper(new AsyncFaviconStepperInternal(aCallback))
{
}


nsresult
AsyncFaviconStepper::Start()
{
  FAVICONSTEP_FAIL_IF_FALSE_RV(mStepper->mStatus == STEPPER_INITING,
                               NS_ERROR_FAILURE);
  mStepper->mStatus = STEPPER_RUNNING;
  nsresult rv = mStepper->Step();
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);
  return NS_OK;
}


nsresult
AsyncFaviconStepper::AppendStep(AsyncFaviconStep* aStep)
{
  FAVICONSTEP_FAIL_IF_FALSE_RV(aStep, NS_ERROR_OUT_OF_MEMORY);
  FAVICONSTEP_FAIL_IF_FALSE_RV(mStepper->mStatus == STEPPER_INITING,
                               NS_ERROR_FAILURE);

  aStep->SetStepper(mStepper);
  nsresult rv = mStepper->mSteps.AppendObject(aStep);
  FAVICONSTEP_FAIL_IF_FALSE_RV(NS_SUCCEEDED(rv), rv);
  return NS_OK;
}


nsresult
AsyncFaviconStepper::SetIconData(const nsACString& aMimeType,
                                 const PRUint8* _data,
                                 PRUint32 _dataLen)
{
  mStepper->mMimeType = aMimeType;
  mStepper->mData.Adopt(TO_CHARBUFFER(_data), _dataLen);
  mStepper->mIconStatus |= ICON_STATUS_CHANGED;
  return NS_OK;
}


nsresult
AsyncFaviconStepper::GetIconData(nsACString& aMimeType,
                                 const PRUint8** aData,
                                 PRUint32* aDataLen)
{
  PRUint32 dataLen = mStepper->mData.Length();
  NS_ENSURE_TRUE(dataLen > 0, NS_ERROR_NOT_AVAILABLE);
  aMimeType = mStepper->mMimeType;
  *aDataLen = dataLen;
  *aData = TO_INTBUFFER(mStepper->mData);
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
//// AsyncFaviconStepperInternal

NS_IMPL_ISUPPORTS0(
  AsyncFaviconStepperInternal
)


AsyncFaviconStepperInternal::AsyncFaviconStepperInternal(
  nsIFaviconDataCallback* aCallback
)
  : mCallback(aCallback)
  , mPageId(0)
  , mIconId(0)
  , mExpiration(0)
  , mIsRevisit(false)
  , mIconStatus(ICON_STATUS_UNKNOWN)
  , mStatus(STEPPER_INITING)
{
}


nsresult
AsyncFaviconStepperInternal::Step()
{
  if (mStatus != STEPPER_RUNNING) {
    Failure();
    return NS_ERROR_FAILURE;
  }

  PRInt32 stepCount = mSteps.Count();
  if (!stepCount) {
    mStatus = STEPPER_COMPLETED;
    // Ran all steps, let's notify.
    if (mCallback) {
      (void)mCallback->OnFaviconDataAvailable(mIconURI,
                                              mData.Length(),
                                              TO_INTBUFFER(mData),
                                              mMimeType);
    }
    return NS_OK;
  }

  // Get the next step.
  nsCOMPtr<AsyncFaviconStep> step = mSteps[0];
  if (!step) {
    Failure();
    return NS_ERROR_UNEXPECTED;
  }

  // Break the cycle.
  nsresult rv = mSteps.RemoveObjectAt(0);
  if (NS_FAILED(rv)) {
    Failure();
    return NS_ERROR_UNEXPECTED;
  }

  // Run the extracted step.
  step->Run();

  return NS_OK;
}


void
AsyncFaviconStepperInternal::Failure()
{
  mStatus = STEPPER_FAILED;

  // Break the cycles so steps are collected.
  mSteps.Clear();
}


void
AsyncFaviconStepperInternal::Cancel(bool aNotify)
{
  mStatus = STEPPER_CANCELED;

  // Break the cycles so steps are collected.
  mSteps.Clear();

  if (aNotify && mCallback) {
    (void)mCallback->OnFaviconDataAvailable(mIconURI,
                                            mData.Length(),
                                            TO_INTBUFFER(mData),
                                            mMimeType);
  }
}


////////////////////////////////////////////////////////////////////////////////

} // namespace places
} // namespace mozilla
