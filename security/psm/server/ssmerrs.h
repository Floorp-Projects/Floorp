/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#ifndef __SSMERRS_H__
#define __SSMERRS_H__

#include "prtypes.h"

enum
{
  SSM_SUCCESS = (int) 0,
  SSM_FAILURE = (int) PR_FAILURE,

  SSM_ERROR_BASE = (int) -10000,
  SSM_ERR_ALREADY_SHUT_DOWN,
  SSM_ERR_INVALID_FUNC,
  SSM_ERR_BAD_RESOURCE_TYPE,
  SSM_ERR_BAD_RESOURCE_ID,
  SSM_ERR_BAD_ATTRIBUTE_ID,
  SSM_ERR_ATTRIBUTE_TYPE_MISMATCH,
  SSM_ERR_ATTRIBUTE_MISSING,
  SSM_ERR_DEFER_RESPONSE, /* internal: tell control connection to wait
                             on a response from another thread -- the
                             response is placed in the control queue */
  SSM_ERR_BAD_REQUEST, /* couldn't understand the request */
  SSM_ERR_USER_CANCEL, /* explicit cancel by user */
  SSM_ERR_WAIT_TIMEOUT, /* wait has timed out */
  SSM_ERR_CLIENT_DESTROY, /* explicit destroy by client sw */
  SSM_ERR_NO_USER_CERT,  /* there was no usable user cert for client auth */
  SSM_ERR_NO_TARGET, /*Could not find a target to handle a form submission */
  SSM_ERR_NO_BUTTON, /* Could not find an OK or Cancel value in a 
                        form submission
                      */
  SSM_ERR_NO_FORM_NAME,
  SSM_ERR_OUT_OF_MEMORY,
  SSM_TIME_EXPIRED,
  SSM_ERR_NO_PASSWORD,
  SSM_ERR_BAD_PASSWORD,
  SSM_ERR_BAD_DB_PASSWORD,
  SSM_ERR_BAD_FILENAME,
  SSM_ERR_NEED_USER_INIT_DB,
  SSM_ERR_CANNOT_DECODE,
  SSM_ERR_NEW_DEF_MAIL_CERT,
  SSM_PKCS12_CERT_ALREADY_EXISTS,
  SSM_ERR_MAX_ERR
};

#endif

