/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"
#include "nsILoadContextInfo.h"
#include "../cache2/CacheFileUtils.h"

int
main(int32_t argc, char *argv[])
{
  nsCOMPtr<nsILoadContextInfo> info;
  nsAutoCString key, enh;

#define CHECK(a) MOZ_ASSERT(a)

  info = ParseKey(NS_LITERAL_CSTRING(""));
  CHECK(info && !info->IsPrivate() && !info->IsAnonymous() && !info->IsInBrowserElement() && info->AppId() == 0);
  info = ParseKey(NS_LITERAL_CSTRING(":"));
  CHECK(info && !info->IsPrivate() && !info->IsAnonymous() && !info->IsInBrowserElement() && info->AppId() == 0);
  info = ParseKey(NS_LITERAL_CSTRING("a,"));
  CHECK(info && !info->IsPrivate() && info->IsAnonymous() && !info->IsInBrowserElement() && info->AppId() == 0);
  info = ParseKey(NS_LITERAL_CSTRING("a,:"));
  CHECK(info && !info->IsPrivate() && info->IsAnonymous() && !info->IsInBrowserElement() && info->AppId() == 0);
  info = ParseKey(NS_LITERAL_CSTRING("a,:xxx"), &enh, &key);
  CHECK(info && !info->IsPrivate() && info->IsAnonymous() && !info->IsInBrowserElement() && info->AppId() == 0);
  CHECK(NS_LITERAL_CSTRING("xxx").Equals(key));
  info = ParseKey(NS_LITERAL_CSTRING("b,:xxx"));
  CHECK(info && !info->IsPrivate() && !info->IsAnonymous() && info->IsInBrowserElement() && info->AppId() == 0);
  info = ParseKey(NS_LITERAL_CSTRING("a,b,:xxx"), &enh, &key);
  CHECK(info && !info->IsPrivate() && info->IsAnonymous() && info->IsInBrowserElement() && info->AppId() == 0);
  CHECK(NS_LITERAL_CSTRING("xxx").Equals(key));
  CHECK(enh.IsEmpty());
  info = ParseKey(NS_LITERAL_CSTRING("a,b,i123,:xxx"));
  CHECK(info && !info->IsPrivate() && info->IsAnonymous() && info->IsInBrowserElement() && info->AppId() == 123);
  info = ParseKey(NS_LITERAL_CSTRING("a,b,c,h***,i123,:xxx"), &enh, &key);
  CHECK(info && !info->IsPrivate() && info->IsAnonymous() && info->IsInBrowserElement() && info->AppId() == 123);
  CHECK(NS_LITERAL_CSTRING("xxx").Equals(key));
  info = ParseKey(NS_LITERAL_CSTRING("a,b,c,h***,i123,~enh,:xxx"), &enh, &key);
  CHECK(info && !info->IsPrivate() && info->IsAnonymous() && info->IsInBrowserElement() && info->AppId() == 123);
  CHECK(NS_LITERAL_CSTRING("xxx").Equals(key));
  CHECK(NS_LITERAL_CSTRING("enh").Equals(enh));
  info = ParseKey(NS_LITERAL_CSTRING("0x,1,a,b,i123,:xxx"));
  CHECK(info && !info->IsPrivate() && info->IsAnonymous() && info->IsInBrowserElement() && info->AppId() == 123);

  nsAutoCString test;
  AppendTagWithValue(test, '~', NS_LITERAL_CSTRING("e,nh,"));
  info = ParseKey(test, &enh, &key);
  CHECK(info && !info->IsPrivate() && !info->IsAnonymous() && !info->IsInBrowserElement() && info->AppId() == 0);
  CHECK(NS_LITERAL_CSTRING("e,nh,").Equals(enh));

  info = ParseKey(NS_LITERAL_CSTRING("a,i123,b,:xxx"));
  CHECK(!info);
  info = ParseKey(NS_LITERAL_CSTRING("a"));
  CHECK(!info);
  info = ParseKey(NS_LITERAL_CSTRING("a:"));
  CHECK(!info);
  info = ParseKey(NS_LITERAL_CSTRING("a:xxx"));
  CHECK(!info);
  info = ParseKey(NS_LITERAL_CSTRING("i123"));
  CHECK(!info);
  info = ParseKey(NS_LITERAL_CSTRING("i123:"));
  CHECK(!info);
  info = ParseKey(NS_LITERAL_CSTRING("i123:xxx"));
  CHECK(!info);
  info = ParseKey(NS_LITERAL_CSTRING("i123,x:"));
  CHECK(!info);
  info = ParseKey(NS_LITERAL_CSTRING("i,x,:"));
  CHECK(!info);
  info = ParseKey(NS_LITERAL_CSTRING("i:"));
  CHECK(!info);

#undef CHECK

  passed("ok");
  return 0;
}
