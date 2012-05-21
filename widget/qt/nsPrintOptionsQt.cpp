/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsPrintSettingsQt.h"
#include "nsPrintOptionsQt.h"

nsPrintOptionsQt::nsPrintOptionsQt()
{
}

nsPrintOptionsQt::~nsPrintOptionsQt()
{
}

nsresult nsPrintOptionsQt::_CreatePrintSettings(nsIPrintSettings** _retval)
{
    nsPrintSettingsQt* printSettings = 
        new nsPrintSettingsQt(); // does not initially ref count
    NS_ADDREF(*_retval = printSettings); // ref count
    return NS_OK;
}
