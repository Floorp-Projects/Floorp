/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#include "nscore.h"
#include "nsIAllocator.h"
#include "plstr.h"
#include "stdio.h"

#include "prlink.h"

#include "nsSound.h"

#include <unistd.h>

#include <gtk/gtk.h>
/* used with esd_open_sound */
//static int esdref = -1;
static PRLibrary *lib = nsnull;

//typedef int (PR_CALLBACK *EsdOpenSoundType)(const char *host);
//typedef int (PR_CALLBACK *EsdCloseType)(int);

/* used to play the sounds from the find symbol call */
typedef int (PR_CALLBACK *EsdPlayFileType)(const char *, const char *, int);

NS_IMPL_ISUPPORTS1(nsSound, nsISound);

////////////////////////////////////////////////////////////////////////
nsSound::nsSound()
{
  NS_INIT_REFCNT();

  /* we don't need to do esd_open_sound if we are only going to play files
     but we will if we want to do things like streams, etc
  */
  //  EsdOpenSoundType EsdOpenSound;

  lib = PR_LoadLibrary("libesd.so");

  /*
    if (!lib)
    return;
    EsdOpenSound = (EsdOpenSoundType) PR_FindSymbol(lib, "esd_open_sound");
    esdref = (*EsdOpenSound)("localhost");
  */
}

nsSound::~nsSound()
{
  /* see above comment */
  /*
    if (esdref != -1)
    {
    EsdCloseType EsdClose = (EsdCloseType) PR_FindSymbol(lib, "esd_close");
    (*EsdClose)(esdref);
    esdref = -1;
    }
  */
}

nsresult NS_NewSound(nsISound** aSound)
{
  NS_PRECONDITION(aSound != nsnull, "null ptr");
  if (! aSound)
    return NS_ERROR_NULL_POINTER;
  
  *aSound = new nsSound();
  if (! *aSound)
    return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(*aSound);
  return NS_OK;
}


NS_METHOD nsSound::Init(void)
{
  return NS_OK;
}


NS_METHOD nsSound::Beep()
{
	::gdk_beep();

  return NS_OK;
}

NS_METHOD nsSound::Play(nsIFileSpec *filespec)
{
  if (lib)
  {
    char *filename;
    filespec->GetNativePath(&filename);

    g_print("there are some issues with playing sound right now, but this should work\n");
    EsdPlayFileType EsdPlayFile = (EsdPlayFileType) PR_FindSymbol(lib, "esd_play_file");
    (*EsdPlayFile)("mozilla", filename, 1);

    nsCRT::free(filename);

    return NS_OK;
  }
  return NS_OK;
}
