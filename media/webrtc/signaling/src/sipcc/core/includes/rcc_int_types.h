/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _RCC_INT_TYPES_H_
#define _RCC_INT_TYPES_H_

#include "phone_types.h"
#include "xml_parser_defines.h"
#include "sll_lite.h"

// The table below should be kept in sync with the enum type
// in the RemoteCC XML
typedef enum {
    RCC_SOFTKEY_UNDEFINED = 0,
    RCC_SOFTKEY_REDIAL = 1,
    RCC_SOFTKEY_NEWCALL,
    RCC_SOFTKEY_HOLD,
    RCC_SOFTKEY_TRANSFER,
    RCC_SOFTKEY_CFWDALL,
    RCC_SOFTKEY_CFWDBUSY,
    RCC_SOFTKEY_CFWDNOANS,
    RCC_SOFTKEY_BACKSPACE,
    RCC_SOFTKEY_ENDCALL,
    RCC_SOFTKEY_RESUME,
    RCC_SOFTKEY_ANSWER,
    RCC_SOFTKEY_INFO,
    RCC_SOFTKEY_CONFERENCE,
    RCC_SOFTKEY_JOIN,
    RCC_SOFTKEY_RMLASTCONF,
    RCC_SOFTKEY_BARGE,
    RCC_SOFTKEY_DIRECTXFER,
    RCC_SOFTKEY_SELECT,
    RCC_SOFTKEY_CBARGE,
    RCC_SOFTKEY_DIV_ALL,
    RCC_SOFTKEY_TRANSFERTOVM,
    RCC_SOFTKEY_UNSELECT,
    RCC_SOFTKEY_CANCEL,
    RCC_SOFTKEY_CONFDETAILS,
    RCC_IPMA_SKEY_TRANSFAS = 66,
    RCC_IPMA_SKEY_INTRCPT = 67,
    RCC_IPMA_SKEY_TRANSFVM = 68,
    RCC_IPMA_SKEY_TRANSMG = 69,
    RCC_IPMA_SKEY_SETWTCH = 70,
} rcc_softkey_event_t;

/*
 * 400 - Invalid line handle
 * 401 - Invalid calling address
 * 402 - Invalid called address
 * 403 - User data too large
 * 404 - PassThrough data too large
 * 405 - Cannot process request in current line state
 */

typedef enum {
    RCC_ERR = -1,
    RCC_NONE = 0,
    RCC_TRYING = 100,
    RCC_SUCCESS = 200,
    RCC_SUCCESS_COMPLETE = 201,
    RCC_COMPLETE = 300,
    RCC_FAILED = 400,
    RCC_INVALID_LINE = 401,
    RCC_INVALID_CALLING_ADDR = 402,
    RCC_INVALID_CALLED_ADDR = 403,
    RCC_LARGE_DATA = 404,
    RCC_PROCESS_REQ_FAILED = 405,
    RCC_RESP_MAX
} rcc_resp_code_e;

#endif //_RCC_INT_TYPES_H_
