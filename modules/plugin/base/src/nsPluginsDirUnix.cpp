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
#include "prerror.h"
#include <sys/stat.h>

#include "nsIPref.h"
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

#define PLUGIN_PATH 	"MOZ_PLUGIN_PATH"	
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
	    p++;
	    if ((p = PL_strchr(p, MTYPE_END_OLD)) != 0) {
	        *p = MTYPE_END;	
		p++;
	    }
        }
    }
}

#ifdef MOZ_WIDGET_GTK 

#define LOCAL_PLUGIN_DLL_SUFFIX ".so"
#if defined(HPUX11)
#define DEFAULT_X11_PATH "/usr/lib/X11R6/"
#undef LOCAL_PLUGIN_DLL_SUFFIX
#define LOCAL_PLUGIN_DLL_SUFFIX ".sl"
#elif defined(SOLARIS)
#define DEFAULT_X11_PATH "/usr/openwin/lib/"
#elif defined(LINUX)
#define DEFAULT_X11_PATH "/usr/X11R6/lib/"
#else
#define DEFAULT_X11_PATH ""
#endif

#define PLUGIN_MAX_LEN_OF_TMP_ARR 512

static void DisplayPR_LoadLibraryErrorMessage(const char *libName)
{
    char errorMsg[PLUGIN_MAX_LEN_OF_TMP_ARR] = "Cannot get error from NSPR.";
    if (PR_GetErrorTextLength() < (int) sizeof(errorMsg))
        PR_GetErrorText(errorMsg);

    fprintf(stderr, "LoadPlugin: failed to initialize shared library %s [%s]\n",
        libName, errorMsg);
}

static void SearchForSoname(const char* name, char** soname)
{
    if (!(name && soname))
        return;
    PRDir *fdDir = PR_OpenDir(DEFAULT_X11_PATH);
    if (!fdDir)
        return;       

    int n = PL_strlen(name);
    PRDirEntry *dirEntry;
    while ((dirEntry = PR_ReadDir(fdDir, PR_SKIP_BOTH))) {
        if (!PL_strncmp(dirEntry->name, name, n)) {
            if (dirEntry->name[n] == '.' && dirEntry->name[n+1] && !dirEntry->name[n+2]) {
                // name.N, wild guess this is what we need
                char out[PLUGIN_MAX_LEN_OF_TMP_ARR] = DEFAULT_X11_PATH;
                PL_strcat(out, dirEntry->name);
                *soname = PL_strdup(out);
               break;
            }
        }
    }

    PR_CloseDir(fdDir);
}

static PRBool LoadExtraSharedLib(const char *name, char **soname, PRBool tryToGetSoname)
{
    PRBool ret = PR_TRUE;
    PRLibSpec tempSpec;
    PRLibrary *handle;
    tempSpec.type = PR_LibSpec_Pathname;
    tempSpec.value.pathname = name;
    handle = PR_LoadLibraryWithFlags(tempSpec, PR_LD_NOW|PR_LD_GLOBAL);
    if (!handle) {
        ret = PR_FALSE;
        DisplayPR_LoadLibraryErrorMessage(name);
        if (tryToGetSoname) {
            SearchForSoname(name, soname);
            if (*soname) {
                ret = LoadExtraSharedLib((const char *) *soname, NULL, PR_FALSE);
            }
        }
    }
    return ret;
}

#define PLUGIN_MAX_NUMBER_OF_EXTRA_LIBS 32
#define PREF_PLUGINS_SONAME "plugin.soname.list"
#define DEFAULT_EXTRA_LIBS_LIST "libXt" LOCAL_PLUGIN_DLL_SUFFIX ":libXext" LOCAL_PLUGIN_DLL_SUFFIX
/*
 this function looks for
 user_pref("plugin.soname.list", "/usr/X11R6/lib/libXt.so.6:libXiext.so");
 in user's pref.js
 and loads all libs in specified order
*/

static void LoadExtraSharedLibs()
{
    // check out if user's prefs.js has libs name
    nsresult res;
    nsCOMPtr<nsIPref> prefs = do_GetService(kPrefServiceCID, &res);
    if (NS_SUCCEEDED(res) && (prefs != nsnull)) {
        char *sonamesListFromPref = PREF_PLUGINS_SONAME;
        char *sonameList = NULL;
        PRBool prefSonameListIsSet = PR_TRUE;
        res = prefs->CopyCharPref(sonamesListFromPref, &sonameList);
        if (!sonameList) {
            // pref is not set, lets use hardcoded list
            prefSonameListIsSet = PR_FALSE;
            sonameList = DEFAULT_EXTRA_LIBS_LIST;
            sonameList = PL_strdup(sonameList);
        }
        if (sonameList) {
            char *arrayOfLibs[PLUGIN_MAX_NUMBER_OF_EXTRA_LIBS] = {0};
            int numOfLibs = 0;
            char *nextToken;
            char *p = nsCRT::strtok(sonameList,":",&nextToken);
            if (p) {
                while (p && numOfLibs < PLUGIN_MAX_NUMBER_OF_EXTRA_LIBS) {
                    arrayOfLibs[numOfLibs++] = p;
                    p = nsCRT::strtok(nextToken,":",&nextToken);
                }
            } else // there is just one lib
                arrayOfLibs[numOfLibs++] = sonameList;

            char sonameListToSave[PLUGIN_MAX_LEN_OF_TMP_ARR] = "";
            for (int i=0; i<numOfLibs; i++) {
                // trim out head/tail white spaces (just in case)
                PRBool head = PR_TRUE;
                p = arrayOfLibs[i];
                while (*p) {
                    if (*p == ' ' || *p == '\t') {
                        if (head) {
                            arrayOfLibs[i] = ++p;
                        } else {
                            *p = 0;
                        }
                    } else {
                        head = PR_FALSE;
                        p++;
                    }
                }
                if (!arrayOfLibs[i][0]) {
                    continue; // null string
                }
                PRBool tryToGetSoname = PR_TRUE;
                if (PL_strchr(arrayOfLibs[i], '/')) {
                    //assuming it's real name, try to stat it
                    struct stat st;
                    if (stat((const char*) arrayOfLibs[i], &st)) {
                        //get just a file name
                        arrayOfLibs[i] = PL_strrchr(arrayOfLibs[i], '/') + 1;
                    } else
                        tryToGetSoname = PR_FALSE;
                }
                char *soname = NULL;
                if (LoadExtraSharedLib(arrayOfLibs[i], &soname, tryToGetSoname)) {
                    //construct soname's list to save in prefs
                    p = soname ? soname : arrayOfLibs[i];
                    int n = PLUGIN_MAX_LEN_OF_TMP_ARR -
                        (PL_strlen(sonameListToSave) + PL_strlen(p));
                    if (n > 0) {
                        PL_strcat(sonameListToSave, p);
                        PL_strcat(sonameListToSave,":");
                    }
                    if (soname) {
                        PL_strfree(soname); // it's from strdup
                    }
                    if (numOfLibs > 1)
                        arrayOfLibs[i][PL_strlen(arrayOfLibs[i])] = ':'; //restore ":" in sonameList
                }
            }
            for (p = &sonameListToSave[PL_strlen(sonameListToSave) - 1]; *p == ':'; p--)
                *p = 0; //delete tail ":" delimiters

            if (!prefSonameListIsSet || PL_strcmp(sonameList, sonameListToSave)) {
                // if user specified some bogus soname I overwrite it here,
                // otherwise it'll decrease performance by calling popen() in SearchForSoname
                // every time for each bogus name
                prefs->SetCharPref(sonamesListFromPref, (const char *)sonameListToSave);
            }
            PL_strfree(sonameList);
        }
    }
}
#endif //MOZ_WIDGET_GTK



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
    libSpec.type = PR_LibSpec_Pathname;
    libSpec.value.pathname = this->GetCString();

    pLibrary = outLibrary = PR_LoadLibraryWithFlags(libSpec, 0);

#ifdef MOZ_WIDGET_GTK

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
    if (!pLibrary) {
        LoadExtraSharedLibs();
        // try reload plugin ones more
        pLibrary = outLibrary = PR_LoadLibraryWithFlags(libSpec, 0);
        if (!pLibrary)
            DisplayPR_LoadLibraryErrorMessage(libSpec.value.pathname);
    }
#endif

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
    const char* mimedescr = 0, *name = 0, *description = 0;
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
        // if fileName parameter == 0 ns4xPlugin::CreatePlugin() will not call NP_Initialize()
        rv = ns4xPlugin::CreatePlugin(mgr, 0, pLibrary, 
				      getter_AddRefs(plugin));
        if (NS_FAILED(rv)) return rv;
    }

    if (plugin) {
        info.fFileName = PL_strdup(this->GetCString());
        plugin->GetValue(nsPluginVariable_NameString, &name);
        if (!name)
            name = PL_strrchr(info.fFileName, '/') + 1;
        info.fName = PL_strdup(name);

        plugin->GetValue(nsPluginVariable_DescriptionString, &description);
        if (!description)
            description = "";
        info.fDescription = PL_strdup(description);
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

#ifdef DEBUG_av
        printf("Registering plugin %d for: \"%s\",\"%s\",\"%s\"\n",
               i, mtype,descr ? descr : "null",exten ? exten : "null");
#endif /* DEBUG_av */

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
