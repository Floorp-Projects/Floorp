/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * To add additional errors to Storage, please append entries to the bottom of
 * the list in the following format:
 * '#define NS_ERROR_STORAGE_YOUR_ERR \
 *    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_STORAGE, n)'
 * where n is the next unique positive integer.  You must also add an entry to
 * js/xpconnect/src/xpc.msg under the code block beginning with the comment
 * 'storage related codes (from mozStorage.h)', in the following format:
 * 'XPC_MSG_DEF(NS_ERROR_STORAGE_YOUR_ERR, "brief description of your error")'
 */

#ifndef MOZSTORAGE_H
#define MOZSTORAGE_H

#define NS_ERROR_STORAGE_BUSY \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_STORAGE, 1)

#define NS_ERROR_STORAGE_IOERR \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_STORAGE, 2)

#define NS_ERROR_STORAGE_CONSTRAINT \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_STORAGE, 3)

#endif /* MOZSTORAGE_H */
