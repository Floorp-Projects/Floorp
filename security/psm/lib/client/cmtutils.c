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
#if defined(XP_UNIX) || defined(XP_BEOS) || defined(XP_OS2)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#ifdef XP_MAC
#include "macsocket.h"
#else /* Windows */
#include <windows.h>
#include <winsock.h>
#endif
#endif
#include "cmtcmn.h"
#include "cmtutils.h"
#include "newproto.h"
#include <string.h>

/* Local defines */
#if 0
#define PSM_WAIT_BEFORE_SLEEP           (CM_TicksPerSecond() * 60)
#define PSM_SPINTIME                    PSM_WAIT_BEFORE_SLEEP
#define PSM_KEEP_CONNECTION_ALIVE       (PSM_WAIT_BEFORE_SLEEP * 900)
#endif

/* If you want to dump the messages sent between the plug-in and the PSM 
 * server, then remove the comment for the appropriate define.
 */
#if 0
#define PRINT_SEND_MESSAGES
#define PRINT_RECEIVE_MESSAGES
#endif

#ifdef PRINT_SEND_MESSAGES
#ifndef DEBUG_MESSAGES
#define DEBUG_MESSAGES
#endif /*DEBUG_MESSAGES*/
#endif /*PRINT_SEND_MESSAGES*/

#ifdef PRINT_RECEIVE_MESSAGES
#ifndef DEBUG_MESSAGES
#define DEBUG_MESSAGES
#endif /*DEBUG_MESSAGES*/
#endif /*PRINT_RECEIVE_MESSAGES*/

#ifdef DEBUG_MESSAGES
#define LOG(x) do { FILE *f; f=fopen("cmnav.log","a+"); if (f) { \
   fprintf(f, x); fclose(f); } } while(0);
#define LOG_S(x) do { FILE *f; f=fopen("cmnav.log","a+"); if (f) { \
   fprintf(f, "%s", x); fclose(f); } } while(0);
#define ASSERT(x) if (!(x)) { LOG("ASSERT:"); LOG(#x); LOG("\n"); exit(-1); }
#else
#define LOG(x)
#define LOG_S(x)
#define ASSERT(x)
#endif

CMUint32
cmt_Strlen(char *str)
{
    CMUint32 len = strlen(str);
    return sizeof(CMInt32) + (((len + 3)/4)*4);
}

CMUint32
cmt_Bloblen(CMTItem *blob)
{
    return sizeof(CMInt32) + (((blob->len +3)/4)*4);
}

char *
cmt_PackString(char *buf, char *str)
{
    CMUint32 len = strlen(str);
    CMUint32 networkLen = htonl(len);
    CMUint32 padlen = ((len + 3)/4)*4;

    memcpy(buf, &networkLen, sizeof(CMUint32));
    memcpy(buf + sizeof(CMUint32), str, len);
    memset(buf + sizeof(CMUint32) + len, 0, padlen - len);

    return buf+sizeof(CMUint32)+padlen;
}

char *
cmt_PackBlob(char *buf, CMTItem *blob)
{
    CMUint32 len = blob->len;
    CMUint32 networkLen = htonl(len);
    CMUint32 padlen = (((blob->len + 3)/4)*4);

    *((CMUint32*)buf) = networkLen;
    memcpy(buf + sizeof(CMUint32), blob->data, len);
    memset(buf + sizeof(CMUint32) + len, 0, padlen - len);

    return buf + sizeof(CMUint32) + padlen;
}

char *
cmt_UnpackString(char *buf, char **str)
{
    char *p = NULL;
    CMUint32 len, padlen;

    /* Get the string length */
    len = ntohl(*(CMUint32*)buf);

    /* Get the padded length */
    padlen = ((len + 3)/4)*4;

    /* Allocate the string and copy the data */
    p = (char *) malloc(len + 1);
    if (!p) {
        goto loser;
    }
    /* Copy the data and NULL terminate */
    memcpy(p, buf+sizeof(CMUint32), len);
    p[len] = 0;

    *str = p;
    return buf+sizeof(CMUint32)+padlen;
loser:
    *str = NULL;
    if (p) {
        free(p);
    }
    return buf+sizeof(CMUint32)+padlen;
}

char *
cmt_UnpackBlob(char *buf, CMTItem **blob)
{
    CMTItem *p = NULL;
    CMUint32 len, padlen;

    /* Get the blob length */
    len = ntohl(*(CMUint32*)buf);
    
    /* Get the padded length */
    padlen = ((len + 3)/4)*4;

    /* Allocate the CMTItem for the blob */
    p = (CMTItem*)malloc(sizeof(CMTItem));
    if (!p) {
        goto loser;
    }
    p->len = len;
    p->data = (unsigned char *) malloc(len);
    if (!p->data) {
        goto loser;
    }

    /* Copy that data across */
    memcpy(p->data, buf+sizeof(CMUint32), len);
    *blob = p;

    return buf+sizeof(CMUint32)+padlen;

loser:
    *blob = NULL;
    CMT_FreeMessage(p);

    return buf+sizeof(CMUint32)+padlen;
}

#ifdef DEBUG_MESSAGES
void prettyPrintMessage(CMTItem *msg)
{
  int numLines = ((msg->len+7)/8);
  char curBuffer[9], *cursor, string[2], hexVal[8];
  char hexArray[25];
  int i, j, numToCopy;

  /*Try printing out 8 bytes at a time. */
  LOG("\n**********************************************************\n");
  LOG("About to pretty Print Message\n\n");
  curBuffer[8] = '\0';
  hexArray[24] = '\0';
  hexVal[2] = '\0';
  string[1] = '\0';
  LOG("Header Info\n");
  LOG("Message Type: ");
  sprintf(hexArray, "%lx\n", msg->type);
  LOG(hexArray);
  LOG("Message Length: ");
  sprintf (hexArray, "%ld\n\n", msg->len);
  LOG(hexArray);
  LOG("Body of Message\n");
  for (i=0, cursor=msg->data; i<numLines; i++, cursor+=8) {
    /* First copy over the buffer to our local array */
    numToCopy = ((msg->len - (unsigned int)((unsigned long)cursor-(unsigned long)msg->data)) < 8) ?
                msg->len - (unsigned int)((unsigned long)cursor-(unsigned long)msg->data) : 8;
    memcpy(curBuffer, cursor, 8);
    for (j=0;j<numToCopy;j++) {
      string[0] = curBuffer[j];
      if (isprint(curBuffer[j])) {
	string[0] = curBuffer[j];
      } else {
	string[0] = ' ';
      }
      LOG(string);
    }
    string[0] = ' ';
    for (;j<8;j++) {
      LOG(string);
    }
    LOG("\t");
    for (j=0; j<numToCopy; j++) {
      sprintf (hexVal,"%.2x", 0x0ff & (unsigned short)curBuffer[j]);
      LOG(hexVal);
      LOG(" ");
    }
    LOG("\n");
  }
  LOG("Done Pretty Printing Message\n");
  LOG("**********************************************************\n\n");
}
#endif

CMTStatus CMT_ReadMessageDispatchEvents(PCMT_CONTROL control, CMTItem* message)
{
    CMTStatus status;
    CMBool done = CM_FALSE;
    CMUint32 msgCategory;
    
    /* We have to deal with other types of data on the socket and */
    /* handle them accordingly */
    while (!done) {
    	status = CMT_ReceiveMessage(control, message);
        if (status != CMTSuccess) {
            goto loser;
        }
        msgCategory = (message->type & SSM_CATEGORY_MASK);
        switch (msgCategory) {
            case SSM_REPLY_OK_MESSAGE:
                done = CM_TRUE;
                break;
            case SSM_REPLY_ERR_MESSAGE:
                done = CM_TRUE;
                break;
            case SSM_EVENT_MESSAGE:
                CMT_DispatchEvent(control, message);
                break;
            /* XXX FIX THIS!!! For the moment I'm ignoring all other types */
            default:
                break;
        }
    }
    return CMTSuccess;
 loser:
    return CMTFailure;
}

CMTStatus CMT_SendMessage(PCMT_CONTROL control, CMTItem* message)
{
	CMTStatus status;
#ifdef PRINT_SEND_MESSAGES
    LOG("About to print message sent to PSM\n");
    prettyPrintMessage(message);
#endif

    /* Acquire lock on the control connection */
    CMT_LOCK(control->mutex);

    /* Try to send pending random data */
    if (message->type != (SSM_REQUEST_MESSAGE | SSM_HELLO_MESSAGE))
    {
        /* If we've already said hello, then flush random data
           just before sending the request. */
        status = CMT_FlushPendingRandomData(control);
        if (status != CMTSuccess)
            goto loser;
    }
    
	status = CMT_TransmitMessage(control, message);
	if (status != CMTSuccess) {
		goto loser;
	}

    if (CMT_ReadMessageDispatchEvents(control, message) != CMTSuccess) {
        goto loser;
    }
    /* Release the control connection lock */
    CMT_UNLOCK(control->mutex);
	return CMTSuccess;
loser:
    /* Release the control connection lock */
    CMT_UNLOCK(control->mutex);
	return CMTFailure;
}

CMTStatus CMT_TransmitMessage(PCMT_CONTROL control, CMTItem * message)
{
    CMTMessageHeader header;
	CMUint32 sent;

    /* Set up the message header */
    header.type = htonl(message->type);
    header.len = htonl(message->len);

	/* Send the message header */
	sent = CMT_WriteThisMany(control, control->sock, 
                             (void *)&header, sizeof(CMTMessageHeader));
	if (sent != sizeof(CMTMessageHeader)) {
		goto loser;
	}

	/* Send the message body */
	sent = CMT_WriteThisMany(control, control->sock, (void *)message->data, 
                             message->len);
	if (sent != message->len) {
		goto loser;
	}

    /* Free the buffer */
    free(message->data);
    message->data = NULL;
	return CMTSuccess;

loser:
	return CMTFailure;
}

CMTStatus CMT_ReceiveMessage(PCMT_CONTROL control, CMTItem * response)
{
    CMTMessageHeader header;
    CMUint32 numread;

    /* Get the message header */
    numread = CMT_ReadThisMany(control, control->sock, 
                            (void *)&header, sizeof(CMTMessageHeader));
    if (numread != sizeof(CMTMessageHeader)) {
        goto loser;
    }

    response->type = ntohl(header.type);
    response->len = ntohl(header.len);
    response->data = (unsigned char *) malloc(response->len);
    if (response->data == NULL && response->len != 0) {
        goto loser;
    }

    numread = CMT_ReadThisMany(control, control->sock, 
                            (void *)(response->data), response->len); 
    if (numread != response->len) {
        goto loser;
    }

#ifdef PRINT_RECEIVE_MESSAGES
    LOG("About to print message received from PSM.\n");
    prettyPrintMessage(response);
#endif /*PRINT_RECEIVE_MESSAGES*/
    return CMTSuccess;
loser:
    if (response->data) {
        free(response->data);
    }
    return CMTFailure;
}

CMUint32 CMT_ReadThisMany(PCMT_CONTROL control, CMTSocket sock, 
                          void * buffer, CMUint32 thisMany)
{
	CMUint32 total = 0;
  
	while (total < thisMany) {
		int got;
		got = control->sockFuncs.recv(sock, (void*)((char*)buffer + total),
                                      thisMany-total);
		if (got < 0 ) {
			break;
		} 
		total += got;
	} 
	return total;
}
 
CMUint32 CMT_WriteThisMany(PCMT_CONTROL control, CMTSocket sock, 
                           void * buffer, CMUint32 thisMany)
{
	CMUint32 total = 0;
  
	while (total < thisMany) {
		CMInt32 got;
		got = control->sockFuncs.send(sock, (void*)((char*)buffer+total), 
                                      thisMany-total);
		if (got < 0) {
			break;
		}
		total += got;
	}
	return total;
}

CMTItem* CMT_ConstructMessage(CMUint32 type, CMUint32 length)
{
    CMTItem * p;

    p = (CMTItem*)malloc(sizeof(CMTItem));
    if (!p) {
	goto loser;
    }

    p->type = type;
    p->len = length;
    p->data = (unsigned char *) malloc(length);
    if (!p->data) {
	goto loser;
    }
    return p;

loser:
    CMT_FreeMessage(p);
    return NULL;
}

void CMT_FreeMessage(CMTItem * p)
{
    if (p != NULL) {
	if (p->data != NULL) {
	    free(p->data);
	}
	free(p);
    }
}

CMTStatus CMT_AddDataConnection(PCMT_CONTROL control, CMTSocket sock, 
                                CMUint32 connectionID)
{
	PCMT_DATA ptr;

	/* This is the first connection */
	if (control->cmtDataConnections == NULL) {
		control->cmtDataConnections = ptr = 
            (PCMT_DATA)calloc(sizeof(CMT_DATA), 1);
		if (!ptr) {
			goto loser;
		}
	} else {
	    /* Position at the last entry */
	    for (ptr = control->cmtDataConnections; (ptr != NULL && ptr->next 
                                                 != NULL); ptr = ptr->next);
		ptr->next = (PCMT_DATA)calloc(sizeof(CMT_DATA), 1);
		if (!ptr->next) {
			goto loser;
		} 
		/* Fix up the pointers */
		ptr->next->previous = ptr;
		ptr = ptr->next;
	}

	/* Fill in the data */
	ptr->sock = sock;
	ptr->connectionID = connectionID;

	return CMTSuccess;
loser:
	return CMTFailure;
}

int
CMT_DestroyDataConnection(PCMT_CONTROL control, CMTSocket sock)
{
    PCMT_DATA ptr, pptr = NULL;
    int rv=CMTSuccess;

    if (!control) return rv;

    control->sockFuncs.close(sock);
    for (ptr = control->cmtDataConnections; ptr != NULL;
	 pptr = ptr, ptr = ptr->next) {
	if (ptr->sock == sock) {
	    if (pptr == NULL) {
		/* node is at head */
		control->cmtDataConnections = ptr->next;
		if (ptr->priv != NULL)
		    ptr->priv->dest(ptr->priv);
		free(ptr);
		return rv;
	    }
	    /* node is elsewhere */
	    pptr->next = ptr->next;
	    if (ptr->priv != NULL)
	        ptr->priv->dest(ptr->priv);
	    free(ptr);
	    return rv;
	}
    }
    return rv;
}

CMTStatus CMT_CloseDataConnection(PCMT_CONTROL control, CMUint32 connectionID)
{
    /* PCMT_DATA ptr, pptr = NULL; */
    CMTSocket sock;
    /* int rv;*/

    /* Get the socket for this connection */
    if (CMT_GetDataSocket(control, connectionID, &sock) == CMTFailure) {
        goto loser;
    }

    /* Free data connection associated with this socket */
    if (CMT_DestroyDataConnection(control, sock) == CMTFailure) {
        goto loser;
    }

    return CMTSuccess;
loser:
    return CMTFailure;
}

CMTStatus CMT_GetDataConnectionID(PCMT_CONTROL control, CMTSocket sock, CMUint32 * connectionID)
{
	PCMT_DATA ptr;

	for (ptr = control->cmtDataConnections; ptr != NULL; ptr = ptr->next) {
		if (ptr->sock == sock) {
			*connectionID = ptr->connectionID;
			return CMTSuccess;
		}
	}

	return CMTFailure;
}

CMTStatus CMT_GetDataSocket(PCMT_CONTROL control, CMUint32 connectionID, CMTSocket * sock)
{
	PCMT_DATA ptr;

	for (ptr = control->cmtDataConnections; ptr != NULL; ptr = ptr->next) {
		if (ptr->connectionID == connectionID) {
			*sock = ptr->sock;
			return CMTSuccess;
		}
	}

	return CMTFailure;
}


CMTStatus CMT_SetPrivate(PCMT_CONTROL control, CMUint32 connectionID,
			 CMTPrivate *cmtpriv)
{
  PCMT_DATA ptr;

  for (ptr = control->cmtDataConnections; ptr != NULL; ptr = ptr->next) {
    if (ptr->connectionID == connectionID) {
      ptr->priv = cmtpriv;
      return CMTSuccess;
    }
  }
  return CMTFailure;
}

CMTPrivate *CMT_GetPrivate(PCMT_CONTROL control, CMUint32 connectionID)
{
  PCMT_DATA ptr;

  for (ptr = control->cmtDataConnections; ptr != NULL; ptr = ptr->next) {
    if (ptr->connectionID == connectionID) {
      return ptr->priv;
    }
  }
  return NULL;
}

void CMT_FreeItem(CMTItem *p)
{
  CMT_FreeMessage(p);
}

CMTItem CMT_CopyPtrToItem(void* p)
{
    CMTItem value = {0, NULL, 0};

    if (!p) {
        return value;
    }

    value.len = sizeof(p);
    value.data = (unsigned char *) malloc(value.len);
    memcpy(value.data, &p, value.len);

    return value;
}

void * CMT_CopyItemToPtr(CMTItem value)
{
    void * p = NULL;

    if (value.len == sizeof(void*)) {
        memcpy(&p, value.data, value.len);
    }

    return p;
}

CMTStatus CMT_ReferenceControlConnection(PCMT_CONTROL control)
{    CMT_LOCK(control->mutex);
    control->refCount++;
    CMT_UNLOCK(control->mutex);
    return CMTSuccess;
}

void
CMT_LockConnection(PCMT_CONTROL control)
{
    CMT_LOCK(control->mutex);
}

void
CMT_UnlockConnection(PCMT_CONTROL control)
{
    CMT_UNLOCK(control->mutex);
}
