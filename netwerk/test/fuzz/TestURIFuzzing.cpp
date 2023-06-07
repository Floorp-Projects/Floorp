/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <iostream>

#include "FuzzingInterface.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIURL.h"
#include "nsIStandardURL.h"
#include "nsIURIMutator.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "nsString.h"
#include "mozilla/Encoding.h"
#include "mozilla/Span.h"
#include "mozilla/Unused.h"

template <typename T>
T get_numeric(char** buf, size_t* size) {
  if (sizeof(T) > *size) {
    return 0;
  }

  T* iptr = reinterpret_cast<T*>(*buf);
  *buf += sizeof(T);
  *size -= sizeof(T);
  return *iptr;
}

nsAutoCString get_string(char** buf, size_t* size) {
  uint8_t len = get_numeric<uint8_t>(buf, size);
  if (len > *size) {
    len = static_cast<uint8_t>(*size);
  }
  nsAutoCString str(*buf, len);

  *buf += len;
  *size -= len;
  return str;
}

const char* charsets[] = {
    "Big5",         "EUC-JP",       "EUC-KR",         "gb18030",
    "gbk",          "IBM866",       "ISO-2022-JP",    "ISO-8859-10",
    "ISO-8859-13",  "ISO-8859-14",  "ISO-8859-15",    "ISO-8859-16",
    "ISO-8859-2",   "ISO-8859-3",   "ISO-8859-4",     "ISO-8859-5",
    "ISO-8859-6",   "ISO-8859-7",   "ISO-8859-8",     "ISO-8859-8-I",
    "KOI8-R",       "KOI8-U",       "macintosh",      "replacement",
    "Shift_JIS",    "UTF-16BE",     "UTF-16LE",       "UTF-8",
    "windows-1250", "windows-1251", "windows-1252",   "windows-1253",
    "windows-1254", "windows-1255", "windows-1256",   "windows-1257",
    "windows-1258", "windows-874",  "x-mac-cyrillic", "x-user-defined"};

static int FuzzingRunURIParser(const uint8_t* data, size_t size) {
  char* buf = (char*)data;

  nsCOMPtr<nsIURI> uri;
  nsAutoCString spec = get_string(&buf, &size);

  nsresult rv = NS_NewURI(getter_AddRefs(uri), spec);

  if (NS_FAILED(rv)) {
    return 0;
  }

  uint8_t iters = get_numeric<uint8_t>(&buf, &size);
  for (int i = 0; i < iters; i++) {
    if (get_numeric<uint8_t>(&buf, &size) % 25 != 0) {
      NS_MutateURI mutator(uri);
      nsAutoCString acdata = get_string(&buf, &size);

      switch (get_numeric<uint8_t>(&buf, &size) % 12) {
        default:
          mutator.SetSpec(acdata);
          break;
        case 1:
          mutator.SetScheme(acdata);
          break;
        case 2:
          mutator.SetUserPass(acdata);
          break;
        case 3:
          mutator.SetUsername(acdata);
          break;
        case 4:
          mutator.SetPassword(acdata);
          break;
        case 5:
          mutator.SetHostPort(acdata);
          break;
        case 6:
          // Called via SetHostPort
          mutator.SetHost(acdata);
          break;
        case 7:
          // Called via multiple paths
          mutator.SetPathQueryRef(acdata);
          break;
        case 8:
          mutator.SetRef(acdata);
          break;
        case 9:
          mutator.SetFilePath(acdata);
          break;
        case 10:
          mutator.SetQuery(acdata);
          break;
        case 11: {
          const uint8_t index = get_numeric<uint8_t>(&buf, &size) %
                                (sizeof(charsets) / sizeof(char*));
          const char* charset = charsets[index];
          auto encoding = mozilla::Encoding::ForLabelNoReplacement(
              mozilla::MakeStringSpan(charset));
          mutator.SetQueryWithEncoding(acdata, encoding);
          break;
        }
      }

      nsresult rv = mutator.Finalize(uri);
      if (NS_FAILED(rv)) {
        return 0;
      }
    } else {
      nsAutoCString out;

      if (uri) {
        switch (get_numeric<uint8_t>(&buf, &size) % 26) {
          default:
            uri->GetSpec(out);
            break;
          case 1:
            uri->GetPrePath(out);
            break;
          case 2:
            uri->GetScheme(out);
            break;
          case 3:
            uri->GetUserPass(out);
            break;
          case 4:
            uri->GetUsername(out);
            break;
          case 5:
            uri->GetPassword(out);
            break;
          case 6:
            uri->GetHostPort(out);
            break;
          case 7:
            uri->GetHost(out);
            break;
          case 8: {
            int rv;
            uri->GetPort(&rv);
            break;
          }
          case 9:
            uri->GetPathQueryRef(out);
            break;
          case 10: {
            nsCOMPtr<nsIURI> other;
            bool rv;
            nsAutoCString spec = get_string(&buf, &size);
            NS_NewURI(getter_AddRefs(other), spec);
            uri->Equals(other, &rv);
            break;
          }
          case 11: {
            nsAutoCString scheme = get_string(&buf, &size);
            bool rv;
            uri->SchemeIs("https", &rv);
            break;
          }
          case 12: {
            nsAutoCString in = get_string(&buf, &size);
            uri->Resolve(in, out);
            break;
          }
          case 13:
            uri->GetAsciiSpec(out);
            break;
          case 14:
            uri->GetAsciiHostPort(out);
            break;
          case 15:
            uri->GetAsciiHost(out);
            break;
          case 16:
            uri->GetRef(out);
            break;
          case 17: {
            nsCOMPtr<nsIURI> other;
            bool rv;
            nsAutoCString spec = get_string(&buf, &size);
            NS_NewURI(getter_AddRefs(other), spec);
            uri->EqualsExceptRef(other, &rv);
            break;
          }
          case 18:
            uri->GetSpecIgnoringRef(out);
            break;
          case 19: {
            bool rv;
            uri->GetHasRef(&rv);
            break;
          }
          case 20:
            uri->GetFilePath(out);
            break;
          case 21:
            uri->GetQuery(out);
            break;
          case 22:
            uri->GetDisplayHost(out);
            break;
          case 23:
            uri->GetDisplayHostPort(out);
            break;
          case 24:
            uri->GetDisplaySpec(out);
            break;
          case 25:
            uri->GetDisplayPrePath(out);
            break;
        }
      }
    }
  }

  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(nullptr, FuzzingRunURIParser, URIParser);
