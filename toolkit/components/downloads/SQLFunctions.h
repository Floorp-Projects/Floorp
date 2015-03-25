/* vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_downloads_SQLFunctions_h
#define mozilla_downloads_SQLFunctions_h

#include "mozIStorageFunction.h"
#include "mozilla/Attributes.h"

class nsCString;
class mozIStorageConnection;

namespace mozilla {
namespace downloads {

/**
 * SQL function to generate a GUID for a place or bookmark item.  This is just
 * a wrapper around GenerateGUID in SQLFunctions.cpp.
 *
 * @return a guid for the item.
 * @see toolkit/components/places/SQLFunctions.h - keep this in sync
 */
class GenerateGUIDFunction final : public mozIStorageFunction
{
  ~GenerateGUIDFunction() {}
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  /**
   * Registers the function with the specified database connection.
   *
   * @param aDBConn
   *        The database connection to register with.
   */
   static nsresult create(mozIStorageConnection *aDBConn);
};

nsresult GenerateGUID(nsCString& _guid);

} // namespace downloads
} // namespace mozilla

#endif
