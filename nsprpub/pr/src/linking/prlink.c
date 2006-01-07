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
 * Contributor(s): Steve Streeter (Hewlett-Packard Company)
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

#ifdef XP_MACOSX
#include <CodeFragments.h>
#include <TextUtils.h>
#include <Types.h>
#include <Aliases.h>
#include <CFURL.h>
#include <CFBundle.h>
#include <CFString.h>
#include <CFDictionary.h>
#include <CFData.h>
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
#ifdef AIX
#include <sys/ldr.h>
#endif
#ifdef OSF1
#include <loader.h>
#include <rld_interface.h>
#endif
#elif defined(USE_HPSHL)
#include <dl.h>
#elif defined(USE_MACH_DYLD)
#include <mach-o/dyld.h>
#endif
#endif /* XP_UNIX */

#define _PR_DEFAULT_LD_FLAGS PR_LD_LAZY

#ifdef VMS
/* These are all require for the PR_GetLibraryFilePathname implementation */
#include <descrip.h>
#include <dvidef.h>
#include <fibdef.h>
#include <iodef.h>
#include <lib$routines.h>
#include <ssdef.h>
#include <starlet.h>
#include <stsdef.h>
#include <unixlib.h>

#pragma __nostandard 
#pragma __member_alignment __save
#pragma __nomember_alignment
#ifdef __INITIAL_POINTER_SIZE
#pragma __required_pointer_size __save 
#pragma __required_pointer_size __short
#endif
 
typedef struct _imcb {
    struct _imcb *imcb$l_flink;         
    struct _imcb *imcb$l_blink;         
    unsigned short int imcb$w_size;     
    unsigned char imcb$b_type;          
    char imcb$b_resv_1;                 
    unsigned char imcb$b_access_mode;   
    unsigned char imcb$b_act_code;      
    unsigned short int imcb$w_chan;     
    unsigned int imcb$l_flags;		
    char imcb$t_image_name [40];        
    unsigned int imcb$l_symvec_size; 
    unsigned __int64 imcb$q_ident;
    void *imcb$l_starting_address;
    void *imcb$l_end_address;
} IMCB;
 
#pragma __member_alignment __restore
#ifdef __INITIAL_POINTER_SIZE 
#pragma __required_pointer_size __restore
#endif
#pragma __standard
 
typedef struct {
    short   buflen;
    short   itmcode;
    void    *buffer;
    void    *retlen;
} ITMLST;

typedef struct {
    short cond;
    short count;
    int   rest;
} IOSB;

typedef unsigned long int ulong_t;

struct _imcb *IAC$GL_IMAGE_LIST = NULL;

#define MAX_DEVNAM 64
#define MAX_FILNAM 255
#endif  /* VMS */

/*
 * On these platforms, symbols have a leading '_'.
 */
#if defined(SUNOS4) || defined(DARWIN) || defined(NEXTSTEP) \
    || defined(WIN16) || defined(XP_OS2) \
    || ((defined(OPENBSD) || defined(NETBSD)) && !defined(__ELF__))
#define NEED_LEADING_UNDERSCORE
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

#ifdef XP_MACOSX
    CFragConnectionID           connection;
    CFBundleRef                 bundle;
    Ptr                         main;
    CFMutableDictionaryRef      wrappers;
    const struct mach_header*   image;
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
    PRLibrary *lm = NULL;
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

    if (lm) {
        PR_LOG(_pr_linker_lm, PR_LOG_MIN,
            ("Loaded library %s (init)", lm->name));
    }

    PR_ExitMonitor(pr_linker_lock);
}

#if defined(WIN16)
/*
 * _PR_ShutdownLinker unloads all dlls loaded by the application via
 * calls to PR_LoadLibrary
 */
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
#else
/*
 * _PR_ShutdownLinker was originally only used on WIN16 (see above),
 * but I think it should also be used on other platforms.  However,
 * I disagree with the original implementation's unloading the dlls
 * for the application.  Any dlls that still remain on the pr_loadmap
 * list when NSPR shuts down are application programming errors.  The
 * only exception is pr_exe_loadmap, which was added to the list by
 * NSPR and hence should be cleaned up by NSPR.
 */
void _PR_ShutdownLinker(void)
{
    /* FIXME: pr_exe_loadmap should be destroyed. */
    
    PR_DestroyMonitor(pr_linker_lock);
    pr_linker_lock = NULL;

    if (_pr_currentLibPath) {
        free(_pr_currentLibPath);
        _pr_currentLibPath = NULL;
    }

#if !defined(USE_DLFCN) && !defined(HAVE_STRERROR)
    PR_DELETE(errStrBuf);
#endif
}
#endif

/******************************************************************************/

PR_IMPLEMENT(PRStatus) PR_SetLibraryPath(const char *path)
{
    PRStatus rv = PR_SUCCESS;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    PR_EnterMonitor(pr_linker_lock);
    if (_pr_currentLibPath) {
        free(_pr_currentLibPath);
    }
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
PR_GetLibraryPath(void)
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

    p = (char*) malloc(len);
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
        if (path) {
            fullname = PR_smprintf("%s\\%s%s", path, lib, PR_DLL_SUFFIX);
        } else {
            fullname = PR_smprintf("%s%s", lib, PR_DLL_SUFFIX);
        }
    } else {
        if (path) {
            fullname = PR_smprintf("%s\\%s", path, lib);
        } else {
            fullname = PR_smprintf("%s", lib);
        }
    }
#endif /* XP_PC */
#if defined(XP_UNIX) || defined(XP_BEOS)
    if (strstr(lib, PR_DLL_SUFFIX) == NULL)
    {
        if (path) {
            fullname = PR_smprintf("%s/lib%s%s", path, lib, PR_DLL_SUFFIX);
        } else {
            fullname = PR_smprintf("lib%s%s", lib, PR_DLL_SUFFIX);
        }
    } else {
        if (path) {
            fullname = PR_smprintf("%s/%s", path, lib);
        } else {
            fullname = PR_smprintf("%s", lib);
        }
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
#ifdef WIN32
        /* Windows DLL names are case insensitive... */
    if (strcmpi(np, cp) == 0) 
#elif defined(XP_OS2)
    if (stricmp(np, cp) == 0)
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

#if defined(USE_MACH_DYLD)
static NSModule
pr_LoadMachDyldModule(const char *name)
{
    NSObjectFileImage ofi;
    NSModule h = NULL;
    if (NSCreateObjectFileImageFromFile(name, &ofi)
            == NSObjectFileImageSuccess) {
        h = NSLinkModule(ofi, name, NSLINKMODULE_OPTION_PRIVATE
                         | NSLINKMODULE_OPTION_RETURN_ON_ERROR);
        /*
         * TODO: If NSLinkModule fails, use NSLinkEditError to retrieve
         * error information.
         */
        if (NSDestroyObjectFileImage(ofi) == FALSE) {
            if (h) {
                (void)NSUnLinkModule(h, NSUNLINKMODULE_OPTION_NONE);
                h = NULL;
            }
        }
    }
    return h;
}
#endif

#ifdef XP_MACOSX

/*
** macLibraryLoadProc is a function definition for a Mac shared library
** loading method. The "name" param is the same full or partial pathname
** that was passed to pr_LoadLibraryByPathName. The function must fill
** in the fields of "lm" which apply to its library type. Returns
** PR_SUCCESS if successful.
*/

typedef PRStatus (*macLibraryLoadProc)(const char *name, PRLibrary *lm);

#ifdef __ppc__

/*
** CFM and its TVectors only exist on PowerPC.  Other OS X architectures
** only use Mach-O as a native binary format.
*/

static void* TV2FP(CFMutableDictionaryRef dict, const char* name, void *tvp)
{
    static uint32 glue[6] = { 0x3D800000, 0x618C0000, 0x800C0000, 0x804C0004, 0x7C0903A6, 0x4E800420 };
    uint32* newGlue = NULL;

    if (tvp != NULL) {
        CFStringRef nameRef = CFStringCreateWithCString(NULL, name, kCFStringEncodingASCII);
        if (nameRef) {
            CFMutableDataRef glueData = (CFMutableDataRef) CFDictionaryGetValue(dict, nameRef);
            if (glueData == NULL) {
                glueData = CFDataCreateMutable(NULL, sizeof(glue));
                if (glueData != NULL) {
                    newGlue = (uint32*) CFDataGetMutableBytePtr(glueData);
                    memcpy(newGlue, glue, sizeof(glue));
                    newGlue[0] |= ((UInt32)tvp >> 16);
                    newGlue[1] |= ((UInt32)tvp & 0xFFFF);
                    MakeDataExecutable(newGlue, sizeof(glue));
                    CFDictionaryAddValue(dict, nameRef, glueData);
                    CFRelease(glueData);

                    PR_LOG(_pr_linker_lm, PR_LOG_MIN, ("TV2FP: created wrapper for CFM function %s().", name));
                }
            } else {
                PR_LOG(_pr_linker_lm, PR_LOG_MIN, ("TV2FP: found wrapper for CFM function %s().", name));

                newGlue = (uint32*) CFDataGetMutableBytePtr(glueData);
            }
            CFRelease(nameRef);
        }
    }
    
    return newGlue;
}

static PRStatus
pr_LoadViaCFM(const char *name, PRLibrary *lm)
{
    OSErr err;
    Str255 errName;
    FSRef ref;
    FSSpec fileSpec;
    Boolean tempUnusedBool;

    /*
     * Make an FSSpec from the path name and call GetDiskFragment.
     */

    /* Use direct conversion of POSIX path to FSRef to FSSpec. */
    err = FSPathMakeRef((const UInt8*)name, &ref, NULL);
    if (err != noErr)
        return PR_FAILURE;
    err = FSGetCatalogInfo(&ref, kFSCatInfoNone, NULL, NULL,
                           &fileSpec, NULL);
    if (err != noErr)
        return PR_FAILURE;

    /* Resolve an alias if this was one */
    err = ResolveAliasFile(&fileSpec, true, &tempUnusedBool,
                           &tempUnusedBool);
    if (err != noErr)
        return PR_FAILURE;

    /* Finally, try to load the library */
    err = GetDiskFragment(&fileSpec, 0, kCFragGoesToEOF, fileSpec.name,
                          kLoadCFrag, &lm->connection, &lm->main, errName);

    if (err == noErr && lm->connection) {
        /*
         * if we're a mach-o binary, need to wrap all CFM function
         * pointers. need a hash-table of already seen function
         * pointers, etc.
         */
        lm->wrappers = CFDictionaryCreateMutable(NULL, 16,
                       &kCFTypeDictionaryKeyCallBacks,
                       &kCFTypeDictionaryValueCallBacks);
        if (lm->wrappers) {
            lm->main = TV2FP(lm->wrappers, "main", lm->main);
        } else
            err = memFullErr;
    }
    return (err == noErr) ? PR_SUCCESS : PR_FAILURE;
}
#endif /* __ppc__ */

/*
** Creates a CFBundleRef if the pathname refers to a Mac OS X bundle
** directory. The caller is responsible for calling CFRelease() to
** deallocate.
*/

static PRStatus
pr_LoadCFBundle(const char *name, PRLibrary *lm)
{
    CFURLRef bundleURL;
    CFBundleRef bundle = NULL;
    char pathBuf[PATH_MAX];
    const char *resolvedPath;
    CFStringRef pathRef;

    /* Takes care of relative paths and symlinks */
    resolvedPath = realpath(name, pathBuf);
    if (!resolvedPath)
        return PR_FAILURE;
        
    pathRef = CFStringCreateWithCString(NULL, pathBuf, kCFStringEncodingUTF8);
    if (pathRef) {
        bundleURL = CFURLCreateWithFileSystemPath(NULL, pathRef,
                                                  kCFURLPOSIXPathStyle, true);
        if (bundleURL) {
            bundle = CFBundleCreate(NULL, bundleURL);
            CFRelease(bundleURL);
        }
        CFRelease(pathRef);
    }

    lm->bundle = bundle;
    return (bundle != NULL) ? PR_SUCCESS : PR_FAILURE;
}

static PRStatus
pr_LoadViaDyld(const char *name, PRLibrary *lm)
{
    lm->dlh = pr_LoadMachDyldModule(name);
    if (lm->dlh == NULL) {
        lm->image = NSAddImage(name, NSADDIMAGE_OPTION_RETURN_ON_ERROR
                               | NSADDIMAGE_OPTION_WITH_SEARCHING);
        /*
         * TODO: If NSAddImage fails, use NSLinkEditError to retrieve
         * error information.
         */
    }
    return (lm->dlh != NULL || lm->image != NULL) ? PR_SUCCESS : PR_FAILURE;
}

#endif /* XP_MACOSX */

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
    }
#endif /* WIN32 || WIN16 */

#ifdef XP_MACOSX
    {
    int     i;
    PRStatus status;

    static const macLibraryLoadProc loadProcs[] = {
#ifdef __ppc__
        pr_LoadViaDyld, pr_LoadCFBundle, pr_LoadViaCFM
#else  /* __ppc__ */
        pr_LoadViaDyld, pr_LoadCFBundle
#endif /* __ppc__ */
    };

    for (i = 0; i < sizeof(loadProcs) / sizeof(loadProcs[0]); i++) {
        if ((status = loadProcs[i](name, lm)) == PR_SUCCESS)
            break;
    }
    if (status != PR_SUCCESS) {
        oserr = cfragNoLibraryErr;
        PR_DELETE(lm);
        goto unlock;        
    }
    lm->name = strdup(name);
    lm->next = pr_loadmap;
    pr_loadmap = lm;
    }
#endif

#if defined(XP_UNIX) && !defined(XP_MACOSX)
#ifdef HAVE_DLL
    {
#if defined(USE_DLFCN)
#ifdef NTO
    /* Neutrino needs RTLD_GROUP to load Netscape plugins. (bug 71179) */
    int dl_flags = RTLD_GROUP;
#elif defined(AIX)
    /* AIX needs RTLD_MEMBER to load an archive member.  (bug 228899) */
    int dl_flags = RTLD_MEMBER;
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
    NSModule h = pr_LoadMachDyldModule(name);
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
                    const char *endOfSystemName = strrchr(info.name, '/');
                    const char *endOfPassedName = strrchr(name, '/');
                    if( 0 == endOfSystemName ) 
                        endOfSystemName = info.name;
                    else
                        endOfSystemName++;
                    if( 0 == endOfPassedName )
                        endOfPassedName = name;
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
    result = NSUnLinkModule(lib->dlh, NSUNLINKMODULE_OPTION_NONE) ? 0 : -1;
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

#ifdef XP_MACOSX
    /* Close the connection */
    if (lib->connection)
        CloseConnection(&(lib->connection));
    if (lib->bundle)
        CFRelease(lib->bundle);
    if (lib->wrappers)
        CFRelease(lib->wrappers);
    /* No way to unload an image (lib->image) */
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
    free(lib->name);
    lib->name = NULL;
    PR_DELETE(lib);
    if (result != 0) {
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
#ifdef XP_OS2
    int rc;
#endif

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
    rc = DosQueryProcAddr(lm->dlh, 0, (PSZ) name, (PFN *) &f);
#if defined(NEED_LEADING_UNDERSCORE)
    /*
     * Older plugins (not built using GCC) will have symbols that are not
     * underscore prefixed.  We check for that here.
     */
    if (rc != NO_ERROR) {
        name++;
        DosQueryProcAddr(lm->dlh, 0, (PSZ) name, (PFN *) &f);
    }
#endif
#endif  /* XP_OS2 */

#if defined(WIN32) || defined(WIN16)
    f = GetProcAddress(lm->dlh, name);
#endif  /* WIN32 || WIN16 */

#ifdef XP_MACOSX
/* add this offset to skip the leading underscore in name */
#define SYM_OFFSET 1
    if (lm->bundle) {
        CFStringRef nameRef = CFStringCreateWithCString(NULL, name + SYM_OFFSET, kCFStringEncodingASCII);
        if (nameRef) {
            f = CFBundleGetFunctionPointerForName(lm->bundle, nameRef);
            CFRelease(nameRef);
        }
    }
    if (lm->connection) {
        Ptr                 symAddr;
        CFragSymbolClass    symClass;
        Str255              pName;
        
        PR_LOG(_pr_linker_lm, PR_LOG_MIN, ("Looking up symbol: %s", name + SYM_OFFSET));
        
        c2pstrcpy(pName, name + SYM_OFFSET);
        
        f = (FindSymbol(lm->connection, pName, &symAddr, &symClass) == noErr) ? symAddr : NULL;
        
#ifdef __ppc__
        /* callers expect mach-o function pointers, so must wrap tvectors with glue. */
        if (f && symClass == kTVectorCFragSymbol) {
            f = TV2FP(lm->wrappers, name + SYM_OFFSET, f);
        }
#endif /* __ppc__ */
        
        if (f == NULL && strcmp(name + SYM_OFFSET, "main") == 0) f = lm->main;
    }
    if (lm->image) {
        NSSymbol symbol;
        symbol = NSLookupSymbolInImage(lm->image, name,
                 NSLOOKUPSYMBOLINIMAGE_OPTION_BIND
                 | NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
        if (symbol != NULL)
            f = NSAddressOfSymbol(symbol);
        else
            f = NULL;
    }
#undef SYM_OFFSET
#endif /* XP_MACOSX */

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
    if (lm->dlh) {
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

PR_IMPLEMENT(char *)
PR_GetLibraryFilePathname(const char *name, PRFuncPtr addr)
{
#if defined(USE_DLFCN) && (defined(SOLARIS) || defined(FREEBSD) \
        || defined(LINUX) || defined(__GNU__) || defined(__GLIBC__))
    Dl_info dli;
    char *result;

    if (dladdr((void *)addr, &dli) == 0) {
        PR_SetError(PR_LIBRARY_NOT_LOADED_ERROR, _MD_ERRNO());
        DLLErrorInternal(_MD_ERRNO());
        return NULL;
    }
    result = PR_Malloc(strlen(dli.dli_fname)+1);
    if (result != NULL) {
        strcpy(result, dli.dli_fname);
    }
    return result;
#elif defined(USE_MACH_DYLD)
    char *result;
    const char *image_name;
    int i, count = _dyld_image_count();

    for (i = 0; i < count; i++) {
        image_name = _dyld_get_image_name(i);
        if (strstr(image_name, name) != NULL) {
            result = PR_Malloc(strlen(image_name)+1);
            if (result != NULL) {
                strcpy(result, image_name);
            }
            return result;
        }
    }
    PR_SetError(PR_LIBRARY_NOT_LOADED_ERROR, 0);
    return NULL;
#elif defined(AIX)
    char *result;
#define LD_INFO_INCREMENT 64
    struct ld_info *info;
    unsigned int info_length = LD_INFO_INCREMENT * sizeof(struct ld_info);
    struct ld_info *infop;
    int loadflags = L_GETINFO | L_IGNOREUNLOAD;

    for (;;) {
        info = PR_Malloc(info_length);
        if (info == NULL) {
            return NULL;
        }
        /* If buffer is too small, loadquery fails with ENOMEM. */
        if (loadquery(loadflags, info, info_length) != -1) {
            break;
        }
        /*
         * Calling loadquery when compiled for 64-bit with the
         * L_IGNOREUNLOAD flag can cause an invalid argument error
         * on AIX 5.1. Detect this error the first time that
         * loadquery is called, and try calling it again without
         * this flag set.
         */
        if (errno == EINVAL && (loadflags & L_IGNOREUNLOAD)) {
            loadflags &= ~L_IGNOREUNLOAD;
            if (loadquery(loadflags, info, info_length) != -1) {
                break;
            }
        }
        PR_Free(info);
        if (errno != ENOMEM) {
            /* should not happen */
            _PR_MD_MAP_DEFAULT_ERROR(_MD_ERRNO());
            return NULL;
        }
        /* retry with a larger buffer */
        info_length += LD_INFO_INCREMENT * sizeof(struct ld_info);
    }

    for (infop = info;
         ;
         infop = (struct ld_info *)((char *)infop + infop->ldinfo_next)) {
        unsigned long start = (unsigned long)infop->ldinfo_dataorg;
        unsigned long end = start + infop->ldinfo_datasize;
        if (start <= (unsigned long)addr && end > (unsigned long)addr) {
            result = PR_Malloc(strlen(infop->ldinfo_filename)+1);
            if (result != NULL) {
                strcpy(result, infop->ldinfo_filename);
            }
            break;
        }
        if (!infop->ldinfo_next) {
            PR_SetError(PR_LIBRARY_NOT_LOADED_ERROR, 0);
            result = NULL;
            break;
        }
    }
    PR_Free(info);
    return result;
#elif defined(OSF1)
    /* Contributed by Steve Streeter of HP */
    ldr_process_t process, ldr_my_process();
    ldr_module_t mod_id;
    ldr_module_info_t info;
    ldr_region_t regno;
    ldr_region_info_t reginfo;
    size_t retsize;
    int rv;
    char *result;

    /* Get process for which dynamic modules will be listed */

    process = ldr_my_process();

    /* Attach to process */

    rv = ldr_xattach(process);
    if (rv) {
        /* should not happen */
        _PR_MD_MAP_DEFAULT_ERROR(_MD_ERRNO());
        return NULL;
    }

    /* Print information for list of modules */

    mod_id = LDR_NULL_MODULE;

    for (;;) {

        /* Get information for the next module in the module list. */

        ldr_next_module(process, &mod_id);
        if (ldr_inq_module(process, mod_id, &info, sizeof(info),
                           &retsize) != 0) {
            /* No more modules */
            break;
        }
        if (retsize < sizeof(info)) {
            continue;
        }

        /*
         * Get information for each region in the module and check if any
         * contain the address of this function.
         */

        for (regno = 0; ; regno++) {
            if (ldr_inq_region(process, mod_id, regno, &reginfo,
                               sizeof(reginfo), &retsize) != 0) {
                /* No more regions */
                break;
            }
            if (((unsigned long)reginfo.lri_mapaddr <=
                (unsigned long)addr) &&
                (((unsigned long)reginfo.lri_mapaddr + reginfo.lri_size) >
                (unsigned long)addr)) {
                /* Found it. */
                result = PR_Malloc(strlen(info.lmi_name)+1);
                if (result != NULL) {
                    strcpy(result, info.lmi_name);
                }
                return result;
            }
        }
    }
    PR_SetError(PR_LIBRARY_NOT_LOADED_ERROR, 0);
    return NULL;
#elif defined(VMS)
    /* Contributed by Colin Blake of HP */
    struct _imcb	*icb;
    ulong_t 		status;
    char                device_name[MAX_DEVNAM];
    int                 device_name_len;
    $DESCRIPTOR         (device_name_desc, device_name);
    struct fibdef	fib;
    struct dsc$descriptor_s fib_desc = 
	{ sizeof(struct fibdef), DSC$K_DTYPE_Z, DSC$K_CLASS_S, (char *)&fib } ;
    IOSB		iosb;
    ITMLST		devlst[2] = {
            		{MAX_DEVNAM, DVI$_ALLDEVNAM, device_name, &device_name_len},
            		{0,0,0,0}};
    short               file_name_len;
    char                file_name[MAX_FILNAM+1];
    char		*result = NULL;
    struct dsc$descriptor_s file_name_desc = 
	{ MAX_FILNAM, DSC$K_DTYPE_T, DSC$K_CLASS_S, (char *) &file_name[0] } ;

    /*
    ** The address for the process image list could change in future versions
    ** of the operating system. 7FFD0688 is valid for V7.2 and V7.3 releases,
    ** so we use that for the default, but allow an environment variable
    ** (logical name) to override.
    */
    if (IAC$GL_IMAGE_LIST == NULL) {
        char *p = getenv("MOZILLA_IAC_GL_IMAGE_LIST");
        if (p)
            IAC$GL_IMAGE_LIST = (struct _imcb *) strtol(p,NULL,0);
        else
            IAC$GL_IMAGE_LIST = (struct _imcb *) 0x7FFD0688;
    }

    for (icb = IAC$GL_IMAGE_LIST->imcb$l_flink;
         icb != IAC$GL_IMAGE_LIST;
         icb = icb->imcb$l_flink) {
        if (((void *)addr >= icb->imcb$l_starting_address) && 
	    ((void *)addr <= icb->imcb$l_end_address)) {
	    /*
	    ** This is the correct image.
	    ** Get the device name.
	    */
	    status = sys$getdviw(0,icb->imcb$w_chan,0,&devlst,0,0,0,0);
	    if ($VMS_STATUS_SUCCESS(status))
		device_name_desc.dsc$w_length = device_name_len;

	    /*
	    ** Get the FID.
	    */
	    memset(&fib,0,sizeof(struct fibdef));
	    status = sys$qiow(0,icb->imcb$w_chan,IO$_ACCESS,&iosb,
                		0,0,&fib_desc,0,0,0,0,0);

	    /*
	    ** If we got the FID, now look up its name (if for some reason
	    ** we didn't get the device name, this call will fail).
	    */
	    if (($VMS_STATUS_SUCCESS(status)) && ($VMS_STATUS_SUCCESS(iosb.cond))) {
		status = lib$fid_to_name (
                    &device_name_desc,
                    &fib.fib$w_fid,
                    &file_name_desc,
                    &file_name_len,
                    0, 0);

		/*
		** If we succeeded then remove the version number and
		** return a copy of the UNIX format version of the file name.
		*/
		if ($VMS_STATUS_SUCCESS(status)) {
		    char *p, *result;
		    file_name[file_name_len] = 0;
		    p = strrchr(file_name,';');
		    if (p) *p = 0;
		    p = decc$translate_vms(&file_name[0]);
		    result = PR_Malloc(strlen(p)+1);
		    if (result != NULL) {
			strcpy(result, p);
		    }
		    return result;
		}
            }
	}
    }

    /* Didn't find it */
    PR_SetError(PR_LIBRARY_NOT_LOADED_ERROR, 0);
    return NULL;

#elif defined(HPUX) && defined(USE_HPSHL)
    int index;
    struct shl_descriptor desc;
    char *result;

    for (index = 0; shl_get_r(index, &desc) == 0; index++) {
        if (strstr(desc.filename, name) != NULL) {
            result = PR_Malloc(strlen(desc.filename)+1);
            if (result != NULL) {
                strcpy(result, desc.filename);
            }
            return result;
        }
    }
    /*
     * Since the index value of a library is decremented if
     * a library preceding it in the shared library search
     * list was unloaded, it is possible that we missed some
     * libraries as we went up the list.  So we should go
     * down the list to be sure that we not miss anything.
     */
    for (index--; index >= 0; index--) {
        if ((shl_get_r(index, &desc) == 0)
                && (strstr(desc.filename, name) != NULL)) {
            result = PR_Malloc(strlen(desc.filename)+1);
            if (result != NULL) {
                strcpy(result, desc.filename);
            }
            return result;
        }
    }
    PR_SetError(PR_LIBRARY_NOT_LOADED_ERROR, 0);
    return NULL;
#elif defined(HPUX) && defined(USE_DLFCN)
    struct load_module_desc desc;
    char *result;
    const char *module_name;

    if (dlmodinfo((unsigned long)addr, &desc, sizeof desc, NULL, 0, 0) == 0) {
        PR_SetError(PR_LIBRARY_NOT_LOADED_ERROR, _MD_ERRNO());
        DLLErrorInternal(_MD_ERRNO());
        return NULL;
    }
    module_name = dlgetname(&desc, sizeof desc, NULL, 0, 0);
    if (module_name == NULL) {
        /* should not happen */
        _PR_MD_MAP_DEFAULT_ERROR(_MD_ERRNO());
        DLLErrorInternal(_MD_ERRNO());
        return NULL;
    }
    result = PR_Malloc(strlen(module_name)+1);
    if (result != NULL) {
        strcpy(result, module_name);
    }
    return result;
#elif defined(WIN32)
    HMODULE handle;
    char module_name[MAX_PATH];
    char *result;

    handle = GetModuleHandle(name);
    if (handle == NULL) {
        PR_SetError(PR_LIBRARY_NOT_LOADED_ERROR, _MD_ERRNO());
        DLLErrorInternal(_MD_ERRNO());
        return NULL;
    }
    if (GetModuleFileName(handle, module_name, sizeof module_name) == 0) {
        /* should not happen */
        _PR_MD_MAP_DEFAULT_ERROR(_MD_ERRNO());
        return NULL;
    }
    result = PR_Malloc(strlen(module_name)+1);
    if (result != NULL) {
        strcpy(result, module_name);
    }
    return result;
#elif defined(XP_OS2)
    HMODULE module = NULL;
    char module_name[_MAX_PATH];
    char *result;
    APIRET ulrc = DosQueryModFromEIP(&module, NULL, 0, NULL, NULL, (ULONG) addr);
    if ((NO_ERROR != ulrc) || (NULL == module) ) {
        PR_SetError(PR_LIBRARY_NOT_LOADED_ERROR, _MD_ERRNO());
        DLLErrorInternal(_MD_ERRNO());
        return NULL;
    }
    ulrc = DosQueryModuleName(module, sizeof module_name, module_name);
    if (NO_ERROR != ulrc) {
        /* should not happen */
        _PR_MD_MAP_DEFAULT_ERROR(_MD_ERRNO());
        return NULL;
    }
    result = PR_Malloc(strlen(module_name)+1);
    if (result != NULL) {
        strcpy(result, module_name);
    }
    return result;
#else
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
#endif
}
