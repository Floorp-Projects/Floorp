/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*
 * Stub protocol for turning socket i/o into a stream interface.
 */

#include "xp.h"
#include "netutils.h"
#include "mktcp.h"
#include "mkgeturl.h"
#include "mkselect.h"
#include "netstream.h"
#include "plstr.h"
#include "prerror.h"
#include "prmem.h"
#include "prmon.h"

#include "nsHashtable.h"
#include "sockstub.h"

#define DEF_SOCKSTUB_PORT 80

extern "C" {
extern int MK_UNABLE_TO_LOCATE_HOST;
extern int MK_ZERO_LENGTH_FILE;
extern int MK_TCP_READ_ERROR;
extern int MK_OUT_OF_MEMORY;

}

/* definitions of state for the state machine design
 */
typedef enum {
    SOCKSTUB_START_CONNECT,
    SOCKSTUB_FINISH_CONNECT,
    SOCKSTUB_SETUP_STREAM,
    SOCKSTUB_FLUSH_DATA,
    SOCKSTUB_DONE,
    SOCKSTUB_ERROR_DONE,
    SOCKSTUB_FREE
} SockStubStatesEnum;

typedef struct _SockStubConData {
    SockStubStatesEnum  next_state;       /* the next state or action to be taken */
    char*               line_buffer;      /* temporary string storage */
    int32               line_buffer_size; /* current size of the line buffer */
    NET_StreamClass*    stream; /* The output stream */
    Bool                pause_for_read;   /* Pause now for next read? */
    TCP_ConData*        tcp_con_data;  /* Data pointer for tcp connect state machine */
    Bool                reuse_stream;
    Bool                connection_is_valid;
    char*               hostname;       /* hostname string (may contain :port) */
    PRFileDesc*         sock;       /* socket */
    int32               original_content_length;
} SockStubConData;

PRIVATE int32
net_InterruptSockStub (ActiveEntry * ce);

PRIVATE int32
net_ProcessSockStub (ActiveEntry * ce);

PRIVATE
int ReturnErrorStatus (int status)
{
  if (status < 0)
    status |= status; /* set a breakpoint HERE to find errors */
  return status;
}


#define STATUS(Status)      ReturnErrorStatus (Status)

#define PUTBLOCK(b, l)  (*cd->stream->put_block) \
                                    (cd->stream, b, l)
#define PUTSTRING(s)    (*cd->stream->put_block) \
                                    (cd->stream, s, PL_strlen(s))
#define COMPLETE_STREAM (*cd->stream->complete) \
                                    (cd->stream)
#define ABORT_STREAM(s) (*cd->stream->abort) \
                                    (cd->stream, s)


// forward declaration
PRIVATE int32 net_ProcessSockStub (ActiveEntry * ce);

static nsHashtable *theUrlToSocketHashtable = new nsHashtable();


class net_IntegerKey: public nsHashKey {
private:
  PRUint32 itsHash;
  
public:
  net_IntegerKey(PRUint32 hash) {
    itsHash = hash;
  }
  
  PRUint32 HashValue(void) const {
    return itsHash;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (itsHash == (((const net_IntegerKey *) aKey)->itsHash)) ? PR_TRUE : PR_FALSE;
  }

  nsHashKey *Clone(void) const {
    return new net_IntegerKey(itsHash);
  }
};


static PRMonitor *sockStub_lock = NULL;

static
PRBool
nsSockStub_lock(void)
{
    if(sockStub_lock == NULL)
    {
    sockStub_lock = PR_NewMonitor();
    if(sockStub_lock == NULL)
        return PR_FALSE;
    } 
    PR_EnterMonitor(sockStub_lock);
    return PR_TRUE;
}

static
void
nsSockStub_unlock(void)
{
    PR_ASSERT(sockStub_lock != NULL);
  
    if(sockStub_lock != NULL)
    {
        PR_ExitMonitor(sockStub_lock);
    }
}



PRIVATE void
net_AddSocketToHashTable(URL_Struct* URL_s, ActiveEntry * ce) 
{
  nsSockStub_lock();
  net_IntegerKey ikey((PRUint32)URL_s);
  theUrlToSocketHashtable->Put(&ikey, ce); 
  nsSockStub_unlock();
}

PRIVATE void
net_RemoveSocketToHashTable(URL_Struct* URL_s) 
{
  nsSockStub_lock();
  net_IntegerKey ikey((PRUint32)URL_s);
  theUrlToSocketHashtable->Remove(&ikey); 
  nsSockStub_unlock();
}

extern "C" {
PUBLIC PRFileDesc *
NET_GetSocketToHashTable(URL_Struct_* URL_s) 
{
  nsSockStub_lock();
  net_IntegerKey ikey((PRUint32)URL_s);
  ActiveEntry * ce = (ActiveEntry *)theUrlToSocketHashtable->Get(&ikey); 
  nsSockStub_unlock();

  SockStubConData * cd = (SockStubConData *)ce->con_data;
  return cd->sock;
}
}

extern "C" {
PUBLIC int32
NET_FreeSocket(URL_Struct_* URL_s) 
{
  nsSockStub_lock();
  net_IntegerKey ikey((PRUint32)URL_s);
  ActiveEntry * ce = (ActiveEntry *)theUrlToSocketHashtable->Get(&ikey); 
  nsSockStub_unlock();

  /* XXX: The interrupt will set the error state and will do the 
   * cleanup of the socket 
   */
  SockStubConData * cd = (SockStubConData *)ce->con_data;

  ce->status = MK_DATA_LOADED;
  cd->next_state = SOCKSTUB_DONE;
 
  return STATUS (net_ProcessSockStub(ce));
}
}

/* begins the connect process
 *
 * returns the TCP status code
 */
PRIVATE int
net_start_sockstub_connect(ActiveEntry * ce) {
    SockStubConData * cd = (SockStubConData *)ce->con_data;

   int def_port = DEF_SOCKSTUB_PORT;
   char * host_string = NET_ParseURL(ce->URL_s->address, GET_HOST_PART);

	// now try to extract the port number from the url
	char * startOfPort = PL_strchr(host_string, ':');
	if (startOfPort && *startOfPort && *(startOfPort+1))
		def_port = atoi(startOfPort+1);

    ce->status = NET_BeginConnect (ce->URL_s->address,
                    ce->URL_s->IPAddressString,
                    "SOCKSTUB", 
                    def_port, 
                    &cd->sock, 
                    HG02873  
                    &cd->tcp_con_data, 
                    ce->window_id, 
                    &ce->URL_s->error_msg,
                    ce->socks_host,
                    ce->socks_port,
                    ce->URL_s->localIP);


    /* set this so mkgeturl can select on it */
    ce->socket = cd->sock;

    if(cd->sock != NULL)
        NET_TotalNumberOfOpenConnections++;

    if (ce->status < 0) {
        if(ce->status == MK_UNABLE_TO_LOCATE_HOST)
        {
            /* no need to set the state since this
             * is the state we want again
             */
            if(cd->sock != NULL) {
                PR_Close(cd->sock);
                cd->sock = NULL;
                ce->socket = NULL;
            }
            return(0);
        }

        TRACEMSG(("SockStub: Unable to connect to host for `%s' (errno = %d).", 
                          ce->URL_s->address, PR_GetOSError()));
        /*
         * Proxy failover
         */

        cd->next_state = SOCKSTUB_ERROR_DONE;
        return STATUS(ce->status);
    } /* end status < 0 */

    if(ce->status == MK_CONNECTED)
    {
        cd->next_state = SOCKSTUB_SETUP_STREAM;
        TRACEMSG(("Connected through SockStub gateway on sock #%d",cd->sock));
        NET_SetReadSelect(ce->window_id, cd->sock);
    } else {
        cd->next_state = SOCKSTUB_FINISH_CONNECT;
        cd->pause_for_read = TRUE;
        ce->con_sock = cd->sock;  /* set the socket for select */
        NET_SetConnectSelect(ce->window_id, ce->con_sock);
    }

    net_AddSocketToHashTable(ce->URL_s, ce);
    return STATUS(ce->status);
}


/*  begins the connect process
 *
 *  returns the TCP status code
 */
PRIVATE int
net_finish_sockstub_connect(ActiveEntry * ce)
{
    SockStubConData * cd = (SockStubConData *)ce->con_data;
    int def_port;

    def_port = DEF_SOCKSTUB_PORT;

    ce->status = NET_FinishConnect(ce->URL_s->address,
                    "SOCKSTUB",
                    def_port,
                    &cd->sock,  
                    &cd->tcp_con_data, 
                    ce->window_id,
                    &ce->URL_s->error_msg,
                    ce->URL_s->localIP);

    if (ce->status < 0) {
        NET_ClearConnectSelect(ce->window_id, ce->con_sock);
        TRACEMSG(("SockStub: Unable to connect to host for `%s' (errno = %d).",
                                              ce->URL_s->address, PR_GetOSError()));

        cd->next_state = SOCKSTUB_ERROR_DONE;
        return STATUS(ce->status);
    } /* end status < 0 */

    if(ce->status == MK_CONNECTED) {
        NET_ClearConnectSelect(ce->window_id, ce->con_sock);
        ce->con_sock = NULL;  /* reset the socket so we don't select on it  */
        /* set this so mkgeturl can select on it */
        ce->socket = cd->sock;
        cd->next_state = SOCKSTUB_SETUP_STREAM;
        NET_SetReadSelect(ce->window_id, cd->sock);
        TRACEMSG(("Connected through SockStub on sock #%d",cd->sock));
    } else {
        /* unregister the old ce->socket from the select list
         * and register the new value in the case that it changes
         */
        if(ce->con_sock != cd->sock) {
            NET_ClearConnectSelect(ce->window_id, ce->con_sock);
            ce->con_sock = cd->sock; 
            NET_SetConnectSelect(ce->window_id, ce->con_sock);
        }
  
        cd->pause_for_read = TRUE;
    } 

    net_AddSocketToHashTable(ce->URL_s, ce);

    return STATUS(ce->status);
}


/* sets up the stream
 *
 * returns the tcp status code
 */
PRIVATE int
net_setup_sockstub_stream(ActiveEntry * ce) {
    SockStubConData * cd = (SockStubConData *)ce->con_data;
    XP_Bool need_to_do_again = FALSE;
    MWContext * stream_context;

    TRACEMSG(("NET_ProcessSockStub: setting up stream"));

    /* set a default content type if one wasn't given 
     */
    if(!ce->URL_s->content_type || !*ce->URL_s->content_type)
        StrAllocCopy(ce->URL_s->content_type, TEXT_HTML);

    /* If a stream previously exists from a partial cache
     * situation, reuse it
     */
    if(!cd->stream) {
        /* clear to prevent tight loop */
        NET_ClearReadSelect(ce->window_id, cd->sock);
        stream_context = ce->window_id;

        /* Set up the stream stack to handle the body of the message */
        cd->stream = NET_StreamBuilder(ce->format_out, 
                        ce->URL_s, 
                        stream_context);

        if (!cd->stream) {
            ce->status = MK_UNABLE_TO_CONVERT;
            ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CONVERT);
            return STATUS(ce->status);
        }

        NET_SetReadSelect(ce->window_id, cd->sock);

    }

    cd->next_state = SOCKSTUB_FLUSH_DATA;

    return STATUS(ce->status);
}


/* flushes data from/to socket to/from stream
 * (For now, it only reads from the socket to the stream.)
 * returns the tcp status code
 */
PRIVATE int
net_flush_sockstub_data(ActiveEntry * ce)
{
    SockStubConData * cd = (SockStubConData *)ce->con_data;
    unsigned int write_ready, read_size;

    TRACEMSG(("NET_ProcessSockStub: flushing data"));

    ///////// SOCKET->STREAM (reading from the net) ////////////////


    /* check to see if the stream is ready for writing
   */
    write_ready = (*cd->stream->is_write_ready)(cd->stream);

    TRACEMSG(("NET_ProcessSockStub: Flush: write ready returned %d", write_ready));

  if(!write_ready)
    {
    cd->pause_for_read = TRUE;
    return(0);  /* wait until we are ready to write */
    }
  else if(write_ready < (unsigned int) NET_Socket_Buffer_Size)
    {
    read_size = write_ready;
    }
  else
    {
    read_size = NET_Socket_Buffer_Size;
    }

    ce->status = PR_Read(cd->sock, NET_Socket_Buffer, read_size);

    TRACEMSG(("NET_ProcessSockStub: Flush: just read %d bytes.", ce->status));
    cd->pause_for_read = TRUE;  /* pause for the next read */

    if(ce->status > 0)
    {
        ce->bytes_received += ce->status;
        ce->status = PUTBLOCK(NET_Socket_Buffer, ce->status);

        // For now, just so we exit when we have at least gotten something,
        // to test that stream data is crossing the thread.
        /* cd->next_state = SOCKSTUB_DONE; */


        if(cd->original_content_length 
          && ce->bytes_received >= cd->original_content_length)
        {
            // normal end of transfer
            /* XXX: We should go back and wait for data
             ce->status = MK_DATA_LOADED;
             cd->next_state = SOCKSTUB_DONE;
             */
            cd->pause_for_read = FALSE;
        }
    }
    else if(ce->status == 0)
    {
        // temporary mscott testing hack:
        cd->next_state = SOCKSTUB_FLUSH_DATA;
        ce->status = 1;
        cd->pause_for_read = TRUE;
        return STATUS(ce->status);

        // transfer finished 
        TRACEMSG(("SOCKSTUB: Caught TCP EOF ending stream"));

        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_ZERO_LENGTH_FILE);
        cd->next_state = SOCKSTUB_ERROR_DONE;
        cd->pause_for_read = FALSE;
        return(MK_ZERO_LENGTH_FILE);
    }
    else /* error */
    {
        // temporary hack by mscott
        // otherwise data isn't ready yet:
        cd->pause_for_read = TRUE;
        ce->status = 1;
        return STATUS(ce->status);

        int err = PR_GetError();

        TRACEMSG(("TCP Error: %d", err));

        if (err == PR_WOULD_BLOCK_ERROR)
        {
            cd->pause_for_read = TRUE;
            return (0);
        }

        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_TCP_READ_ERROR, err);

        /* return TCP error
         */
        return MK_TCP_READ_ERROR;
    }

     return STATUS(ce->status);
}


PRIVATE int32
net_LoadSockStub (ActiveEntry * ce)
{
    SockStubConData * cd = PR_NEW(SockStubConData);
    ce->con_data = cd;
    if(!ce->con_data)
    {
        ce->status = MK_OUT_OF_MEMORY;
        ce->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
        return STATUS(ce->status);
    }

    char *use_host = NET_ParseURL(ce->URL_s->address, GET_HOST_PART);

    /* Init connection data */
    memset(cd, 0, sizeof(SockStubConData));
    cd->sock = NULL;
    StrAllocCopy(cd->hostname, use_host);

    PR_Free(use_host);

    return STATUS (net_ProcessSockStub(ce));
}

PRIVATE int32
net_ProcessSockStub (ActiveEntry * ce)
{
    SockStubConData * cd = (SockStubConData *)ce->con_data;
    TRACEMSG(("Entering NET_ProcessSockStub"));

    cd->pause_for_read = FALSE; /* already paused; reset */

    while(!cd->pause_for_read)
    {
    
        switch(cd->next_state)
        {

        case SOCKSTUB_START_CONNECT:
            ce->status = net_start_sockstub_connect(ce);
            break;
        
        case SOCKSTUB_FINISH_CONNECT:
            ce->status = net_finish_sockstub_connect(ce);
            break;
        
        case SOCKSTUB_SETUP_STREAM:
            ce->status = net_setup_sockstub_stream(ce);
            break;

        case SOCKSTUB_FLUSH_DATA:
            ce->status = net_flush_sockstub_data(ce);
            break;
        
        case SOCKSTUB_DONE:
            net_RemoveSocketToHashTable(ce->URL_s);
            NET_ClearReadSelect(ce->window_id, cd->sock);
            NET_TotalNumberOfOpenConnections--;
            PR_Close(cd->sock);

            if(cd->stream) {
                COMPLETE_STREAM;
                PR_Free(cd->stream);
                cd->stream = 0;
            }

            cd->next_state = SOCKSTUB_FREE;
            break;
        
        case SOCKSTUB_ERROR_DONE:
            if(cd->sock != NULL) {
                net_RemoveSocketToHashTable(ce->URL_s);
                NET_ClearDNSSelect(ce->window_id, cd->sock);
                NET_ClearReadSelect(ce->window_id, cd->sock);
                NET_ClearConnectSelect(ce->window_id, cd->sock);
                PR_Close(cd->sock);
                NET_TotalNumberOfOpenConnections--;
                cd->sock = NULL;
            }

            cd->next_state = SOCKSTUB_FREE;

            if(cd->stream) {
                ABORT_STREAM(ce->status);
                PR_Free(cd->stream);
                cd->stream = 0;
            }

            break; /* SOCKSTUB_ERROR_DONE */
        
        case SOCKSTUB_FREE:

            PR_FREEIF(cd->line_buffer);
            PR_Free(cd->stream); /* don't forget the stream */
            PR_Free(cd->hostname);
            if(cd->tcp_con_data)
                NET_FreeTCPConData(cd->tcp_con_data);
            PR_FREEIF(cd);
            return STATUS (-1); /* final end SOCKSTUB_FREE */
        
        default: /* should never happen !!! */
            TRACEMSG(("SOCKSTUB: BAD STATE!"));
            cd->next_state = SOCKSTUB_ERROR_DONE;
            break;
        } /* end switch */

        /* check for errors during load and call error state if found */
        if(ce->status < 0 && cd->next_state != SOCKSTUB_FREE)
        {
            cd->next_state = SOCKSTUB_ERROR_DONE;
        }
        
    } /* while(!cd->pause_for_read) */
    
    return STATUS(ce->status);
}

PRIVATE int32
net_InterruptSockStub (ActiveEntry * ce)
{
    SockStubConData * cd = (SockStubConData *)ce->con_data;

    cd->next_state = SOCKSTUB_ERROR_DONE;
    ce->status = MK_INTERRUPTED;
 
    return STATUS (net_ProcessSockStub(ce));
}

PRIVATE void
net_CleanupSockStub(void)
{
}


#define SOCKSTUB_SCHEME "sockstub:"

extern "C" {

MODULE_PRIVATE void
NET_InitSockStubProtocol(void)
{
  static NET_ProtoImpl sockstub_proto_impl;

  sockstub_proto_impl.init = net_LoadSockStub;
  sockstub_proto_impl.process = net_ProcessSockStub;
  sockstub_proto_impl.interrupt = net_InterruptSockStub;
  sockstub_proto_impl.resume = NULL;
  sockstub_proto_impl.cleanup = net_CleanupSockStub;
  StrAllocCopy(sockstub_proto_impl.scheme, SOCKSTUB_SCHEME); 


  NET_RegisterProtocolImplementation(&sockstub_proto_impl, SOCKSTUB_TYPE_URL);
}

}
