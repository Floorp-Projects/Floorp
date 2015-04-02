/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOMRT_INIT_H__
#define XPCOMRT_INIT_H__

#include "nsError.h"

nsresult NS_InitXPCOMRT();
nsresult NS_ShutdownXPCOMRT();

#endif // define XPCOMRT_INIT_H__
