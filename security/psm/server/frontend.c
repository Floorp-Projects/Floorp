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
#include "ctrlconn.h"
#include "dataconn.h"
#include "sslconn.h"
#include "p7cinfo.h"
#include "hashconn.h"
#include "servimpl.h"
#include "newproto.h"
#include "messages.h"
#include "serv.h"
#include "advisor.h"
#include "ssmerrs.h"

#ifdef TIMEBOMB
#include "timebomb.h"
#include "timebomb.c"
#endif

/* The ONLY reason why we can use these macros for both control and
   data connections is that they inherit from the same superclass.
   Do NOT try this at home. */
#define SSMCONNECTION(c) (&(c)->super)
#define SSMRESOURCE(c) (&(c)->super.super)

extern long ssm_ctrl_count;


/* This thread reads the control connection socket and 
 * forwards messages to appropriate queues. 
 */

void SSM_FrontEndThread(void * arg)
{
  PRFileDesc *                socket = NULL;
  SECItem    *                message = NULL;
  SSMControlConnection *      control = NULL;
  PRIntn                      read;
  SSMStatus                    rv = PR_SUCCESS;
  char *                      passwd = NULL;
  char *                      tmp;
  PasswordReply               reply;
  CMTMessageHeader            header;
  
  control = (SSMControlConnection *)arg;
  SSM_RegisterNewThread("ctrl frontend", (SSMResource *) arg);
  SSM_DEBUG("initializing.\n");
  if (!control || !control->m_socket) 
    {
      PORT_SetError(PR_INVALID_ARGUMENT_ERROR);
      goto loser;
    }
  control->m_frontEndThread = PR_GetCurrentThread();
  socket = control->m_socket;

#ifdef TIMEBOMB
   if (SSMTimeBombExpired) 
     SSM_CartmanHasExpired(control);
#endif  

    /* start a write thread for this connection */
    SSM_DEBUG("creating accompanying write thread.\n");
    control->m_writeThread = SSM_CreateThread(SSMRESOURCE(control),
					      SSM_WriteCtrlThread);
    if (!control->m_writeThread) 
    {
        rv = PR_FAILURE;
        goto loser;
    }

    /* initialize connection outgoing queue */
    SSM_DEBUG("Initialize OUT queue for control connection\n");
    SSM_SendQMessage(control->m_controlOutQ, 
                     SSM_PRIORITY_NORMAL,
                     SSM_DATA_PROVIDER_OPEN, 0, NULL,
                     PR_TRUE);

  while ((SSMRESOURCE(control)->m_status == PR_SUCCESS) 
	 && (rv == PR_SUCCESS))
    {
      /* read and process data here
       */
      
      SSM_DEBUG("waiting for new message from socket.\n");
      read = SSM_ReadThisMany(socket, &header, sizeof(CMTMessageHeader));
      if (read != sizeof(CMTMessageHeader)) 
	{
	  SSM_DEBUG("Bytes read (%ld) != bytes expected (%ld). (hdr)\n",
				(long) read, 
				(long) sizeof(CMTMessageHeader));
	  rv = PR_FAILURE;
	  goto loser;
	}

      message = SSM_ConstructMessage(PR_ntohl(header.len));
      if (!message)
	{
	  SSM_DEBUG("Missing message.\n");
	  rv = PR_OUT_OF_MEMORY_ERROR;
	  goto loser;
	}
      message->type = (SECItemType) PR_ntohl(header.type);

      /* Read the message body */
      SSM_DEBUG("reading %ld more from socket.\n", message->len);
      read = SSM_ReadThisMany(socket, (void *)message->data, message->len);
      if ((unsigned int) read != message->len)
	{
	  SSM_DEBUG("Bytes read (%ld) != bytes expected (%ld). (msg)\n",
				(long) read, 
				(long) message->len);
	  rv = PR_GetError();
	  if (rv == PR_SUCCESS) rv = PR_FAILURE;
	  goto loser;
	}
       
      /* 
       * Message successfully received, now file it to appropriate queue 
       *//*
      if ((message->type & SSM_CATEGORY_MASK) == SSM_EVENT_MESSAGE) {
	 */
      switch (message->type &SSM_CATEGORY_MASK) {
      case SSM_EVENT_MESSAGE:
	switch (message->type & SSM_TYPE_MASK) {
	case SSM_AUTH_EVENT:
	  /* deal with authentication message */
	  
      /* Decode the the password reply message */
      if (CMT_DecodeMessage(PasswordReplyTemplate, &reply, (CMTItem*)message) != CMTSuccess) {
          goto loser;
      }

	  /* Get the lock for the password hash table */
	  SSM_LockPasswdTable((SSMConnection *) control);
	  rv = SSM_HashRemove(control->m_passwdTable, reply.tokenID, (void **)&tmp);
	  if (rv != PR_SUCCESS) { 
	    SSM_DEBUG("Passwd request for token %d hasn't been registered.\n",
		      reply.tokenID);
	    SSM_DEBUG("Drop the message, continue waiting...\n");
	    PR_Free(passwd);
	    passwd = NULL;
	    goto done_auth;
	  }
	  if (reply.result != 0) {
	      SSM_DEBUG("Error getting password %d.\n", reply.result);
	      reply.passwd = (char*)SSM_CANCEL_PASSWORD;
	  }

	  if (!reply.passwd) {
	      /* user entered a zero length password, which is valid */
	      reply.passwd = "";
	  }

	  rv = SSM_HashInsert(control->m_passwdTable, reply.tokenID, 
			      reply.passwd);
	  if (rv != PR_SUCCESS) 
	    SSM_DEBUG("%ld: can't enter passwd in hash table.\n", control);
	  
	  passwd = NULL; /* passwd is now pointed to the hash table entry */
	  /* notify all waiting threads that the password has arrived */
	  SSM_DEBUG("%ld : notify password table that passwd is available\n", 
		    control);
	  rv = SSM_NotifyAllPasswdTable((SSMConnection *)control);
	  if (rv != PR_SUCCESS) {
	    SSM_DEBUG("Error on NotifyAll on the password table:%d.\n", 
		      PR_GetError());
	    goto done_auth;
	  }
	done_auth:
	  /* We are done, release the lock */
	  SSM_UnlockPasswdTable((SSMConnection *)control);
	  break;
	case SSM_FILE_PATH_EVENT:
	  SSM_HandleFilePathReply(control, message);
	  break;
	case SSM_PROMPT_EVENT:
	  SSM_HandleUserPromptReply(control, message);
	  break;
#if 0
	case SSM_GET_JAVA_PRINCIPALS_EVENT:
	  SSM_HandleGetJavaPrincipalsReply(control, message);
	  break;
#endif
        case SSM_MISC_EVENT:
            if ((message->type & SSM_SUBTYPE_MASK) == SSM_MISC_PUT_RNG_DATA)
            {
                /* Got fodder for the RNG. Seed it. */
                SSM_DEBUG("Got %ld bytes of RNG seed data.\n", message->len);
                rv = RNG_RandomUpdate(message->data, message->len) 
                    == SECSuccess ? SSM_SUCCESS : SSM_FAILURE;
            }
	default:
	  break;
	}
	break;
      default:
	/* process the message */
	rv = SSMControlConnection_ProcessMessage(control, message);
      }

      /* Free the message */
      SSM_FreeMessage(message);
      message = NULL;
    } /* end of the read-socket loop */
 loser:
  SSM_DEBUG("shutting down, status = %ld.\n", rv);
  if (control)
    {
      SSM_ShutdownResource(SSMRESOURCE(control), rv);
      SSM_FreeResource(SSMRESOURCE(control));
    }
  return;
}

