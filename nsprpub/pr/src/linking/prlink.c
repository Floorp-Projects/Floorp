/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "primpl.h"

#include <string.h>

#ifdef XP_BEOS
#include <image.h>
#endif

#ifdef XP_MAC
#include <CodeFragments.h>
#include <TextUtils.h>
#include <Types.h>
#include <Strings.h>
#include <Aliases.h>

#if TARGET_CARBON
#include <CFURL.h>
#include <CFBundle.h>
#include <CFString.h>
#endif

#include "macdll.h"
#include "mdmac.h"
#endif

#ifdef XP_UNIX
#ifdef USE_DLFCN
#include <dlfcn.h>
/* Define these on systems that don't have them. */
#ifndef RTLD_NOW
#define RTLD_NOW 0
#endif
#ifndef RTLD_LAZY
#define RTLD_LAZY RTLD_NOW
#endif
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 0
#endif
#ifndef RTLD_LOCAL
#define RTLD_LOCAL 0
#endif
#elif defined(USE_HPSHL)
#include <dl.h>
#elif defined(USE_MACH_DYLD)
#include <mach-o/dyld.h>
#endif
#endif /* XP_UNIX */

#define _PR_DEFAULT_LD_FLAGS PR_LD_LAZY

/*
 * On these platforms, symbols have a leading '_'.
 */
#if defined(SUNOS4) || defined(RHAPSODY) || defined(NEXTSTEP) \
    || defined(OPENBSD) || defined(WIN16) \
    || (defined(NETBSD) && !defined(__ELF__))
#define NEED_LEADING_UNDERSCORE
#endif

#ifdef XP_PC
typedef PRStaticLinkTable *NODL_PROC(void);
#endif

/************************************************************************/

struct PRLibrary {
    char*                       name;  /* Our own copy of the name string */
    PRLibrary*                  next;
    int                         refCount;
    const PRStaticLinkTable*    staticTable;

#ifdef XP_PC
#ifdef XP_OS2
    HMODULE                     dlh;
#else
    HINSTANCE                   dlh;
#endif
#endif

#ifdef XP_MAC
    CFragConnectionID           dlh;

#if TARGET_CARBON
    CFBundleRef                 bundle;
#endif

    Ptr                         main;
#endif

#ifdef XP_UNIX
#if defined(USE_HPSHL)
    shl_t                       dlh;
#elif defined(USE_MACH_DYLD)
    NSModule                    dlh;
#else
    void*                       dlh;
#endif 
#endif 

#ifdef XP_BEOS
    void*                       dlh;
    void*                       stub_dlh;
#endif
};

static PRLibrary *pr_loadmap;
static PRLibrary *pr_exe_loadmap;
static PRMonitor *pr_linker_lock;
static char* _pr_currentLibPath = NULL;

static PRLibrary *pr_LoadLibraryByPathname(const char *name, PRIntn flags);
#ifdef XP_MAC
static PRLibrary *pr_Mac_LoadNamedFragment(const FSSpec *fileSpec,
    const char* fragmentName);
static PRLibrary *pr_Mac_LoadIndexedFragment(const FSSpec *fileSpec,
    PRUint32 fragIndex);
#endif /* XP_MAC */

/************************************************************************/

#if !defined(USE_DLFCN) && !defined(HAVE_STRERROR)
static char* errStrBuf = NULL;
#define ERR_STR_BUF_LENGTH    20
static char* errno_string(PRIntn oserr)
{
    if (errStrBuf == NULL)
        errStrBuf = PR_MALLOC(ERR_STR_BUF_LENGTH);
    PR_snprintf(errStrBuf, ERR_STR_BUF_LENGTH, "error %d", oserr);
    return errStrBuf;
}
#endif

static void DLLErrorInternal(PRIntn oserr)
/*
** This whole function, and most of the code in this file, are run
** with a big hairy lock wrapped around it. Not the best of situations,
** but will eventually come up with the right answer.
*/
{
    const char *error = NULL;
#ifdef USE_DLFCN
    error = dlerror();  /* $$$ That'll be wrong some of the time - AOF */
#elif defined(HAVE_STRERROR)
    error = strerror(oserr);  /* this should be okay */
#else
    error = errno_string(oserr);
#endif
    if (NULL != error)
        PR_SetErrorText(strlen(error), error);
}  /* DLLErrorInternal */

void _PR_InitLinker(void)
{
#ifndef XP_MAC
    PRLibrary *lm;
#endif
#if defined(XP_UNIX)
    void *h;
#endif

    if (!pr_linker_lock) {
        pr_linker_lock = PR_NewNamedMonitor("linker-lock");
    }
    PR_EnterMonitor(pr_linker_lock);

#if defined(XP_PC)
    lm = PR_NEWZAP(PRLibrary);
    lm->name = strdup("Executable");
        /* 
        ** In WIN32, GetProcAddress(...) expects a module handle in order to
        ** get exported symbols from the executable...
        **
        ** However, in WIN16 this is accomplished by passing NULL to 
        ** GetProcAddress(...)
        */
#if defined(_WIN32)
        lm->dlh = GetModuleHandle(NULL);
#else
        lm->dlh = (HINSTANCE)NULL;
#endif /* ! _WIN32 */

    lm->refCount    = 1;
    lm->staticTable = NULL;
    pr_exe_loadmap  = lm;
    pr_loadmap      = lm;

#elif defined(XP_UNIX)
#ifdef HAVE_DLL
#ifdef USE_DLFCN
    h = dlopen(0, RTLD_LAZY);
    if (!h) {
        char *error;
        
        DLLErrorInternal(_MD_ERRNO());
        error = (char*)PR_MALLOC(PR_GetErrorTextLength());
        (void) PR_GetErrorText(error);
        fprintf(stderr, "failed to initialize shared libraries [%s]\n",
            error);
        PR_DELETE(error);
        abort();/* XXX */
    }
#elif defined(USE_HPSHL)
    h = NULL;
    /* don't abort with this NULL */
#elif defined(USE_MACH_DYLD)
    h = NULL; /* XXXX  toshok */
#else
#error no dll strategy
#endif /* USE_DLFCN */

    lm = PR_NEWZAP(PRLibrary);
    if (lm) {
        lm->name = strdup("a.out");
        lm->refCount = 1;
        lm->dlh = h;
        lm->staticTable = NULL;
    }
    pr_exe_loadmap = lm;
    pr_loadmap = lm;
#endif /* HAVE_DLL */
#endif /* XP_UNIX */

#ifndef XP_MAC
    PR_LOG(_pr_linker_lm, PR_LOG_MIN, ("Loaded library %s (init)", lm?lm->name:"NULL"));
#endif

    PR_ExitMonitor(pr_linker_lock);
}

#if defined(WIN16)
void _PR_ShutdownLinker(void)
{
    PR_EnterMonitor(pr_linker_lock);

    while (pr_loadmap) {
    if (pr_loadmap->refCount > 1) {
#ifdef DEBUG
        fprintf(stderr, "# Forcing library to unload: %s (%d outstanding references)\n",
            pr_loadmap->name, pr_loadmap->refCount);
#endif
        pr_loadmap->refCount = 1;
    }
    PR_UnloadLibrary(pr_loadmap);
    }
    
    PR_ExitMonitor(pr_linker_lock);

    PR_DestroyMonitor(pr_linker_lock);
    pr_linker_lock = NULL;
}
#endif

/******************************************************************************/

PR_IMPLEMENT(PRStatus) PR_SetLibraryPath(const char *path)
{
    PRStatus rv = PR_SUCCESS;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    PR_EnterMonitor(pr_linker_lock);
    PR_FREEIF(_pr_currentLibPath);
    if (path) {
        _pr_currentLibPath = strdup(path);
        if (!_pr_currentLibPath) {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        rv = PR_FAILURE;
        }
    } else {
        _pr_currentLibPath = 0;
    }
    PR_ExitMonitor(pr_linker_lock);
    return rv;
}

/*
** Return the library path for finding shared libraries.
*/
PR_IMPLEMENT(char *) 
PR_GetLibraryPath()
{
    char *ev;
    char *copy = NULL;  /* a copy of _pr_currentLibPath */

    if (!_pr_initialized) _PR_ImplicitInitialization();
    PR_EnterMonitor(pr_linker_lock);
    if (_pr_currentLibPath != NULL) {
        goto exit;
    }

    /* initialize pr_currentLibPath */

#ifdef XP_PC
    ev = getenv("LD_LIBRARY_PATH");
    if (!ev) {
    ev = ".;\\lib";
    }
    ev = strdup(ev);
#endif

#ifdef XP_MAC
    {
    char *p;
    int len;

    ev = getenv("LD_LIBRARY_PATH");
    
    if (!ev)
        ev = "";
    
    len = strlen(ev) + 1;        /* +1 for the null */
    p = (char*) PR_MALLOC(len);
    if (p) {
        strcpy(p, ev);
    }
    ev = p;
    }
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
#if defined(USE_DLFCN) || defined(USE_MACH_DYLD) || defined(XP_BEOS)
    {
    char *p=NULL;
    int len;

#ifdef XP_BEOS
    ev = getenv("LIBRARY_PATH");
    if (!ev) {
        ev = "%A/lib:/boot/home/config/lib:/boot/beos/system/lib";
    }
#else
    ev = getenv("LD_LIBRARY_PATH");
    if (!ev) {
        ev = "/usr/lib:/lib";
    }
#endif
    len = strlen(ev) + 1;        /* +1 for the null */

    p = (char*) PR_MALLOC(len);
    if (p) {
        strcpy(p, ev);
    }   /* if (p)  */
    ev = p;
    PR_LOG(_pr_io_lm, PR_LOG_NOTICE, ("linker path '%s'", ev));

    }
#else
    /* AFAIK there isn't a library path with the HP SHL interface --Rob */
    ev = strdup("");
#endif
#endif

    /*
     * If ev is NULL, we have run out of memory
     */
    _pr_currentLibPath = ev;

  exit:
    if (_pr_currentLibPath) {
        copy = strdup(_pr_currentLibPath);
    }
    PR_ExitMonitor(pr_linker_lock);
    if (!copy) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
    return copy;
}

/*
** Build library name from path, lib and extensions
*/
PR_IMPLEMENT(char*) 
PR_GetLibraryName(const char *path, const char *lib)
{
    char *fullname;

#ifdef XP_PC
    if (strstr(lib, PR_DLL_SUFFIX) == NULL)
    {
        fullname = PR_smprintf("%s\\%s%s", path, lib, PR_DLL_SUFFIX);
    } else {
        fullname = PR_smprintf("%s\\%s", path, lib);
    }
#endif /* XP_PC */
#ifdef XP_MAC
    fullname = PR_smprintf("%s%s", path, lib);
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)
    if (strstr(lib, PR_DLL_SUFFIX) == NULL)
    {
        fullname = PR_smprintf("%s/lib%s%s", path, lib, PR_DLL_SUFFIX);
    } else {
        fullname = PR_smprintf("%s/%s", path, lib);
    }
#endif /* XP_UNIX || XP_BEOS */
    return fullname;
}

/*
** Free the memory allocated, for the caller, by PR_GetLibraryName
*/
PR_IMPLEMENT(void) 
PR_FreeLibraryName(char *mem)
{
    PR_smprintf_free(mem);
}

static PRLibrary* 
pr_UnlockedFindLibrary(const char *name)
{
    PRLibrary* lm = pr_loadmap;
    const char* np = strrchr(name, PR_DIRECTORY_SEPARATOR);
    np = np ? np + 1 : name;
    while (lm) {
    const char* cp = strrchr(lm->name, PR_DIRECTORY_SEPARATOR);
    cp = cp ? cp + 1 : lm->name;
#ifdef XP_PC
        /* Windows DLL names are case insensitive... */
    if (strcmpi(np, cp) == 0) 
#else
    if (strcmp(np, cp)  == 0) 
#endif
    {
        /* found */
        lm->refCount++;
        PR_LOG(_pr_linker_lm, PR_LOG_MIN,
           ("%s incr => %d (find lib)",
            lm->name, lm->refCount));
        return lm;
    }
    lm = lm->next;
    }
    return NULL;
}

PR_IMPLEMENT(PRLibrary*)
PR_LoadLibraryWithFlags(PRLibSpec libSpec, PRIntn flags)
{
    if (flags == 0) {
        flags = _PR_DEFAULT_LD_FLAGS;
    }
    switch (libSpec.type) {
        case PR_LibSpec_Pathname:
            return pr_LoadLibraryByPathname(libSpec.value.pathname, flags);
#ifdef XP_MAC
        case PR_LibSpec_MacNamedFragment:
            return pr_Mac_LoadNamedFragment(
                libSpec.value.mac_named_fragment.fsspec,
                libSpec.value.mac_named_fragment.name);
        case PR_LibSpec_MacIndexedFragment:
            return pr_Mac_LoadIndexedFragment(
                libSpec.value.mac_indexed_fragment.fsspec,
                libSpec.value.mac_indexed_fragment.index);
#endif /* XP_MAC */
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            return NULL;
    }
}
            
PR_IMPLEMENT(PRLibrary*) 
PR_LoadLibrary(const char *name)
{
    PRLibSpec libSpec;

    libSpec.type = PR_LibSpec_Pathname;
    libSpec.value.pathname = name;
    return PR_LoadLibraryWithFlags(libSpec, 0);
}

#if TARGET_CARBON

/*
** Returns a CFBundleRef if the FSSpec refers to a Mac OS X bundle directory.
** The caller is responsible for calling CFRelease() to deallocate.
*/
static CFBundleRef getLibraryBundle(const FSSpec* spec)
{
    CFBundleRef bundle = NULL;
    FSRef ref;
    OSErr err = FSpMakeFSRef(spec, &ref);
    char path[512];
    if (err == noErr && ((UInt32)(FSRefMakePath) != kUnresolvedCFragSymbolAddress)) {
        err = FSRefMakePath(&ref, (UInt8*)path, sizeof(path) - 1);
        if (err == noErr) {
            CFStringRef pathRef = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
            if (pathRef) {
            	CFURLRef bundleURL = CFURLCreateWithFileSystemPath(NULL, pathRef, kCFURLPOSIXPathStyle, true);
            	if (bundleURL != NULL) {
                    bundle = CFBundleCreate(NULL, bundleURL);
                    CFRelease(bundleURL);
            	}
            	CFRelease(pathRef);
            }
        }
    }
    return bundle;
}

#endif

/*
** Dynamically load a library. Only load libraries once, so scan the load
** map first.
*/
static PRLibrary*
pr_LoadLibraryByPathname(const char *name, PRIntn flags)
{
    PRLibrary *lm;
    PRLibrary* result;
    PRInt32 oserr;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    /* See if library is already loaded */
    PR_EnterMonitor(pr_linker_lock);

    result = pr_UnlockedFindLibrary(name);
    if (result != NULL) goto unlock;

    lm = PR_NEWZAP(PRLibrary);
    if (lm == NULL) {
        oserr = _MD_ERRNO();
        goto unlock;
    }
    lm->staticTable = NULL;

#ifdef XP_OS2  /* Why isn't all this stuff in MD code?! */
    {
        HMODULE h;
        UCHAR pszError[_MAX_PATH];
        ULONG ulRc = NO_ERROR;

        retry:
              ulRc = DosLoadModule(pszError, _MAX_PATH, (PSZ) name, &h);
          if (ulRc != NO_ERROR) {
              oserr = ulRc;
              PR_DELETE(lm);
              goto unlock;
          }
          lm->name = strdup(name);
          lm->dlh  = h;
          lm->next = pr_loadmap;
          pr_loadmap = lm;
    }
#endif /* XP_OS2 */

#if defined(WIN32) || defined(WIN16)
    {
    HINSTANCE h;
    NODL_PROC *pfn;

    h = LoadLibrary(name);
    if (h < (HINSTANCE)HINSTANCE_ERROR) {
        oserr = _MD_ERRNO();
        PR_DELETE(lm);
        goto unlock;
    }
    lm->name = strdup(name);
    lm->dlh = h;
    lm->next = pr_loadmap;
    pr_loadmap = lm;

        /*
        ** Try to load a table of "static functions" provided by the DLL
        */

        pfn = (NODL_PROC *)GetProcAddress(h, "NODL_TABLE");
        if (pfn != NULL) {
            lm->staticTable = (*pfn)();
        }
    }
#endif /* WIN32 || WIN16 */

#if defined(XP_MAC) && TARGET_RT_MAC_CFM
    {
    OSErr                 err;
    CFragConnectionID     connectionID;
    Str255                errName;
    Str255                pName;
    char                  cName[64];
    const char*           libName;
        
    /*
     * Algorithm: The "name" passed in could be either a shared
     * library name that we should look for in the normal library
     * search paths, or a full path name to a specific library on
     * disk.  Since the full path will always contain a ":"
     * (shortest possible path is "Volume:File"), and since a
     * library name can not contain a ":", we can test for the
     * presence of a ":" to see which type of library we should load.
     * or its a full UNIX path which we for now assume is Java
     * enumerating all the paths (see below)
     */
    if (strchr(name, PR_PATH_SEPARATOR) == NULL)
    {
        if (strchr(name, PR_DIRECTORY_SEPARATOR) == NULL)
        {
        /*
         * The name did not contain a ":", so it must be a
         * library name.  Convert the name to a Pascal string
         * and try to find the library.
         */
        }
        else
        {
            /* name contained a "/" which means we need to suck off the last part */
            /* of the path and pass that on the NSGetSharedLibrary */
            /* this may not be what we really want to do .. because Java could */
            /* be iterating through the whole LD path, and we'll find it if it's */
            /* anywhere on that path -- it appears that's what UNIX and the PC do */
            /* too...so we'll emulate but it could be wrong. */
            name = strrchr(name, PR_DIRECTORY_SEPARATOR) + 1;
        }
        
        PStrFromCStr(name, pName);
    
        /*
         * beard: NSGetSharedLibrary was so broken that I just decided to
         * use GetSharedLibrary for now.  This will need to change for
         * plugins, but those should go in the Extensions folder anyhow.
         */
#if 0
        err = NSGetSharedLibrary(pName, &connectionID, &lm->main);
#else
        err = GetSharedLibrary(pName, kCompiledCFragArch, kReferenceCFrag,
                &connectionID, &lm->main, errName);
#endif
        if (err != noErr)
        {
            oserr = err;
            PR_DELETE(lm);
            goto unlock;    
        }
        
        libName = name;
    }
    else    
    {
        /*
         * The name did contain a ":", so it must be a full path name.
         * Now we have to do a lot of work to convert the path name to
         * an FSSpec (silly, since we were probably just called from the
         * MacFE plug-in code that already knew the FSSpec and converted
         * it to a full path just to pass to us).  First we copy out the
         * volume name (the text leading up to the first ":"); then we
         * separate the file name (the text following the last ":") from
         * rest of the path.  After converting the strings to Pascal
         * format we can call GetCatInfo to get the parent directory ID
         * of the file, and then (finally) make an FSSpec and call
         * GetDiskFragment.
         */
        FSSpec fileSpec;
        Boolean tempUnusedBool;

        PStrFromCStr(name, pName);
        err = FSMakeFSSpec(0, 0, pName, &fileSpec);
        if (err != noErr) {
            oserr = _MD_ERRNO();
            PR_DELETE(lm);
            goto unlock;
        }

        /* Resolve an alias if this was one */
        err = ResolveAliasFile(&fileSpec, true, &tempUnusedBool, &tempUnusedBool);
        if (err != noErr)
        {
            oserr = err;
            PR_DELETE(lm);
            goto unlock;
        }

        /* Finally, try to load the library */
        err = GetDiskFragment(&fileSpec, 0, kCFragGoesToEOF, fileSpec.name, 
                              kLoadCFrag, &connectionID, &lm->main, errName);

        memcpy(cName, fileSpec.name + 1, fileSpec.name[0]);
        cName[fileSpec.name[0]] = '\0';
        libName = cName;
        
        if (err != noErr)
        {
#if TARGET_CARBON
            /* If not a CFM library, perhaps it's a CFBundle. */
            lm->bundle = getLibraryBundle(&fileSpec);
#ifdef DEBUG
            fprintf(stderr, "*** loading bundle for library '%s' [%s]. ***\n",
                    libName, lm->bundle ? "SUCCEEDED" : "FAILED");
#endif
            if (lm->bundle == NULL) {
                oserr = err;
                PR_DELETE(lm);
                goto unlock;
            }
#else
            oserr = err;
            PR_DELETE(lm);
            goto unlock;
#endif
        }
    }
    
    lm->name = strdup(libName);
    lm->dlh = connectionID;
    lm->next = pr_loadmap;
    pr_loadmap = lm;
    }
#elif defined(XP_MAC) && !TARGET_RT_MAC_CFM
    {

    }
#endif

#ifdef XP_UNIX
#ifdef HAVE_DLL
    {
#if defined(USE_DLFCN)
#ifdef NTO
    /* Neutrino needs RTLD_GROUP to load Netscape plugins. (bug 71179) */
    int dl_flags = RTLD_GROUP;
#else
    int dl_flags = 0;
#endif
    void *h;

    if (flags & PR_LD_LAZY) {
        dl_flags |= RTLD_LAZY;
    }
    if (flags & PR_LD_NOW) {
        dl_flags |= RTLD_NOW;
    }
    if (flags & PR_LD_GLOBAL) {
        dl_flags |= RTLD_GLOBAL;
    }
    if (flags & PR_LD_LOCAL) {
        dl_flags |= RTLD_LOCAL;
    }
    h = dlopen(name, dl_flags);
#elif defined(USE_HPSHL)
    int shl_flags = 0;
    shl_t h;

    /*
     * Use the DYNAMIC_PATH flag only if 'name' is a plain file
     * name (containing no directory) to match the behavior of
     * dlopen().
     */
    if (strchr(name, PR_DIRECTORY_SEPARATOR) == NULL) {
        shl_flags |= DYNAMIC_PATH;
    }
    if (flags & PR_LD_LAZY) {
        shl_flags |= BIND_DEFERRED;
    }
    if (flags & PR_LD_NOW) {
        shl_flags |= BIND_IMMEDIATE;
    }
    /* No equivalent of PR_LD_GLOBAL and PR_LD_LOCAL. */
    h = shl_load(name, shl_flags, 0L);
#elif defined(USE_MACH_DYLD)
    NSObjectFileImage ofi;
    NSModule h = NULL;
    if (NSCreateObjectFileImageFromFile(name, &ofi)
            == NSObjectFileImageSuccess) {
        h = NSLinkModule(ofi, name, NSLINKMODULE_OPTION_PRIVATE);
    }
#else
#error Configuration error
#endif
    if (!h) {
        oserr = _MD_ERRNO();
        PR_DELETE(lm);
        goto unlock;
    }
    lm->name = strdup(name);
    lm->dlh = h;
    lm->next = pr_loadmap;
    pr_loadmap = lm;
    }
#endif /* HAVE_DLL */
#endif /* XP_UNIX */

    lm->refCount = 1;

#ifdef XP_BEOS
    {
        image_info info;
        int32 cookie = 0;
        image_id imageid = B_ERROR;
        image_id stubid = B_ERROR;
        PRLibrary *p;

        for (p = pr_loadmap; p != NULL; p = p->next) {
            /* hopefully, our caller will always use the same string
               to refer to the same library */
            if (strcmp(name, p->name) == 0) {
                /* we've already loaded this library */
                imageid = info.id;
                lm->refCount++;
                break;
            }
        }

        if(imageid == B_ERROR) {
            /* it appears the library isn't yet loaded - load it now */
            char stubName [B_PATH_NAME_LENGTH + 1];

            /* the following is a work-around to a "bug" in the beos -
               the beos system loader allows only 32M (system-wide)
               to be used by code loaded as "add-ons" (code loaded
               through the 'load_add_on()' system call, which includes
               mozilla components), but allows 256M to be used by
               shared libraries.
               
               unfortunately, mozilla is too large to fit into the
               "add-on" space, so we must trick the loader into
               loading some of the components as shared libraries.  this
               is accomplished by creating a "stub" add-on (an empty
               shared object), and linking it with the component
               (the actual .so file generated by the build process,
               without any modifications).  when this stub is loaded
               by load_add_on(), the loader will automatically load the
               component into the shared library space.
            */

            strcpy(stubName, name);
            strcat(stubName, ".stub");

            /* first, attempt to load the stub (thereby loading the
               component as a shared library */
            if ((stubid = load_add_on(stubName)) > B_ERROR) {
                /* the stub was loaded successfully. */
                imageid = B_FILE_NOT_FOUND;

                cookie = 0;
                while (get_next_image_info(0, &cookie, &info) == B_OK) {
                    char *endOfSystemName = strrchr(info.name, '/');
                    char *endOfPassedName = strrchr(name, '/');
                    if( 0 == endOfSystemName ) 
                        endOfSystemName=info.name;
                    else
                        endOfSystemName++;
                    if( 0 == endOfPassedName )
                        endOfPassedName=name;
                    else
                        endOfPassedName++;
                    if (strcmp(endOfSystemName, endOfPassedName) == 0) {
                        /* this is the actual component - remember it */
                        imageid = info.id;
                        break;
                    }
                }

            } else {
                /* we failed to load the "stub" - try to load the
                   component directly as an add-on */
                stubid = B_ERROR;
                imageid = load_add_on(name);
            }
        }

        if (imageid <= B_ERROR) {
            oserr = imageid;
            PR_DELETE( lm );
            goto unlock;
        }
        lm->name = strdup(name);
        lm->dlh = (void*)imageid;
        lm->stub_dlh = (void*)stubid;
        lm->next = pr_loadmap;
        pr_loadmap = lm;
    }
#endif

    result = lm;    /* success */
    PR_LOG(_pr_linker_lm, PR_LOG_MIN, ("Loaded library %s (load lib)", lm->name));

  unlock:
    if (result == NULL) {
        PR_SetError(PR_LOAD_LIBRARY_ERROR, oserr);
        DLLErrorInternal(oserr);  /* sets error text */
    }
    PR_ExitMonitor(pr_linker_lock);
    return result;
}

PR_IMPLEMENT(PRLibrary*) 
PR_FindLibrary(const char *name)
{
    PRLibrary* result;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    PR_EnterMonitor(pr_linker_lock);
    result = pr_UnlockedFindLibrary(name);
    PR_ExitMonitor(pr_linker_lock);
    return result;
}


#ifdef XP_MAC

static PRLibrary*
pr_Mac_LoadNamedFragment(const FSSpec *fileSpec, const char* fragmentName)
{
	PRLibrary*					newLib = NULL;
	PRLibrary* 					result;
	FSSpec							resolvedSpec = *fileSpec;
	CFragConnectionID		connectionID = 0;
	Boolean							isFolder, wasAlias;
	OSErr								err = noErr;
	
	if (!_pr_initialized) _PR_ImplicitInitialization();

	/* See if library is already loaded */
	PR_EnterMonitor(pr_linker_lock);
  
	result = pr_UnlockedFindLibrary(fragmentName);
	if (result != NULL) goto unlock;

	newLib = PR_NEWZAP(PRLibrary);
	if (newLib == NULL) goto unlock;
	newLib->staticTable = NULL;


	/* Resolve an alias if this was one */
	err = ResolveAliasFile(&resolvedSpec, true, &isFolder, &wasAlias);
	if (err != noErr)
		goto unlock;

  if (isFolder)
  {
  	err = fnfErr;
  	goto unlock;
  }
    
	/* Finally, try to load the library */
	err = NSLoadNamedFragment(&resolvedSpec, fragmentName, &connectionID);
	if (err != noErr)
		goto unlock;

  newLib->name = strdup(fragmentName);
  newLib->dlh = connectionID;
  newLib->next = pr_loadmap;
  pr_loadmap = newLib;

  result = newLib;    /* success */
  PR_LOG(_pr_linker_lm, PR_LOG_MIN, ("Loaded library %s (load lib)", newLib->name));

unlock:
	if (result == NULL) {
		if (newLib != NULL)
			PR_DELETE(newLib);
		PR_SetError(PR_LOAD_LIBRARY_ERROR, _MD_ERRNO());
		DLLErrorInternal(_MD_ERRNO());  /* sets error text */
	}
	PR_ExitMonitor(pr_linker_lock);
	return result;
}


static PRLibrary*
pr_Mac_LoadIndexedFragment(const FSSpec *fileSpec, PRUint32 fragIndex)
{
	PRLibrary*					newLib = NULL;
	PRLibrary* 					result;
	FSSpec							resolvedSpec = *fileSpec;
	char*								fragmentName = NULL;
	UInt32              fragOffset, fragLength;                
	CFragConnectionID		connectionID = 0;
	Boolean							isFolder, wasAlias;
	OSErr								err = noErr;

	if (!_pr_initialized) _PR_ImplicitInitialization();

	/* See if library is already loaded */
	PR_EnterMonitor(pr_linker_lock);

	/* Resolve an alias if this was one */
	err = ResolveAliasFile(&resolvedSpec, true, &isFolder, &wasAlias);
	if (err != noErr)
		goto unlock;

  if (isFolder)
  {
  	err = fnfErr;
  	goto unlock;
  }
    err = GetIndexedFragmentOffsets(&resolvedSpec, fragIndex, &fragOffset, &fragLength, &fragmentName);
  if (err != noErr) goto unlock;
  
	result = pr_UnlockedFindLibrary(fragmentName);
	free(fragmentName);
	fragmentName = NULL;
	if (result != NULL) goto unlock;

	newLib = PR_NEWZAP(PRLibrary);
	if (newLib == NULL) goto unlock;
	newLib->staticTable = NULL;
    
	/* Finally, try to load the library */
	err = NSLoadIndexedFragment(&resolvedSpec, fragIndex, &fragmentName, &connectionID);
	if (err != noErr) {
		PR_DELETE(newLib);
		goto unlock;
	}

  newLib->name = fragmentName;			/* was malloced in NSLoadIndexedFragment */
  newLib->dlh = connectionID;
  newLib->next = pr_loadmap;
  pr_loadmap = newLib;

  result = newLib;    /* success */
  PR_LOG(_pr_linker_lm, PR_LOG_MIN, ("Loaded library %s (load lib)", newLib->name));

unlock:
	if (result == NULL) {
		if (newLib != NULL)
			PR_DELETE(newLib);
		PR_SetError(PR_LOAD_LIBRARY_ERROR, _MD_ERRNO());
		DLLErrorInternal(_MD_ERRNO());  /* sets error text */
	}
	PR_ExitMonitor(pr_linker_lock);
	return result;
}


#endif

/*
** Unload a shared library which was loaded via PR_LoadLibrary
*/
PR_IMPLEMENT(PRStatus) 
PR_UnloadLibrary(PRLibrary *lib)
{
    int result = 0;
    PRStatus status = PR_SUCCESS;

    if ((lib == 0) || (lib->refCount <= 0)) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    PR_EnterMonitor(pr_linker_lock);
    if (--lib->refCount > 0) {
    PR_LOG(_pr_linker_lm, PR_LOG_MIN,
           ("%s decr => %d",
        lib->name, lib->refCount));
    goto done;
    }

#ifdef XP_BEOS
    if(((image_id)lib->stub_dlh) == B_ERROR)
        unload_add_on( (image_id) lib->dlh );
    else
        unload_add_on( (image_id) lib->stub_dlh);
#endif

#ifdef XP_UNIX
#ifdef HAVE_DLL
#ifdef USE_DLFCN
    result = dlclose(lib->dlh);
#elif defined(USE_HPSHL)
    result = shl_unload(lib->dlh);
#elif defined(USE_MACH_DYLD)
    result = NSUnLinkModule(lib->dlh, FALSE);
#else
#error Configuration error
#endif
#endif /* HAVE_DLL */
#endif /* XP_UNIX */
#ifdef XP_PC
    if (lib->dlh) {
        FreeLibrary((HINSTANCE)(lib->dlh));
        lib->dlh = (HINSTANCE)NULL;
    }
#endif  /* XP_PC */

#if defined(XP_MAC) && TARGET_RT_MAC_CFM
    /* Close the connection */
#if TARGET_CARBON
    if (lib->bundle)
        CFRelease(lib->bundle);
    else
#endif
    CloseConnection(&(lib->dlh));
#endif

    /* unlink from library search list */
    if (pr_loadmap == lib)
        pr_loadmap = pr_loadmap->next;
    else if (pr_loadmap != NULL) {
        PRLibrary* prev = pr_loadmap;
        PRLibrary* next = pr_loadmap->next;
        while (next != NULL) {
            if (next == lib) {
                prev->next = next->next;
                goto freeLib;
            }
            prev = next;
            next = next->next;
        }
        /*
         * fail (the library is not on the _pr_loadmap list),
         * but don't wipe out an error from dlclose/shl_unload.
         */
        PR_ASSERT(!"_pr_loadmap and lib->refCount inconsistent");
        if (result == 0) {
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            status = PR_FAILURE;
        }
    }
    /*
     * We free the PRLibrary structure whether dlclose/shl_unload
     * succeeds or not.
     */

  freeLib:
    PR_LOG(_pr_linker_lm, PR_LOG_MIN, ("Unloaded library %s", lib->name));
    PR_DELETE(lib->name);
    PR_DELETE(lib);
    if (result == -1) {
        PR_SetError(PR_UNLOAD_LIBRARY_ERROR, _MD_ERRNO());
        DLLErrorInternal(_MD_ERRNO());
        status = PR_FAILURE;
    }

done:
    PR_ExitMonitor(pr_linker_lock);
    return status;
}

static void* 
pr_FindSymbolInLib(PRLibrary *lm, const char *name)
{
    void *f = NULL;

    if (lm->staticTable != NULL) {
    const PRStaticLinkTable* tp;
    for (tp = lm->staticTable; tp->name; tp++) {
        if (strcmp(name, tp->name) == 0) {
        return (void*) tp->fp;
        }
    }
        /* 
        ** If the symbol was not found in the static table then check if
        ** the symbol was exported in the DLL... Win16 only!!
        */
#if !defined(WIN16) && !defined(XP_BEOS)
        PR_SetError(PR_FIND_SYMBOL_ERROR, 0);
        return (void*)NULL;
#endif
    }
    
#ifdef XP_OS2
    DosQueryProcAddr(lm->dlh, 0, (PSZ) name, (PFN *) &f);
#endif  /* XP_OS2 */

#if defined(WIN32) || defined(WIN16)
    f = GetProcAddress(lm->dlh, name);
#endif  /* WIN32 || WIN16 */

#ifdef XP_MAC
#if TARGET_CARBON
    if (lm->bundle)
    {
        CFStringRef nameRef = CFStringCreateWithCString(NULL, name, kCFStringEncodingASCII);
        if (nameRef) {
            f = CFBundleGetFunctionPointerForName(lm->bundle, nameRef);
            CFRelease(nameRef);
        }
    }
    else
#endif
    {
        Ptr                 symAddr;
        CFragSymbolClass    symClass;
        Str255              pName;
            
        PStrFromCStr(name, pName);    
        
        f = (NSFindSymbol(lm->dlh, pName, &symAddr, &symClass) == noErr) ? symAddr : NULL;
        
        if (f == NULL && strcmp(name, "main") == 0) f = lm->main;
    }
#endif /* XP_MAC */

#ifdef XP_BEOS
    if( B_NO_ERROR != get_image_symbol( (image_id)lm->dlh, name, B_SYMBOL_TYPE_TEXT, &f ) ) {
        f = NULL;
    }
#endif

#ifdef XP_UNIX
#ifdef HAVE_DLL
#ifdef USE_DLFCN
    f = dlsym(lm->dlh, name);
#elif defined(USE_HPSHL)
    if (shl_findsym(&lm->dlh, name, TYPE_PROCEDURE, &f) == -1) {
        f = NULL;
    }
#elif defined(USE_MACH_DYLD)
    {
        NSSymbol symbol;
        symbol = NSLookupSymbolInModule(lm->dlh, name);
        if (symbol != NULL)
            f = NSAddressOfSymbol(symbol);
        else
            f = NULL;
    }
#endif
#endif /* HAVE_DLL */
#endif /* XP_UNIX */
    if (f == NULL) {
        PR_SetError(PR_FIND_SYMBOL_ERROR, _MD_ERRNO());
        DLLErrorInternal(_MD_ERRNO());
    }
    return f;
}

/*
** Called by class loader to resolve missing native's
*/
PR_IMPLEMENT(void*) 
PR_FindSymbol(PRLibrary *lib, const char *raw_name)
{
    void *f = NULL;
#if defined(NEED_LEADING_UNDERSCORE)
    char *name;
#else
    const char *name;
#endif
    /*
    ** Mangle the raw symbol name in any way that is platform specific.
    */
#if defined(NEED_LEADING_UNDERSCORE)
    /* Need a leading _ */
    name = PR_smprintf("_%s", raw_name);
#elif defined(AIX)
    /*
    ** AIX with the normal linker put's a "." in front of the symbol
    ** name.  When use "svcc" and "svld" then the "." disappears. Go
    ** figure.
    */
    name = raw_name;
#else
    name = raw_name;
#endif

    PR_EnterMonitor(pr_linker_lock);
    PR_ASSERT(lib != NULL);
    f = pr_FindSymbolInLib(lib, name);

#if defined(NEED_LEADING_UNDERSCORE)
    PR_smprintf_free(name);
#endif

    PR_ExitMonitor(pr_linker_lock);
    return f;
}

/*
** Return the address of the function 'raw_name' in the library 'lib'
*/
PR_IMPLEMENT(PRFuncPtr) 
PR_FindFunctionSymbol(PRLibrary *lib, const char *raw_name)
{
    return ((PRFuncPtr) PR_FindSymbol(lib, raw_name));
}

PR_IMPLEMENT(void*) 
PR_FindSymbolAndLibrary(const char *raw_name, PRLibrary* *lib)
{
    void *f = NULL;
#if defined(NEED_LEADING_UNDERSCORE)
    char *name;
#else
    const char *name;
#endif
    PRLibrary* lm;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    /*
    ** Mangle the raw symbol name in any way that is platform specific.
    */
#if defined(NEED_LEADING_UNDERSCORE)
    /* Need a leading _ */
    name = PR_smprintf("_%s", raw_name);
#elif defined(AIX)
    /*
    ** AIX with the normal linker put's a "." in front of the symbol
    ** name.  When use "svcc" and "svld" then the "." disappears. Go
    ** figure.
    */
    name = raw_name;
#else
    name = raw_name;
#endif

    PR_EnterMonitor(pr_linker_lock);

    /* search all libraries */
    for (lm = pr_loadmap; lm != NULL; lm = lm->next) {
        f = pr_FindSymbolInLib(lm, name);
        if (f != NULL) {
            *lib = lm;
            lm->refCount++;
            PR_LOG(_pr_linker_lm, PR_LOG_MIN,
                       ("%s incr => %d (for %s)",
                    lm->name, lm->refCount, name));
            break;
        }
    }
#if defined(NEED_LEADING_UNDERSCORE)
    PR_smprintf_free(name);
#endif

    PR_ExitMonitor(pr_linker_lock);
    return f;
}

PR_IMPLEMENT(PRFuncPtr) 
PR_FindFunctionSymbolAndLibrary(const char *raw_name, PRLibrary* *lib)
{
    return ((PRFuncPtr) PR_FindSymbolAndLibrary(raw_name, lib));
}

/*
** Add a static library to the list of loaded libraries. If LoadLibrary
** is called with the name then we will pretend it was already loaded
*/
PR_IMPLEMENT(PRLibrary*) 
PR_LoadStaticLibrary(const char *name, const PRStaticLinkTable *slt)
{
    PRLibrary *lm=NULL;
    PRLibrary* result = NULL;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    /* See if library is already loaded */
    PR_EnterMonitor(pr_linker_lock);

    /* If the lbrary is already loaded, then add the static table information... */
    result = pr_UnlockedFindLibrary(name);
    if (result != NULL) {
        PR_ASSERT( (result->staticTable == NULL) || (result->staticTable == slt) );
        result->staticTable = slt;
        goto unlock;
    }

    /* Add library to list...Mark it static */
    lm = PR_NEWZAP(PRLibrary);
    if (lm == NULL) goto unlock;

    lm->name = strdup(name);
    lm->refCount    = 1;
    lm->dlh         = pr_exe_loadmap ? pr_exe_loadmap->dlh : 0;
    lm->staticTable = slt;
    lm->next        = pr_loadmap;
    pr_loadmap      = lm;

    result = lm;    /* success */
    PR_ASSERT(lm->refCount == 1);
  unlock:
    PR_LOG(_pr_linker_lm, PR_LOG_MIN, ("Loaded library %s (static lib)", lm->name));
    PR_ExitMonitor(pr_linker_lock);
    return result;
}
