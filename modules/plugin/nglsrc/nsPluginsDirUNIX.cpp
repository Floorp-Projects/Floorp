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
	nsPluginsDirUNIX.cpp
	
	UNIX implementation of the nsPluginsDir/nsPluginsFile classes.
	
	by Alex Musil
 */

#include "xp_core.h"
#include "nsplugin.h"
#include "ns4xPlugin.h"
#include "ns4xPluginInstance.h"
#include "nsIServiceManager.h"
#include "nsIMemory.h"
#include "nsIPluginStreamListener.h"
#include "nsPluginsDir.h"
#include "nsSpecialSystemDirectory.h"
#include "prmem.h"
#include "nslog.h"

NS_IMPL_LOG(nsPluginsDirUNIXLog, 0)
#define PRINTF NS_LOG_PRINTF(nsPluginsDirUNIXLog)
#define FLUSH  NS_LOG_FLUSH(nsPluginsDirUNIXLog)

/* Local helper functions */
 
static PRUint32 CalculateVariantCount(const char* mimeTypes)
{
        PRUint32 variants = 0;
        const char* ptr = mimeTypes;
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
    // this is somewhat lacking, in that it doesn't fall back to any 
    // other directories. then again, I'm not sure we should be falling 
    // back at all.  plugins have been (and probably should continue to be) 
    // loaded from both <libdir>/plugins and ~/.mozilla/plugins.  There
    // doesn't seem to be any way to do this in the current nsPluginsDir 
    // code, which is disheartening.

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
    PRLibSpec libSpec;
    PRLibSpec tempSpec;
    PRLibrary * handle;

    ///////////////////////////////////////////////////////////
    // Normally, Mozilla isn't linked against libXt and libXext
    // since it's a Gtk/Gdk application.  On the other hand,
    // legacy plug-ins expect the libXt and libXext symbols
    // to already exist in the global name space.  This plug-in
    // wrapper is linked against libXt and libXext, but since
    // we never call on any of these libraries, plug-ins still
    // fail to resolve Xt symbols when trying to do a dlopen
    // at runtime.  Explicitly opening Xt/Xext into the global
    // namespace before attempting to load the plug-in seems to
    // work fine.

    tempSpec.type = PR_LibSpec_Pathname;
    tempSpec.value.pathname = "libXt.so";
    handle = PR_LoadLibraryWithFlags(tempSpec, PR_LD_NOW|PR_LD_GLOBAL);

    tempSpec.value.pathname = "libXext.so";
    handle = PR_LoadLibraryWithFlags(tempSpec, PR_LD_NOW|PR_LD_GLOBAL);

    libSpec.type = PR_LibSpec_Pathname;
    libSpec.value.pathname = this->GetCString();
    pLibrary = outLibrary = PR_LoadLibraryWithFlags(libSpec, 0);
    
    PRINTF("LoadPlugin() %s returned %lx\n", 
           libSpec.value.pathname, (unsigned long)pLibrary);
    
    return NS_OK;
}

/**
 * Obtains all of the information currently available for this plugin.
 */
nsresult nsPluginFile::GetPluginInfo(nsPluginInfo& info)
{
    nsresult rv;
    const char* mimedescr, *name, *description;
    char *mdesc,*start,*nexttoc,*mtype,*exten,*descr;
    int i,num;

    // No, this doesn't leak. GetGlobalServiceManager() doesn't addref
    // it's out pointer. Maybe it should.
    nsIServiceManager* mgr;
    nsServiceManager::GetGlobalServiceManager(&mgr);

    nsFactoryProc nsGetFactory =
        (nsFactoryProc) PR_FindSymbol(pLibrary, "NSGetFactory");

    nsCOMPtr<nsIPlugin> plugin;

    if (nsGetFactory) {
        // It's an almost-new-style plugin. The "truly new" plugins
        // are just XPCOM components, but there are some Mozilla
        // Classic holdovers that live in the plugins directory but
        // implement nsIPlugin and the factory stuff.
        static NS_DEFINE_CID(kPluginCID, NS_PLUGIN_CID);

        nsCOMPtr<nsIFactory> factory;
        rv = nsGetFactory(mgr, kPluginCID, nsnull, nsnull, getter_AddRefs(factory));
        if (NS_FAILED(rv)) return rv;

        plugin = do_QueryInterface(factory);
    }
    else {
        // It's old sk00l
        rv = ns4xPlugin::CreatePlugin(mgr, this->GetCString(), pLibrary, getter_AddRefs(plugin));
        if (NS_FAILED(rv)) return rv;
    }

    if (plugin) {
        plugin->GetValue(nsPluginVariable_NameString, &name);
        info.fName = PL_strdup(name);

        plugin->GetValue(nsPluginVariable_DescriptionString, &description);
        info.fDescription = PL_strdup(description);

        plugin->GetMIMEDescription(&mimedescr);
    }
    else {
        info.fName = PL_strdup(this->GetCString());
        info.fDescription = PL_strdup("");
    }

    PRINTF("GetMIMEDescription() returned \"%s\"\n", mimedescr);

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
            info.fMimeTypeArray[i] = mtype ? PL_strdup(mtype) : PL_strdup("");
            info.fMimeDescriptionArray[i] = descr ? PL_strdup(descr) : PL_strdup("");
            info.fExtensionArray[i] = exten ? PL_strdup(exten) : PL_strdup("");
        }
        start=nexttoc;
    }

    PR_Free(mdesc);

	return NS_OK;
}


nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
  if(info.fName != nsnull)
    PL_strfree(info.fName);

  if(info.fDescription != nsnull)
    PL_strfree(info.fDescription);

  for(PRUint32 i = 0; i < info.fVariantCount; i++)
  {
    if(info.fMimeTypeArray[i] != nsnull)
      PL_strfree(info.fMimeTypeArray[i]);

    if(info.fMimeDescriptionArray[i] != nsnull)
      PL_strfree(info.fMimeDescriptionArray[i]);

    if(info.fExtensionArray[i] != nsnull)
      PL_strfree(info.fExtensionArray[i]);
  }
  return NS_OK;
}
