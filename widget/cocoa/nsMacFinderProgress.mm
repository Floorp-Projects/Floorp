/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>

#include "nsMacFinderProgress.h"
#include "nsThreadUtils.h"
#include "nsString.h"
#include "nsObjCExceptions.h"

NS_IMPL_ISUPPORTS(nsMacFinderProgress, nsIMacFinderProgress)

nsMacFinderProgress::nsMacFinderProgress() : mProgress(nil) {}

nsMacFinderProgress::~nsMacFinderProgress() {
  if (mProgress) {
    [mProgress unpublish];
    [mProgress release];
  }
}

NS_IMETHODIMP
nsMacFinderProgress::Init(const nsAString& path,
                          nsIMacFinderProgressCanceledCallback* cancellationCallback) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSURL* pathUrl = [NSURL
      fileURLWithPath:[NSString
                          stringWithCharacters:reinterpret_cast<const unichar*>(path.BeginReading())
                                        length:path.Length()]];
  NSDictionary* userInfo = @{
    @"NSProgressFileOperationKindKey" : @"NSProgressFileOperationKindDownloading",
    @"NSProgressFileURLKey" : pathUrl
  };

  mProgress = [[NSProgress alloc] initWithParent:nil userInfo:userInfo];
  mProgress.kind = NSProgressKindFile;
  mProgress.cancellable = YES;

  nsMainThreadPtrHandle<nsIMacFinderProgressCanceledCallback> cancellationCallbackHandle(
      new nsMainThreadPtrHolder<nsIMacFinderProgressCanceledCallback>(
          "MacFinderProgress::CancellationCallback", cancellationCallback));

  mProgress.cancellationHandler = ^{
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("MacFinderProgress::Canceled", [cancellationCallbackHandle] {
          MOZ_ASSERT(NS_IsMainThread());
          cancellationCallbackHandle->Canceled();
        }));
  };

  [mProgress publish];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsMacFinderProgress::UpdateProgress(uint64_t currentProgress, uint64_t totalProgress) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;
  if (mProgress) {
    mProgress.totalUnitCount = totalProgress;
    mProgress.completedUnitCount = currentProgress;
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsMacFinderProgress::End() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (mProgress) {
    [mProgress unpublish];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
