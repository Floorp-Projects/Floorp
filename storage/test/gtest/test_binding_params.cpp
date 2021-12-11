/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"

#include "mozilla/ArrayUtils.h"
#include "mozStorageHelper.h"

using namespace mozilla;

/**
 * This file tests binding and reading out string parameters through the
 * mozIStorageStatement API.
 */

TEST(storage_binding_params, ASCIIString)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create table with a single string column.
  (void)db->ExecuteSimpleSQL("CREATE TABLE test (str STRING)"_ns);

  // Create statements to INSERT and SELECT the string.
  nsCOMPtr<mozIStorageStatement> insert, select;
  (void)db->CreateStatement("INSERT INTO test (str) VALUES (?1)"_ns,
                            getter_AddRefs(insert));
  (void)db->CreateStatement("SELECT str FROM test"_ns, getter_AddRefs(select));

  // Roundtrip a string through the table, and ensure it comes out as expected.
  nsAutoCString inserted("I'm an ASCII string");
  {
    mozStorageStatementScoper scoper(insert);
    bool hasResult;
    do_check_true(NS_SUCCEEDED(insert->BindUTF8StringByIndex(0, inserted)));
    do_check_true(NS_SUCCEEDED(insert->ExecuteStep(&hasResult)));
    do_check_false(hasResult);
  }

  nsAutoCString result;
  {
    mozStorageStatementScoper scoper(select);
    bool hasResult;
    do_check_true(NS_SUCCEEDED(select->ExecuteStep(&hasResult)));
    do_check_true(hasResult);
    do_check_true(NS_SUCCEEDED(select->GetUTF8String(0, result)));
  }

  do_check_true(result == inserted);

  (void)db->ExecuteSimpleSQL("DELETE FROM test"_ns);
}

TEST(storage_binding_params, CString)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create table with a single string column.
  (void)db->ExecuteSimpleSQL("CREATE TABLE test (str STRING)"_ns);

  // Create statements to INSERT and SELECT the string.
  nsCOMPtr<mozIStorageStatement> insert, select;
  (void)db->CreateStatement("INSERT INTO test (str) VALUES (?1)"_ns,
                            getter_AddRefs(insert));
  (void)db->CreateStatement("SELECT str FROM test"_ns, getter_AddRefs(select));

  // Roundtrip a string through the table, and ensure it comes out as expected.
  static const char sCharArray[] =
      "I'm not a \xff\x00\xac\xde\xbb ASCII string!";
  nsAutoCString inserted(sCharArray, ArrayLength(sCharArray) - 1);
  do_check_true(inserted.Length() == ArrayLength(sCharArray) - 1);
  {
    mozStorageStatementScoper scoper(insert);
    bool hasResult;
    do_check_true(NS_SUCCEEDED(insert->BindUTF8StringByIndex(0, inserted)));
    do_check_true(NS_SUCCEEDED(insert->ExecuteStep(&hasResult)));
    do_check_false(hasResult);
  }

  {
    nsAutoCString result;

    mozStorageStatementScoper scoper(select);
    bool hasResult;
    do_check_true(NS_SUCCEEDED(select->ExecuteStep(&hasResult)));
    do_check_true(hasResult);
    do_check_true(NS_SUCCEEDED(select->GetUTF8String(0, result)));

    do_check_true(result == inserted);
  }

  (void)db->ExecuteSimpleSQL("DELETE FROM test"_ns);
}

TEST(storage_binding_params, UTFStrings)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create table with a single string column.
  (void)db->ExecuteSimpleSQL("CREATE TABLE test (str STRING)"_ns);

  // Create statements to INSERT and SELECT the string.
  nsCOMPtr<mozIStorageStatement> insert, select;
  (void)db->CreateStatement("INSERT INTO test (str) VALUES (?1)"_ns,
                            getter_AddRefs(insert));
  (void)db->CreateStatement("SELECT str FROM test"_ns, getter_AddRefs(select));

  // Roundtrip a UTF8 string through the table, using UTF8 input and output.
  static const char sCharArray[] = R"(I'm a ûüâäç UTF8 string!)";
  nsAutoCString insertedUTF8(sCharArray, ArrayLength(sCharArray) - 1);
  do_check_true(insertedUTF8.Length() == ArrayLength(sCharArray) - 1);
  NS_ConvertUTF8toUTF16 insertedUTF16(insertedUTF8);
  do_check_true(insertedUTF8 == NS_ConvertUTF16toUTF8(insertedUTF16));
  {
    mozStorageStatementScoper scoper(insert);
    bool hasResult;
    do_check_true(NS_SUCCEEDED(insert->BindUTF8StringByIndex(0, insertedUTF8)));
    do_check_true(NS_SUCCEEDED(insert->ExecuteStep(&hasResult)));
    do_check_false(hasResult);
  }

  {
    nsAutoCString result;

    mozStorageStatementScoper scoper(select);
    bool hasResult;
    do_check_true(NS_SUCCEEDED(select->ExecuteStep(&hasResult)));
    do_check_true(hasResult);
    do_check_true(NS_SUCCEEDED(select->GetUTF8String(0, result)));

    do_check_true(result == insertedUTF8);
  }

  // Use UTF8 input and UTF16 output.
  {
    nsAutoString result;

    mozStorageStatementScoper scoper(select);
    bool hasResult;
    do_check_true(NS_SUCCEEDED(select->ExecuteStep(&hasResult)));
    do_check_true(hasResult);
    do_check_true(NS_SUCCEEDED(select->GetString(0, result)));

    do_check_true(result == insertedUTF16);
  }

  (void)db->ExecuteSimpleSQL("DELETE FROM test"_ns);

  // Roundtrip the same string using UTF16 input and UTF8 output.
  {
    mozStorageStatementScoper scoper(insert);
    bool hasResult;
    do_check_true(NS_SUCCEEDED(insert->BindStringByIndex(0, insertedUTF16)));
    do_check_true(NS_SUCCEEDED(insert->ExecuteStep(&hasResult)));
    do_check_false(hasResult);
  }

  {
    nsAutoCString result;

    mozStorageStatementScoper scoper(select);
    bool hasResult;
    do_check_true(NS_SUCCEEDED(select->ExecuteStep(&hasResult)));
    do_check_true(hasResult);
    do_check_true(NS_SUCCEEDED(select->GetUTF8String(0, result)));

    do_check_true(result == insertedUTF8);
  }

  // Use UTF16 input and UTF16 output.
  {
    nsAutoString result;

    mozStorageStatementScoper scoper(select);
    bool hasResult;
    do_check_true(NS_SUCCEEDED(select->ExecuteStep(&hasResult)));
    do_check_true(hasResult);
    do_check_true(NS_SUCCEEDED(select->GetString(0, result)));

    do_check_true(result == insertedUTF16);
  }

  (void)db->ExecuteSimpleSQL("DELETE FROM test"_ns);
}
