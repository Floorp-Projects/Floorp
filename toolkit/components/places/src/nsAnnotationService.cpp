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
 * The Original Code is Mozilla Annotation Service
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
 *   Asaf Romano <mano@mozilla.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

#include "nsAnnotationService.h"
#include "mozStorageCID.h"
#include "nsNavHistory.h"
#include "nsNetUtil.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageStatement.h"
#include "mozIStorageFunction.h"
#include "mozStorageHelper.h"
#include "nsIServiceManager.h"
#include "nsIVariant.h"
#include "nsString.h"
#include "nsVariant.h"
#include "nsNavBookmarks.h"
#include "nsPlacesTables.h"

const PRInt32 nsAnnotationService::kAnnoIndex_ID = 0;
const PRInt32 nsAnnotationService::kAnnoIndex_PageOrItem = 1;
const PRInt32 nsAnnotationService::kAnnoIndex_Name = 2;
const PRInt32 nsAnnotationService::kAnnoIndex_MimeType = 3;
const PRInt32 nsAnnotationService::kAnnoIndex_Content = 4;
const PRInt32 nsAnnotationService::kAnnoIndex_Flags = 5;
const PRInt32 nsAnnotationService::kAnnoIndex_Expiration = 6;
const PRInt32 nsAnnotationService::kAnnoIndex_Type = 7;
const PRInt32 nsAnnotationService::kAnnoIndex_DateAdded = 8;
const PRInt32 nsAnnotationService::kAnnoIndex_LastModified = 9;

nsAnnotationService* nsAnnotationService::gAnnotationService;

NS_IMPL_ISUPPORTS1(nsAnnotationService,
                   nsIAnnotationService)

// nsAnnotationService::nsAnnotationService

nsAnnotationService::nsAnnotationService()
{
  NS_ASSERTION(!gAnnotationService,
               "ATTEMPTING TO CREATE TWO INSTANCES OF THE ANNOTATION SERVICE!");
  gAnnotationService = this;
}


// nsAnnotationService::~nsAnnotationService

nsAnnotationService::~nsAnnotationService()
{
  NS_ASSERTION(gAnnotationService == this,
               "Deleting a non-singleton annotation service");
  if (gAnnotationService == this)
    gAnnotationService = nsnull;
}


// nsAnnotationService::Init

nsresult
nsAnnotationService::Init()
{
  nsresult rv;

  // The history service will normally already be created and will call our
  // static InitTables function. It will get autocreated here if it hasn't
  // already been created.
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  if (! history)
    return NS_ERROR_FAILURE;
  mDBConn = history->GetStorageConnection();

  // annotation statements

  // mDBSetAnnotation
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_annos "
      "SET mime_type = ?4, content = ?5, flags = ?6, expiration = ?7, "
        "type = ?8, lastModified = ?10 "
      "WHERE id = ?1"),
    getter_AddRefs(mDBSetAnnotation));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBSetItemAnnotation
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE moz_items_annos "
      "SET mime_type = ?4, content = ?5, flags = ?6, expiration = ?7, "
        "type = ?8, lastModified = ?10 "
      "WHERE id = ?1"),
    getter_AddRefs(mDBSetItemAnnotation));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetAnnotation
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT * "
      "FROM moz_annos "
      "WHERE place_id = ?1 AND anno_attribute_id = "
        "(SELECT id FROM moz_anno_attributes WHERE name = ?2)"),
    getter_AddRefs(mDBGetAnnotation));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetItemAnnotation
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT * "
      "FROM moz_items_annos "
      "WHERE item_id = ?1 AND anno_attribute_id = "
        "(SELECT id FROM moz_anno_attributes WHERE name = ?2)"),
    getter_AddRefs(mDBGetItemAnnotation));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetAnnotationNames
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT n.name "
      "FROM moz_annos a "
      "JOIN moz_anno_attributes n ON a.anno_attribute_id = n.id "
      "WHERE a.place_id = ?1"),
    getter_AddRefs(mDBGetAnnotationNames));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetItemAnnotationNames
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT n.name "
      "FROM moz_items_annos a "
      "JOIN moz_anno_attributes n ON a.anno_attribute_id = n.id "
      "WHERE a.item_id = ?1"),
    getter_AddRefs(mDBGetItemAnnotationNames));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetAnnotationFromURI
  // We are not checking for duplicated ids into the unified table
  // for perf reasons, LIMIT 1 will discard duplicates faster since we
  // can only have one anno with a certain name for every place_id.
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT a.id, a.place_id, ?2, a.mime_type, a.content, a.flags, "
        "a.expiration, a.type "
      "FROM ( "
        "SELECT id FROM moz_places_temp "
        "WHERE url = ?1 "
        "UNION ALL "
        "SELECT id FROM moz_places "
        "WHERE url = ?1 "
      ") AS h JOIN moz_annos a ON h.id = a.place_id "
      "WHERE a.anno_attribute_id = "
        "(SELECT id FROM moz_anno_attributes WHERE name = ?2) "
      "LIMIT 1"),
    getter_AddRefs(mDBGetAnnotationFromURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetAnnotationFromItemId
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT a.id, a.item_id, ?2, a.mime_type, a.content, a.flags, "
        "a.expiration, a.type "
      "FROM moz_items_annos a "
      "WHERE a.item_id = ?1 AND a.anno_attribute_id = "
        "(SELECT id FROM moz_anno_attributes WHERE name = ?2)"),
    getter_AddRefs(mDBGetAnnotationFromItemId));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetAnnotationNameID
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT id FROM moz_anno_attributes WHERE name = ?1"),
    getter_AddRefs(mDBGetAnnotationNameID));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBAddAnnotationName
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO moz_anno_attributes (name) VALUES (?1)"),
    getter_AddRefs(mDBAddAnnotationName));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBAddAnnotation
  //   Note: kAnnoIndex_Name here is a name ID and not a string like the getters
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO moz_annos "
        "(place_id, anno_attribute_id, mime_type, content, flags, expiration, "
         "type, dateAdded) "
      "VALUES (?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)"),
    getter_AddRefs(mDBAddAnnotation));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBAddItemAnnotation
  //   Note: kAnnoIndex_Name here is a name ID and not a string like the getters
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO moz_items_annos "
        "(item_id, anno_attribute_id, mime_type, content, flags, expiration, "
         "type, dateAdded) "
      "VALUES (?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)"),
    getter_AddRefs(mDBAddItemAnnotation));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBRemoveAnnotation
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_annos WHERE place_id = ?1 AND anno_attribute_id = "
        "(SELECT id FROM moz_anno_attributes WHERE name = ?2)"),
    getter_AddRefs(mDBRemoveAnnotation));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBRemoveItemAnnotation
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_items_annos WHERE item_id = ?1 AND anno_attribute_id = "
        "(SELECT id FROM moz_anno_attributes WHERE name = ?2)"),
    getter_AddRefs(mDBRemoveItemAnnotation));
  NS_ENSURE_SUCCESS(rv, rv);

  // mDBGetItemsWithAnnotation
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT a.item_id FROM moz_anno_attributes n "
    "JOIN moz_items_annos a ON n.id = a.anno_attribute_id "
    "WHERE n.name = ?1"),
    getter_AddRefs(mDBGetItemsWithAnnotation));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsAnnotationService::InitTables
//
//    All commands that initialize the schema of the DB go in here. This is
//    called from history init before the dummy DB connection is started that
//    will prevent us from modifying the schema.
//
//    The history service will always be created before us (we get it at the
//    beginning of the init function which covers us if it's not).

nsresult // static
nsAnnotationService::InitTables(mozIStorageConnection* aDBConn)
{
  nsresult rv;
  PRBool exists;
  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_annos"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! exists) {
    rv = aDBConn->ExecuteSimpleSQL(CREATE_MOZ_ANNOS);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE UNIQUE INDEX moz_annos_placeattributeindex "
        "ON moz_annos (place_id, anno_attribute_id)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_anno_attributes"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! exists) {
    rv = aDBConn->ExecuteSimpleSQL(CREATE_MOZ_ANNO_ATTRIBUTES);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aDBConn->TableExists(NS_LITERAL_CSTRING("moz_items_annos"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! exists) {
    rv = aDBConn->ExecuteSimpleSQL(CREATE_MOZ_ITEMS_ANNOS);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aDBConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE UNIQUE INDEX moz_items_annos_itemattributeindex "
          "ON moz_items_annos (item_id, anno_attribute_id)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


// nsAnnotationService::SetAnnotationStringInternal

nsresult
nsAnnotationService::SetAnnotationStringInternal(PRInt64 aFkId,
                                                 PRBool aIsItemAnnotation,
                                                 const nsACString& aName,
                                                 const nsAString& aValue,
                                                 PRInt32 aFlags,
                                                 PRUint16 aExpiration)
{
  mozStorageTransaction transaction(mDBConn, PR_FALSE);
  mozIStorageStatement* statement; // class var, not owned by this function
  nsresult rv = StartSetAnnotation(aFkId, aIsItemAnnotation, aName, aFlags,
                                   aExpiration,
                                   nsIAnnotationService::TYPE_STRING,
                                   &statement);
  NS_ENSURE_SUCCESS(rv, rv);
  mozStorageStatementScoper statementResetter(statement);

  rv = statement->BindStringParameter(kAnnoIndex_Content, aValue);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindNullParameter(kAnnoIndex_MimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  transaction.Commit();

  // should reset the statement; observers may call our service back to get
  // annotation values!
  statement->Reset();
  statementResetter.Abandon();
  return NS_OK;
}

NS_IMETHODIMP
nsAnnotationService::SetPageAnnotation(nsIURI* aURI,
                                       const nsACString& aName,
                                       nsIVariant* aValue,
                                       PRInt32 aFlags,
                                       PRUint16 aExpiration)
{
  NS_ENSURE_ARG(aValue);

  PRUint16 dataType;
  nsresult rv = aValue->GetDataType(&dataType);
  NS_ENSURE_SUCCESS(rv, rv);

  switch (dataType) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_UINT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_UINT32:
    case nsIDataType::VTYPE_BOOL: {
      PRInt32 valueInt;
      rv = aValue->GetAsInt32(&valueInt);
      if (NS_SUCCEEDED(rv)) {  // fall through PRInt64 case otherwise
        NS_ENSURE_SUCCESS(rv, rv);
        rv = SetPageAnnotationInt32(aURI, aName, valueInt, aFlags, aExpiration);
        NS_ENSURE_SUCCESS(rv, rv);
        return NS_OK;
      }
    }
    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT64: {
      PRInt64 valueLong;
      rv = aValue->GetAsInt64(&valueLong);
      if (NS_SUCCEEDED(rv)) {  // fall through double case otherwise
        NS_ENSURE_SUCCESS(rv, rv);
        rv = SetPageAnnotationInt64(aURI, aName, valueLong, aFlags, aExpiration);
        NS_ENSURE_SUCCESS(rv, rv);
        return NS_OK;
      }
    }
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE: {
      double valueDouble;
      rv = aValue->GetAsDouble(&valueDouble);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = SetPageAnnotationDouble(aURI, aName, valueDouble, aFlags, aExpiration);
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_WCHAR:
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_ASTRING: {
      nsAutoString stringValue;
      rv = aValue->GetAsAString(stringValue);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = SetPageAnnotationString(aURI, aName, stringValue, aFlags, aExpiration);
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAnnotationService::SetItemAnnotation(PRInt64 aItemId,
                                       const nsACString& aName,
                                       nsIVariant* aValue,
                                       PRInt32 aFlags,
                                       PRUint16 aExpiration)
{
  NS_ENSURE_ARG(aValue);

  if (aExpiration == EXPIRE_WITH_HISTORY)
    return NS_ERROR_INVALID_ARG;

  PRUint16 dataType;
  nsresult rv = aValue->GetDataType(&dataType);
  NS_ENSURE_SUCCESS(rv, rv);

  switch (dataType) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_INT16:
    case nsIDataType::VTYPE_UINT16:
    case nsIDataType::VTYPE_INT32:
    case nsIDataType::VTYPE_UINT32:
    case nsIDataType::VTYPE_BOOL: {
      PRInt32 valueInt;
      rv = aValue->GetAsInt32(&valueInt);
      if (NS_SUCCEEDED(rv)) {  // fall through PRInt64 case otherwise
        NS_ENSURE_SUCCESS(rv, rv);
        rv = SetItemAnnotationInt32(aItemId, aName, valueInt, aFlags, aExpiration);
        NS_ENSURE_SUCCESS(rv, rv);
        return NS_OK;
      }
    }
    case nsIDataType::VTYPE_INT64:
    case nsIDataType::VTYPE_UINT64: {
      PRInt64 valueLong;
      rv = aValue->GetAsInt64(&valueLong);
      if (NS_SUCCEEDED(rv)) {  // fall through double case otherwise
        NS_ENSURE_SUCCESS(rv, rv);
        rv = SetItemAnnotationInt64(aItemId, aName, valueLong, aFlags, aExpiration);
        NS_ENSURE_SUCCESS(rv, rv);
        return NS_OK;
      }
    }
    case nsIDataType::VTYPE_FLOAT:
    case nsIDataType::VTYPE_DOUBLE: {
      double valueDouble;
      rv = aValue->GetAsDouble(&valueDouble);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = SetItemAnnotationDouble(aItemId, aName, valueDouble, aFlags, aExpiration);
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }
    case nsIDataType::VTYPE_CHAR:
    case nsIDataType::VTYPE_WCHAR:
    case nsIDataType::VTYPE_DOMSTRING:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
    case nsIDataType::VTYPE_UTF8STRING:
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_ASTRING: {
      nsAutoString stringValue;
      rv = aValue->GetAsAString(stringValue);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = SetItemAnnotationString(aItemId, aName, stringValue, aFlags, aExpiration);
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsAnnotationService::SetPageAnnotationString

NS_IMETHODIMP
nsAnnotationService::SetPageAnnotationString(nsIURI* aURI,
                                             const nsACString& aName,
                                             const nsAString& aValue,
                                             PRInt32 aFlags,
                                             PRUint16 aExpiration)
{
  if (InPrivateBrowsingMode())
    return NS_OK;

  PRInt64 placeId;
  nsresult rv = GetPlaceIdForURI(aURI, &placeId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetAnnotationStringInternal(placeId, PR_FALSE, aName, aValue, aFlags,
                                   aExpiration);
  NS_ENSURE_SUCCESS(rv, rv);

  CallSetForPageObservers(aURI, aName);
  return NS_OK;
}


// nsAnnotationService::SetItemAnnotationString

NS_IMETHODIMP
nsAnnotationService::SetItemAnnotationString(PRInt64 aItemId,
                                             const nsACString& aName,
                                             const nsAString& aValue,
                                             PRInt32 aFlags,
                                             PRUint16 aExpiration)
{
  if (aExpiration == EXPIRE_WITH_HISTORY)
    return NS_ERROR_INVALID_ARG;

  nsresult rv = SetAnnotationStringInternal(aItemId, PR_TRUE, aName, aValue,
                                            aFlags, aExpiration);
  NS_ENSURE_SUCCESS(rv, rv);

  CallSetForItemObservers(aItemId, aName);
  return NS_OK;
}

// nsAnnotationService::SetAnnotationInt32Internal

nsresult
nsAnnotationService::SetAnnotationInt32Internal(PRInt64 aFkId,
                                                PRBool aIsItemAnnotation,
                                                const nsACString& aName,
                                                PRInt32 aValue,
                                                PRInt32 aFlags,
                                                PRUint16 aExpiration)
{
  mozStorageTransaction transaction(mDBConn, PR_FALSE);
  mozIStorageStatement* statement; // class var, not owned by this function
  nsresult rv = StartSetAnnotation(aFkId, aIsItemAnnotation, aName, aFlags,
                                   aExpiration,
                                   nsIAnnotationService::TYPE_INT32,
                                   &statement);
  NS_ENSURE_SUCCESS(rv, rv);
  mozStorageStatementScoper statementResetter(statement);

  rv = statement->BindInt32Parameter(kAnnoIndex_Content, aValue);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindNullParameter(kAnnoIndex_MimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  transaction.Commit();

  // should reset the statement; observers may call our service back to get
  // annotation values!
  statement->Reset();
  statementResetter.Abandon();
  return NS_OK;
}

// nsAnnotationService::SetPageAnnotationInt32

NS_IMETHODIMP
nsAnnotationService::SetPageAnnotationInt32(nsIURI* aURI,
                                            const nsACString& aName,
                                            PRInt32 aValue,
                                            PRInt32 aFlags,
                                            PRUint16 aExpiration)
{
  if (InPrivateBrowsingMode())
    return NS_OK;

  PRInt64 placeId;
  nsresult rv = GetPlaceIdForURI(aURI, &placeId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetAnnotationInt32Internal(placeId, PR_FALSE, aName, aValue, aFlags,
                                  aExpiration);
  NS_ENSURE_SUCCESS(rv, rv);

  CallSetForPageObservers(aURI, aName);
  return NS_OK;
}

// nsAnnotationService::SetItemAnnotationInt32

NS_IMETHODIMP
nsAnnotationService::SetItemAnnotationInt32(PRInt64 aItemId,
                                            const nsACString& aName,
                                            PRInt32 aValue,
                                            PRInt32 aFlags,
                                            PRUint16 aExpiration)
{
  if (aExpiration == EXPIRE_WITH_HISTORY)
    return NS_ERROR_INVALID_ARG;

  nsresult rv = SetAnnotationInt32Internal(aItemId, PR_TRUE, aName, aValue,
                                           aFlags, aExpiration);
  NS_ENSURE_SUCCESS(rv, rv);

  CallSetForItemObservers(aItemId, aName);
  return NS_OK;
}

// nsAnnotationService::SetAnnotationInt64Internal

nsresult
nsAnnotationService::SetAnnotationInt64Internal(PRInt64 aFkId,
                                                PRBool aIsItemAnnotation,
                                                const nsACString& aName,
                                                PRInt64 aValue,
                                                PRInt32 aFlags,
                                                PRUint16 aExpiration)
{
  mozStorageTransaction transaction(mDBConn, PR_FALSE);
  mozIStorageStatement* statement; // class var, not owned by this function
  nsresult rv = StartSetAnnotation(aFkId, aIsItemAnnotation, aName, aFlags,
                                   aExpiration,
                                   nsIAnnotationService::TYPE_INT64,
                                   &statement);
  NS_ENSURE_SUCCESS(rv, rv);
  mozStorageStatementScoper statementResetter(statement);

  rv = statement->BindInt64Parameter(kAnnoIndex_Content, aValue);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindNullParameter(kAnnoIndex_MimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  transaction.Commit();

  // should reset the statement; observers may call our service back to get
  // annotation values!
  statement->Reset();
  statementResetter.Abandon();
  return NS_OK;
}

// nsAnnotationService::SetPageAnnotationInt64

NS_IMETHODIMP
nsAnnotationService::SetPageAnnotationInt64(nsIURI* aURI,
                                            const nsACString& aName,
                                            PRInt64 aValue,
                                            PRInt32 aFlags,
                                            PRUint16 aExpiration)
{
  if (InPrivateBrowsingMode())
    return NS_OK;

  PRInt64 placeId;
  nsresult rv = GetPlaceIdForURI(aURI, &placeId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetAnnotationInt64Internal(placeId, PR_FALSE, aName, aValue, aFlags,
                                  aExpiration);
  NS_ENSURE_SUCCESS(rv, rv);

  CallSetForPageObservers(aURI, aName);
  return NS_OK;
}

// nsAnnotationService::SetItemAnnotationInt64

NS_IMETHODIMP
nsAnnotationService::SetItemAnnotationInt64(PRInt64 aItemId,
                                            const nsACString& aName,
                                            PRInt64 aValue,
                                            PRInt32 aFlags,
                                            PRUint16 aExpiration)
{
  if (aExpiration == EXPIRE_WITH_HISTORY)
    return NS_ERROR_INVALID_ARG;

  nsresult rv = SetAnnotationInt64Internal(aItemId, PR_TRUE, aName, aValue,
                                           aFlags, aExpiration);
  NS_ENSURE_SUCCESS(rv, rv);

  CallSetForItemObservers(aItemId, aName);
  return NS_OK;
}

// nsAnnotationService::SetAnnotationDoubleInternal

nsresult
nsAnnotationService::SetAnnotationDoubleInternal(PRInt64 aFkId,
                                                 PRBool aIsItemAnnotation,
                                                 const nsACString& aName,
                                                 double aValue,
                                                 PRInt32 aFlags,
                                                 PRUint16 aExpiration)
{
  mozStorageTransaction transaction(mDBConn, PR_FALSE);
  mozIStorageStatement* statement; // class var, not owned by this function
  nsresult rv = StartSetAnnotation(aFkId, aIsItemAnnotation, aName, aFlags,
                                   aExpiration,
                                   nsIAnnotationService::TYPE_DOUBLE,
                                   &statement);
  NS_ENSURE_SUCCESS(rv, rv);
  mozStorageStatementScoper statementResetter(statement);

  rv = statement->BindDoubleParameter(kAnnoIndex_Content, aValue);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindNullParameter(kAnnoIndex_MimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  transaction.Commit();

  // should reset the statement; observers may call our service back to get
  // annotation values!
  statement->Reset();
  statementResetter.Abandon();
  return NS_OK;
}

// nsAnnotationService::SetPageAnnotationDouble

NS_IMETHODIMP
nsAnnotationService::SetPageAnnotationDouble(nsIURI* aURI,
                                             const nsACString& aName,
                                             double aValue,
                                             PRInt32 aFlags,
                                             PRUint16 aExpiration)
{
  if (InPrivateBrowsingMode())
    return NS_OK;

  PRInt64 placeId;
  nsresult rv = GetPlaceIdForURI(aURI, &placeId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetAnnotationDoubleInternal(placeId, PR_FALSE, aName, aValue, aFlags,
                                   aExpiration);                      
  NS_ENSURE_SUCCESS(rv, rv);

  CallSetForPageObservers(aURI, aName);
  return NS_OK;
}

// nsAnnotationService::SetItemAnnotationDouble

NS_IMETHODIMP
nsAnnotationService::SetItemAnnotationDouble(PRInt64 aItemId,
                                             const nsACString& aName,
                                             double aValue,
                                             PRInt32 aFlags,
                                             PRUint16 aExpiration)
{
  if (aExpiration == EXPIRE_WITH_HISTORY)
    return NS_ERROR_INVALID_ARG;

  nsresult rv = SetAnnotationDoubleInternal(aItemId, PR_TRUE, aName, aValue,
                                            aFlags, aExpiration);                      
  NS_ENSURE_SUCCESS(rv, rv);

  CallSetForItemObservers(aItemId, aName);
  return NS_OK;
}

// nsAnnotationService::SetAnnotationBinaryInternal

nsresult
nsAnnotationService::SetAnnotationBinaryInternal(PRInt64 aFkId,
                                                 PRBool aIsItemAnnotation,
                                                 const nsACString& aName,
                                                 const PRUint8 *aData,
                                                 PRUint32 aDataLen,
                                                 const nsACString& aMimeType,
                                                 PRInt32 aFlags,
                                                 PRUint16 aExpiration)
{
  if (aMimeType.Length() == 0)
    return NS_ERROR_INVALID_ARG;

  mozStorageTransaction transaction(mDBConn, PR_FALSE);
  mozIStorageStatement* statement; // class var, not owned by this function
  nsresult rv = StartSetAnnotation(aFkId, aIsItemAnnotation, aName, aFlags,
                                   aExpiration,
                                   nsIAnnotationService::TYPE_BINARY,
                                   &statement);
  NS_ENSURE_SUCCESS(rv, rv);
  mozStorageStatementScoper statementResetter(statement);

  rv = statement->BindBlobParameter(kAnnoIndex_Content, aData, aDataLen);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindUTF8StringParameter(kAnnoIndex_MimeType, aMimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);
  transaction.Commit();

  // should reset the statement; observers may call our service back to get
  // annotation values!
  statement->Reset();
  statementResetter.Abandon();
  return NS_OK;
}

// nsAnnotationService::SetAnnotationBinary

NS_IMETHODIMP
nsAnnotationService::SetPageAnnotationBinary(nsIURI* aURI,
                                             const nsACString& aName,
                                             const PRUint8 *aData,
                                             PRUint32 aDataLen,
                                             const nsACString& aMimeType,
                                             PRInt32 aFlags,
                                             PRUint16 aExpiration)
{
  if (InPrivateBrowsingMode())
    return NS_OK;

  PRInt64 placeId;
  nsresult rv = GetPlaceIdForURI(aURI, &placeId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetAnnotationBinaryInternal(placeId, PR_FALSE, aName, aData, aDataLen,
                                   aMimeType, aFlags, aExpiration);
  CallSetForPageObservers(aURI, aName);
  return NS_OK;
}

// nsAnnotationService::SetItemAnnotationBinary

NS_IMETHODIMP
nsAnnotationService::SetItemAnnotationBinary(PRInt64 aItemId,
                                             const nsACString& aName,
                                             const PRUint8 *aData,
                                             PRUint32 aDataLen,
                                             const nsACString& aMimeType,
                                             PRInt32 aFlags,
                                             PRUint16 aExpiration)
{
  if (aExpiration == EXPIRE_WITH_HISTORY)
    return NS_ERROR_INVALID_ARG;

  nsresult rv = SetAnnotationBinaryInternal(aItemId, PR_TRUE, aName, aData,
                                            aDataLen, aMimeType, aFlags,
                                            aExpiration);
  NS_ENSURE_SUCCESS(rv, rv);
  CallSetForItemObservers(aItemId, aName);
  return NS_OK;
}

#define ENSURE_ANNO_TYPE(aType, aStatement)                         \
  PRInt32 type = aStatement->AsInt32(kAnnoIndex_Type);              \
  if (type != nsIAnnotationService::aType) {                        \
    aStatement->Reset();                                            \
    return NS_ERROR_INVALID_ARG;                                    \
  }


// nsAnnotationService::GetPageAnnotationString

NS_IMETHODIMP
nsAnnotationService::GetPageAnnotationString(nsIURI* aURI,
                                             const nsACString& aName,
                                             nsAString& _retval)
{
  nsresult rv = StartGetAnnotationFromURI(aURI, aName);
  if (NS_FAILED(rv))
    return rv;
  ENSURE_ANNO_TYPE(TYPE_STRING, mDBGetAnnotationFromURI)
  rv = mDBGetAnnotationFromURI->GetString(kAnnoIndex_Content, _retval);
  mDBGetAnnotationFromURI->Reset();
  return rv;
}


// nsAnnotationService::GetItemAnnotationString

NS_IMETHODIMP
nsAnnotationService::GetItemAnnotationString(PRInt64 aItemId,
                                             const nsACString& aName,
                                             nsAString& _retval)
{
  nsresult rv = StartGetAnnotationFromItemId(aItemId, aName);
  if (NS_FAILED(rv))
    return rv;
  ENSURE_ANNO_TYPE(TYPE_STRING, mDBGetAnnotationFromItemId)
  rv = mDBGetAnnotationFromItemId->GetString(kAnnoIndex_Content, _retval);
  mDBGetAnnotationFromItemId->Reset();
  return rv;
}


NS_IMETHODIMP
nsAnnotationService::GetPageAnnotation(nsIURI* aURI,
                                       const nsACString& aName,
                                       nsIVariant** _retval)
{
  *_retval = nsnull;
  nsresult rv = StartGetAnnotationFromURI(aURI, aName);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIWritableVariant> value = new nsVariant();
  PRInt32 type = mDBGetAnnotationFromURI->AsInt32(kAnnoIndex_Type);
  switch (type) {
    case nsIAnnotationService::TYPE_INT32:
    case nsIAnnotationService::TYPE_INT64:
    case nsIAnnotationService::TYPE_DOUBLE: {
      rv = value->SetAsDouble(mDBGetAnnotationFromURI->AsDouble(kAnnoIndex_Content));
      break;
    }
    case nsIAnnotationService::TYPE_STRING: {
      nsAutoString valueString;
      rv = mDBGetAnnotationFromURI->GetString(kAnnoIndex_Content, valueString);
      if (NS_SUCCEEDED(rv))
        rv = value->SetAsAString(valueString);
      break;
    }
    case nsIAnnotationService::TYPE_BINARY: {
      rv = NS_ERROR_INVALID_ARG;
      break;
    }
    default: {
      rv = NS_ERROR_UNEXPECTED;
      break;
    }
  }

  if (NS_SUCCEEDED(rv))
    NS_ADDREF(*_retval = value);

  mDBGetAnnotationFromURI->Reset();
  return rv;
}

NS_IMETHODIMP
nsAnnotationService::GetItemAnnotation(PRInt64 aItemId,
                                       const nsACString& aName,
                                       nsIVariant** _retval)
{
  *_retval = nsnull;
  nsresult rv = StartGetAnnotationFromItemId(aItemId, aName);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIWritableVariant> value = new nsVariant();
  PRInt32 type = mDBGetAnnotationFromItemId->AsInt32(kAnnoIndex_Type);
  switch (type) {
    case nsIAnnotationService::TYPE_INT32:
    case nsIAnnotationService::TYPE_INT64:
    case nsIAnnotationService::TYPE_DOUBLE: {
      rv = value->SetAsDouble(mDBGetAnnotationFromItemId->AsDouble(kAnnoIndex_Content));
      break;
    }
    case nsIAnnotationService::TYPE_STRING: {
      nsAutoString valueString;
      rv = mDBGetAnnotationFromItemId->GetString(kAnnoIndex_Content, valueString);
      if (NS_SUCCEEDED(rv))
        rv = value->SetAsAString(valueString);
      break;
    }
    case nsIAnnotationService::TYPE_BINARY: {
      rv = NS_ERROR_INVALID_ARG;
      break;
    }
    default: {
      rv = NS_ERROR_UNEXPECTED;
      break;
    }
  }

  if (NS_SUCCEEDED(rv))
    NS_ADDREF(*_retval = value);

  mDBGetAnnotationFromItemId->Reset();
  return rv;
}

// nsAnnotationService::GetPageAnnotationInt32

NS_IMETHODIMP
nsAnnotationService::GetPageAnnotationInt32(nsIURI* aURI,
                                        const nsACString& aName,
                                        PRInt32 *_retval)
{
  nsresult rv = StartGetAnnotationFromURI(aURI, aName);
  if (NS_FAILED(rv))
    return rv;
  ENSURE_ANNO_TYPE(TYPE_INT32, mDBGetAnnotationFromURI)
  *_retval = mDBGetAnnotationFromURI->AsInt32(kAnnoIndex_Content);
  mDBGetAnnotationFromURI->Reset();
  return NS_OK;
}


// nsAnnotationService::GetItemAnnotationInt32

NS_IMETHODIMP
nsAnnotationService::GetItemAnnotationInt32(PRInt64 aItemId,
                                            const nsACString& aName,
                                            PRInt32 *_retval)
{
  nsresult rv = StartGetAnnotationFromItemId(aItemId, aName);
  if (NS_FAILED(rv))
    return rv;
  ENSURE_ANNO_TYPE(TYPE_INT32, mDBGetAnnotationFromItemId)
  *_retval = mDBGetAnnotationFromItemId->AsInt32(kAnnoIndex_Content);
  mDBGetAnnotationFromItemId->Reset();
  return NS_OK;
}

// nsAnnotationService::GetPageAnnotationInt64

NS_IMETHODIMP
nsAnnotationService::GetPageAnnotationInt64(nsIURI* aURI,
                                            const nsACString& aName,
                                            PRInt64 *_retval)
{
  nsresult rv = StartGetAnnotationFromURI(aURI, aName);
  NS_ENSURE_SUCCESS(rv, rv);
  ENSURE_ANNO_TYPE(TYPE_INT64, mDBGetAnnotationFromURI)
  *_retval = mDBGetAnnotationFromURI->AsInt64(kAnnoIndex_Content);
  mDBGetAnnotationFromURI->Reset();
  return NS_OK;
}

// nsAnnotationService::GetItemAnnotationInt64

NS_IMETHODIMP
nsAnnotationService::GetItemAnnotationInt64(PRInt64 aItemId,
                                            const nsACString& aName,
                                            PRInt64 *_retval)
{
  nsresult rv = StartGetAnnotationFromItemId(aItemId, aName);
  NS_ENSURE_SUCCESS(rv, rv);
  ENSURE_ANNO_TYPE(TYPE_INT64, mDBGetAnnotationFromItemId)
  *_retval = mDBGetAnnotationFromItemId->AsInt64(kAnnoIndex_Content);
  mDBGetAnnotationFromItemId->Reset();
  return NS_OK;
}

// nsAnnotationService::GetPageAnnotationType

NS_IMETHODIMP
nsAnnotationService::GetPageAnnotationType(nsIURI* aURI,
                                           const nsACString& aName,
                                           PRUint16* _retval)
{
  nsresult rv = StartGetAnnotationFromURI(aURI, aName);
  NS_ENSURE_SUCCESS(rv, rv);
  *_retval = mDBGetAnnotationFromURI->AsInt32(kAnnoIndex_Type);
  mDBGetAnnotationFromURI->Reset();
  return NS_OK;
}

// nsAnnotationService::GetItemAnnotationType

NS_IMETHODIMP
nsAnnotationService::GetItemAnnotationType(PRInt64 aItemId,
                                           const nsACString& aName,
                                           PRUint16* _retval)
{
  nsresult rv = StartGetAnnotationFromItemId(aItemId, aName);
  NS_ENSURE_SUCCESS(rv, rv);
  *_retval = mDBGetAnnotationFromItemId->AsInt32(kAnnoIndex_Type);
  mDBGetAnnotationFromItemId->Reset();
  return NS_OK;
}

// nsAnnotationService::GetPageAnnotationDouble

NS_IMETHODIMP
nsAnnotationService::GetPageAnnotationDouble(nsIURI* aURI,
                                             const nsACString& aName,
                                             double *_retval)
{
  nsresult rv = StartGetAnnotationFromURI(aURI, aName);
  if (NS_FAILED(rv))
    return rv;
  ENSURE_ANNO_TYPE(TYPE_DOUBLE, mDBGetAnnotationFromURI)
  *_retval = mDBGetAnnotationFromURI->AsDouble(kAnnoIndex_Content);
  mDBGetAnnotationFromURI->Reset();
  return NS_OK;
}

// nsAnnotationService::GetItemAnnotationDouble

NS_IMETHODIMP
nsAnnotationService::GetItemAnnotationDouble(PRInt64 aItemId,
                                             const nsACString& aName,
                                             double *_retval)
{
  nsresult rv = StartGetAnnotationFromItemId(aItemId, aName);
  if (NS_FAILED(rv))
    return rv;
  ENSURE_ANNO_TYPE(TYPE_DOUBLE, mDBGetAnnotationFromItemId)
  *_retval = mDBGetAnnotationFromItemId->AsDouble(kAnnoIndex_Content);
  mDBGetAnnotationFromItemId->Reset();
  return NS_OK;
}

// nsAnnotationService::GetPageAnnotationBinary

NS_IMETHODIMP
nsAnnotationService::GetPageAnnotationBinary(nsIURI* aURI,
                                             const nsACString& aName,
                                             PRUint8** aData,
                                             PRUint32* aDataLen,
                                             nsACString& aMimeType)
{
  nsresult rv = StartGetAnnotationFromURI(aURI, aName);
  if (NS_FAILED(rv))
    return rv;
  ENSURE_ANNO_TYPE(TYPE_BINARY, mDBGetAnnotationFromURI)
  rv = mDBGetAnnotationFromURI->GetBlob(kAnnoIndex_Content, aDataLen, aData);
  if (NS_FAILED(rv)) {
    mDBGetAnnotationFromURI->Reset();
    return rv;
  }
  rv = mDBGetAnnotationFromURI->GetUTF8String(kAnnoIndex_MimeType, aMimeType);
  mDBGetAnnotationFromURI->Reset();
  return rv;
}

// nsAnnotationService::GetItemAnnotationBinary

NS_IMETHODIMP
nsAnnotationService::GetItemAnnotationBinary(PRInt64 aItemId,
                                             const nsACString& aName,
                                             PRUint8** aData,
                                             PRUint32* aDataLen,
                                             nsACString& aMimeType)
{
  nsresult rv = StartGetAnnotationFromItemId(aItemId, aName);
  if (NS_FAILED(rv))
    return rv;
  ENSURE_ANNO_TYPE(TYPE_BINARY, mDBGetAnnotationFromItemId)
  rv = mDBGetAnnotationFromItemId->GetBlob(kAnnoIndex_Content, aDataLen, aData);
  if (NS_FAILED(rv)) {
    mDBGetAnnotationFromItemId->Reset();
    return rv;
  }
  rv = mDBGetAnnotationFromItemId->GetUTF8String(kAnnoIndex_MimeType, aMimeType);
  mDBGetAnnotationFromItemId->Reset();
  return rv;
}

// nsAnnotationService::GetPageAnnotationInfo

NS_IMETHODIMP
nsAnnotationService::GetPageAnnotationInfo(nsIURI* aURI,
                                           const nsACString& aName,
                                           PRInt32 *aFlags,
                                           PRUint16 *aExpiration,
                                           nsACString& aMimeType,
                                           PRUint16 *aStorageType)
{
  nsresult rv = StartGetAnnotationFromURI(aURI, aName);
  if (NS_FAILED(rv))
    return rv;
  mozStorageStatementScoper resetter(mDBGetAnnotationFromURI);

  *aFlags = mDBGetAnnotationFromURI->AsInt32(kAnnoIndex_Flags);
  *aExpiration = (PRUint16)mDBGetAnnotationFromURI->AsInt32(kAnnoIndex_Expiration);
  rv = mDBGetAnnotationFromURI->GetUTF8String(kAnnoIndex_MimeType, aMimeType);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32 type = (PRUint16)mDBGetAnnotationFromURI->AsInt32(kAnnoIndex_Type);
  if (type == 0) {
    // For annotations created before explicit typing,
    // we can't determine type, just return as string type.
    *aStorageType = nsIAnnotationService::TYPE_STRING;
  } else {
    *aStorageType = type;
  }
  return rv;
}

// nsAnnotationService::GetAnnotationInfo

NS_IMETHODIMP
nsAnnotationService::GetItemAnnotationInfo(PRInt64 aItemId,
                                           const nsACString& aName,
                                           PRInt32 *aFlags,
                                           PRUint16 *aExpiration,
                                           nsACString& aMimeType,
                                           PRUint16 *aStorageType)
{
  nsresult rv = StartGetAnnotationFromItemId(aItemId, aName);
  if (NS_FAILED(rv))
    return rv;
  mozStorageStatementScoper resetter(mDBGetAnnotationFromItemId);

  *aFlags = mDBGetAnnotationFromItemId->AsInt32(kAnnoIndex_Flags);
  *aExpiration = (PRUint16)mDBGetAnnotationFromItemId->AsInt32(kAnnoIndex_Expiration);
  rv = mDBGetAnnotationFromItemId->GetUTF8String(kAnnoIndex_MimeType, aMimeType);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32 type = (PRUint16)mDBGetAnnotationFromItemId->AsInt32(kAnnoIndex_Type);
  if (type == 0) {
    // For annotations created before explicit typing,
    // we can't determine type, just return as string type.
    *aStorageType = nsIAnnotationService::TYPE_STRING;
  } else {
    *aStorageType = type;
  }
  return rv;
}

// nsAnnotationService::GetPagesWithAnnotation

NS_IMETHODIMP
nsAnnotationService::GetPagesWithAnnotation(const nsACString& aName,
                                            PRUint32* aResultCount,
                                            nsIURI*** aResults)
{
  if (aName.IsEmpty() || ! aResultCount || ! aResults)
    return NS_ERROR_INVALID_ARG;
  *aResultCount = 0;
  *aResults = nsnull;
  nsCOMArray<nsIURI> results;

  nsresult rv = GetPagesWithAnnotationCOMArray(aName, &results);
  NS_ENSURE_SUCCESS(rv, rv);

  // convert to raw array
  if (results.Count() == 0)
    return NS_OK;
  *aResults = static_cast<nsIURI**>
                         (nsMemory::Alloc(results.Count() * sizeof(nsIURI*)));
  if (! *aResults)
    return NS_ERROR_OUT_OF_MEMORY;
  *aResultCount = results.Count();
  for (PRUint32 i = 0; i < *aResultCount; i ++) {
    (*aResults)[i] = results[i];
    NS_ADDREF((*aResults)[i]);
  }
  return NS_OK;
}

// nsAnnotationService::GetPagesWithAnnotationCOMArray

nsresult
nsAnnotationService::GetPagesWithAnnotationCOMArray(
    const nsACString& aName, nsCOMArray<nsIURI>* aResults){
  // this probably isn't a common operation, so we don't have a precompiled
  // statement. Perhaps this should change.
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT h.url "
      "FROM moz_places_temp h "
      "JOIN moz_annos a ON h.id = a.place_id "
      "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id "
      "WHERE n.name = ?1 "
      "UNION ALL "
      "SELECT h.url "
      "FROM moz_places h "
      "JOIN moz_annos a ON h.id = a.place_id "
      "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id "
      "WHERE n.name = ?1 "
        "AND h.id NOT IN (SELECT id FROM moz_places_temp)"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindUTF8StringParameter(0, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(rv = statement->ExecuteStep(&hasMore)) && hasMore) {
    nsCAutoString uristring;
    rv = statement->GetUTF8String(0, uristring);
    NS_ENSURE_SUCCESS(rv, rv);

    // convert to a URI, in case of some invalid URI, just ignore this row
    // so we can mostly continue.
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), uristring);
    if (NS_FAILED(rv))
      continue;
    PRBool added = aResults->AppendObject(uri);
    NS_ENSURE_TRUE(added, NS_ERROR_OUT_OF_MEMORY);
  }
  return NS_OK;
}


// nsIAnnotationService::GetItemsWithAnnotation

NS_IMETHODIMP
nsAnnotationService::GetItemsWithAnnotation(const nsACString& aName,
                                            PRUint32* aResultCount,
                                            PRInt64** aResults)
{
  if (aName.IsEmpty() || !aResultCount || !aResults)
    return NS_ERROR_INVALID_ARG;

  *aResultCount = 0;
  *aResults = nsnull;
  nsTArray<PRInt64> results;

  nsresult rv = GetItemsWithAnnotationTArray(aName, &results);
  NS_ENSURE_SUCCESS(rv, rv);

  // convert to raw array
  if (results.Length() == 0)
    return NS_OK;

  *aResults = static_cast<PRInt64*>
                         (nsMemory::Alloc(results.Length() * sizeof(PRInt64)));
  if (! *aResults)
    return NS_ERROR_OUT_OF_MEMORY;

  *aResultCount = results.Length();
  for (PRUint32 i = 0; i < *aResultCount; i ++) {
    (*aResults)[i] = results[i];
  }
  return NS_OK;
}

nsresult
nsAnnotationService::GetItemsWithAnnotationTArray(const nsACString& aName,
                                                  nsTArray<PRInt64>* aResults) {
  mozStorageStatementScoper scoper(mDBGetItemsWithAnnotation);
  nsresult rv = mDBGetItemsWithAnnotation->BindUTF8StringParameter(0, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(rv = mDBGetItemsWithAnnotation->ExecuteStep(&hasMore)) && hasMore) {
    if (!aResults->AppendElement(mDBGetItemsWithAnnotation->AsInt64(0)))
      return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}
  
// nsAnnotationService::GetPageAnnotationNames

NS_IMETHODIMP
nsAnnotationService::GetPageAnnotationNames(nsIURI* aURI, PRUint32* aCount,
                                            nsIVariant*** _result)
{
  *aCount = 0;
  *_result = nsnull;

  PRInt64 placeId;
  nsresult rv = GetPlaceIdForURI(aURI, &placeId, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (placeId == 0) // Check if URI exists.
    return NS_OK;

  nsTArray<nsCString> names;
  rv = GetAnnotationNamesTArray(placeId, &names, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (names.Length() == 0)
    return NS_OK;

  *_result = static_cast<nsIVariant**>
                        (nsMemory::Alloc(sizeof(nsIVariant*) * names.Length()));
  NS_ENSURE_TRUE(*_result, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0; i < names.Length(); i ++) {
    nsCOMPtr<nsIWritableVariant> var = new nsVariant();
    if (! var) {
      // need to release all the variants we've already created
      for (PRUint32 j = 0; j < i; j ++)
        NS_RELEASE((*_result)[j]);
      nsMemory::Free(*_result);
      *_result = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    var->SetAsAUTF8String(names[i]);
    NS_ADDREF((*_result)[i] = var);
  }
  *aCount = names.Length();

  return NS_OK;
}


// nsAnnotationService::GetAnnotationNamesTArray

nsresult
nsAnnotationService::GetAnnotationNamesTArray(PRInt64 aFkId,
                                              nsTArray<nsCString>* aResult,
                                              PRBool aIsFkItemId)
{
  mozIStorageStatement* statement = aIsFkItemId ?
    mDBGetItemAnnotationNames.get() : mDBGetAnnotationNames.get();

  aResult->Clear();

  mozStorageStatementScoper scoper(statement);
  nsresult rv = statement->BindInt64Parameter(0, aFkId);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult;
  nsCAutoString name;
  while (NS_SUCCEEDED(statement->ExecuteStep(&hasResult)) &&
         hasResult) {
    rv = statement->GetUTF8String(0, name);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!aResult->AppendElement(name))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsAnnotationService::GetItemAnnotationNames(PRInt64 aItemId, PRUint32* aCount,
                                            nsIVariant*** _result)
{
  *aCount = 0;
  *_result = nsnull;

  nsTArray<nsCString> names;
  nsresult rv = GetAnnotationNamesTArray(aItemId, &names, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (names.Length() == 0)
    return NS_OK;

  *_result = static_cast<nsIVariant**>
                        (nsMemory::Alloc(sizeof(nsIVariant*) * names.Length()));
  NS_ENSURE_TRUE(*_result, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0; i < names.Length(); i ++) {
    nsCOMPtr<nsIWritableVariant> var = new nsVariant();
    if (! var) {
      // need to release all the variants we've already created
      for (PRUint32 j = 0; j < i; j ++)
        NS_RELEASE((*_result)[j]);
      nsMemory::Free(*_result);
      *_result = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    var->SetAsAUTF8String(names[i]);
    NS_ADDREF((*_result)[i] = var);
  }
  *aCount = names.Length();

  return NS_OK;
}

// nsAnnotationService::HasAnnotation

NS_IMETHODIMP
nsAnnotationService::PageHasAnnotation(nsIURI* aURI,
                                       const nsACString& aName,
                                       PRBool *_retval)
{
  nsresult rv = StartGetAnnotationFromURI(aURI, aName);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    *_retval = PR_FALSE;
    rv = NS_OK;
  } else if (NS_SUCCEEDED(rv)) {
    *_retval = PR_TRUE;
  }
  mDBGetAnnotationFromURI->Reset();
  return rv;
}


// nsAnnotationService::ItemHasAnnotation

NS_IMETHODIMP
nsAnnotationService::ItemHasAnnotation(PRInt64 aItemId,
                                       const nsACString& aName,
                                       PRBool *_retval)
{
  nsresult rv = StartGetAnnotationFromItemId(aItemId, aName);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    *_retval = PR_FALSE;
    rv = NS_OK;
  } else if (NS_SUCCEEDED(rv)) {
    *_retval = PR_TRUE;
  }
  mDBGetAnnotationFromItemId->Reset();
  return rv;
}

// nsAnnotationService::RemoveAnnotationInternal
//
//    We don't remove anything from the moz_anno_attributes table. If we delete
//    the last item of a given name, that item really should go away. It will
//    get cleaned up on the next shutdown.
//
nsresult
nsAnnotationService::RemoveAnnotationInternal(PRInt64 aFkId,
                                              PRBool aIsItemAnnotation,
                                              const nsACString& aName)
{
  mozIStorageStatement* statement = aIsItemAnnotation ?
    mDBRemoveItemAnnotation.get() : mDBRemoveAnnotation.get();

  mozStorageStatementScoper resetter(statement);

  nsresult rv = statement->BindInt64Parameter(0, aFkId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindUTF8StringParameter(1, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  resetter.Abandon();
  return NS_OK;
}

// nsAnnotationService::RemovePageAnnotation

NS_IMETHODIMP
nsAnnotationService::RemovePageAnnotation(nsIURI* aURI,
                                          const nsACString& aName)
{
  PRInt64 placeId;
  nsresult rv = GetPlaceIdForURI(aURI, &placeId, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (placeId == 0)
    return NS_OK; // URI doesn't exist, nothing to delete

  rv = RemoveAnnotationInternal(placeId, PR_FALSE, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Update observers
  for (PRInt32 i = 0; i < mObservers.Count(); i ++)
    mObservers[i]->OnPageAnnotationRemoved(aURI, aName);

  return NS_OK;
}

// nsAnnotationService::RemoveItemAnnotation

NS_IMETHODIMP
nsAnnotationService::RemoveItemAnnotation(PRInt64 aItemId,
                                          const nsACString& aName)
{
  nsresult rv = RemoveAnnotationInternal(aItemId, PR_TRUE, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Update observers
  for (PRInt32 i = 0; i < mObservers.Count(); i ++)
    mObservers[i]->OnItemAnnotationRemoved(aItemId, aName);

  return NS_OK;
}

// nsAnnotationSerivce::RemovePageAnnotations
//
//    I don't believe this is a common operation, so am not using a precompiled
//    statement. If this ends up being used a lot, the statement should be
//    precompiled and added to the class.

NS_IMETHODIMP
nsAnnotationService::RemovePageAnnotations(nsIURI* aURI)
{
  PRInt64 placeId;
  nsresult rv = GetPlaceIdForURI(aURI, &placeId, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (placeId == 0)
    return NS_OK; // URI doesn't exist, nothing to delete

  nsCOMPtr<mozIStorageStatement> statement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_annos WHERE place_id = ?1"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(0, placeId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Update observers
  for (PRInt32 i = 0; i < mObservers.Count(); i ++)
    mObservers[i]->OnPageAnnotationRemoved(aURI, EmptyCString());
  return NS_OK;
}


// nsAnnotationSerivce::RemoveItemAnnotations

NS_IMETHODIMP
nsAnnotationService::RemoveItemAnnotations(PRInt64 aItemId)
{
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM moz_items_annos WHERE item_id = ?1"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = statement->BindInt64Parameter(0, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Update observers
  for (PRInt32 i = 0; i < mObservers.Count(); i ++)
    mObservers[i]->OnItemAnnotationRemoved(aItemId, EmptyCString());
  return NS_OK;
}

// nsAnnotationService::CopyPageAnnotations
//
//    This function currently assumes there are very few annotations per
//    URI and that brute-force is therefore a good strategy for intersecting
//    the two sets. If this ends up not being the case, this function should
//    be changed to do this more efficiently.
//
//    XXX: If we use annotations for some standard items like GeckoFlags, it
//    might be a good idea to blacklist these standard annotations from this
//    copy function.
//
//    We operate on strings from GetPageAnnotationNamesTArray. This is less
//    efficient than operating on name IDs, which we should consider if this
//    is too slow.

NS_IMETHODIMP
nsAnnotationService::CopyPageAnnotations(nsIURI* aSourceURI,
                                         nsIURI* aDestURI,
                                         PRBool aOverwriteDest)
{
  if (InPrivateBrowsingMode())
    return NS_OK;

  mozStorageTransaction transaction(mDBConn, PR_FALSE);

  // source
  PRInt64 sourcePlaceId;
  nsresult rv = GetPlaceIdForURI(aSourceURI, &sourcePlaceId, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (sourcePlaceId == 0) // Check if URI exists.
    return NS_OK;

  nsTArray<nsCString> sourceNames;
  rv = GetAnnotationNamesTArray(sourcePlaceId, &sourceNames, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  if (sourceNames.Length() == 0)
    return NS_OK; // nothing to copy

  // dest
  PRInt64 destPlaceId;
  rv = GetPlaceIdForURI(aSourceURI, &destPlaceId);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsTArray<nsCString> destNames;
  rv = GetAnnotationNamesTArray(destPlaceId, &destNames, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  // we assume you will only have a couple annotations max per URI
#ifdef DEBUG
  if (sourceNames.Length() > 10 || destNames.Length() > 10) {
    NS_WARNING("There are a lot of annotations, copying them may be inefficient.");
  }
#endif

  if (aOverwriteDest) {
    // overwrite existing ones, remove dest dupes from DB and our list
    for (PRUint32 i = 0; i < sourceNames.Length(); i ++) {
      PRUint32 destIndex = destNames.IndexOf(sourceNames[i]);
      if (destIndex != destNames.NoIndex) {
        destNames.RemoveElementAt(destIndex);
        RemovePageAnnotation(aDestURI, sourceNames[i]);
      }
    }
  } else {
    // don't overwrite existing ones, remove dupes from the list of source names
    for (PRUint32 i = 0; i < destNames.Length(); i ++) {
      PRUint32 sourceIndex = sourceNames.IndexOf(destNames[i]);
      if (sourceIndex != sourceNames.NoIndex)
        sourceNames.RemoveElementAt(sourceIndex);
    }
  }

  // given (sourceID, destID, name) this will insert a new annotation on
  // source with the same values of the annotation on dest.
  nsCOMPtr<mozIStorageStatement> statement;
  rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO moz_annos "
      "(place_id, anno_attribute_id, mime_type, content, flags, expiration) "
      "SELECT ?1, anno_attribute_id, mime_type, content, flags, expiration "
      "FROM moz_annos "
      "WHERE place_id = ?2 AND anno_attribute_id = "
        "(SELECT id FROM moz_anno_attributes WHERE name = ?3)"),
    getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the IDs of the pages in quesion.  PERFORMANCE: This is the second time
  // we do this for each page, since GetPageAnnotationNamesTArray does it when
  // it gets the names. If this function requires optimization, we should only
  // do this once and get the names ourselves using the IDs.
  PRInt64 sourceID, destID;
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_FAILURE);

  rv = history->GetUrlIdFor(aSourceURI, &sourceID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(sourceID, NS_ERROR_UNEXPECTED); // we should have caught this above

  rv = history->GetUrlIdFor(aSourceURI, &destID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(destID, NS_ERROR_UNEXPECTED); // we should have caught this above

  // now we know to create annotations from all sources names and there won't
  // be any collisions
  for (PRUint32 i = 0; i < sourceNames.Length(); i ++) {
    statement->BindInt64Parameter(0, sourceID);
    statement->BindInt64Parameter(1, destID);
    statement->BindUTF8StringParameter(2, sourceNames[i]);
    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  transaction.Commit();
  return NS_OK;
}

// nsAnnotationService::CopyItemAnnotations

NS_IMETHODIMP
nsAnnotationService::CopyItemAnnotations(PRInt64 aSourceItemId,
                                         PRInt64 aDestItemId,
                                         PRBool aOverwriteDest)
{
 // XXX: Implement Me!
 return NS_ERROR_NOT_IMPLEMENTED;
}

// nsAnnotationService::AddObserver

NS_IMETHODIMP
nsAnnotationService::AddObserver(nsIAnnotationObserver* aObserver)
{
  if (mObservers.IndexOfObject(aObserver) >= 0)
    return NS_ERROR_INVALID_ARG; // already registered
  if (!mObservers.AppendObject(aObserver))
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}


// nsAnnotationService::RemoveObserver

NS_IMETHODIMP
nsAnnotationService::RemoveObserver(nsIAnnotationObserver* aObserver)
{
  if (!mObservers.RemoveObject(aObserver))
    return NS_ERROR_INVALID_ARG;
  return NS_OK;
}


// nsAnnotationService::GetAnnotationURI
//
// XXX: does not support item-annotations

NS_IMETHODIMP
nsAnnotationService::GetAnnotationURI(nsIURI* aURI, const nsACString& aName,
                                      nsIURI** _result)
{
  if (aName.IsEmpty())
    return NS_ERROR_INVALID_ARG;

  nsCAutoString annoSpec;
  nsresult rv = aURI->GetSpec(annoSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString spec;
  spec.AssignLiteral("moz-anno:");
  spec += aName;
  spec += NS_LITERAL_CSTRING(":");
  spec += annoSpec;

  return NS_NewURI(_result, spec);
}

// nsAnnotationService::HasAnnotationInternal
//
//    This is just like Has[Item]Annotation but takes the foreign key
//    directly (i.e. the places or item id). It will also give you the ID of
//    the annotation, if it exists. This value can be NULL and it won't
//    retrieve the annotation ID. If it doesn't exist, annotationID is not
//    touched.

nsresult
nsAnnotationService::HasAnnotationInternal(PRInt64 aFkId,
                                           PRBool aIsItemAnnotation,
                                           const nsACString& aName,
                                           PRBool* hasAnnotation,
                                           PRInt64* annotationID)
{
  mozIStorageStatement* statement =
    aIsItemAnnotation ? mDBGetItemAnnotation.get() : mDBGetAnnotation.get();
  mozStorageStatementScoper resetter(statement);
  nsresult rv;

  rv = statement->BindInt64Parameter(0, aFkId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindUTF8StringParameter(1, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->ExecuteStep(hasAnnotation);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! annotationID || ! *hasAnnotation)
    return NS_OK;

  return statement->GetInt64(0, annotationID);
}

// nsAnnotationService::StartGetAnnotationFromURI
//
//    This loads the statement GetAnnotationFromURI and steps it once so you
//    can get data out of it. YOU NEED TO RESET THIS STATEMENT WHEN YOU ARE
//    DONE! Returns error if the annotation is not found, in which case you
//    don't need to reset anything.

nsresult
nsAnnotationService::StartGetAnnotationFromURI(nsIURI* aURI,
                                               const nsACString& aName)
{
  mozStorageStatementScoper statementResetter(mDBGetAnnotationFromURI);
  nsresult rv;

  rv = BindStatementURI(mDBGetAnnotationFromURI, 0, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBGetAnnotationFromURI->BindUTF8StringParameter(1, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult = PR_FALSE;
  rv = mDBGetAnnotationFromURI->ExecuteStep(&hasResult);
  if (NS_FAILED(rv) || ! hasResult)
    return NS_ERROR_NOT_AVAILABLE;

  // on success, DON'T reset the statement, the caller needs to read from it,
  // and it is the caller's job to do the reseting.
  statementResetter.Abandon();
  return NS_OK;
}

// nsAnnotationService::StartGetAnnotationFromItemId
//
//    This loads the statement GetAnnotationFromItemId and steps it once so you
//    can get data out of it. YOU NEED TO RESET THIS STATEMENT WHEN YOU ARE
//    DONE! Returns error if the annotation is not found, in which case you
//    don't need to reset anything.

nsresult
nsAnnotationService::StartGetAnnotationFromItemId(PRInt64 aItemId,
                                                  const nsACString& aName)
{
  mozStorageStatementScoper statementResetter(mDBGetAnnotationFromItemId);
  nsresult rv;

  rv = mDBGetAnnotationFromItemId->BindInt64Parameter(0, aItemId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBGetAnnotationFromItemId->BindUTF8StringParameter(1, aName);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasResult = PR_FALSE;
  rv = mDBGetAnnotationFromItemId->ExecuteStep(&hasResult);
  if (NS_FAILED(rv) || ! hasResult)
    return NS_ERROR_NOT_AVAILABLE;

  // on success, DON'T reset the statement, the caller needs to read from it,
  // and it is the caller's job to do the reseting.
  statementResetter.Abandon();
  return NS_OK;
}


PRBool
nsAnnotationService::InPrivateBrowsingMode() const
{
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  return history && history->InPrivateBrowsingMode();
}


nsresult
nsAnnotationService::GetPlaceIdForURI(nsIURI* aURI, PRInt64* _retval,
                                      PRBool aAutoCreate)
{
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_FAILURE);

  return history->GetUrlIdFor(aURI, _retval, aAutoCreate);
}

// nsAnnotationService::StartSetAnnotation
//
//    This does most of the work of setting an annotation, except for setting
//    the actual value and MIME type and executing the statement. It will
//    create a URL entry if necessary. It will either update an existing
//    annotation or insert a new one, and aStatement will be set to either
//    mDBAddAnnotation or mDBSetAnnotation. The aStatement RESULT IS NOT
//    ADDREFED. This is just one of the class vars, which control its scope.
//    DO NOT RELEASE.
//
//    The caller must make sure the statement is reset. On error, the
//    statement will not need reseting.

nsresult
nsAnnotationService::StartSetAnnotation(PRInt64 aFkId,
                                        PRBool aIsItemAnnotation, 
                                        const nsACString& aName,
                                        PRInt32 aFlags,
                                        PRUint16 aExpiration,
                                        PRUint16 aType,
                                        mozIStorageStatement** aStatement)
{
  // Disallow setting item-annotations on invalid item ids
  if (aIsItemAnnotation) {
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_STATE(bookmarks);
    if (!bookmarks->ItemExists(aFkId))
      return NS_ERROR_INVALID_ARG;
  }

  PRBool hasAnnotation;
  PRInt64 annotationID;
  nsresult rv = HasAnnotationInternal(aFkId, aIsItemAnnotation, aName,
                                      &hasAnnotation, &annotationID);
  NS_ENSURE_SUCCESS(rv, rv);

  // either update the existing annotation (using the old annotation's ID)
  // or insert a new one tied to the URI.
  if (hasAnnotation) {
    *aStatement = aIsItemAnnotation ? mDBSetItemAnnotation : mDBSetAnnotation;
    // id
    rv = (*aStatement)->BindInt64Parameter(kAnnoIndex_ID, annotationID);
    NS_ENSURE_SUCCESS(rv, rv);
    // lastModified
    rv = (*aStatement)->BindInt64Parameter(kAnnoIndex_LastModified, PR_Now());
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    *aStatement = aIsItemAnnotation ? mDBAddItemAnnotation : mDBAddAnnotation;

    // make sure the name exists
    {
      mozStorageStatementScoper scoper(mDBGetAnnotationNameID);
      rv = mDBGetAnnotationNameID->BindUTF8StringParameter(0, aName);
      NS_ENSURE_SUCCESS(rv, rv);
      PRBool hasName;
      PRInt64 nameID;
      if (NS_FAILED(mDBGetAnnotationNameID->ExecuteStep(&hasName)) || ! hasName) {
        // add a new annotation name
        mDBGetAnnotationNameID->Reset();
        mozStorageStatementScoper addNameScoper(mDBAddAnnotationName);
        rv = mDBAddAnnotationName->BindUTF8StringParameter(0, aName);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = mDBAddAnnotationName->Execute();
        NS_ENSURE_SUCCESS(rv, rv);

        {
          mozStorageStatementScoper scoper(mDBGetAnnotationNameID);

          rv = mDBGetAnnotationNameID->BindUTF8StringParameter(0, aName);
          NS_ENSURE_SUCCESS(rv, rv);

          PRBool hasResult;
          rv = mDBGetAnnotationNameID->ExecuteStep(&hasResult);
          NS_ENSURE_SUCCESS(rv, rv);
          NS_ASSERTION(hasResult, "hasResult is false but the call succeeded?");
          nameID = mDBGetAnnotationNameID->AsInt64(0);
        }
      } else {
        nameID = mDBGetAnnotationNameID->AsInt64(0);
      }
      rv = (*aStatement)->BindInt64Parameter(kAnnoIndex_PageOrItem, aFkId);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = (*aStatement)->BindInt64Parameter(kAnnoIndex_Name, nameID);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    
    // dateAdded
    rv = (*aStatement)->BindInt64Parameter(kAnnoIndex_DateAdded, PR_Now());
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mozStorageStatementScoper statementResetter(*aStatement);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = (*aStatement)->BindInt32Parameter(kAnnoIndex_Flags, aFlags);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = (*aStatement)->BindInt32Parameter(kAnnoIndex_Expiration, aExpiration);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = (*aStatement)->BindInt32Parameter(kAnnoIndex_Type, aType);
  NS_ENSURE_SUCCESS(rv, rv);

  // on success, leave the statement open, the caller will set the value
  // and MIME type and execute the statement
  statementResetter.Abandon();
  return NS_OK;
}


// nsAnnotationService::CallSetForPageObservers

void
nsAnnotationService::CallSetForPageObservers(nsIURI* aURI, const nsACString& aName)
{
  for (PRInt32 i = 0; i < mObservers.Count(); i ++)
    mObservers[i]->OnPageAnnotationSet(aURI, aName);
}

// nsAnnotationService::CallSetForItemObservers

void
nsAnnotationService::CallSetForItemObservers(PRInt64 aItemId, const nsACString& aName)
{
  for (PRInt32 i = 0; i < mObservers.Count(); i ++)
    mObservers[i]->OnItemAnnotationSet(aItemId, aName);
}

void
nsAnnotationService::FinalizeStatements() {
    mDBSetAnnotation = nsnull;
    mDBSetItemAnnotation = nsnull;
    mDBGetAnnotation = nsnull;
    mDBGetItemAnnotation = nsnull;
    mDBGetAnnotationNames = nsnull;
    mDBGetItemAnnotationNames = nsnull;
    mDBGetAnnotationFromURI = nsnull;
    mDBGetAnnotationFromItemId = nsnull;
    mDBGetAnnotationNameID = nsnull;
    mDBAddAnnotationName = nsnull;
    mDBAddAnnotation = nsnull;
    mDBAddItemAnnotation = nsnull;
    mDBRemoveAnnotation = nsnull;
    mDBRemoveItemAnnotation = nsnull;
    mDBGetItemsWithAnnotation = nsnull;
}
