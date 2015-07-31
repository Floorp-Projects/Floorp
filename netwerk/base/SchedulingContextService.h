/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 ;*; */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__net__SchedulingContextService_h
#define mozilla__net__SchedulingContextService_h

#include "nsCOMPtr.h"
#include "nsInterfaceHashtable.h"
#include "nsIObserver.h"
#include "nsISchedulingContext.h"

class nsIUUIDGenerator;

namespace mozilla {
namespace net {

class SchedulingContextService final : public nsISchedulingContextService
                                     , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCHEDULINGCONTEXTSERVICE
  NS_DECL_NSIOBSERVER

  SchedulingContextService();

  nsresult Init();
  void Shutdown();
  static nsresult Create(nsISupports *outer, const nsIID& iid, void **result);

private:
  virtual ~SchedulingContextService();

  static SchedulingContextService *sSelf;

  nsInterfaceHashtable<nsIDHashKey, nsISchedulingContext> mTable;
  nsCOMPtr<nsIUUIDGenerator> mUUIDGen;
};

} // ::mozilla::net
} // ::mozilla

#endif // mozilla__net__SchedulingContextService_h
