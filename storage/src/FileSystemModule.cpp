/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemModule.h"

#include "sqlite3.h"
#include "nsString.h"
#include "nsISimpleEnumerator.h"
#include "nsIFile.h"
#include "nsILocalFile.h"

namespace {

struct VirtualTableCursorBase
{
  VirtualTableCursorBase()
  {
    memset(&mBase, 0, sizeof(mBase));
  }

  sqlite3_vtab_cursor mBase;
};

struct VirtualTableCursor : public VirtualTableCursorBase
{
public:
  VirtualTableCursor()
  : mRowId(-1)
  {
    mCurrentFileName.SetIsVoid(true);
  }

  const nsString& DirectoryPath() const
  {
    return mDirectoryPath;
  }

  const nsString& CurrentFileName() const
  {
    return mCurrentFileName;
  }

  PRInt64 RowId() const
  {
    return mRowId;
  }

  nsresult Init(const nsAString& aPath);
  nsresult NextFile();

private:
  nsCOMPtr<nsISimpleEnumerator> mEntries;

  nsString mDirectoryPath;
  nsString mCurrentFileName;

  PRInt64 mRowId;
};

nsresult
VirtualTableCursor::Init(const nsAString& aPath)
{
  nsCOMPtr<nsILocalFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  NS_ENSURE_TRUE(directory, NS_ERROR_FAILURE);

  nsresult rv = directory->InitWithPath(aPath);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = directory->GetPath(mDirectoryPath);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = directory->GetDirectoryEntries(getter_AddRefs(mEntries));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NextFile();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
VirtualTableCursor::NextFile()
{
  bool hasMore;
  nsresult rv = mEntries->HasMoreElements(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!hasMore) {
    mCurrentFileName.SetIsVoid(true);
    return NS_OK;
  }

  nsCOMPtr<nsISupports> entry;
  rv = mEntries->GetNext(getter_AddRefs(entry));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  rv = file->GetLeafName(mCurrentFileName);
  NS_ENSURE_SUCCESS(rv, rv);

  mRowId++;

  return NS_OK;
}

int Connect(sqlite3* aDB, void* aAux, int aArgc, const char* const* aArgv,
            sqlite3_vtab** aVtab, char** aErr)
{
  static const char virtualTableSchema[] =
    "CREATE TABLE fs ("
      "name TEXT, "
      "path TEXT"
    ")";

  int rc = sqlite3_declare_vtab(aDB, virtualTableSchema);
  if (rc != SQLITE_OK) {
    return rc;
  }

  sqlite3_vtab* vt = new sqlite3_vtab();
  memset(vt, 0, sizeof(*vt));

  *aVtab = vt;

  return SQLITE_OK;
}

int Disconnect(sqlite3_vtab* aVtab )
{
  delete aVtab;

  return SQLITE_OK;
}

int BestIndex(sqlite3_vtab* aVtab, sqlite3_index_info* aInfo)
{
  // Here we specify what index constraints we want to handle. That is, there
  // might be some columns with particular constraints in which we can help
  // SQLite narrow down the result set.
  //
  // For example, take the "path = x" where x is a directory. In this case,
  // we can narrow our search to just this directory instead of the entire file
  // system. This can be a significant optimization. So, we want to handle that
  // constraint. To do so, we would look for two specific input conditions:
  //
  // 1. aInfo->aConstraint[i].iColumn == 1
  // 2. aInfo->aConstraint[i].op == SQLITE_INDEX_CONSTRAINT_EQ
  //
  // The first states that the path column is being used in one of the input
  // constraints and the second states that the constraint involves the equal
  // operator.
  //
  // An even more specific search would be for name='xxx', in which case we
  // can limit the search to a single file, if it exists.
  //
  // What we have to do here is look for all of our index searches and select
  // the narrowest. We can only pick one, so obviously we want the one that
  // is the most specific, which leads to the smallest result set.

  for(int i = 0; i < aInfo->nConstraint; i++) {
    if (aInfo->aConstraint[i].iColumn == 1 && aInfo->aConstraint[i].usable) {
      if (aInfo->aConstraint[i].op & SQLITE_INDEX_CONSTRAINT_EQ) {
        aInfo->aConstraintUsage[i].argvIndex = 1;
      }
      break;
    }

    // TODO: handle single files (constrained also by the name column)
  }

  return SQLITE_OK;
}

int Open(sqlite3_vtab* aVtab, sqlite3_vtab_cursor** aCursor)
{
  VirtualTableCursor* cursor = new VirtualTableCursor();

  *aCursor = reinterpret_cast<sqlite3_vtab_cursor*>(cursor);

  return SQLITE_OK;
}

int Close(sqlite3_vtab_cursor* aCursor)
{
  VirtualTableCursor* cursor = reinterpret_cast<VirtualTableCursor*>(aCursor);

  delete cursor;

  return SQLITE_OK;
}

int Filter(sqlite3_vtab_cursor* aCursor, int aIdxNum, const char* aIdxStr,
           int aArgc, sqlite3_value** aArgv)
{
  VirtualTableCursor* cursor = reinterpret_cast<VirtualTableCursor*>(aCursor);

  if(aArgc <= 0) {
    return SQLITE_OK;
  }

  nsDependentString path(
    reinterpret_cast<const PRUnichar*>(::sqlite3_value_text16(aArgv[0])));

  nsresult rv = cursor->Init(path);
  NS_ENSURE_SUCCESS(rv, SQLITE_ERROR);

  return SQLITE_OK;
}

int Next(sqlite3_vtab_cursor* aCursor)
{
  VirtualTableCursor* cursor = reinterpret_cast<VirtualTableCursor*>(aCursor);

  nsresult rv = cursor->NextFile();
  NS_ENSURE_SUCCESS(rv, SQLITE_ERROR);

  return SQLITE_OK;
}

int Eof(sqlite3_vtab_cursor* aCursor)
{
  VirtualTableCursor* cursor = reinterpret_cast<VirtualTableCursor*>(aCursor);
  return cursor->CurrentFileName().IsVoid() ? 1 : 0;
}

int Column(sqlite3_vtab_cursor* aCursor, sqlite3_context* aContext,
           int aColumnIndex)
{
  VirtualTableCursor* cursor = reinterpret_cast<VirtualTableCursor*>(aCursor);

  switch (aColumnIndex) {
    // name
    case 0: {
      const nsString& name = cursor->CurrentFileName();
      sqlite3_result_text16(aContext, name.get(),
                            name.Length() * sizeof(PRUnichar),
                            SQLITE_TRANSIENT);
      break;
    }

    // path
    case 1: {
      const nsString& path = cursor->DirectoryPath();
      sqlite3_result_text16(aContext, path.get(),
                            path.Length() * sizeof(PRUnichar),
                            SQLITE_TRANSIENT);
      break;
    }
    default:
      NS_NOTREACHED("Unsupported column!");
  }

  return SQLITE_OK;
}

int RowId(sqlite3_vtab_cursor* aCursor, sqlite3_int64* aRowid)
{
  VirtualTableCursor* cursor = reinterpret_cast<VirtualTableCursor*>(aCursor);

  *aRowid = cursor->RowId();

  return SQLITE_OK;
}

} // anonymous namespace

namespace mozilla {
namespace storage {

int RegisterFileSystemModule(sqlite3* aDB, const char* aName)
{
  static sqlite3_module module = {
    1,
    Connect,
    Connect,
    BestIndex,
    Disconnect,
    Disconnect,
    Open,
    Close,
    Filter,
    Next,
    Eof,
    Column,
    RowId,
    nsnull,
    nsnull,
    nsnull,
    nsnull,
    nsnull,
    nsnull,
    nsnull
  };

  return sqlite3_create_module(aDB, aName, &module, nsnull);
}

} // namespace storage
} // namespace mozilla
