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
#ifndef __CMTUTILS_H__
#define __CMTUTILS_H__

#include "cmtcmn.h"

#define New(type) (type*)malloc(sizeof(type))
#define NewArray(type, size) (type*)malloc(sizeof(type)*(size))

PCMT_EVENT CMT_GetEventHandler(PCMT_CONTROL control, CMUint32 type,
                               CMUint32 resourceID);

CMUint32 cmt_Strlen(char *str);
char *cmt_PackString(char *buf, char *str);
char *cmt_UnpackString(char *buf, char **str);

CMUint32 cmt_Bloblen(CMTItem* len);
char *cmt_PackBlob(char *buf, CMTItem * blob);
char *cmt_UnpackBlob(char *buf, CMTItem **blob);

CMTStatus CMT_SendMessage(PCMT_CONTROL control, CMTItem* message);
CMTStatus CMT_TransmitMessage(PCMT_CONTROL control, CMTItem * message);
CMTStatus CMT_ReceiveMessage(PCMT_CONTROL control, CMTItem * response);
CMTStatus CMT_ReadMessageDispatchEvents(PCMT_CONTROL control, 
					CMTItem* message);
CMUint32 CMT_ReadThisMany(PCMT_CONTROL control, CMTSocket sock, 
			  void * buffer, CMUint32 thisMany);
CMUint32 CMT_WriteThisMany(PCMT_CONTROL control, CMTSocket sock, 
			   void * buffer, CMUint32 thisMany);
CMTItem* CMT_ConstructMessage(CMUint32 type, CMUint32 length);
void CMT_FreeMessage(CMTItem * p);
CMTStatus CMT_AddDataConnection(PCMT_CONTROL control, CMTSocket sock, CMUint32 connectionID);
CMTStatus CMT_GetDataConnectionID(PCMT_CONTROL control, CMTSocket sock, CMUint32 * connectionID);
CMTStatus CMT_GetDataSocket(PCMT_CONTROL control, CMUint32 connectionID, CMTSocket * sock);
CMTStatus CMT_CloseDataConnection(PCMT_CONTROL control, CMUint32 connectionID);
CMTStatus CMT_SetPrivate(PCMT_CONTROL control, CMUint32 connectionID,
			 CMTPrivate *cmtpriv);
CMTPrivate *CMT_GetPrivate(PCMT_CONTROL control, CMUint32 connectionID);
void CMT_ServicePasswordRequest(PCMT_CONTROL cm_control, CMTItem * requestData);
void CMT_ProcessEvent(PCMT_CONTROL cm_control);
void CMT_DispatchEvent(PCMT_CONTROL cm_control, CMTItem * eventData);
CMTItem CMT_CopyPtrToItem(void* p);
void * CMT_CopyItemToPtr(CMTItem value);

#endif /* __CMTUTILS_H__ */

