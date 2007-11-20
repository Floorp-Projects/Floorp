/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com> (original author)
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

#include "nsMorkReader.h"
#include "nsNavHistory.h"
#include "mozStorageHelper.h"
#include "prprf.h"
#include "nsNetUtil.h"
#include "nsTArray.h"

// Columns for entry (non-meta) history rows
enum {
  kURLColumn,
  kNameColumn,
  kVisitCountColumn,
  kHiddenColumn,
  kTypedColumn,
  kLastVisitColumn,
  kColumnCount // keep me last
};

static const char * const gColumnNames[] = {
  "URL", "Name", "VisitCount", "Hidden", "Typed", "LastVisitDate"
};

struct TableReadClosure
{
  TableReadClosure(nsMorkReader *aReader, nsNavHistory *aHistory)
    : reader(aReader), history(aHistory), swapBytes(PR_FALSE),
      byteOrderColumn(-1)
  {
    for (PRUint32 i = 0; i < kColumnCount; ++i) {
      columnIndexes[i] = -1;
    }
  }

  // Backpointers to the reader and history we're operating on
  const nsMorkReader *reader;
  nsNavHistory *history;

  // Whether we need to swap bytes (file format is other-endian)
  PRBool swapBytes;

  // Indexes of the columns that we care about
  PRInt32 columnIndexes[kColumnCount];
  PRInt32 byteOrderColumn;
};

// Reverses the high and low bytes in a PRUnichar buffer.
// This is used if the file format has a different endianness from the
// current architecture.
static void
SwapBytes(PRUnichar *buffer)
{
  for (PRUnichar *b = buffer; *b; ++b) {
    PRUnichar c = *b;
    *b = (c << 8) | ((c >> 8) & 0xff);
  }
}

// Enumerator callback to add a table row to history
static PLDHashOperator PR_CALLBACK
AddToHistoryCB(const nsCSubstring &aRowID,
               const nsTArray<nsCString> *aValues,
               void *aData)
{
  TableReadClosure *data = static_cast<TableReadClosure*>(aData);
  const nsMorkReader *reader = data->reader;
  nsCString values[kColumnCount];
  const PRInt32 *columnIndexes = data->columnIndexes;

  for (PRInt32 i = 0; i < kColumnCount; ++i) {
    if (columnIndexes[i] != -1) {
      values[i] = (*aValues)[columnIndexes[i]];
      reader->NormalizeValue(values[i]);
      if (i == kHiddenColumn && values[i].EqualsLiteral("1"))
        return PL_DHASH_NEXT; // Do not import hidden records.
    }
  }

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), values[kURLColumn]);

  if (uri) {
    // title is really a UTF-16 string at this point
    nsCString &titleC = values[kNameColumn];

    PRUint32 titleLength;
    const char *titleBytes;
    if (titleC.IsEmpty()) {
      titleBytes = "\0";
      titleLength = 0;
    } else {
      titleLength = titleC.Length() / 2;

      // add an extra null byte onto the end, so that the buffer ends
      // with a complete unicode null character.
      titleC.Append('\0');

      // Swap the bytes in the unicode characters if necessary.
      if (data->swapBytes) {
        SwapBytes(reinterpret_cast<PRUnichar*>(titleC.BeginWriting()));
      }
      titleBytes = titleC.get();
    }

    const PRUnichar *title = reinterpret_cast<const PRUnichar*>(titleBytes);

    PRInt32 err;
    PRInt32 count = values[kVisitCountColumn].ToInteger(&err);
    if (err != 0 || count == 0) {
      count = 1;
    }

    PRTime date;
    if (PR_sscanf(values[kLastVisitColumn].get(), "%lld", &date) != 1) {
      date = -1;
    }

    PRBool isTyped = values[kTypedColumn].EqualsLiteral("1");
    PRInt32 transition = isTyped ?
        (PRInt32) nsINavHistoryService::TRANSITION_TYPED
      : (PRInt32) nsINavHistoryService::TRANSITION_LINK;
    nsNavHistory *history = data->history;

    history->AddPageWithVisit(uri, nsDependentString(title, titleLength),
                              PR_FALSE, isTyped, count, transition, date);
  }
  return PL_DHASH_NEXT;
}

// nsNavHistory::ImportHistory
//
// ImportHistory is the main entry point to the importer.
// It sets up the file stream and loops over the lines in the file to
// parse them, then adds the resulting row set to history.
//
NS_IMETHODIMP
nsNavHistory::ImportHistory(nsIFile* aFile)
{
  NS_ENSURE_TRUE(aFile, NS_ERROR_NULL_POINTER);

  // Check that the file exists before we try to open it
  PRBool exists;
  aFile->Exists(&exists);
  if (!exists) {
    return NS_OK;
  }
  
  // Read in the mork file
  nsMorkReader reader;
  nsresult rv = reader.Init();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = reader.Read(aFile);
  NS_ENSURE_SUCCESS(rv, rv);

  // Gather up the column ids so we don't need to find them on each row
  TableReadClosure data(&reader, this);
  const nsTArray<nsMorkReader::MorkColumn> &columns = reader.GetColumns();
  for (PRUint32 i = 0; i < columns.Length(); ++i) {
    const nsCSubstring &name = columns[i].name;
    for (PRUint32 j = 0; j < kColumnCount; ++j) {
      if (name.Equals(gColumnNames[j])) {
        data.columnIndexes[j] = i;
        break;
      }
    }
    if (name.EqualsLiteral("ByteOrder")) {
      data.byteOrderColumn = i;
    }
  }

  // Determine the byte order from the table's meta-row.
  const nsTArray<nsCString> *metaRow = reader.GetMetaRow();
  if (metaRow && data.byteOrderColumn != -1) {
    const nsCString &byteOrder = (*metaRow)[data.byteOrderColumn];
    if (!byteOrder.IsVoid()) {
      // Note whether the file uses a non-native byte ordering.
      // If it does, we'll have to swap bytes for PRUnichar values.
      // "BE" and "LE" are the only recognized values, anything
      // else is garbage and the file will be treated as native-endian
      // (no swapping).
      nsCAutoString byteOrderValue(byteOrder);
      reader.NormalizeValue(byteOrderValue);
#ifdef IS_LITTLE_ENDIAN
      data.swapBytes = byteOrderValue.EqualsLiteral("BE");
#else
      data.swapBytes = byteOrderValue.EqualsLiteral("LE");
#endif
    }
  }

  // Now add the results to history
  // Note: Duplicates are handled by Places internally, no need to filter them
  // out before importing.
  mozIStorageConnection *conn = GetStorageConnection();
  NS_ENSURE_TRUE(conn, NS_ERROR_NOT_INITIALIZED);
  mozStorageTransaction transaction(conn, PR_FALSE);
#ifdef IN_MEMORY_LINKS
  mozIStorageConnection *memoryConn = GetMemoryStorageConnection();
  mozStorageTransaction memTransaction(memoryConn, PR_FALSE);
#endif

  reader.EnumerateRows(AddToHistoryCB, &data);

#ifdef IN_MEMORY_LINKS
  memTransaction.Commit();
#endif
  return transaction.Commit();
}
