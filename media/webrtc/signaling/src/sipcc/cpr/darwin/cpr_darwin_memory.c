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

#include "cpr_types.h"
#include "cpr_debug.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include "cpr_memory.h"
#include "cpr_locks.h"
#include "plat_api.h"
#include <errno.h>
#include <sys/syslog.h>


/*-------------------------------------
 *
 * Structures
 *
 */

/**
 * @defgroup  Memory The Memory Management Module
 * @ingroup CPR
 * @brief The Memory management subsystem in CPR. The pSIPCC uses this subsystem for allocating and releasing memory.
 * @addtogroup MemoryStructures Linux memory structures (private)
 * @ingroup  Memory
 * @brief Internal structures for managing memory
 *
 * @{
 */

/**
 * Phone messaging system header
 *
 * Copied from phone.h to avoid the include pollution
 * MUST BE KEPT IN SYNC
 */
#define MISC_LN 8
typedef struct
{
    void *Next;
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
//  void *TempPtr;     
} phn_syshdr_t;

/** @} */ /* End private structures */


/*-------------------------------------
 *
 * Functions
 *
 */

/**
 * @addtogroup MemoryAPIs The memory related APIs 
 * @ingroup  Memory
 * @brief Memory routines
 *
 * @{
 */

/**
 * @brief Retrieve a buffer. 
 *
 * The cprGetBuffer function retrieves a single buffer from the heap. 
 * The buffer is guaranteed to be at least the size passed in, but it may be
 * larger. CPR does not clean the buffers when they are returned so it is highly
 * recommended that the application set all values in the bufferto some default
 * values before using it.
 *
 * @param[in] size          requested size of the buffer in bytes
 *
 * @return              Handle to a buffer or #NULL if the get failed
 *
 * @note                The buffer may be larger than the size requested.
 * @note                The buffer will come from the system's heap
 */
void *
cprGetBuffer (uint32_t size)
{
    static const char fname[] = "cprGetBuffer";
    cprLinuxBuffer_t *bufferPtr;
    char *charPtr;

    CPR_INFO("%s: Enter\n", fname);
    bufferPtr = (cprLinuxBuffer_t *) cpr_calloc(1, sizeof(cprLinuxBuffer_t) + size);
    if (bufferPtr != NULL) {
        bufferPtr->inUse = BUF_USED_FROM_HEAP;
        charPtr = (char *) (bufferPtr);
        return (charPtr + sizeof(cprLinuxBuffer_t));
    }

    CPR_ERROR("%s - Unable to malloc a buffer from the heap.\n", fname);
    errno = ENOMEM;
    return NULL;
}


/**
 * cprReleaseBuffer
 *
 * @brief Returns a buffer to the heap.
 * CPR keeps track of this information so the application only
 * needs to pass a pointer to the buffer to return.
 *
 * @param[in] bufferPtr  pointer to the buffer to return
 *
 * @return        none
 */
void
cprReleaseBuffer (void *bufferPtr)
{
    static const char fname[] = "cprReleaseBuffer";
    cprLinuxBuffer_t *linuxBufferPtr;
    char *charPtr;

    CPR_INFO("%s: Enter\n", fname);

    /*
     * Sanitize buffer pointer
     */
    if (bufferPtr == NULL) {
        CPR_ERROR("\n%s - Buffer pointer is NULL.\n", fname);
        return;
    }

    /*
     * Applications are only given a pointer to the buffer itself
     * not the Linux buffer structure. So backtrack in memory to get
     * the start of the header.
     */
    charPtr = (char *) (bufferPtr);
    linuxBufferPtr = (cprLinuxBuffer_t *) (charPtr - sizeof(cprLinuxBuffer_t));

    if (linuxBufferPtr->inUse == BUF_USED_FROM_HEAP) {
        cpr_free(linuxBufferPtr);
        return;
    }
    CPR_ERROR("%s: Buffer pointer 0x%08x: does not point to an in use buffer\n",
              fname, linuxBufferPtr);
    return;
}

/**
 * @brief Given a msg buffer, returns a pointer to the buffer's header
 *
 * The cprGetSysHeader function retrieves the system header buffer for the
 * passed in message buffer.
 *
 * @param[in] buffer  pointer to the buffer whose sysHdr to return
 *
 * @return        Abstract pointer to the msg buffer's system header
 *                or #NULL if failure
 */
void *
cprGetSysHeader (void *buffer)
{
    phn_syshdr_t *syshdr;

    /*
     * Stinks that an external structure is necessary,
     * but this is a side-effect of porting from IRX.
     */
    syshdr = cpr_calloc(1, sizeof(phn_syshdr_t));
    if (syshdr) {
        syshdr->Data = buffer;
    }
    return (void *)syshdr;
}

/**
 * @brief Called when the application is done with this system header
 *
 * The cprReleaseSysHeader function returns the system header buffer to the
 * system.
 * @param[in] syshdr  pointer to the sysHdr to be released
 *
 * @return        none
 */
void
cprReleaseSysHeader (void *syshdr)
{
    if (syshdr == NULL) {
        CPR_ERROR("cprReleaseSysHeader: Sys header pointer is NULL\n");
        return;
    }

    cpr_free(syshdr);
}

/** @} 
 *
 * @addtogroup MemoryPrivRoutines Linux memory routines (private)
 * @ingroup  Memory
 * @{
 */

/**
 * @brief An internal function to update the system header
 *
 * A CPR-only function. Given a sysHeader and data, this function fills
 * in the data.  The purpose for this function is to help prevent CPR
 * code from having to know about the phn_syshdr_t layout.
 *
 * @param[in] buffer    pointer to a syshdr from a successful call to
 *                  cprGetSysHeader
 * @param[in] cmd       command to place in the syshdr buffer
 * @param[in] len       length to place in the syshdr buffer
 * @param[in] timerMsg  msg being sent to the calling thread
 *
 * @return          none
 *
 * @pre (buffer != NULL)
 */
void
fillInSysHeader (void *buffer, uint16_t cmd, uint16_t len, void *timerMsg)
{
    phn_syshdr_t *syshdr;

    syshdr = (phn_syshdr_t *) buffer;
    syshdr->Cmd = cmd;
    syshdr->Len = len;
    syshdr->Usr.UsrPtr = timerMsg;
    return;
}

