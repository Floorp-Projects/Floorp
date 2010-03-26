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

/**
 * How to use this stepper:
 *
 * nsCOMPtr<AsyncFaviconStepper> stepper = new AsyncFaviconStepper(callback);
 * stepper->SetPageURI(aPageURI);
 * stepper->SetIconURI(aFaviconURI);
 * rv = stepper->AppendStep(new SomeStep());
 * NS_ENSURE_SUCCESS(rv, rv);
 * rv = stepper->AppendStep(new SomeOtherStep());
 * NS_ENSURE_SUCCESS(rv, rv);
 * rv = stepper->Start();
 * NS_ENSURE_SUCCESS(rv, rv);
 */

#ifndef AsyncFaviconHelpers_h_
#define AsyncFaviconHelpers_h_

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIURI.h"

#include "nsIFaviconService.h"
#include "Helpers.h"

#include "mozilla/storage.h"

#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"

#include "nsCycleCollectionParticipant.h"

#define FAVICONSTEP_FAIL_IF_FALSE(_cond) \
  FAVICONSTEP_FAIL_IF_FALSE_RV(_cond, )

#define FAVICONSTEP_FAIL_IF_FALSE_RV(_cond, _rv) \
  PR_BEGIN_MACRO \
  if (!(_cond)) { \
    NS_WARNING("AsyncFaviconStep failed!"); \
    mStepper->Failure(); \
    return _rv; \
  } \
  PR_END_MACRO

#define FAVICONSTEP_CANCEL_IF_TRUE(_cond, _notify) \
  FAVICONSTEP_CANCEL_IF_TRUE_RV(_cond, _notify, )

#define FAVICONSTEP_CANCEL_IF_TRUE_RV(_cond, _notify, _rv) \
  PR_BEGIN_MACRO \
  if (_cond) { \
    mStepper->Cancel(_notify); \
    return _rv; \
  } \
  PR_END_MACRO

#define ICON_STATUS_UNKNOWN 0
#define ICON_STATUS_CHANGED 1 << 0
#define ICON_STATUS_SAVED 1 << 1
#define ICON_STATUS_ASSOCIATED 1 << 2

namespace mozilla {
namespace places {


// Forward declarations.
class AsyncFaviconStepperInternal;
class AsyncFaviconStepper;


/**
 * Executes a single async step on a favicon resource.
 * Once done, call backs to the stepper to proceed to the next step.
 */
class AsyncFaviconStep : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  AsyncFaviconStep() {}

  /**
   * Associate this step to a stepper.
   *
   * Automatically called by the stepper when the step is added to it.
   * @see AsyncFaviconStepper::appendStep
   */
  void SetStepper(AsyncFaviconStepperInternal* aStepper) { mStepper = aStepper; }

  /**
   * Executes the step.  Virtual since it MUST be overridden.
   */
  virtual void Run() {};

protected:
  nsCOMPtr<AsyncFaviconStepperInternal> mStepper;
};


/**
 * Status definitions for the stepper.
 */
enum AsyncFaviconStepperStatus {
  STEPPER_INITING = 0
, STEPPER_RUNNING = 1
, STEPPER_FAILED = 2
, STEPPER_COMPLETED = 3
, STEPPER_CANCELED = 4
};


/**
 * This class provides public methods and properties to steps.
 * Any other code should use the wrapper (AsyncFaviconStepper) instead.
 *
 * @see AsyncFaviconStepper
 */
class AsyncFaviconStepperInternal : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  /**
   * Creates the stepper.
   *
   * @param aCallback
   *        An nsIFaviconDataCallback to be called when done.
   */
  AsyncFaviconStepperInternal(nsIFaviconDataCallback* aCallback);

  /**
   * Proceed to next step.
   */
  nsresult Step();

  /**
   * Called by the steps when something goes wrong.
   * Will unlink all steps and gently return.
   */
  void Failure();

  /**
   * Called by the steps when they require us to stop walking.
   * This is not an error condition, sometimes we could want to bail out to
   * avoid useless additional work.
   * Will unlink all steps and gently return.
   */
  void Cancel(bool aNotify);

  nsCOMPtr<nsIFaviconDataCallback> mCallback;
  PRInt64 mPageId;
  nsCOMPtr<nsIURI> mPageURI;
  PRInt64 mIconId;
  nsCOMPtr<nsIURI> mIconURI;
  nsCString mData;
  nsCString mMimeType;
  PRTime mExpiration;
  bool mIsRevisit;
  PRUint16 mIconStatus; // This is a bitset, see ICON_STATUS_* defines above.

private:
  enum AsyncFaviconStepperStatus mStatus;
  nsCOMArray<AsyncFaviconStep> mSteps;

  friend class AsyncFaviconStepper;
};


/**
 * Walks through an ordered list of AsyncFaviconSteps.
 * Each step call backs the stepper that will proceed to the next one.
 * When all steps are complete it calls aCallback, if valid.
 *
 * This class is a wrapper around AsyncFaviconStepperInternal, where the actual
 * work is done.
 */
class AsyncFaviconStepper : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  /**
   * Creates the stepper.
   *
   * @param aCallback
   *        An nsIFaviconDataCallback to call when done.
   */
  AsyncFaviconStepper(nsIFaviconDataCallback* aCallback);

  /**
   * Kick-off the first step.
   */
  nsresult Start();

  /**
   * Appends a new step to this stepper.
   *
   * @param aStep
   *        An AsyncFaviconStep to append.
   */
  nsresult AppendStep(AsyncFaviconStep* aStep);

  // Setters and getters.
  // Some definitions are inline to try getting some love from the compiler.

  void SetPageId(PRInt64 aPageId) { mStepper->mPageId = aPageId; }
  PRInt64 GetPageId() { return mStepper->mPageId; }

  void SetPageURI(nsIURI* aURI) { mStepper->mPageURI = aURI; }
  already_AddRefed<nsIURI> GetPageURI() { return mStepper->mPageURI.forget(); }

  void SetIconId(PRInt64 aIconId) { mStepper->mIconId = aIconId; }
  PRInt64 GetIconId() { return mStepper->mIconId; }

  void SetIconURI(nsIURI* aURI) { mStepper->mIconURI = aURI; }
  already_AddRefed<nsIURI> GetIconURI() { return mStepper->mIconURI.forget(); }

  nsresult SetIconData(const nsACString& aMimeType,
                       const PRUint8* aData,
                       PRUint32 aDataLen);
  nsresult GetIconData(nsACString& aMimeType,
                       const PRUint8** aData,
                       PRUint32* aDataLen);

  void SetExpiration(PRTime aExpiration) { mStepper->mExpiration = aExpiration; }
  PRTime GetExpiration() { return mStepper->mExpiration; }

private:
  nsCOMPtr<AsyncFaviconStepperInternal> mStepper;
};


/**
 * Determines the real page URI that a favicon should be stored for.
 * Will ensure we can save this icon, and return the correct bookmark to
 * associate it with.
 */
class GetEffectivePageStep : public AsyncFaviconStep
                           , public mozilla::places::AsyncStatementCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK

  GetEffectivePageStep();
  void Run();

private:
  void CheckPageAndProceed();

  PRUint8 mSubStep;
  bool mIsBookmarked;
};


/**
 * Fetch an existing icon and associated information from the database.
 */
class FetchDatabaseIconStep : public AsyncFaviconStep
                            , public mozilla::places::AsyncStatementCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK

  FetchDatabaseIconStep() {};
  void Run();
};


/**
 * Fetch an existing icon and associated information from the database.
 * Requires mDBInsertIcon statement.
 */
class EnsureDatabaseEntryStep : public AsyncFaviconStep
                              , public mozilla::places::AsyncStatementCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK

  EnsureDatabaseEntryStep() {};
  void Run();
};

enum AsyncFaviconFetchMode {
  FETCH_NEVER = 0
, FETCH_IF_MISSING = 1
, FETCH_ALWAYS = 2
};


/**
 * Fetch an icon and associated information from the network.
 * Requires mDBGetIconInfoWithPage statement.
 */
class FetchNetworkIconStep : public AsyncFaviconStep
                           , public nsIStreamListener
                           , public nsIInterfaceRequestor
                           , public nsIChannelEventSink
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(FetchNetworkIconStep, AsyncFaviconStep)
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIREQUESTOBSERVER

  FetchNetworkIconStep(enum AsyncFaviconFetchMode aFetchMode);
  void Run();

private:
  enum AsyncFaviconFetchMode mFetchMode;
  nsCOMPtr<nsIChannel> mChannel;
  nsCString mData;
};


/**
 * Saves icon data in the database if it has changed.
 */
class SetFaviconDataStep : public AsyncFaviconStep
                         , public mozilla::places::AsyncStatementCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK

  SetFaviconDataStep() {};
  void Run();
};


/**
 * Associate icon with page.
 */
class AssociateIconWithPageStep : public AsyncFaviconStep
                                , public mozilla::places::AsyncStatementCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK

  AssociateIconWithPageStep() {};
  void Run();
};


/**
 * Notify favicon changes.
 */
class NotifyStep : public AsyncFaviconStep
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NotifyStep() {};
  void Run();
};

} // namespace places
} // namespace mozilla

#endif // AsyncFaviconHelpers_h_
