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

#ifndef _CPR_DARWIN_MEMORY_API_H_
#define _CPR_DARWIN_MEMORY_API_H_

/*-------------------------------------
 *
 * Macros
 *
 */

/**
 * Define internal calls to standard memory calls
 *
 */
#define MALLOC   malloc
#define CALLOC   calloc
#define REALLOC  realloc
#define FREE     free
/* if memalign is not supported then just use malloc */
#ifndef memalign
#define MEMALIGN(align, sz) malloc(sz)
#else
#define MEMALIGN(align, sz) memalign(align, sz)
#endif


/** The following defines are used to tune the total memory that pSIPCC
 * allocates and uses. */
/** Block size for emulated heap space, i.e. 1kB */
#define BLK_SZ  1024

/** 5 MB Heap Based on 0.5 MB initial use + 4 MB for calls (20K * 200) + 0.5 MB misc */
/** The number of supported blocks for the emulated heap space */
#define NUM_BLK 5120

/** Size of the emulated heap space. This is the value passed to the memory
 * management pre-init procedure. The total memory allocated is
 * "PRIVATE_SYS_MEM_SIZE + 2*64"  where the additional numbers are for a gaurd
 * band. */
#define PRIVATE_SYS_MEM_SIZE (NUM_BLK * BLK_SZ)

/**
 * Initialize memory management before anything is started,
 * i.e. this routine is called by cprPreInit.
 *
 * @param[in] size The total size of the memory (heap) to be allocated.
 *
 * @return TRUE or FALSE (FALSE on Failure)
 *
 * @warning This routine needs to be the @b first initialization call
 *          within CPR as even CPR layer initialization may require
 *          memory.
 *
 */
boolean cpr_memory_mgmt_pre_init(size_t size);

/**
 * The second stage of memory management initialization where
 * those items that can wait for everything else to initialize
 * are now done.  This typically is the binding of CLI commands.
 *
 * @return TRUE (if a failure could occur then FALSE would also
 *         be a possibility)
 *
 */
boolean cpr_memory_mgmt_post_init(void);

/**
 * Destroy those items created during initialization by memory management.
 *
 * @return none
 *
 * @warning This should be the @b last item called to be destroyed
 *          as all safety to memory calls will be gone.
 */
void cpr_memory_mgmt_destroy(void);

/**
 * Force a crash dump which will allow a stack trace to be generated
 *
 * @return none
 *
 * @note crash dump is created by an illegal write to memory
 */
void cpr_crashdump(void);


#endif
