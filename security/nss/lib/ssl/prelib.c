/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil -*- */

/*
 * Functions used by https servers to send (download) pre-encrypted files
 * over SSL connections that use Fortezza ciphersuites.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: prelib.c,v 1.8 2012/04/25 14:50:12 gerv%gerv.net Exp $ */

#include "cert.h"
#include "ssl.h"
#include "keyhi.h"
#include "secitem.h"
#include "sslimpl.h"
#include "pkcs11t.h"
#include "preenc.h"
#include "pk11func.h"

PEHeader *SSL_PreencryptedStreamToFile(PRFileDesc *fd, PEHeader *inHeader, 
				       int *headerSize)
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return NULL;
}

PEHeader *SSL_PreencryptedFileToStream(PRFileDesc *fd, PEHeader *header, 
							int *headerSize)
{
    PORT_SetError(PR_NOT_IMPLEMENTED_ERROR);
    return NULL;
}


