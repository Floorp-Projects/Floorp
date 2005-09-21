/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
	nsPluginsDirUNIX.cpp
	
	UNIX implementation of the nsPluginsDir/nsPluginsFile classes.
	
	by Alex Musil
 */

#include "nsplugin.h"
#include "ns4xPlugin.h"
#include "ns4xPluginInstance.h"
#include "nsIServiceManager.h"
#include "nsIMemory.h"
#include "nsIPluginStreamListener.h"
#include "nsPluginsDir.h"
#include "nsPluginsDirUtils.h"
#include "nsObsoleteModuleLoading.h"
#include "prmem.h"
#include "prenv.h"
#include "prerror.h"
#include <sys/stat.h>
#include "nsString.h"
#include "nsILocalFile.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#ifdef AIX
#include "nsPluginLogging.h"
#include "prprf.h"
#define LOG(args)  PLUGIN_LOG(PLUGIN_LOG_NORMAL, args)
#endif

#define LOCAL_PLUGIN_DLL_SUFFIX ".so"
#if defined(__hpux)
#define DEFAULT_X11_PATH "/usr/lib/X11R6/"
#undef LOCAL_PLUGIN_DLL_SUFFIX
#define LOCAL_PLUGIN_DLL_SUFFIX ".sl"
#define LOCAL_PLUGIN_DLL_ALT_SUFFIX ".so"
#elif defined(_AIX)
#define DEFAULT_X11_PATH "/usr/lib"
#define LOCAL_PLUGIN_DLL_ALT_SUFFIX ".a"
#elif defined(SOLARIS)
#define DEFAULT_X11_PATH "/usr/openwin/lib/"
#elif defined(LINUX)
#define DEFAULT_X11_PATH "/usr/X11R6/lib/"
#else
#define DEFAULT_X11_PATH ""
#endif

#if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_GTK2)

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
#if defined(SOLARIS) || defined(HPUX)
#define DEFAULT_EXTRA_LIBS_LIST "libXt" LOCAL_PLUGIN_DLL_SUFFIX ":libXext" LOCAL_PLUGIN_DLL_SUFFIX ":libXm" LOCAL_PLUGIN_DLL_SUFFIX
#else
#define DEFAULT_EXTRA_LIBS_LIST "libXt" LOCAL_PLUGIN_DLL_SUFFIX ":libXext" LOCAL_PLUGIN_DLL_SUFFIX
#endif
/*
 this function looks for
 user_pref("plugin.soname.list", "/usr/X11R6/lib/libXt.so.6:libXext.so");
 in user's pref.js
 and loads all libs in specified order
*/

static void LoadExtraSharedLibs()
{
    // check out if user's prefs.js has libs name
    nsresult res;
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &res));
    if (NS_SUCCEEDED(res) && (prefs != nsnull)) {
        char *sonamesListFromPref = PREF_PLUGINS_SONAME;
        char *sonameList = NULL;
        PRBool prefSonameListIsSet = PR_TRUE;
        res = prefs->GetCharPref(sonamesListFromPref, &sonameList);
        if (!sonameList) {
            // pref is not set, lets use hardcoded list
            prefSonameListIsSet = PR_FALSE;
            sonameList = PL_strdup(DEFAULT_EXTRA_LIBS_LIST);
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
#endif //MOZ_WIDGET_GTK || MOZ_WIDGET_GTK2



///////////////////////////////////////////////////////////////////////////

/* nsPluginsDir implementation */

PRBool nsPluginsDir::IsPluginFile(nsIFile* file)
{
    nsCAutoString filename;
    if (NS_FAILED(file->GetNativeLeafName(filename)))
        return PR_FALSE;

    NS_NAMED_LITERAL_CSTRING(dllSuffix, LOCAL_PLUGIN_DLL_SUFFIX);
    if (filename.Length() > dllSuffix.Length() &&
        StringEndsWith(filename, dllSuffix))
        return PR_TRUE;
    
#ifdef LOCAL_PLUGIN_DLL_ALT_SUFFIX
    NS_NAMED_LITERAL_CSTRING(dllAltSuffix, LOCAL_PLUGIN_DLL_ALT_SUFFIX);
    if (filename.Length() > dllAltSuffix.Length() &&
        StringEndsWith(filename, dllAltSuffix))
        return PR_TRUE;
#endif
    return PR_FALSE;
}

///////////////////////////////////////////////////////////////////////////

/* nsPluginFile implementation */

nsPluginFile::nsPluginFile(nsIFile* file)
: mPlugin(file)
{
    // nada
}

nsPluginFile::~nsPluginFile()
{
    // nada
}

#ifdef AIX
/**
 * This code is necessary for Java 1.4.2 SR2 and later on AIX, as the
 * loadquery() function is no longer used to determine the path to the
 * installed JVM, so we need to manually set the LIBPATH variable to
 * include the proper directories needed to run the Java plug-in.
 *
 * See Bug 297807 for more information.
 */
static char *javaLibPath = NULL;

static void SetJavaLibPath(const nsCString& pluginPath)
{
    // If javaLibPath is non-NULL, that means we have already set the LIBPATH
    // variable once this session, so we don't need to do it again.
    if (javaLibPath)
        return;

    nsCAutoString javaDir, newLibPath;

    PRInt32 pos = pluginPath.RFindChar('/');
    if (pos == kNotFound || pos == 0)
        return;

    javaDir = Substring(pluginPath, 0, pos);
    LOG(("AIX: Java dir is %s\n", javaDir.get()));

    // Add jre/bin to new LIBPATH
    newLibPath += javaDir;

    // Check for existance of jre/bin/classic dir, and append it to
    // LIBPATH if found
    PRFileInfo info;
    javaDir.AppendLiteral("/classic");
    if (PR_GetFileInfo(javaDir.get(), &info) == PR_SUCCESS &&
        info.type == PR_FILE_DIRECTORY)
    {
        newLibPath.Append(':');
        newLibPath.Append(javaDir);
    }

    // Get the current LIBPATH, and append it to the new LIBPATH
    const char *currentLibPath = PR_GetEnv("LIBPATH");
    LOG(("AIX: current LIBPATH=%s\n", currentLibPath));
    if (currentLibPath && *currentLibPath) {
        newLibPath.Append(':');
        newLibPath.Append(currentLibPath);
    }

    // Set the LIBPATH to include the path to the JRE directories.
    // NOTE: We are leaking javaLibPath here, as it needs to remain in memory
    // for PR_SetEnv to work properly.
    javaLibPath = PR_smprintf("LIBPATH=%s", newLibPath.get());
    if (javaLibPath) {
        LOG(("AIX: new LIBPATH=%s\n", newLibPath.get()));
        PR_SetEnv(javaLibPath);
    }
}
#endif

/**
 * Loads the plugin into memory using NSPR's shared-library loading
 * mechanism. Handles platform differences in loading shared libraries.
 */
nsresult nsPluginFile::LoadPlugin(PRLibrary* &outLibrary)
{
    PRLibSpec libSpec;
    libSpec.type = PR_LibSpec_Pathname;
    PRBool exists = PR_FALSE;
    mPlugin->Exists(&exists);
    if (!exists)
        return NS_ERROR_FILE_NOT_FOUND;

    nsresult rv;
    nsCAutoString path;
    rv = mPlugin->GetNativePath(path);
    if (NS_FAILED(rv))
        return rv;

#ifdef AIX
    nsCAutoString leafName;
    rv = mPlugin->GetNativeLeafName(leafName);
    if (NS_FAILED(rv))
        return rv;

    if (StringBeginsWith(leafName, NS_LITERAL_CSTRING("libjavaplugin_oji")))
        SetJavaLibPath(path);
#endif

    libSpec.value.pathname = path.get();

#if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_GTK2)

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


#if defined(SOLARIS) || defined(HPUX)
    // Acrobat/libXm: Lazy resolving might cause crash later (bug 211587)
    pLibrary = outLibrary = PR_LoadLibraryWithFlags(libSpec, PR_LD_NOW);
#else
    // Some dlopen() doesn't recover from a failed PR_LD_NOW (bug 223744)
    pLibrary = outLibrary = PR_LoadLibraryWithFlags(libSpec, 0);
#endif
    if (!pLibrary) {
        LoadExtraSharedLibs();
        // try reload plugin once more
        pLibrary = outLibrary = PR_LoadLibraryWithFlags(libSpec, 0);
        if (!pLibrary)
            DisplayPR_LoadLibraryErrorMessage(libSpec.value.pathname);
    }
#else
    pLibrary = outLibrary = PR_LoadLibraryWithFlags(libSpec, 0);
#endif  // MOZ_WIDGET_GTK || MOZ_WIDGET_GTK2

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

    // No, this doesn't leak. GetGlobalServiceManager() doesn't addref
    // it's out pointer. Maybe it should.
    nsIServiceManagerObsolete* mgr;
    nsServiceManager::GetGlobalServiceManager((nsIServiceManager**)&mgr);

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
        rv = ns4xPlugin::CreatePlugin(mgr, 0, 0, pLibrary, 
				      getter_AddRefs(plugin));
        if (NS_FAILED(rv)) return rv;
    }

    if (plugin) {
        plugin->GetMIMEDescription(&mimedescr);
#ifdef NS_DEBUG
        printf("GetMIMEDescription() returned \"%s\"\n", mimedescr);
#endif
        if (NS_FAILED(rv = ParsePluginMimeDescription(mimedescr, info)))
            return rv;
        nsCAutoString filename;
        if (NS_FAILED(rv = mPlugin->GetNativePath(filename)))
            return rv;
        info.fFileName = PL_strdup(filename.get());
        plugin->GetValue(nsPluginVariable_NameString, &name);
        if (!name)
            name = PL_strrchr(info.fFileName, '/') + 1;
        info.fName = PL_strdup(name);

        plugin->GetValue(nsPluginVariable_DescriptionString, &description);
        if (!description)
            description = "";
        info.fDescription = PL_strdup(description);
    }
    return NS_OK;
}


nsresult nsPluginFile::FreePluginInfo(nsPluginInfo& info)
{
    if (info.fName != nsnull)
        PL_strfree(info.fName);

    if (info.fDescription != nsnull)
        PL_strfree(info.fDescription);

    for (PRUint32 i = 0; i < info.fVariantCount; i++) {
        if (info.fMimeTypeArray[i] != nsnull)
            PL_strfree(info.fMimeTypeArray[i]);

        if (info.fMimeDescriptionArray[i] != nsnull)
            PL_strfree(info.fMimeDescriptionArray[i]);

        if (info.fExtensionArray[i] != nsnull)
            PL_strfree(info.fExtensionArray[i]);
    }

    PR_FREEIF(info.fMimeTypeArray);
    PR_FREEIF(info.fMimeDescriptionArray);
    PR_FREEIF(info.fExtensionArray);

    if (info.fFileName != nsnull)
        PL_strfree(info.fFileName);

    return NS_OK;
}
