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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.	Portions created by Netscape are 
 * Copyright (C) 2003 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.	If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * This file should be cvs removed and the function
 * freebl_GetLibraryFilePathname should be replaced by
 * PR_GetLibraryFilePathname when NSPR 4.3 is released.
 */

#ifdef LINUX
#define _GNU_SOURCE 1  /* for Dl_info and dladdr */
#endif

#include "prmem.h"
#include "prerror.h"

#include <stdio.h>
#include <errno.h>

#if defined(SOLARIS) || defined(LINUX) || defined(AIX) || defined(OSF1) \
    || (defined(HPUX) && defined(__LP64__))
#include <dlfcn.h>
#elif (defined(HPUX) && !defined(__LP64__))
#include <dl.h>
#elif defined(DARWIN)
#include <mach-o/dyld.h>
#elif defined(WIN32)
#include <windows.h>
#endif

#ifdef AIX
#include <sys/ldr.h>
#endif
#ifdef OSF1
#include <loader.h>
#include <rld_interface.h>
#endif

/*
 * Return the pathname of the file that the library "name" was loaded
 * from. "addr" is the address of a function defined in the library.
 *
 * The caller is responsible for freeing the result with PR_Free.
 */

char *
freebl_GetLibraryFilePathname(const char *name, void (*addr)())
{
#if defined(SOLARIS) || defined(LINUX)
    Dl_info dli;
    char *result;

    if (dladdr((void *)addr, &dli) == 0) {
        PR_SetError(PR_UNKNOWN_ERROR, errno);
        return NULL;
    }
    result = PR_Malloc(strlen(dli.dli_fname)+1);
    if (result != NULL) {
        strcpy(result, dli.dli_fname);
    }
    return result;
#elif defined(DARWIN)
    char *result;
    char *image_name;
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
    PR_SetError(PR_UNKNOWN_ERROR, 0);
    return NULL;
#elif defined(AIX)
    char *result;
#define LD_INFO_INCREMENT 64
    struct ld_info *info;
    unsigned int info_length = LD_INFO_INCREMENT * sizeof(struct ld_info);
    struct ld_info *infop;

    for (;;) {
        info = PR_Malloc(info_length);
        if (info == NULL) {
            return NULL;
        }
        /* If buffer is too small, loadquery fails with ENOMEM. */
        if (loadquery(L_GETINFO, info, info_length) != -1) {
            break;
        }
        PR_Free(info);
        if (errno != ENOMEM) {
            /* should not happen */
            PR_SetError(PR_UNKNOWN_ERROR, errno);
            return NULL;
        }
        /* retry with a larger buffer */
        info_length += LD_INFO_INCREMENT * sizeof(struct ld_info);
    }

    for (infop = info;
         ;
         infop = (struct ld_info *)((char *)infop + infop->ldinfo_next)) {
        if (strstr(infop->ldinfo_filename, name) != NULL) {
            result = PR_Malloc(strlen(infop->ldinfo_filename)+1);
            if (result != NULL) {
                strcpy(result, infop->ldinfo_filename);
            }
            break;
        }
        if (!infop->ldinfo_next) {
            PR_SetError(PR_UNKNOWN_ERROR, 0);
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
        PR_SetError(PR_UNKNOWN_ERROR, errno);
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
    PR_SetError(PR_UNKNOWN_ERROR, 0);
    return NULL;
#elif (defined(HPUX) && !defined(__LP64__))
    shl_t handle;
    struct shl_descriptor desc;
    char *result;

    handle = shl_load(name, DYNAMIC_PATH, 0L);
    if (handle == NULL) {
        PR_SetError(PR_UNKNOWN_ERROR, errno);
        return NULL;
    }
    if (shl_gethandle_r(handle, &desc) == -1) {
        /* should not happen */
        PR_SetError(PR_UNKNOWN_ERROR, errno);
        return NULL;
    }
    result = PR_Malloc(strlen(desc.filename)+1);
    if (result != NULL) {
        strcpy(result, desc.filename);
    }
    return result;
#elif (defined(HPUX) && defined(__LP64__))
    struct load_module_desc desc;
    char *result;
    const char *module_name;

    if (dlmodinfo((unsigned long)addr, &desc, sizeof desc, NULL, 0, 0) == 0) {
        PR_SetError(PR_UNKNOWN_ERROR, errno);
        return NULL;
    }
    module_name = dlgetname(&desc, sizeof desc, NULL, 0, 0);
    if (module_name == NULL) {
        /* should not happen */
        PR_SetError(PR_UNKNOWN_ERROR, errno);
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
        PR_SetError(PR_UNKNOWN_ERROR, GetLastError());
        return NULL;
    }
    if (GetModuleFileName(handle, module_name, sizeof module_name) == 0) {
        /* should not happen */
        PR_SetError(PR_UNKNOWN_ERROR, GetLastError());
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
