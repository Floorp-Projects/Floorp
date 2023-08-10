/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <iostream>

#include "FuzzingInterface.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIZipReader.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "mozilla/Span.h"
#include "mozilla/Unused.h"
#include "nsIInputStream.h"
#include "nsIStringEnumerator.h"

enum FuzzMethodType {
  eTest = 0,
  eGetEntry,
  eHasEntry,
  eFindEntries,
  eInputStream,
  eOpenInner,
  eLastMethod,
};
static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);

template <typename T>
T jar_get_num(char** buf, size_t* size) {
  if (sizeof(T) > *size) {
    return 0;
  }

  T* iptr = reinterpret_cast<T*>(*buf);
  *buf += sizeof(T);
  *size -= sizeof(T);
  return *iptr;
}

nsAutoCString jar_get_string(char** buf, size_t* size) {
  uint8_t len = jar_get_num<uint8_t>(buf, size);
  if (len > *size) {
    len = static_cast<uint8_t>(*size);
  }
  nsAutoCString str(*buf, len);

  *buf += len;
  *size -= len;
  return str;
}

nsresult FuzzEntries(char** buf, size_t* size, nsIZipReader* aReader,
                     const nsACString& aName) {
  uint8_t iters = jar_get_num<uint8_t>(buf, size);
  nsresult rv;
  for (uint8_t i = 0; i < iters; ++i) {
    nsAutoCString out;
    uint64_t written;
    nsCOMPtr<nsIZipEntry> entry;
    nsCOMPtr<nsIInputStream> stream;

    switch (jar_get_num<uint8_t>(buf, size) % eLastMethod) {
      case eTest: {
        rv = aReader->Test(aName);
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }
      case eGetEntry: {
        rv = aReader->GetEntry(aName, getter_AddRefs(entry));
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }
      case eHasEntry: {
        bool has = false;
        rv = aReader->HasEntry(aName, &has);
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      }
      case eInputStream:
        rv = aReader->GetInputStream(aName, getter_AddRefs(stream));
        NS_ENSURE_SUCCESS(rv, rv);
        if (NS_FAILED(rv)) {
          break;
        }
        rv = NS_ReadInputStreamToString(stream, out, -1, &written);
        NS_ENSURE_SUCCESS(rv, rv);
        break;
      default:
        break;
    }
  }
  return NS_OK;
}

nsresult FuzzReader(char** buf, size_t* size, nsIZipReader* aReader) {
  nsresult rv;
  nsAutoCString name;
  nsCOMPtr<nsIZipEntry> entry;
  bool has = false;
  nsAutoCString pattern;
  nsCOMPtr<nsIUTF8StringEnumerator> enumerator;
  nsCOMPtr<nsIInputStream> stream;
  bool hasMore;
  nsAutoCString out;
  uint64_t written;
  nsCOMPtr<nsIZipReader> newReader = do_CreateInstance(kZipReaderCID, &rv);
  switch (jar_get_num<uint8_t>(buf, size) % eLastMethod) {
    case eTest:
      rv = aReader->Test(""_ns);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    case eGetEntry:
      name = jar_get_string(buf, size);
      rv = aReader->GetEntry(name, getter_AddRefs(entry));
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    case eHasEntry:
      name = jar_get_string(buf, size);
      rv = aReader->HasEntry(name, &has);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    case eFindEntries:
      pattern = jar_get_string(buf, size);
      rv = aReader->FindEntries(pattern, getter_AddRefs(enumerator));
      NS_ENSURE_SUCCESS(rv, rv);
      while (NS_SUCCEEDED(enumerator->HasMore(&hasMore)) && hasMore) {
        if (NS_FAILED(enumerator->GetNext(name))) {
          break;
        }
        rv = FuzzEntries(buf, size, aReader, name);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      break;
    case eInputStream:
      name = jar_get_string(buf, size);
      rv = aReader->GetInputStream(name, getter_AddRefs(stream));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = NS_ReadInputStreamToString(stream, out, -1, &written);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    case eOpenInner:
      name = jar_get_string(buf, size);
      rv = newReader->OpenInner(aReader, name);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = FuzzReader(buf, size, newReader);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    default:
      break;
  }
  return rv;
}

static int FuzzingRunJARParser(const uint8_t* data, size_t size) {
  char* buf = (char*)data;
  nsresult rv;

  nsCOMPtr<nsIURI> uri;
  nsAutoCString jardata = jar_get_string(&buf, &size);

  nsCOMPtr<nsIZipReader> reader = do_CreateInstance(kZipReaderCID, &rv);
  rv = reader->OpenMemory((void*)jardata.get(), jardata.Length());
  NS_ENSURE_SUCCESS(rv, 0);

#if 0
  // For easily exporting the last test case that triggered a crash.
  FILE * f = fopen("/tmp/input.jar", "wb");
  fwrite((void*)jardata.get(), 1, jardata.Length(), f);
  fclose(f);
#endif

  uint8_t iters = jar_get_num<uint8_t>(&buf, &size);
  for (uint8_t i = 0; i < iters; ++i) {
    rv = FuzzReader(&buf, &size, reader);
    NS_ENSURE_SUCCESS(rv, 0);
  }

  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(nullptr, FuzzingRunJARParser, JARParser);
