/* vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#ifndef mozilla_places_Helpers_h_
#define mozilla_places_Helpers_h_

/**
 * This file contains helper classes used by various bits of Places code.
 */

#include "mozilla/storage.h"
#include "nsIURI.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// Asynchronous Statement Callback Helper

class AsyncStatementCallback : public mozIStorageStatementCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK
  AsyncStatementCallback() {}

protected:
  virtual ~AsyncStatementCallback() {}
};

/**
 * Macros to use in place of NS_DECL_MOZISTORAGESTATEMENTCALLBACK to declare the
 * methods this class assumes silent or notreached.
 */
#define NS_DECL_ASYNCSTATEMENTCALLBACK \
  NS_IMETHOD HandleResult(mozIStorageResultSet *); \
  NS_IMETHOD HandleCompletion(PRUint16);

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
                       PRInt32 index,
                       nsIURI* aURI);
  // Statement URLCString to statement by index.
  static nsresult Bind(mozIStorageStatement* statement,
                       PRInt32 index,
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
                       PRInt32 index,
                       nsIURI* aURI);
  // Bind URLCString to params by index.
  static nsresult Bind(mozIStorageBindingParams* aParams,
                       PRInt32 index,
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
nsresult GenerateGUID(nsCString& _guid);

/**
 * Determines if the string is a valid guid or not.
 *
 * @param aGUID
 *        The guid to test.
 * @return true if it is a valid guid, false otherwise.
 */
bool IsValidGUID(const nsCString& aGUID);

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
 * Used to finalize a statementCache on a specified thread.
 */
template<typename StatementType>
class FinalizeStatementCacheProxy : public nsRunnable
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
    nsISupports* aOwner
  )
  : mStatementCache(aStatementCache)
  , mOwner(aOwner)
  , mCallingThread(do_GetCurrentThread())
  {
  }

  NS_IMETHOD Run()
  {
    mStatementCache.FinalizeStatements();
    // Release the owner back on the calling thread.
    (void)NS_ProxyRelease(mCallingThread, mOwner);
    return NS_OK;
  }

protected:
  mozilla::storage::StatementCache<StatementType>& mStatementCache;
  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<nsIThread> mCallingThread;
};

/**
 * Forces a WAL checkpoint. This will cause all transactions stored in the
 * journal file to be committed to the main database.
 * 
 * @note The checkpoint will force a fsync/flush.
 */
void ForceWALCheckpoint();

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
                    PRUint32 aTransitionType);

/**
 * Notifies a specified topic via the observer service.
 */
class PlacesEvent : public nsRunnable
                  , public mozIStorageCompletionCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_MOZISTORAGECOMPLETIONCALLBACK

  PlacesEvent(const char* aTopic);
protected:
  void Notify();

  const char* const mTopic;
};

} // namespace places
} // namespace mozilla

#endif // mozilla_places_Helpers_h_
