/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintOptionsQt_h__
#define nsPrintOptionsQt_h__

#include "nsPrintOptionsImpl.h"

class nsPrintOptionsQt : public nsPrintOptions
{
public:
    nsPrintOptionsQt();
    virtual ~nsPrintOptionsQt();
    virtual nsresult _CreatePrintSettings(nsIPrintSettings** _retval);
};

#endif /* nsPrintOptionsQt_h__ */
