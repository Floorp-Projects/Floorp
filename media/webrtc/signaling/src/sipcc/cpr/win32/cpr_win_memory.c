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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

#include "cpr.h"
#include "cpr_stdlib.h"
#include "cpr_win_defines.h"

/* debug instrumentation to track allocations by client code (sip/gsm) */
#ifdef TRACK_ALLOCS
uint32_t total = 0;

#define MAX_TRACKED 2048
typedef struct _mem_info
{
    void *ptr;
    int   size;
} mem_info_t;

mem_info_t memdata[MAX_TRACKED] = { {0, 0} };

void
record_ptr (void *ptr, int size)
{
    int i;

    for (i = 0; i < MAX_TRACKED; i++) {
        if (memdata[i].ptr == 0) {
            memdata[i].ptr = ptr;
            memdata[i].size = size;
            return;
        }
    }
    buginf("record_ptr():did not find a slot..\n");
    return;
}

int
get_ptr_size_and_free_slot (void *ptr)
{
    int i;
    int size = 0;

    for (i = 0; i < MAX_TRACKED; i++) {
        if (memdata[i].ptr == ptr) {
            memdata[i].ptr = 0;
            size = memdata[i].size;
            memdata[i].size = 0;
            break;
        }
    }
    if (size == 0) {
        buginf("get_ptr():did not find the ptr..\n");
    }
    return size;
}
#endif

/*
 * Copied from phone.h to avoid the #include pollution
 */
#define MISC_LN 8
typedef struct
{
    void    *Next;
    uint32_t Cmd;
    uint16_t RetID;
    uint16_t Port;
    uint16_t Len;
    uint8_t *Data;
    union {
        void *UsrPtr;
        uint32_t UsrInfo;
    } Usr;
    uint8_t Misc[MISC_LN];
    void *TempPtr;
} phn_syshdr_t;

char *strdup(const char *strSource);

#ifndef CPR_USE_DIRECT_OS_CALL
void *
cpr_malloc (size_t size)
{
#ifdef TRACK_ALLOCS
    char *ptr;

    total = total + size;
    buginf("cpr_malloc(): total=%d size=%d\n", total, size);
    ptr = calloc(1, size);
    record_ptr(ptr, size);
    return (ptr);
#else
    return calloc(1, size);
#endif
}

void *
cpr_calloc (size_t nelem, size_t size)
{
#ifdef TRACK_ALLOCS
    buginf("cpr_calloc(): called nelem=%d size=%d\n", nelem, size);
    return (cpr_malloc(nelem * size));
#else
    return calloc(nelem, size);
#endif
}

void *
cpr_realloc (void *object, size_t size)
{
    return realloc(object, size);
}

char *
cpr_strdup (const char *string)
{
    return strdup(string);
}

void
cpr_free (void *mem)
{
#ifdef TRACK_ALLOCS
    int sz;

    sz = get_ptr_size_and_free_slot(mem);
    if (total && sz) {
        total = total - sz;
    }

    buginf("cpr_free(): total=%d size=%d\n", total, sz);
#endif

    free(mem);
}
#endif



/*
 * cprCreateRegion
 *
 * Creates a buffer region. A region is just a collection
 * of pools.
 *
 * Parameters: regionName  - name of the region
 *
 * Return Value: Handle to the region or NULL if the init failed
 */
cprRegion_t
cprCreateRegion (const char *regionName)
{
    static uint32_t regionId = 0;
    uint32_t *region;

    /*
     * For now, this is nothing but a place holder;
     * all buffers will be malloced/freed on the fly.
     * Just providing a usable API for the applications.
     * We can add the fancy stuff in the next release.
     */
    region = cpr_malloc(sizeof(uint32_t));
    if (!region) {
        return NULL;
    }
    *region = ++regionId;

    return region;
}


/*
 * cprCreatePool
 *
 * Creates and populates a buffer pool
 *
 * Parameters: region         - region to place the pool in
 *             poolName       - name of the buffer pool
 *             initialBuffers - number of buffers to place in the pool
 *             bufferSize     - size of each buffer in bytes
 *
 * Return Value: Handle to the pool or NULL if the init failed
 */
cprPool_t
cprCreatePool (cprRegion_t region,
               const char *name,
               uint32_t initialBuffers,
               uint32_t bufferSize)
{
    static uint32_t poolId = 0;
    uint32_t *pool;

    /*
     * For now, this is nothing but a place holder;
     * all buffers will be malloced/freed on the fly.
     * Just providing a usable API for the applications.
     * We can add the fancy stuff in the next release.
     */
    if (!region) {
        return NULL;
    }

    pool = cpr_malloc(sizeof(cprPool_t));
    if (!pool) {
        return NULL;
    }
    *pool = ++poolId;

    return pool;
}


/**
 * cprDestroyRegion
 *
 * Frees all pools in a region and then destroys the region
 *
 * Parameters: region - The region to destroy
 *
 * Return Value: Success or failure indication
 */
cprRC_t
cprDestroyRegion (cprRegion_t region)
{
    if (region) {
        cpr_free(region);
    }

    return (CPR_SUCCESS);
}


/**
 * cprDestroyPool
 *
 * Frees all buffers in a pool and then destroys the pool
 *
 * Parameters: pool - The pool to destroy
 *
 * Return Value: Success or failure indication
 */
cprRC_t
cprDestroyPool (cprPool_t pool)
{
    if (pool) {
        cpr_free(pool);
    }

    return (CPR_SUCCESS);
}


/*
 * This function call is currently #defined as a call to cpr_calloc
 * in cpr_memory.h to allow better tracking of memory leaks.
 * #define cprGetBuffer(x,y)  cpr_calloc(1,y)
 *
 */

/**
 * cprGetBuffer
 *
 * Retrieve a buffer from a region
 *
 * Parameters: region - region from which to grab the buffer
 *             size   - requested size of the buffer in bytes
 *
 * Return Value: Handle to a buffer or NULL if the get failed
 */

/*
 *cprBuffer_t
 *cprGetBuffer(cprRegion_t pool,
 *            uint32_t size)
 *{
 *
 *   *
 *   * There is no spoon, er region so just
 *   * malloc the callee the largest buffer
 *   * the phone can handle.
 *   *
 *   return cpr_malloc(size);
 * }
*/

/**
 * cprReleaseBuffer
 *
 * Returns a buffer to the pool from which it was retrieved.
 * CPR keeps track of this information so the application only
 * needs to pass a pointer to the buffer to return.
 *
 * Parameters: buffer - buffer to return
 *
 * Return Value: Success or failure indication
 */
void
cprReleaseBuffer (cprBuffer_t buffer)
{

    /*
     * Just do a free since there are no pools.
     */
    if (buffer) {
        free(buffer);
    }
}

/**
 * fillInSysHeader
 *
 * A cpr-only function. Given a sysHeader and data, it fills
 * in the data This is to prevent CPR code from having to know
 * about the phn_syshdr_t layout.
 *
 * Parameters: buffer - pointer to a syshdr from a successful call to cprGetSysHeader
 *             cmd - command to place in the syshdr buffer
 *             len - length to place in the syshdr buffer
 *             timerMsg - msg being sent to calling thread
 *
 * Return Value: Pointer to filled out syshdr
 */
void *
fillInSysHeader (void *buffer, uint16_t cmd, uint16_t len, void *timerMsg)
{
    phn_syshdr_t *syshdr;

    syshdr = (phn_syshdr_t *) buffer;
    syshdr->Cmd = cmd;
    syshdr->Len = len;
    syshdr->Usr.UsrPtr = timerMsg;
    return buffer;
}

/**
 * cprGetSysHeader
 *
 * Obtain a system header (associated with sending messages
 * to other threads)
 *
 * Parameters: buffer - the associated message buffer (which
 *                      is necessary for IRX based system)
 *
 * Return Value: system header or NULL if a failure
 */
void *
cprGetSysHeader (void *buffer)
{
    phn_syshdr_t *syshdr;

    /*
     * Stinks that an external structure is necessary,
     * but this is a side-effect of porting from IRX.
     */
    syshdr = calloc(1, sizeof(phn_syshdr_t));
    if (!syshdr) {
        syshdr->Data = buffer;
    }
    return (void *) syshdr;
}

/**
 * cprReleaseSysHeader
 *
 * Release a system header (associated with sending messages
 * to other threads)
 *
 * Parameters: sysHdr - the system header
 *
 * Return Value: None
 */
void
cprReleaseSysHeader (void *sysHdr)
{
    if (sysHdr) {
        free(sysHdr);
    }
}

