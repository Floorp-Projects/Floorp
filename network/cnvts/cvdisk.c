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
/* Please leave outside of ifdef for windows precompiled headers */
#include "xp.h"
#include "netutils.h"
#include "mkselect.h"
#include "mktcp.h"
#include "mkgeturl.h"

#ifdef MOZILLA_CLIENT

typedef struct _DataObject {
    FILE * fp;
    char * filename;
} DataObject;


PRIVATE int net_SaveToDiskWrite (NET_StreamClass *stream, CONST char* s, int32 l)
{
	DataObject *obj=stream->data_object;	
    fwrite(s, 1, l, obj->fp); 
    return(1);
}

/* is the stream ready for writeing?
 */
PRIVATE unsigned int net_SaveToDiskWriteReady (NET_StreamClass * stream)
{
   DataObject *obj=stream->data_object;
   return(MAX_WRITE_READY);  /* always ready for writing */ 
}


PRIVATE void net_SaveToDiskComplete (NET_StreamClass *stream)
{
	DataObject *obj=stream->data_object;	
    fclose(obj->fp);

    FREEIF(obj->filename);

    FREE(obj);
    return;
}

PRIVATE void net_SaveToDiskAbort (NET_StreamClass *stream, int status)
{
	DataObject *obj=stream->data_object;	
    fclose(obj->fp);

    if(obj->filename)
      {
        remove(obj->filename);
        FREE(obj->filename);
      }

    return;
}


PUBLIC NET_StreamClass * 
fe_MakeSaveAsStream (int         format_out,
                         void       *data_obj,
                         URL_Struct *URL_s,
                         MWContext  *window_id)
{
    DataObject* obj;
    NET_StreamClass* stream;
    static int count=0;
    char filename[256];
    FILE *fp = stdout;
    
    TRACEMSG(("Setting up display stream. Have URL: %s\n", URL_s->address));

    PR_snprintf(filename, sizeof(filename), "foo%d.unknown",count++);
    fp = fopen(filename,"w");

    stream = XP_NEW(NET_StreamClass);
    if(stream == NULL) 
        return(NULL);

    obj = XP_NEW(DataObject);
    if (obj == NULL) 
        return(NULL);
    
    stream->name           = "FileWriter";
    stream->complete       = (MKStreamCompleteFunc) net_SaveToDiskComplete;
    stream->abort          = (MKStreamAbortFunc) net_SaveToDiskAbort;
    stream->put_block      = (MKStreamWriteFunc) net_SaveToDiskWrite;
    stream->is_write_ready = (MKStreamWriteReadyFunc) net_SaveToDiskWriteReady;
    stream->data_object    = obj;  /* document info object */
    stream->window_id      = window_id;

    obj->fp = fp;
    obj->filename = 0;
    StrAllocCopy(obj->filename, filename);

    TRACEMSG(("Returning stream from NET_SaveToDiskConverter\n"));

    return stream;
}

#endif /* MOZILLA_CLIENT */
