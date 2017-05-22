/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSPROFILER_H_
#define _NSPROFILER_H_

#include "nsIProfiler.h"
#include "nsIObserver.h"
#include "mozilla/Attributes.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
class ProfileGatherer;
}

class nsProfiler final : public nsIProfiler, public nsIObserver
{
public:
    nsProfiler();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    NS_DECL_NSIPROFILER

    nsresult Init();

    static nsProfiler* GetOrCreate()
    {
	nsCOMPtr<nsIProfiler> iprofiler =
	    do_GetService("@mozilla.org/tools/profiler;1");
	return static_cast<nsProfiler*>(iprofiler.get());
    }

    void GatheredOOPProfile(const nsACString& aProfile);

private:
    ~nsProfiler();

    RefPtr<mozilla::ProfileGatherer> mGatherer;
    bool mLockedForPrivateBrowsing;
};

#endif /* _NSPROFILER_H_ */

