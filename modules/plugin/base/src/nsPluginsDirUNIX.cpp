/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
	nsPluginsDirUNIX.cpp
	
	Windows implementation of the nsPluginsDir/nsPluginsFile classes.
	
	by Alex Musil
 */

#include "nsPluginsDir.h"
#include "prlink.h"
#include "plstr.h"
#include "prmem.h"

#include "nsSpecialSystemDirectory.h"

/* Local helper functions */
 
static char* GetFileName(const char* pathname)
{
        const char* filename = nsnull;
                
        // this is most likely a path, so skip to the filename
        filename = PL_strrchr(pathname, '/');
        if(filename)
                ++filename;
        else
                filename = pathname;

        return PL_strdup(filename);
}

static PRUint32 CalculateVariantCount(char* mimeTypes)
{
        PRUint32 variants = 0;
        char* index = mimeTypes;
        while (*index)
        {
                if (*index == '|')
                        variants++;

                ++index; 
        }
        return variants;
}

static char** MakeStringArray(PRUint32 variants, char* data)
{
        char** buffer = NULL;
        char* index = data;
        PRUint32 count = 0;
 
        buffer = (char **)PR_Malloc(variants * sizeof(char *));
        buffer[count++] = index;
 
        while (*index && count<variants)
        {
                if (*index == '|')
                {
                  buffer[count++] = index + 1;
                  *index = 0;
                }
                ++index; 
        }
        return buffer;  
}

///////////////////////////////////////////////////////////////////////////

/* nsPluginsDir implementation */

nsPluginsDir::nsPluginsDir()
{
  // this is somewhat lacking, in that it doesn't fall back to any other directories.
  // then again, I'm not sure we should be falling back at all.  plugins have been (and probably
  // should continue to be) loaded from both <libdir>/plugins and ~/.mozilla/plugins.  There
  // doesn't seem to be any way to do this in the current nsPluginsDir code, which is disheartening.
  //

  // use MOZILLA_FIVE_HOME/plugins

  nsSpecialSystemDirectory sysdir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory); 
  sysdir += "plugins"; 
  const char *pluginsDir = sysdir.GetCString(); // native path
  if (pluginsDir != NULL)
  {
      const char* allocPath;
      
      allocPath = PL_strdup(pluginsDir);
      *(nsFileSpec*)this = allocPath;
  }
}

nsPluginsDir::~nsPluginsDir()
{
	// do nothing
}

PRBool nsPluginsDir::IsPluginFile(const nsFileSpec& fileSpec)
{
        const char* pathname = fileSpec.GetCString();

	printf("IsPluginFile(%s)\n", pathname);

	return PR_TRUE;
}

///////////////////////////////////////////////////////////////////////////

/* nsPluginFile implementation */

nsPluginFile::nsPluginFile(const nsFileSpec& spec)
	:	nsFileSpec(spec)
{
	// nada
}

nsPluginFile::~nsPluginFile()
{
	// nada
}

/**
 * Loads the plugin into memory using NSPR's shared-library loading
 * mechanism. Handles platform differences in loading shared libraries.
 */
nsresult nsPluginFile::LoadPlugin(PRLibrary* &outLibrary)
{
        const char* path = this->GetCString();
        outLibrary = PR_LoadLibrary(path);
        return NS_OK;
}

/**
 * Obtains all of the information currently available for this plugin.
 */
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo& info)
{
#if 0
    /*
    ** XXX this needs to change to probe the plugin for mime types/descriptions/etc.
    */

	const char *path = this->GetCString();

	info.fName = GetFileName(path);
	info.fMimeType = "application/x-java-vm";
	info.fMimeDescription = "OJI Plugin";
	info.fExtensions = "class";

    info.fVariantCount = CalculateVariantCount(info.fMimeType) + 1;
    info.fMimeTypeArray = MakeStringArray(info.fVariantCount, info.fMimeType);
    info.fMimeDescriptionArray = MakeStringArray(info.fVariantCount, info.fMimeDescription);
    info.fExtensionArray = MakeStringArray(info.fVariantCount, info.fExtensions);
    
#endif
	return NS_OK;
}
