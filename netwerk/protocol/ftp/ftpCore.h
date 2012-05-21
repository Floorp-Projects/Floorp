/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ftpCore_h___
#define __ftpCore_h___

#include "nsNetError.h"

/**
 * Status nsresult codes
 */
#define NS_NET_STATUS_BEGIN_FTP_TRANSACTION \
    NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_NETWORK, 27)

#define NS_NET_STATUS_END_FTP_TRANSACTION \
    NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_NETWORK, 28)

#endif // __ftpCore_h___
