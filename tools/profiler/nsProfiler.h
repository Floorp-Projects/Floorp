/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSPROFILER_H_
#define _NSPROFILER_H_

#include "nsIProfiler.h"

class nsProfiler : public nsIProfiler
{
public:
    nsProfiler();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIPROFILER
};

#endif /* _NSPROFILER_H_ */

