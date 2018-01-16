/* vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_Helpers_h_
#define mozilla_places_Helpers_h_

/**
 * This file contains helper classes used by various bits of Places code.
 */

#include "mozilla/storage.h"
#include "nsIURI.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "prtime.h"
#include "mozilla/Telemetry.h"

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// Asynchronous Statement Callback Helper

class WeakAsyncStatementCallback : public mozIStorageStatementCallback
{
public:
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK
  WeakAsyncStatementCallback() {}

protected:
  virtual ~WeakAsyncStatementCallback() {}
};

class AsyncStatementCallback : public WeakAsyncStatementCallback
{
public:
  NS_DECL_ISUPPORTS
  AsyncStatementCallback() {}

protected:
  virtual ~AsyncStatementCallback() {}
};

/**
 * Macros to use in place of NS_DECL_MOZISTORAGESTATEMENTCALLBACK to declare the
 * methods this class assumes silent or notreached.
 */
#define NS_DECL_ASYNCSTATEMENTCALLBACK \
  NS_IMETHOD HandleResult(mozIStorageResultSet *) override; \
  NS_IMETHOD HandleCompletion(uint16_t) override;

/**
 * Utils to bind a specified URI (or URL) to a statement or binding params, at
 * the specified index or name.
 * @note URIs are always bound as UTF8.
 */
class URIBinder // static
{
public:
  // Bind URI to statement by index.
  static nsresult Bind(mozIStorageStatement* statement,
                       int32_t index,
                       nsIURI* aURI);
  // Statement URLCString to statement by index.
  static nsresult Bind(mozIStorageStatement* statement,
                       int32_t index,
                       const nsACString& aURLString);
  // Bind URI to statement by name.
  static nsresult Bind(mozIStorageStatement* statement,
                       const nsACString& aName,
                       nsIURI* aURI);
  // Bind URLCString to statement by name.
  static nsresult Bind(mozIStorageStatement* statement,
                       const nsACString& aName,
                       const nsACString& aURLString);
  // Bind URI to params by index.
  static nsresult Bind(mozIStorageBindingParams* aParams,
                       int32_t index,
                       nsIURI* aURI);
  // Bind URLCString to params by index.
  static nsresult Bind(mozIStorageBindingParams* aParams,
                       int32_t index,
                       const nsACString& aURLString);
  // Bind URI to params by name.
  static nsresult Bind(mozIStorageBindingParams* aParams,
                       const nsACString& aName,
                       nsIURI* aURI);
  // Bind URLCString to params by name.
  static nsresult Bind(mozIStorageBindingParams* aParams,
                       const nsACString& aName,
                       const nsACString& aURLString);
};

/**
 * This extracts the hostname from the URI and reverses it in the
 * form that we use (always ending with a "."). So
 * "http://microsoft.com/" becomes "moc.tfosorcim."
 *
 * The idea behind this is that we can create an index over the items in
 * the reversed host name column, and then query for as much or as little
 * of the host name as we feel like.
 *
 * For example, the query "host >= 'gro.allizom.' AND host < 'gro.allizom/'
 * Matches all host names ending in '.mozilla.org', including
 * 'developer.mozilla.org' and just 'mozilla.org' (since we define all
 * reversed host names to end in a period, even 'mozilla.org' matches).
 * The important thing is that this operation uses the index. Any substring
 * calls in a select statement (even if it's for the beginning of a string)
 * will bypass any indices and will be slow).
 *
 * @param aURI
 *        URI that contains spec to reverse
 * @param aRevHost
 *        Out parameter
 */
nsresult GetReversedHostname(nsIURI* aURI, nsString& aRevHost);

/**
 * Similar method to GetReversedHostName but for strings
 */
void GetReversedHostname(const nsString& aForward, nsString& aRevHost);

/**
 * Reverses a string.
 *
 * @param aInput
 *        The string to be reversed
 * @param aReversed
 *        Output parameter will contain the reversed string
 */
void ReverseString(const nsString& aInput, nsString& aReversed);

/**
 * Generates an 12 character guid to be used by bookmark and history entries.
 *
 * @note This guid uses the characters a-z, A-Z, 0-9, '-', and '_'.
 */
nsresult GenerateGUID(nsACString& _guid);

/**
 * Determines if the string is a valid guid or not.
 *
 * @param aGUID
 *        The guid to test.
 * @return true if it is a valid guid, false otherwise.
 */
bool IsValidGUID(const nsACString& aGUID);

/**
 * Truncates the title if it's longer than TITLE_LENGTH_MAX.
 *
 * @param aTitle
 *        The title to truncate (if necessary)
 * @param aTrimmed
 *        Output parameter to return the trimmed string
 */
void TruncateTitle(const nsACString& aTitle, nsACString& aTrimmed);

/**
 * Round down a PRTime value to milliseconds precision (...000).
 *
 * @param aTime
 *        a PRTime value.
 * @return aTime rounded down to milliseconds precision.
 */
PRTime RoundToMilliseconds(PRTime aTime);

/**
 * Round down PR_Now() to milliseconds precision.
 *
 * @return @see PR_Now, RoundToMilliseconds.
 */
PRTime RoundedPRNow();

nsresult HashURL(const nsAString& aSpec, const nsACString& aMode,
                 uint64_t *_hash);

/**
 * Used to finalize a statementCache on a specified thread.
 */
template<typename StatementType>
class FinalizeStatementCacheProxy : public Runnable
{
public:
  /**
   * Constructor.
   *
   * @param aStatementCache
   *        The statementCache that should be finalized.
   * @param aOwner
   *        The object that owns the statement cache.  This runnable will hold
   *        a strong reference to it so aStatementCache will not disappear from
   *        under us.
   */
  FinalizeStatementCacheProxy(
    mozilla::storage::StatementCache<StatementType>& aStatementCache,
    nsISupports* aOwner)
    : Runnable("places::FinalizeStatementCacheProxy")
    , mStatementCache(aStatementCache)
    , mOwner(aOwner)
    , mCallingThread(do_GetCurrentThread())
  {
  }

  NS_IMETHOD Run() override
  {
    mStatementCache.FinalizeStatements();
    // Release the owner back on the calling thread.
    NS_ProxyRelease("FinalizeStatementCacheProxy::mOwner",
      mCallingThread, mOwner.forget());
    return NS_OK;
  }

protected:
  mozilla::storage::StatementCache<StatementType>& mStatementCache;
  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<nsIThread> mCallingThread;
};

/**
 * Determines if a visit should be marked as hidden given its transition type
 * and whether or not it was a redirect.
 *
 * @param aIsRedirect
 *        True if this visit was a redirect, false otherwise.
 * @param aTransitionType
 *        The transition type of the visit.
 * @return true if this visit should be hidden.
 */
bool GetHiddenState(bool aIsRedirect,
                    uint32_t aTransitionType);

/**
 * Used to notify a topic to system observers on async execute completion.
 */
class AsyncStatementCallbackNotifier : public AsyncStatementCallback
{
public:
  explicit AsyncStatementCallbackNotifier(const char* aTopic)
    : mTopic(aTopic)
  {
  }

  NS_IMETHOD HandleCompletion(uint16_t aReason) override;

private:
  const char* mTopic;
};

/**
 * Used to notify a topic to system observers on async execute completion.
 */
class AsyncStatementTelemetryTimer : public AsyncStatementCallback
{
public:
  explicit AsyncStatementTelemetryTimer(Telemetry::HistogramID aHistogramId,
                                        TimeStamp aStart = TimeStamp::Now())
    : mHistogramId(aHistogramId)
    , mStart(aStart)
  {
  }

  NS_IMETHOD HandleCompletion(uint16_t aReason) override;

private:
  const Telemetry::HistogramID mHistogramId;
  const TimeStamp mStart;
};

} // namespace places
} // namespace mozilla

#endif // mozilla_places_Helpers_h_
