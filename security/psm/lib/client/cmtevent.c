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
#include "cmtcmn.h"
#include "cmtutils.h"
#include "messages.h"
#include <string.h>
#ifdef XP_UNIX
#include <sys/time.h>
#endif

/* Typedefs */
typedef void (*taskcompleted_handler_fn)(CMUint32 resourceID, CMUint32 numReqProcessed, CMUint32 resultCode, void* data);

CMTStatus CMT_SetUIHandlerCallback(PCMT_CONTROL control, 
                                   uiHandlerCallback_fn f, void *data)
{
    return CMT_RegisterEventHandler(control, SSM_UI_EVENT, 0,
                                    (void_fun)f, data);
}

void CMT_SetFilePathPromptCallback(PCMT_CONTROL control,
                                   filePathPromptCallback_fn f, void* arg)
{
    control->userFuncs.promptFilePath = f;
    control->userFuncs.filePromptArg  = arg;
}

void CMT_SetPromptCallback(PCMT_CONTROL control, 
                           promptCallback_fn f, void *arg)
{
    control->userFuncs.promptCallback = f;
    control->userFuncs.promptArg      = arg;
}

void CMT_SetSavePrefsCallback(PCMT_CONTROL control, savePrefsCallback_fn f)
{
    control->userFuncs.savePrefs = f;
}

CMTStatus CMT_RegisterEventHandler(PCMT_CONTROL control, CMUint32 type, 
                                   CMUint32 resourceID, void_fun handler, 
                                   void* data)
{
    PCMT_EVENT ptr;

    /* This is the first connection */
    if (control->cmtEventHandlers == NULL) {
        control->cmtEventHandlers = ptr = 
            (PCMT_EVENT)calloc(sizeof(CMT_EVENT), 1);
        if (!ptr) {
            goto loser;
        }
    } else {
        /* Look for another event handler of the same type.  Make sure the
           event handler with a rsrcid of 0 is farther down the list so
	   that it doesn't get chosen when there's an event handler for
	   a specific rsrcid.
	 */
	for (ptr=control->cmtEventHandlers; ptr != NULL; ptr = ptr->next) {
            if (ptr->type == type && resourceID != 0) {
                /* So we've got an event handler that wants to over-ride
                   an existing event handler.  We'll put it before the one
                   that's already here.
                 */
                if (ptr->previous == NULL) {
                    /* We're going to insert at the front of the list*/
                    control->cmtEventHandlers = ptr->previous = 
                        (PCMT_EVENT)calloc(sizeof(CMT_EVENT), 1);
                    if (ptr->previous == NULL) {
                        goto loser;
		    }
		    ptr->previous->next = ptr;
		    ptr = control->cmtEventHandlers;
		} else {
		    /* We want to insert in the middle of the list */
		    PCMT_EVENT tmpEvent;
		    
		    tmpEvent = (PCMT_EVENT)calloc(sizeof(CMT_EVENT), 1);
		    if (tmpEvent == NULL) {
		        goto loser;
		    }
		    tmpEvent->previous  = ptr->previous;
		    ptr->previous->next = tmpEvent;
		    tmpEvent->next      = ptr;
		    ptr->previous       = tmpEvent;
		    ptr = tmpEvent;
		}
		break;
	    }
	    if (ptr->next == NULL) break;
	}
	if (ptr == NULL) {
	    goto loser;	
	}
	if (ptr->next == NULL) {
	    /* We're adding the event handler at the end of the list. */
	    ptr->next = (PCMT_EVENT)calloc(sizeof(CMT_EVENT), 1);
	    if (!ptr->next) {
	        goto loser;
	    } 
	    /* Fix up the pointers */
	    ptr->next->previous = ptr;
	    ptr = ptr->next;
	}
    }

    /* Fill in the data */
    ptr->type = type;
    ptr->resourceID = resourceID;
    ptr->handler = handler;
    ptr->data = data;
    
    return CMTSuccess;
 loser:
    return CMTFailure;
}

CMTStatus CMT_UnregisterEventHandler(PCMT_CONTROL control, CMUint32 type, 
                                     CMUint32 resourceID) 
{
    PCMT_EVENT ptr, pptr = NULL;
    
    for (ptr = control->cmtEventHandlers; ptr != NULL;
	 pptr = ptr, ptr = ptr->next) {
        if ((ptr->type == type) && (ptr->resourceID == resourceID)) {
	    if (pptr == NULL) {
	        /* node is at head */
	        control->cmtEventHandlers = ptr->next;
            if (control->cmtEventHandlers != NULL) {
                control->cmtEventHandlers->previous = NULL;
            }
            free(ptr);
            return CMTSuccess;
	    }
	    /* node is elsewhere */
	    pptr->next = ptr->next;
        if (ptr->next != NULL) {
            ptr->next->previous = pptr;
        }
	    free(ptr);
	    return CMTSuccess;
        }
    }
    return CMTFailure;
}

PCMT_EVENT CMT_GetEventHandler(PCMT_CONTROL control, CMUint32 type, 
                               CMUint32 resourceID)
{
    PCMT_EVENT ptr;
    
    for (ptr = control->cmtEventHandlers; ptr != NULL; ptr = ptr->next) {
        if ((ptr->type == type) && ((ptr->resourceID == resourceID) || 
                                    !ptr->resourceID)) {
            return ptr;
        }
    }
    return NULL;
}

PCMT_EVENT CMT_GetFirstEventHandler(PCMT_CONTROL control, CMUint32 type, 
                                    CMUint32 resourceID)
{
    PCMT_EVENT ptr;

    for (ptr = control->cmtEventHandlers; ptr != NULL; ptr = ptr->next) {
        if ((ptr->type == type) && ((ptr->resourceID == resourceID) || 
                                    !ptr->resourceID)) {
            return ptr;
        }
    }
    return NULL;
}

PCMT_EVENT CMT_GetNextEventHandler(PCMT_CONTROL control, PCMT_EVENT e)
{
    PCMT_EVENT ptr;

    for (ptr = control->cmtEventHandlers; ptr != NULL || ptr == e; 
         ptr = ptr->next) {
    }

    for (; ptr != NULL; ptr = ptr->next) {
        if ((ptr->type == e->type) && ((ptr->resourceID == e->resourceID) || 
                                       !ptr->resourceID)) {
            return ptr;
        }
    }
    return NULL;
}

void CMT_ProcessEvent(PCMT_CONTROL cm_control)
{
    CMTSocket sock; 
    CMTItem eventData={ 0, NULL, 0 };

    /* Get the control socket */
    sock = cm_control->sock;

    /* Acquire a lock on the control connection */
    CMT_LOCK(cm_control->mutex);
    /* Do another select here to be sure 
       that the socket is readable */
    if (cm_control->sockFuncs.select(&sock, 1, 1) != sock) {
        /* There's no event. */
        goto done;
    }

    /* Read the event */
    if (CMT_ReceiveMessage(cm_control, &eventData) == CMTFailure) {
        goto done;
    }
    CMT_UNLOCK(cm_control->mutex);
    /* Dispatch the event */
    CMT_DispatchEvent(cm_control, &eventData);
    return;
done:
    /* Release the lock on the control connection */
    CMT_UNLOCK(cm_control->mutex);
}

void CMT_EventLoop(PCMT_CONTROL cm_control)
{
    CMTSocket sock;

    /* Get the control socket */
    sock = cm_control->sock;
    CMT_ReferenceControlConnection(cm_control);
    /* Select on the control socket to see if it's readable */
    while(cm_control->sockFuncs.select(&sock, 1, 0)) {
      CMT_ProcessEvent(cm_control);
    }
    CMT_CloseControlConnection(cm_control);
    return;
}

void
CMT_PromptUser(PCMT_CONTROL cm_control, CMTItem *eventData)
{
    char *promptReply  = NULL;
    CMTItem   response={ 0, NULL, 0 };
    PromptRequest request;
    PromptReply reply;
    void * clientContext;

    /* Decode the message */
    if (CMT_DecodeMessage(PromptRequestTemplate, &request, eventData) != CMTSuccess) {
        goto loser;
    }

    /* Copy the client context to a pointer */
    clientContext = CMT_CopyItemToPtr(request.clientContext);

    if (cm_control->userFuncs.promptCallback == NULL) {
        goto loser;
    }
    promptReply = 
        cm_control->userFuncs.promptCallback(cm_control->userFuncs.promptArg, 
                                             request.prompt, clientContext, 1);

    response.type = SSM_EVENT_MESSAGE | SSM_PROMPT_EVENT;
    if (!promptReply) {
        /* the user canceled the prompt or other errors occurred */
        reply.cancel = CM_TRUE;
    }
    else {
        /* note that this includes an empty string (zero length) password */
        reply.cancel = CM_FALSE;
    }
    reply.resID = request.resID;
    reply.promptReply = promptReply;

    /* Encode the message */
    if (CMT_EncodeMessage(PromptReplyTemplate, &response, &reply) != CMTSuccess) {
        goto loser;
    }

    CMT_TransmitMessage(cm_control, &response);
 loser:
    if (promptReply != NULL) {
      cm_control->userFuncs.userFree(promptReply);
    }
    return;
}

void CMT_GetFilePath(PCMT_CONTROL cm_control, CMTItem * eventData)
{
    char *fileName=NULL;
    CMTItem response = { 0, NULL, 0 };
    FilePathRequest request;
    FilePathReply reply;

    /* Decode the request */
    if (CMT_DecodeMessage(FilePathRequestTemplate, &request, eventData) != CMTSuccess) {
        goto loser;
    }

    if (cm_control->userFuncs.promptFilePath == NULL) {
        goto loser;
    }
    fileName = 
     cm_control->userFuncs.promptFilePath(cm_control->userFuncs.filePromptArg,
                                          request.prompt, request.fileRegEx, 
                                          request.getExistingFile);

    response.type = SSM_EVENT_MESSAGE | SSM_FILE_PATH_EVENT;
    reply.resID = request.resID;
    reply.filePath = fileName;

    /* Encode the reply */
    if (CMT_EncodeMessage(FilePathReplyTemplate, &response, &reply) != CMTSuccess) {
        goto loser;
    }

    CMT_TransmitMessage(cm_control, &response);
    cm_control->userFuncs.userFree(fileName); 
loser:
   return;
}

void CMT_SavePrefs(PCMT_CONTROL cm_control, CMTItem* eventData)
{
    SetPrefListMessage request;
    int i;

    /* decode the request */
    if (CMT_DecodeMessage(SetPrefListMessageTemplate, &request, eventData) !=
        CMTSuccess) {
        return;
    }

    if (cm_control->userFuncs.savePrefs == NULL) {
        /* callback was not registered: bail */
        return;
    }
    cm_control->userFuncs.savePrefs(request.length, 
                                    (CMTSetPrefElement*)request.list);

    for (i = 0; i < request.length; i++) {
        if (request.list[i].key != NULL) {
            free(request.list[i].key);
        }
        if (request.list[i].value != NULL) {
            free(request.list[i].value);
        }
    }
    return;
}

void CMT_DispatchEvent(PCMT_CONTROL cm_control, CMTItem * eventData)
{
    CMUint32 eventType;
    CMTItem msgCopy;

    /* Init the msgCopy */
    msgCopy.data = 0;    

    /* Get the event type */
    if ((eventData->type & SSM_CATEGORY_MASK) != SSM_EVENT_MESSAGE) {
        /* Somehow there was a message on the socket that was not 
         * an event message.  Dropping it on the floor.
         */
        goto loser;
    }
    eventType = (eventData->type & SSM_TYPE_MASK);

    /* We must now dispatch the event based on it's type */
    switch (eventType) {
        case SSM_UI_EVENT:
        {
            PCMT_EVENT p;
            UIEvent event;
            void * clientContext = NULL;

	    /* Copy the message to allow a second try with the old format */
	    msgCopy.len = eventData->len;
	    msgCopy.data = calloc(msgCopy.len, 1);
	    if (msgCopy.data) {
              memcpy(msgCopy.data, eventData->data, eventData->len);
	    }

            /* Get the event data first */
            if (CMT_DecodeMessage(UIEventTemplate, &event, eventData) != CMTSuccess) {
		/* Attempt to decode using the old format.  Modal is True */
		if (!msgCopy.data || 
		    CMT_DecodeMessage(OldUIEventTemplate, &event, &msgCopy) != CMTSuccess) {
                  goto loser;
                }

                /* Set default modal value */
                event.isModal = CM_TRUE;
            }

            /* Convert the client context to a pointer */
            clientContext = CMT_CopyItemToPtr(event.clientContext);

            /* Call any handlers for this event */
            p = CMT_GetEventHandler(cm_control, eventType, event.resourceID);
            if (!p) {
                goto loser;
            }
            (*(uiHandlerCallback_fn)(p->handler))(event.resourceID, 
                                                  clientContext, event.width,
                                                  event.height, event.isModal, event.url, 
                                                  p->data);
            break;
        }

        case SSM_TASK_COMPLETED_EVENT:
        {
           PCMT_EVENT p;
           TaskCompletedEvent event;

            /* Get the event data */
            if (CMT_DecodeMessage(TaskCompletedEventTemplate, &event, eventData) != CMTSuccess) {
                goto loser;
            }

            /* Call handler for this event */
            p = CMT_GetEventHandler(cm_control, eventType, event.resourceID);
            if (!p) {
                goto loser;
            }
            (*(taskcompleted_handler_fn)(p->handler))(event.resourceID, 
                                                      event.numTasks, 
                                                      event.result, p->data);
            break;
        }
        case SSM_AUTH_EVENT: 
             CMT_ServicePasswordRequest(cm_control, eventData);
             break;
        case SSM_FILE_PATH_EVENT:
    	     CMT_GetFilePath(cm_control, eventData);
             break;
        case SSM_PROMPT_EVENT:
	         CMT_PromptUser(cm_control, eventData);
	         break;
        case SSM_SAVE_PREF_EVENT:
            CMT_SavePrefs(cm_control, eventData);
            break;
        default:
            break;
    }
loser:
    free(eventData->data);
    free(msgCopy.data);
    return;
}

