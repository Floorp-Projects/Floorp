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
#include "nucacheproto.h"   /* ??? */
#include "prmem.h"          /* PR_NEW, PR_DELETE, etc. */
#include "CacheStubs.h"     /* CacheObject_* */
#include "mkutils.h"        /* NET_ExplainErrorDetails */
#include "allxpstr.h"       /* MK_OUT_OF_MEMORY, etc. */

typedef struct _NuCacheConData {
    void*           cache_object;
    NET_StreamClass *stream;
} NuCacheConData;

#define FIRST_BUFF_SIZE 1024

PRIVATE int32
net_NuCacheLoad (ActiveEntry * cur_entry)
{
    if (cur_entry && cur_entry->URL_s)
    {
        NuCacheConData* con_data = PR_NEW(NuCacheConData);
        void* pObject = cur_entry->URL_s->cache_object;

        if (!con_data || !pObject)
        {
            cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
            cur_entry->status = MK_OUT_OF_MEMORY;
            return (cur_entry->status);
        }

        cur_entry->protocol = NU_CACHE_TYPE_URL;
        cur_entry->memory_file = TRUE; /* TODO */
    
        /* CacheObject->SetReadLock(pObject); */
        cur_entry->local_file = TRUE; /* Check about this one- TODO */
        cur_entry->con_data = con_data;
        cur_entry->socket = NULL;

        NET_SetCallNetlibAllTheTime(cur_entry->window_id, "nucache");
        cur_entry->format_out = CLEAR_CACHE_BIT(cur_entry->format_out);
        FE_EnableClicking(cur_entry->window_id);
        /* Build the stream to read data from */
        con_data->stream = NET_StreamBuilder(cur_entry->format_out, cur_entry->URL_s, cur_entry->window_id);
        if(!con_data->stream)
        {
            NET_ClearCallNetlibAllTheTime(cur_entry->window_id, "nucache");
            PR_DELETE(con_data);
            cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_UNABLE_TO_CONVERT);
            cur_entry->status = MK_UNABLE_TO_CONVERT;
            return (cur_entry->status);
        }

        if (!cur_entry->URL_s->load_background)
            FE_GraphProgressInit(cur_entry->window_id, cur_entry->URL_s, cur_entry->URL_s->content_length);
    
        /* Process the first chunk so that images can start loading */
        if (!CacheObject_IsPartial(pObject)) /*todo- change this to is Completed */
        {
            char* firstBuffer = (char*) PR_Malloc(FIRST_BUFF_SIZE);
            PRInt32 amountRead = 0;
            if (!firstBuffer)
            {
                PR_DELETE(con_data);
                cur_entry->URL_s->error_msg = NET_ExplainErrorDetails(MK_OUT_OF_MEMORY);
                return(MK_OUT_OF_MEMORY);
            }

            amountRead = CacheObject_Read(pObject, firstBuffer, FIRST_BUFF_SIZE);
            if (amountRead > 0)
            {
                cur_entry->status = (*con_data->stream->put_block)(con_data->stream, firstBuffer, amountRead);
                if(cur_entry->status < 0)
                {
                    NET_ClearCallNetlibAllTheTime(cur_entry->window_id, "nucache");

                    if (!cur_entry->URL_s->load_background)
                    {
                        FE_GraphProgressDestroy(cur_entry->window_id,
                            cur_entry->URL_s,
                            cur_entry->URL_s->content_length,
                            cur_entry->bytes_received);
                    }
                    PR_DELETE(con_data);
                    return (cur_entry->status);
                }

                cur_entry->bytes_received += amountRead;
                PR_DELETE(firstBuffer);
            }
        }
        else
        {
            cur_entry->status = 0;
        }
        NET_ClearCallNetlibAllTheTime(cur_entry->window_id, "nucache");
    }
    return cur_entry->status;
}

/* called repeatedly from NET_ProcessNet to push all the
 * data up the stream
 */
PRIVATE int32
net_ProcessNuCache (ActiveEntry * cur_entry)
{
    PR_ASSERT(0); /* Not complete as yet */
    NET_SetCallNetlibAllTheTime(cur_entry->window_id, "nucache");
   
    NET_ClearCallNetlibAllTheTime(cur_entry->window_id, "nucache");
    return -1;
}

/* called by functions in mkgeturl to interrupt the loading of
 * an object.  (Usually a user interrupt) 
 */
PRIVATE int32
net_InterruptNuCache (ActiveEntry * cur_entry)
{
    PR_ASSERT(0); /* Not complete as yet */
    NET_SetCallNetlibAllTheTime(cur_entry->window_id, "nucache");
    
    NET_ClearCallNetlibAllTheTime(cur_entry->window_id, "nucache");
    return -1;
}


PRIVATE void
net_CleanupNuCacheProtocol(void)
{
}

void
NET_InitNuCacheProtocol(void)
{
    static NET_ProtoImpl nu_cache_proto_impl;

    nu_cache_proto_impl.init = net_NuCacheLoad;
    nu_cache_proto_impl.process = net_ProcessNuCache;
    nu_cache_proto_impl.interrupt = net_InterruptNuCache;
    nu_cache_proto_impl.cleanup = net_CleanupNuCacheProtocol;

    NET_RegisterProtocolImplementation(&nu_cache_proto_impl, NU_CACHE_TYPE_URL);
}

