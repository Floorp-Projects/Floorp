/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVariant.h"
#include "storage_test_harness.h"
#include "nsThreadUtils.h"
#include "nsIURI.h"
#include "nsIFileURL.h"
#include "nsIVariant.h"
#include "nsNetUtil.h"

////////////////////////////////////////////////////////////////////////////////
//// Tests

TEST(storage_async_thread_naming, MemoryDatabase)
{
  HookSqliteMutex hook;

  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  nsCOMPtr<nsIThread> target(get_conn_async_thread(db));
  do_check_true(target);
  PRThread* prThread;
  target->GetPRThread(&prThread);
  do_check_true(prThread);
  nsAutoCString name(PR_GetThreadName(prThread));
  printf("%s", name.get());
  do_check_true(StringBeginsWith(name, "sqldb:memory"_ns));

  blocking_async_close(db);
}

TEST(storage_async_thread_naming, FileDatabase)
{
  HookSqliteMutex hook;

  nsAutoString filename(u"test_thread_name.sqlite"_ns);
  nsCOMPtr<nsIFile> dbFile;
  do_check_success(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                          getter_AddRefs(dbFile)));
  do_check_success(dbFile->Append(filename));
  nsCOMPtr<mozIStorageService> ss = getService();
  nsCOMPtr<mozIStorageConnection> conn;
  do_check_success(ss->OpenDatabase(dbFile, 0, getter_AddRefs(conn)));

  nsCOMPtr<nsIThread> target(get_conn_async_thread(conn));
  do_check_true(target);
  PRThread* prThread;
  target->GetPRThread(&prThread);
  do_check_true(prThread);
  nsAutoCString name(PR_GetThreadName(prThread));
  nsAutoCString expected("sqldb:"_ns);
  expected.Append(NS_ConvertUTF16toUTF8(filename));
  do_check_true(StringBeginsWith(name, expected));

  {
    nsCOMPtr<mozIStorageConnection> clone;
    do_check_success(conn->Clone(true, getter_AddRefs(clone)));
    nsCOMPtr<nsIThread> target(get_conn_async_thread(conn));
    do_check_true(target);
    PRThread* prThread;
    target->GetPRThread(&prThread);
    do_check_true(prThread);
    nsAutoCString name(PR_GetThreadName(prThread));
    nsAutoCString expected("sqldb:"_ns);
    expected.Append(NS_ConvertUTF16toUTF8(filename));
    do_check_true(StringBeginsWith(name, expected));
    blocking_async_close(clone);
  }

  blocking_async_close(conn);
}

TEST(storage_async_thread_naming, FileUnsharedDatabase)
{
  HookSqliteMutex hook;

  nsAutoString filename(u"test_thread_name.sqlite"_ns);
  nsCOMPtr<nsIFile> dbFile;
  do_check_success(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                          getter_AddRefs(dbFile)));
  do_check_success(dbFile->Append(filename));
  nsCOMPtr<mozIStorageService> ss = getService();
  nsCOMPtr<mozIStorageConnection> conn;
  do_check_success(ss->OpenUnsharedDatabase(dbFile, 0, getter_AddRefs(conn)));

  nsCOMPtr<nsIThread> target(get_conn_async_thread(conn));
  do_check_true(target);
  PRThread* prThread;
  target->GetPRThread(&prThread);
  do_check_true(prThread);
  nsAutoCString name(PR_GetThreadName(prThread));
  nsAutoCString expected("sqldb:"_ns);
  expected.Append(NS_ConvertUTF16toUTF8(filename));
  do_check_true(StringBeginsWith(name, expected));

  blocking_async_close(conn);
}

TEST(storage_async_thread_naming, FileURLDatabase)
{
  HookSqliteMutex hook;

  nsAutoString filename(u"test_thread_name.sqlite"_ns);
  nsCOMPtr<nsIFile> dbFile;
  do_check_success(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                          getter_AddRefs(dbFile)));
  do_check_success(dbFile->Append(filename));
  nsCOMPtr<nsIURI> uri;
  do_check_success(NS_NewFileURI(getter_AddRefs(uri), dbFile));
  nsCOMPtr<nsIFileURL> fileUrl = do_QueryInterface(uri);
  nsCOMPtr<mozIStorageService> ss = getService();
  nsCOMPtr<mozIStorageConnection> conn;
  do_check_success(ss->OpenDatabaseWithFileURL(fileUrl, EmptyCString(), 0,
                                               getter_AddRefs(conn)));

  nsCOMPtr<nsIThread> target(get_conn_async_thread(conn));
  do_check_true(target);
  PRThread* prThread;
  target->GetPRThread(&prThread);
  do_check_true(prThread);
  nsAutoCString name(PR_GetThreadName(prThread));
  nsAutoCString expected("sqldb:"_ns);
  expected.Append(NS_ConvertUTF16toUTF8(filename));
  do_check_true(StringBeginsWith(name, expected));

  blocking_async_close(conn);
}

TEST(storage_async_thread_naming, OverrideFileURLDatabase)
{
  HookSqliteMutex hook;

  nsAutoString filename(u"test_thread_name.sqlite"_ns);
  nsCOMPtr<nsIFile> dbFile;
  do_check_success(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                          getter_AddRefs(dbFile)));
  do_check_success(dbFile->Append(filename));
  nsCOMPtr<nsIURI> uri;
  do_check_success(NS_NewFileURI(getter_AddRefs(uri), dbFile));
  nsCOMPtr<nsIFileURL> fileUrl = do_QueryInterface(uri);
  nsCOMPtr<mozIStorageService> ss = getService();
  nsCOMPtr<mozIStorageConnection> conn;
  nsAutoCString override("override"_ns);
  do_check_success(
      ss->OpenDatabaseWithFileURL(fileUrl, override, 0, getter_AddRefs(conn)));

  nsCOMPtr<nsIThread> target(get_conn_async_thread(conn));
  do_check_true(target);
  PRThread* prThread;
  target->GetPRThread(&prThread);
  do_check_true(prThread);
  nsAutoCString name(PR_GetThreadName(prThread));
  nsAutoCString expected("sqldb:"_ns);
  expected.Append(override);
  do_check_true(StringBeginsWith(name, expected));

  blocking_async_close(conn);
}

TEST(storage_async_thread_naming, AsyncOpenDatabase)
{
  HookSqliteMutex hook;

  nsAutoString filename(u"test_thread_name.sqlite"_ns);
  nsCOMPtr<nsIFile> dbFile;
  do_check_success(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                          getter_AddRefs(dbFile)));
  do_check_success(dbFile->Append(filename));
  nsCOMPtr<mozIStorageService> ss = getService();

  RefPtr<AsyncCompletionSpinner> completionSpinner =
      new AsyncCompletionSpinner();
  RefPtr<nsVariant> variant = new nsVariant();
  variant->SetAsInterface(NS_GET_IID(nsIFile), dbFile);
  do_check_success(ss->OpenAsyncDatabase(variant, 0, 0, completionSpinner));
  completionSpinner->SpinUntilCompleted();
  nsCOMPtr<mozIStorageConnection> conn(
      do_QueryInterface(completionSpinner->mCompletionValue));
  nsCOMPtr<nsIThread> target(get_conn_async_thread(conn));
  do_check_true(target);
  PRThread* prThread;
  target->GetPRThread(&prThread);
  do_check_true(prThread);
  nsAutoCString name(PR_GetThreadName(prThread));
  nsAutoCString expected("sqldb:"_ns);
  expected.Append(NS_ConvertUTF16toUTF8(filename));
  do_check_true(StringBeginsWith(name, expected));

  blocking_async_close(conn);
}

TEST(storage_async_thread_naming, AsyncClone)
{
  HookSqliteMutex hook;

  nsAutoString filename(u"test_thread_name.sqlite"_ns);
  nsCOMPtr<nsIFile> dbFile;
  do_check_success(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                          getter_AddRefs(dbFile)));
  do_check_success(dbFile->Append(filename));
  nsCOMPtr<mozIStorageService> ss = getService();

  nsCOMPtr<mozIStorageConnection> conn;
  do_check_success(ss->OpenDatabase(dbFile, 0, getter_AddRefs(conn)));

  RefPtr<AsyncCompletionSpinner> completionSpinner =
      new AsyncCompletionSpinner();
  RefPtr<nsVariant> variant = new nsVariant();
  variant->SetAsInterface(NS_GET_IID(nsIFile), dbFile);
  do_check_success(conn->AsyncClone(true, completionSpinner));
  completionSpinner->SpinUntilCompleted();
  nsCOMPtr<mozIStorageConnection> clone(
      do_QueryInterface(completionSpinner->mCompletionValue));
  nsCOMPtr<nsIThread> target(get_conn_async_thread(clone));
  do_check_true(target);
  PRThread* prThread;
  target->GetPRThread(&prThread);
  do_check_true(prThread);
  nsAutoCString name(PR_GetThreadName(prThread));
  nsAutoCString expected("sqldb:"_ns);
  expected.Append(NS_ConvertUTF16toUTF8(filename));
  do_check_true(StringBeginsWith(name, expected));

  blocking_async_close(conn);
  blocking_async_close(clone);
}
