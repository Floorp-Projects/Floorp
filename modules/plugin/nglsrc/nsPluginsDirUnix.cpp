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
#include "prenv.h"

#define PLUGIN_PATH 	"NS600_PLUGIN_PATH"	
#define PLUGIN_DIR 	"/plugins"	
#define MTYPE_END	'|'
#define MTYPE_END_OLD	';'
#define MTYPE_PART      ':'


/* Local helper functions */
 
static PRUint32 CalculateVariantCount(char* mimeTypes)
{
    PRUint32 variants = 0;
    char* ptr = mimeTypes;

    while (*ptr) {
        if (*ptr == MTYPE_END) {
            if (!ptr[1]) {
                /*
                 * This is a trailing separator, with nothing after it. We
                 * normalize these away here by slamming a NUL in and
                 * stopping.
                 */
                *ptr = 0;
                break;
            }
            variants++;
        }
        ++ptr; 
    }

    return variants;
}

///////////////////////////////////////////////////////////////////////////////
// Currently, in the MIME type info passed in by plugin, a ';' is used as the 
// separator of two MIME types, and also the sparator of a version in one MIME 
// type. For example: 
// "application/x-java-applet;version1.3::java(TM) plugin;application/x-java-
// applet...".
// The ambiguity of ';'  causes the browser fail to parse the MIME types 
// correctly.
//
// This method parses the MIME type input, and replaces the MIME type 
// separators with '|' to eliminate the ambiguity of ';'. (The Windows version 
// also uses '|' as the MIME type separator.)
//
// Input format: "...type[;version]:[extension]:[desecription];..."
// Output format: "...type[;version]:[extension]:[desecription]|..."
//
static void SetMIMETypeSeparator(char *minfo)
{
    char *p;

    p = minfo;
    while (p) {
	if ((p = PL_strchr(p, MTYPE_PART)) != 0) {
	    *p++;
	    if ((p = PL_strchr(p, MTYPE_END_OLD)) != 0) {
	        *p = MTYPE_END;	
		*p++;
	    }
        }
    }
}

///////////////////////////////////////////////////////////////////////////

/* nsPluginsDir implementation */

// Get path to plugin directory. 
// If already defined in environment, use it; otherwise, use native path.
nsPluginsDir::nsPluginsDir(PRUint16 location)
{
    nsSpecialSystemDirectory sysdir(nsSpecialSystemDirectory::
                                    OS_CurrentProcessDirectory); 
    const char *pluginsDir;
    char       *tmp_dir;

    if ((tmp_dir = PR_GetEnv(PLUGIN_PATH))) {
        pluginsDir = (const char *)tmp_dir;
    } else {
        sysdir += "plugins";
        pluginsDir = sysdir.GetCString(); // native path
    }

    if (pluginsDir != NULL) {
        *(nsFileSpec*)this = pluginsDir;
    }

#ifdef NS_DEBUG
    printf("********** Got plugins path: %s\n", pluginsDir);
#endif
}

nsPluginsDir::~nsPluginsDir()
{
    // do nothing
}

PRBool nsPluginsDir::IsPluginFile(const nsFileSpec& fileSpec)
{
    const char* pathname = fileSpec.GetCString();

#ifdef NS_DEBUG
    printf("IsPluginFile(%s)\n", pathname);
#endif

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
    
#ifdef NS_DEBUG
    printf("LoadPlugin() %s returned %lx\n", 
           libSpec.value.pathname, (unsigned long)pLibrary);
#endif
    
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
        rv = nsGetFactory(mgr, kPluginCID, nsnull, nsnull, 
			  getter_AddRefs(factory));
        if (NS_FAILED(rv)) return rv;

        plugin = do_QueryInterface(factory);
    } else {
        // It's old sk00l
        rv = ns4xPlugin::CreatePlugin(mgr, this->GetCString(), pLibrary, 
				      getter_AddRefs(plugin));
        if (NS_FAILED(rv)) return rv;
    }

    if (plugin) {
        plugin->GetValue(nsPluginVariable_NameString, &name);
        info.fName = PL_strdup(name);

        plugin->GetValue(nsPluginVariable_DescriptionString, &description);
        info.fDescription = PL_strdup(description);

        info.fFileName = PL_strdup(this->GetCString());

        plugin->GetMIMEDescription(&mimedescr);
    } else {
        info.fName = PL_strdup(this->GetCString());
        info.fDescription = PL_strdup("");
    }

#ifdef NS_DEBUG
    printf("GetMIMEDescription() returned \"%s\"\n", mimedescr);
#endif

    // Copy MIME type input.
    mdesc = PL_strdup(mimedescr);

    // Currently in MIME type input, a ';' is used as MIME type separator
    // and a MIME type's version separator, which makes parsing difficult.
    // To eliminate the ambiguity, set MIME type separator to '|'.
    SetMIMETypeSeparator(mdesc);

    // Get number of MIME types.
    num = CalculateVariantCount(mdesc) + 1;

    info.fVariantCount = num;
    info.fMimeTypeArray = (char **)PR_Malloc(num * sizeof(char *));
    info.fMimeDescriptionArray = (char **)PR_Malloc(num * sizeof(char *));
    info.fExtensionArray = (char **)PR_Malloc(num * sizeof(char *));

    // Retrive each MIME type info.
    // Each MIME type info stored in format, "type:extension:description"
    // Sample: "application/x-java-applet;version=1.1::Java(tm) Plug-in;".
    start = mdesc;
    for(i = 0;i < num && *start;i++) {
        if(i+1 < num) {
            if((nexttoc = PL_strchr(start, MTYPE_END)) != 0)
                *nexttoc++=0;
            else
                nexttoc=start+strlen(start);
        } else
            nexttoc=start+strlen(start);

        mtype = start;
        exten = PL_strchr(start, MTYPE_PART);
        if(exten) {
            *exten++ = 0;
            descr = PL_strchr(exten, MTYPE_PART);
        } else
            descr = NULL;

        if(descr)
            *descr++=0;

//#ifdef NS_DEBUG
        printf("Registering plugin %d for: \"%s\",\"%s\",\"%s\"\n",
               i, mtype,descr ? descr : "null",exten ? exten : "null");
//#endif

        if(!*mtype && !descr && !exten) {
            i--;
            info.fVariantCount--;
        } else {
            info.fMimeTypeArray[i] = mtype ? PL_strdup(mtype) : PL_strdup("");
            info.fMimeDescriptionArray[i] = descr ? PL_strdup(descr) :
                                                    PL_strdup("");
            info.fExtensionArray[i] = exten ? PL_strdup(exten) : PL_strdup("");
        }
        start = nexttoc;
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

    for(PRUint32 i = 0; i < info.fVariantCount; i++) {
        if (info.fMimeTypeArray[i] != nsnull)
            PL_strfree(info.fMimeTypeArray[i]);

        if (info.fMimeDescriptionArray[i] != nsnull)
            PL_strfree(info.fMimeDescriptionArray[i]);

        if(info.fExtensionArray[i] != nsnull)
            PL_strfree(info.fExtensionArray[i]);
    }

    PR_FREEIF(info.fMimeTypeArray);
    PR_FREEIF(info.fMimeDescriptionArray);
    PR_FREEIF(info.fExtensionArray);

    if(info.fFileName != nsnull)
        PL_strfree(info.fFileName);

    return NS_OK;
}
