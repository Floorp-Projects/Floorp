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
#ifndef __NEWPROTO_H__
#define __NEWPROTO_H__

#include <stdlib.h>
#include "ssmdefs.h"

typedef enum CMTDataType {
    CMT_DT_END,
    CMT_DT_RID,
    CMT_DT_INT,
    CMT_DT_BOOL,
    CMT_DT_STRING,
    CMT_DT_ITEM,
    CMT_DT_LIST,
    CMT_DT_CHOICE,
    CMT_DT_END_CHOICE,
	CMT_DT_STRUCT_LIST,
	CMT_DT_END_STRUCT_LIST,
	CMT_DT_STRUCT_PTR
} CMTDataType;

typedef struct CMTMessageTemplate {
    CMTDataType type;
    CMUint32 offset;
    CMInt32 validator;
    CMInt32 choiceID;
} CMTMessageTemplate;

typedef struct CMTMessageHeader {
    CMInt32 type;
    CMInt32 len;
} CMTMessageHeader;

typedef void *(* CMT_Alloc_fn) (size_t size);
typedef void (* CMT_Free_fn)(void * ptr);

extern CMT_Alloc_fn cmt_alloc;
extern CMT_Free_fn cmt_free;

/*************************************************************
 *
 * CMT_Init
 *
 *
 ************************************************************/
void
CMT_Init(CMT_Alloc_fn allocfn, CMT_Free_fn freefn);

/*************************************************************
 * CMT_DecodeMessage
 *
 * Decode msg into dest as specified by tmpl.
 *
 ************************************************************/
CMTStatus
CMT_DecodeMessage(CMTMessageTemplate *tmpl, void *dest, CMTItem *msg);


/*************************************************************
 * CMT_EncodeMessage
 *
 * Encode src into msg as specified by tmpl.
 *
 ************************************************************/
CMTStatus
CMT_EncodeMessage(CMTMessageTemplate *tmpl, CMTItem *msg, void *src);


#endif /* __NEWPROTO_H__ */
