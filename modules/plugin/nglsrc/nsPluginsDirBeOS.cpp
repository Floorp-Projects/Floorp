/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
	nsPluginsDirBeOS.cpp
	
	BeOS implementation of the nsPluginsDir/nsPluginsFile classes.
	
	Based on nsPluginsDirUNIX.cpp r1.12 by Alex Musil
 */

#include "nsPluginsDir.h"
#include "prlink.h"
#include "plstr.h"
#include "prmem.h"
#include "nslog.h"

NS_IMPL_LOG(nsPluginsDirBeOSLog, 0)
#define PRINTF NS_LOG_PRINTF(nsPluginsDirBeOSLog)
#define FLUSH  NS_LOG_FLUSH(nsPluginsDirBeOSLog)

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
        char* ptr = mimeTypes;
        while (*ptr)
        {
                if (*ptr == ';')
                        variants++;

                ++ptr; 
        }
        return variants;
}

///////////////////////////////////////////////////////////////////////////

/* nsPluginsDir implementation */

nsPluginsDir::nsPluginsDir(PRUint16 location)
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
      *(nsFileSpec*)this = pluginsDir;
  }
}

nsPluginsDir::~nsPluginsDir()
{
	// do nothing
}

PRBool nsPluginsDir::IsPluginFile(const nsFileSpec& fileSpec)
{
    const char* pathname = fileSpec.GetCString();

	PRINTF("IsPluginFile(%s)\n", pathname);

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
        pLibrary = outLibrary = PR_LoadLibrary(path);

        PRINTF("LoadPlugin() %s returned %lx\n",path,(unsigned long)pLibrary);

        return NS_OK;
}

typedef char* (*BeOS_Plugin_GetMIMEDescription)();


/**
 * Obtains all of the information currently available for this plugin.
 */
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo& info)
{
	const char *path = this->GetCString();
    char *mimedescr,*mdesc,*start,*nexttoc,*mtype,*exten,*descr;
    int i,num;

    BeOS_Plugin_GetMIMEDescription procedure = nsnull;
    mimedescr=(char *)"";

    if((procedure = (BeOS_Plugin_GetMIMEDescription)PR_FindSymbol(pLibrary,"NP_GetMIMEDescription")) != 0) {
        mimedescr = procedure();
    } else {
        PRINTF("Cannot get plugin info: no GetMIMEDescription procedure!\n");
        return NS_ERROR_FAILURE;
    }

	info.fName = GetFileName(path);

    PRINTF("GetMIMEDescription() %lx returned \"%s\"\n",
           (unsigned long)procedure, mimedescr);

    // copy string
    
    mdesc = (char *)PR_Malloc(strlen(mimedescr)+1);
    strcpy(mdesc,mimedescr);
    num=CalculateVariantCount(mimedescr)+1;
    info.fVariantCount = num;

    info.fMimeTypeArray =(char **)PR_Malloc(num * sizeof(char *));
    info.fMimeDescriptionArray =(char **)PR_Malloc(num * sizeof(char *));
    info.fExtensionArray =(char **)PR_Malloc(num * sizeof(char *));

    start=mdesc;
    for(i=0;i<num && *start;i++) {
        // search start of next token (separator is ';')

        if(i+1 < num) {
            if((nexttoc=PL_strchr(start, ';')) != 0)
                *nexttoc++=0;
            else
                nexttoc=start+strlen(start);
        } else
            nexttoc=start+strlen(start);

        // split string into: mime type ':' extensions ':' description

        mtype=start;
        exten=PL_strchr(start, ':');
        if(exten) {
            *exten++=0;
            descr=PL_strchr(exten, ':');
        } else
            descr=NULL;

        if(descr)
            *descr++=0;

        PRINTF("Registering plugin for: \"%s\",\"%s\",\"%s\"\n", mtype,descr ? descr : "null",exten ? exten : "null");

        if(!*mtype && !descr && !exten) {
            i--;
            info.fVariantCount--;
        } else {
            info.fMimeTypeArray[i] = mtype ? mtype : (char *)"";
            info.fMimeDescriptionArray[i] = descr ? descr : (char *)"";
            info.fExtensionArray[i] = exten ? exten : (char *)"";
        }
        start=nexttoc;
    }
	return NS_OK;
}

nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
  return NS_OK;
}
