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

#include "xp.h"

void NotImplemented(char *aName)
{
  printf("%s is not implemented\n");
}

/*---------------------------------------------------------------------------
 * From ns/lib/layout/laysel.c
 *---------------------------------------------------------------------------
 */
/*
    get first(last) if current element is NULL.
*/
Bool
LO_getNextTabableElement( MWContext *context, LO_TabFocusData *pCurrentFocus, int forward )
{
  NotImplemented("LO_getNextTabableElement");
  return FALSE;
}

/* Given a URL, this might return a better suggested name to save it as.

   When you have a URL, you can sometimes get a suggested name from
   URL_s->content_name, but if you're saving a URL to disk before the
   URL_Struct has been filled in by netlib, you don't have that yet.

   So if you're about to prompt for a file name *before* you call FE_GetURL
   with a format_out of FO_SAVE_AS, call this function first to see if it
   can offer you advice about what the suggested name for that URL should be.

   (This works by looking in a cache of recently-displayed MIME objects, and
   seeing if this URL matches.  If it does, the remembered content-name will
   be used.)
 */
char *MimeGuessURLContentName(MWContext *context, const char *url)
{
  NotImplemented("MimeGuessURLContentName");
  return(0);
}

/* asks the user if they want to stream data.
 * -1 cancel
 * 0  No, don't stream data, play from the file
 * 1  Yes, stream the data from the network
 */

int XFE_AskStreamQuestion(MWContext * window_id) /* definition */
{
  NotImplemented("XFE_AskStreamQuestion");
  return(0);
}

void fe_GetProgramDirectory(char *path, int len)
{
}

NET_StreamClass *NET_NewStream          (char                 *name,
                        MKStreamWriteFunc     process,
                        MKStreamCompleteFunc  complete,
                        MKStreamAbortFunc     abort,
                        MKStreamWriteReadyFunc ready,
                        void                 *streamData,
                        MWContext            *windowID)
{
   NotImplemented("NET_NewStream");
   return 0;
}
