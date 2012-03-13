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

/**
 * @brief The generic fixed memory chunk manager.
 *
 * A generic, fixed memory chunk manager.  This code comes from
 * the IOS hawaii branch and has been cut-down to fit the needs
 * of Linux.
 *
 * @author Scott Mackie, April 1994
 * @author Babu Panuganti, September 2001 (common chunk functions)
 *
 * A brief description of the Chunk Manager
 *
 * This is a hack. Plain and simple. It's a neat hack, but it's still
 * a hack. What this code does is allocate a big chunk of memory and
 * carve it up into smaller chunks. How's this different from the
 * memory manager we already have and malloc/free? Well, the main
 * difference is that the memory overhead is only paid for by the
 * big chunk, not for the little ones. This saves about 4+ bytes per
 * element you normally allocate via malloc. Another difference is that
 * there are no interrupt restrictions on this code so you can use
 * chunk_malloc and chunk_free at interrupt level (of course, the
 * original allocation has to happen at process level and the chunk
 * can't grow at interrupt level).
 *
 * This code makes managing huge (>500 elements) lists of tiny structures
 * (i.e. under 24 bytes) more efficient.
 *
 * It's Chunkarific! ;-)
 *
 * @par Caveats
 *
 * @li This code doesn't have the frills of malloc()/free(). It's not meant
 *   to. You can't mem_lock() elements. The protection against screwing up
 *   isn't anywhere near a full API. This code is meant for a narrow range
 *   of applications.
 *
 * @li Dynamic growing of pools won't work from interrupt level. If you
 *   have to ask "why?" here, maybe you should go and read os/free.c
 *   (and os/buffers.c) before delving further.
 *
 * @li It's possible to use this manager in a way that'll make matters
 *   worse. Much worse. Keep the maximum sizes in inverse proportion
 *   with the size of the structure. Use the "saved" count in "show
 *   chunk" to help you.
 *
 *
 * @par chunk_malloc and chunk_free:
 *
 * To make chunk_malloc more efficient under a long sibling linked list,
 * newly created sibling is inserts right after head chunk so that
 * chunk_malloc can immediately find the sibling chunk with free chunks
 *
 * The chunk linked list is:
 *
 * @verbatim
   head_chunk <-> sibling_N <--> sibling_N-1 <->..<--> sibling_1  --> NULL
                  ^ last created sibling               ^ first created sibling
   @endverbatim
 *
 * chunk_malloc starts from head_chunk, sibling_N, sibling_N-1, down to
 * sibling_1 chunk_free starts from head_chunk, sibling_1, sibling_2, up to
 * sibling_N
 *
 * This is done via a union field, p, in chunk_t. In head chunk, this field
 * points to tail of sibling list while points to head chunk in each
 * sibling chunk

 * A total_free field is added to chunk_t data structure to allow
 * chunk_malloc know whether there is any chunk available in the long sibling
 * linked list
 *
 * @par Headers
 * The header will be used for fast freeing and has a overhead of 4 bytes
 * per chunk element. It provides only the fast freeing during chunk_free().
 * This is very good for applications who don't want minimize the overhead
 * per chunk element and speed up the chunk_free().
 *
 * @par 64k upper bound for chunk blocks
 * The size of each sibling and the headchunk block of *all* DYNAMIC chunk
 * pools will be limited to 64k.
 *
 * @par First block implications
 * The memory allocation requests for *all* DYNAMIC chunk blocks will be
 * satisfied with the first block present in the freelist. This means that
 * the block size will not be equal to the requested size. So, the user
 * application cannot assume that the block will have 'x' number of elements
 * as requested during chunk_create(size, x, ...). To find this out, the
 * chunk user can use another api.
 */
#include "cpr_types.h"
#include "cpr_linux_align.h"
#include "cpr_linux_chunk.h"
#include "cpr_assert.h"
#include "cpr_debug.h"
#include "cpr_locks.h"
#include "cpr_stdlib.h"
#include "cpr_linux_memory_api.h"

#ifndef MALLOC
#error "Need MALLOC to be defined"
#endif

/*
 * Uncomment the following line to allow for debugging the chunk manager
 * Note that this will be very chatty if used in a full phone image
 */
//define CPR_CHUNK_DEBUG

#ifdef CPR_CHUNK_DEBUG
#define CHUNK_DEBUG if (cprInfo) buginf
#else
#define CHUNK_DEBUG(fmt, arg)
#endif

#define CHUNK_PRIVATE_MEMPOOL_ARG
#define CHUNK_PRIVATE_MEMPOOL_VAR
#define CHUNK_PRIVATE_MEMPOOL_ARG_REF(chunk)
#define CHUNK_PRIVATE_MEMPOOL_ASSIGN(chunk)


/*-------------------------------------
 *
 * Constants
 *
 */
/** Maximum number of bytes poisoned in a chunk element/node */
#define MAX_CHUNK_POISON  0x100

/** Required memory alignment for chunk node element */
#define MEM_ALIGN_BYTES 0x2


/*-------------------------------------
 *
 * Macros
 *
 */
/** Maximum number of bytes poisoned in a chunk element/node */
#define MEMASSERT(x)  cpr_assert_debug(((int)x & (MEM_ALIGN_BYTES - 1)) == 0)
/* This macro returns the caller_pc of the function */
#define CHUNK_CALLER_PC __builtin_return_address(0)

/*-------------------------------------
 *
 * Local variables
 *
 */
static boolean chunks_initialized = FALSE;
static uint32_t chunks_created = 0;
static uint32_t chunks_destroyed = 0;
static uint32_t chunk_siblings_created = 0;
static uint32_t chunk_siblings_trimmed = 0;
static boolean  chunk_siblings_stop_shuffling = FALSE;
static cprMutex_t chunk_mutex;


/*-------------------------------------
 *
 * Forward declarations
 *
 */
static INLINE chunk_t *
chunk_create_inline(uint32_t cfg_size, uint32_t cfg_max, uint32_t flags,
                    uint32_t alignment,
                    CHUNK_PRIVATE_MEMPOOL_ARG
                    const char *name, void *caller_pc);


/*-------------------------------------
 *
 * Functions
 *
 */

/*
 * Inlines
 */
static INLINE boolean
chunk_is_dynamic (chunk_t *chunk)
{
    return (boolean)(chunk->flags & CHUNK_FLAGS_DYNAMIC);
}

static INLINE boolean
chunk_is_sibling (chunk_t *chunk)
{
    return (boolean)(chunk->flags & CHUNK_FLAGS_SIBLING);
}

static INLINE boolean
chunk_is_independent (chunk_t *chunk)
{
    return (boolean)(chunk->flags & CHUNK_FLAGS_INDEPENDENT);
}

static INLINE boolean
chunk_is_resident (chunk_t *chunk)
{
    return (boolean)(chunk->flags & CHUNK_FLAGS_RESIDENT);
}

static INLINE boolean
chunk_is_unzeroed (chunk_t *chunk)
{
    return (boolean)(chunk->flags & CHUNK_FLAGS_UNZEROED);
}

boolean
chunk_did_malloc_fail (chunk_t *chunk)
{
    return (boolean)(chunk->flags & CHUNK_FLAGS_MALLOCFAILED);
}


/**
 * @addtogroup ChunkAPIs Linux chunk memory routines
 * @ingroup  Memory
 * @brief Chunk memory routines
 *
 * @see cpr_linux_chunk.c Detailed Description for general description
 *
 * @{
 */

/**
 * Initialize the chunk manager variables and queue
 *
 * @return #TRUE or #FALSE
 */
boolean
chunk_init (void)
{
    if (chunks_initialized) {
        return TRUE;
    }

    /*
     * Create mutex
     */
    chunk_mutex = cprCreateMutex("CPR chunk mutex");
    if (!chunk_mutex) {
        CPR_ERROR("ERROR: Unable to create CPR chunk mutex\n");
        return FALSE;
    }

    /*
     * Init some counters
     */
    chunks_created         = 0;
    chunks_destroyed       = 0;
    chunk_siblings_created = 0;
    chunk_siblings_trimmed = 0;

    /*
     * make chunk_init reentrant
     */
    chunks_initialized = TRUE;

    return TRUE;
}

/**
 * Destroy chunk manager, complete shutdown event
 *
 * @return none
 */
void
chunk_exit (void)
{
    /* Destroy mutex */
    (void) cprDestroyMutex(chunk_mutex);

    chunks_initialized = FALSE;
}


/**
 * Calculate the size of the chunk taking into consideration of any
 * data header as specified by the application.
 *
 * @param size          TBD
 * @param alignment     TBD
 * @param flags         TBD
 *
 * @return TBD
 *
 * @post ($rc > 0)
 */
static INLINE uint32_t
chunk_calculate_size (uint32_t size, uint32_t alignment, uint32_t flags)
{
    uint32_t overhead;

#ifdef CPR_CHUNK_DEBUG
    CHUNK_DEBUG("chunk_calculate_size(%u, %u, 0x%u)\n", size, alignment, flags);
#endif

    if (!alignment) {
        alignment = CHUNK_ALIGN_BYTES;
    }
    alignment = ALIGN_UP(alignment, CHUNK_ALIGN_BYTES);

    /* Align the header to the requested alignment */
    overhead = FIELDOFFSET(chunk_header_t, data_area.data);
    overhead = ALIGN_UP(overhead, alignment);

    /*
     * chunk data will keep free_chunk_t info when it is freed
     * free_chunk_t is 8 bytes long.
     */
    if (size < sizeof(free_chunk_t)) {
        size = sizeof(free_chunk_t);
    }
    size = ALIGN_UP(size, alignment);

    CHUNK_DEBUG("chunk_calculate_size yields %u\n", (size + overhead));

    return (size + overhead);
}

#ifdef NEED_TO_ADJUST_MEMSIZE
/**
 * Determine the proper size based on the requested chunk memory size
 *
 * @param chunk         the chunk descriptor
 * @param overhead      overhead associated with alignment between
 *                      the chunk header and chunk's data
 * @param req_memsize   requested memory size
 *
 * @return total memory size that can be requested
 *
 * @pre (overhead != NULL)
 * @pre (req_memsize != NULL)
 *
 * @note For a POSIX implementation, such as this, there is a one-to-one
 *       correlation.  For an implementation like IOS, there is a fixed
 *       memory overhead that can not be avoided, so the overhead and actual
 *       size may be different.
 */
static INLINE uint32_t
chunk_get_req_memsize (chunk_t *chunk, uint32_t *overhead,
                       uint32_t *req_memsize)
{
    *overhead = 0;
    return *req_memsize;
}
#endif


/**
 * Convert chunk node pointer to free chunk data pointer
 *
 * @param hdr   Pointer to convert
 *
 * @return      Pointer to free chunk node data region
 *
 * @pre (p != NULL)
 */
static INLINE free_chunk_t * 
hdr_to_free_chunk (chunk_header_t *hdr)
{
    cpr_assert_debug(hdr != NULL);

    return &(hdr->data_area.free_info);
}

/**
 * Convert chunk node data pointer to chunk node header pointer
 *
 * @param p     Pointer to convert
 *
 * @return      Pointer to chunk node header
 *
 * @pre (p != NULL)
 */
static INLINE chunk_header_t *
data_to_chunk_hdr (chunk_header_t *p)
{
    cpr_assert_debug(p != NULL);

    return (chunk_header_t *)
        ((unsigned char *)p - FIELDOFFSET(chunk_header_t, data_area.data));
}

/**
 * Determine if sibling chunk can be destroyed
 *
 * This routine checks that the chunk is a valid sibling and
 * that @b RESIDENT option has not been enabled.
 *
 * @param chunk  The chunk descriptor
 *
 * @return TRUE if the chunk can be destroyed; otherwise, FALSE
 */
static INLINE boolean
chunk_destroy_sibling_ok (chunk_t *chunk)
{
    return (boolean)((chunk->flags &
            (CHUNK_FLAGS_SIBLING | CHUNK_FLAGS_DYNAMIC | CHUNK_FLAGS_RESIDENT))
            == (CHUNK_FLAGS_SIBLING | CHUNK_FLAGS_DYNAMIC));
}

/**
 * Populates the freelist of chunks.
 *
 * @param chunk         TBD
 * @param real_size     TBD
 *
 * @return none
 *
 * @note The chunk data returned will match the client's requested
 *       alignment, *after* adding the chunk header.
 *
 * @note That we are the moving the 'data' pointer where the chunk
 * header for the first chunk data block begins, based on the alignment
 * requirement of the chunk data. This ends-up adding a padding to the
 * header if adding the header results in the chunk data not matching
 * the requested alignment. The header padding is added to the top so
 * that we can get to the header from the data pointer at fixed offset
 * instead of worrying about header padding.
 *
 * @note The chunk block, in case where the header padding is required,
 * looks something like the given diagram
 *
 * @verbatim
   +------------+  <---- chunk->data
   |   Hdr Pad  |
   |------------|
   |    Hdr     |
   +------------+
   |            |
   | Chunk Data
   |            |
   +------------+
   |  Hdr Pad   |
   +------------+
   |   Hdr      |
   +------------+
   Z            Z
   | chunk n    |
   +------------+  <---- chunk->end
   @endverbatim
 *
 * @note The additional memory for alignment has already been taken
 *       into account as part of overhead in chunk_calculate_size.
 */
static void
chunk_populate_freelist (chunk_t *chunk, uint32_t real_size)
{
    void          *data;
    uint32_t       count;
    free_chunk_t  *freehdr;
    uint32_t       alignment;
    uint32_t       data_alignment;
    chunk_header_t *hdr;

    if (!chunk) {
        return;
    }

#ifdef CPR_CHUNK_DEBUG
    CHUNK_DEBUG("chunk_populate_freelist(%x, %u)\n", chunk, real_size);
#endif

    alignment = chunk->alignment;
    data = chunk->data;

#ifdef CPR_CHUNK_DEBUG
    CHUNK_DEBUG("    alignment = %u, data = %x\n", alignment, data);
#endif

    /*
     * free_chunk_t and data chunk starts at the same location. Take
     * care of the different data headers.
     */
    for (count = 0; count < chunk->real_max; count++) {
        /*
         * Please see the comments above regarding adding aligning the
         * chunk data
         */
        if (alignment) {
            hdr = (chunk_header_t *)data;
            freehdr = hdr_to_free_chunk(hdr);
            data_alignment = ((uintptr_t)freehdr % alignment);
            if (data_alignment) {
                data_alignment = alignment - data_alignment;
                /*
                 * Move the data at which the first chunk header starts to
                 * accommodate for chunk data match the requested alignment
                 */
                data = (void *)((uintptr_t)data + data_alignment);
            }
        }
        hdr = (chunk_header_t *) data;
        freehdr = hdr_to_free_chunk(hdr);

        chunk->freelist[count] = (void *) freehdr;
        freehdr->magic = FREECHUNKMAGIC;
        freehdr->last_deallocator = 0;

        hdr->root_chunk = chunk;
        data = (void *) ((uintptr_t)data + real_size);
    }
}

/**
 * Create a chunk descriptor.
 *
 * This mallocs enough space for the chunk context and the data that
 * it will manage.
 *
 * @param cfg_size   The size of a chunk element
 * @param cfg_max    The maximum number of elements per chunk
 * @param flags      Chunk configuration flags
 * @param alignment  The required alignment, as a power of 2; otherwise,
 *                   set to zero(0) if no preference
 * @param name       The name of the chunk
 * @param caller_pc  The calling program counter
 *
 * @return  A pointer to the create chunk or NULL if failure
 *
 * @note The call tries to align the requested memory block to the
 *       memory free pool such that the next largest memblock is chosen.
 *       That way, reusability of the memory blocks is easier. This will
 *       be done for *ALL* the chunk users except "non-Dynamic" and
 *       those which have "Independent" data blocks.
 */
static chunk_t *
chunk_create_inline (uint32_t cfg_size, uint32_t cfg_max, uint32_t flags,
                     uint32_t alignment,
                     CHUNK_PRIVATE_MEMPOOL_ARG
                     const char *name, void *caller_pc)
{
    chunk_t *chunk;
    uint8_t *chunk_data;
    uint32_t total_datasize, overhead;
#ifdef NEED_TO_ADJUST_MEMSIZE
    uint32_t temp;
#endif
    uint32_t real_size, real_max, req_memsize;
    uint32_t max_chunk_size;

    max_chunk_size = 0;

#ifdef CPR_CHUNK_DEBUG
    CHUNK_DEBUG("chunk_create(%u, %u, 0x%x, %u, %s, %p)\n",
                cfg_size, cfg_max, flags, alignment, name, caller_pc);
#endif

    /*
     * Check whether alignment is a power of 2, if specified
     */
    if (alignment && (alignment & (alignment-1))) {
        CHUNK_DEBUG("chunk_create: alignment not a power of 2: %d\n",
                    alignment);
        return NULL;
    }

    chunk_data = NULL;

    /*
     * If the alignment is non-zero, allocate the header and freelist
     * independently from the data.
     */
    if (alignment) {
        flags |= CHUNK_FLAGS_INDEPENDENT;
    }

    /*
     * The chunk data size is derived from the user supplied value to
     * accommodate the following -
     * o data with overhead should be 4 byte aligned.
     * o size of data should should be multiple of requested alignment.
     * o size of data should be large enough to hold free_chunk info.
     */
    real_size = chunk_calculate_size(cfg_size, alignment, flags);
    overhead = 0;

    if (flags & CHUNK_FLAGS_DYNAMIC) {
        max_chunk_size = CHUNK_MEMBLOCK_MAXSIZE - sizeof(chunk_t)
                         - (sizeof(uint8_t *) * cfg_max);

        /*
         * Limit on the maximum size of the chunk element
         */
        if (real_size > max_chunk_size) {
            CPR_ERROR("CPR Chunk pool size too large: %u", real_size);
            return NULL;
        }
    }

    /*
     * For "Independent" pools, allocate the minimum and actual data as two
     * separate memory blocks.
     * For "non-Dynamic" pools, allocate the bestfit exact match block.
     * For "Sibling"s and "Regular chunk"s, allocate the first available
     * block based on the optimal size and be satisfied with it.
     */
    if (flags & CHUNK_FLAGS_INDEPENDENT) {
        total_datasize = real_size * cfg_max;

        /*
         * We have independent memory. Allocate the data now.
         */
        chunk_data = MEMALIGN(alignment, total_datasize);
        if (!chunk_data) {
            CPR_ERROR("Out-of-memory for chunk\n");
            return NULL;
        }

        /*
         * Calculate the chunk parameters.
         */
#ifdef NEED_TO_ADJUST_MEMSIZE
        total_datasize = chunk_get_req_memsize((chunk_t *)chunk_data,
                                               &temp, &total_datasize);
        total_datasize -= temp;
#endif

        if (flags & CHUNK_FLAGS_DYNAMIC) {
            real_max = total_datasize / real_size;
        } else {
            real_max = cfg_max;
        }
        overhead = total_datasize - real_max*real_size;

        /*
         * Calculate the size of the chunk block. This is the chunk header
         * and the array.
         */
        req_memsize = sizeof(chunk_t) + (sizeof(uint8_t *) * real_max);

        /*
         * Allocate our chunk.
         */
        chunk =(chunk_t *)MALLOC(req_memsize);
        if (!chunk) {
            CPR_ERROR("Out-of-memory for chunk\n");
            free(chunk_data);
            return NULL;
        }
#ifdef NEED_TO_ADJUST_MEMSIZE
        total_datasize = chunk_get_req_memsize(chunk, &temp, &req_memsize);
        overhead += total_datasize - req_memsize - temp;
#else
        total_datasize = req_memsize;
        overhead = 0;
#endif
    } else {
        /*
         * Always create an even number of chunks.  This insures that the
         * pointer array allocated at the front of the set of chunks [the
         * sizeof(uint8_t *) addition for each chunk] works out to a
         * multiple of eight bytes.
         */
        if (cfg_max & 1) {
            cfg_max++;
        }

        /*
         * Find the size of the block requested by the chunk user.
         */
        req_memsize = sizeof(chunk_t) +
            (sizeof(uint8_t *) + real_size) * cfg_max;

        /*
         * Allocate our chunk.
         */
        chunk =(chunk_t *)MALLOC(req_memsize);
        if (!chunk) {
            CHUNK_DEBUG("Out-of-memory for chunk: %d\n", req_memsize);
            return NULL;
        }

CHUNK_DEBUG("create_chunk yields chunk_header = 0x%x\n", chunk);
CHUNK_DEBUG("of size %u\n", req_memsize);

        /*
         * Get the size of the block that can be used.
         * and calculate real_max.
         */
#ifdef NEED_TO_ADJUST_MEMSIZE
        total_datasize = chunk_get_req_memsize(chunk, &temp, &req_memsize);
        total_datasize -= (sizeof(chunk_t) + temp);
#else
        total_datasize = req_memsize - sizeof(chunk_t);
#endif
CHUNK_DEBUG("total_datasize = %u\n", total_datasize);

        /*
         * If it is dynamic then calculate the real_max
         */
        if (flags & CHUNK_FLAGS_DYNAMIC) {
            real_max = total_datasize / (sizeof(uint8_t *) + real_size);
        } else {
            real_max = cfg_max;
        }

        /*
         * Calculate the chunk pool parameters.
         */
        overhead = total_datasize - real_max * (real_size + sizeof(uint8_t *));
CHUNK_DEBUG("overhead = %u\n", overhead);
    }

    /*
     * Fill in some context information.
     */
    chunk->cfg_size     = cfg_size;
    chunk->real_size    = real_size;
    chunk->cfg_max      = cfg_max;
    chunk->real_max     = real_max;
    chunk->index        = real_max;
    chunk->total_inuse  = 0;
    chunk->inuse_hwm    = 0;
    chunk->total_free   = 0;
    chunk->flags        = flags;
    CHUNK_PRIVATE_MEMPOOL_ASSIGN(chunk);
    chunk->next         = NULL;
    chunk->alignment    = (uint16_t) alignment;  //TODO: pass as uint16_t
    chunk->next_sibling = NULL;
    chunk->prev_sibling = NULL;
    chunk->p.tail       = NULL;
    chunk->overhead     = (uint16_t) overhead; //TODO: pass as uint16_t
    strncpy(chunk->name, name, sizeof(chunk->name) - 1);
    chunk->name[sizeof(chunk->name) - 1] = '\0';

    /*
     * Separate data for independent pools and contiguous data for others.
     */
    if (chunk_is_independent(chunk)) {
        chunk->data = chunk_data;
        /* checked for alignment earlier */
    } else {
        chunk->data = (uint8_t *) &chunk->freelist[real_max];
        MEMASSERT(chunk->data);
    }

    /*
     * Build our boundary pointers for this chunk
     */
    chunk->end = chunk->data + (real_size * real_max);

    /*
     * Populate the chunk freelist. This takes care of aligning the data to
     * the requested alignment
     */
    chunk_populate_freelist(chunk, real_size);

    /*
     * Final housekeeping
     */
    chunks_created++;

    /*
     * Remember it for posterity. Reset the field in the sibling for
     * consistency.
     */
    if (!chunk_is_sibling(chunk)) {
        chunk->total_free = real_max;
    } else {
        chunk->total_free = 0;
    }

CHUNK_DEBUG("return chunk 0x%x\n", chunk);
    return chunk;
}

/**
 * Create a chunk descriptor.
 *
 * @param cfg_size   The size of a chunk element
 * @param cfg_max    The maximum number of elements per chunk
 * @param flags      Chunk configuration flags
 * @param alignment  The required alignment, as a power of 2; otherwise,
 *                   set to zero(0) if no preference
 * @param name       The name of the chunk
 *
 * @return  A pointer to the create chunk or NULL if failure
 * chunk_create
 *
 * @note The calling program counter is saved within the chunk header
 *
 * @note This routine will initialized the chunk manager if not already
 *       done.
 */
chunk_t *
chunk_create (uint32_t cfg_size,
              uint32_t cfg_max,
              uint32_t flags,
              uint32_t alignment,
              CHUNK_PRIVATE_MEMPOOL_ARG
              const char *name)
{

    void *caller_pc = CHUNK_CALLER_PC;

    if ((flags & CHUNK_FLAGS_DYNAMIC) &&
        (cfg_size > CHUNK_MEMBLOCK_MAXSIZE)) {
        CPR_ERROR("Invalid chunk size = %u\n", cfg_size);
        return NULL;
    }

    /*
     * If we are called for first time and chunk_init was not done
     * then initialize global chunk variables. No need to worry
     * calling chunk_init for every chunk_create since chunk_init()
     * is reentrant now.
     */
    (void) chunk_init();

    return chunk_create_inline(cfg_size, cfg_max, flags, alignment,
                               CHUNK_PRIVATE_MEMPOOL_VAR
                               name, caller_pc);
}

/**
 * Create a chunk descriptor.
 *
 * @param cfg_size   The size of a chunk element
 * @param cfg_max    The maximum number of elements per chunk
 * @param flags      Chunk configuration flags
 * @param alignment  The required alignment, as a power of 2; otherwise,
 *                   set to zero(0) if no preference
 * @param name       The name of the chunk
 * @param caller_pc  The calling program counter
 *
 * @return  A pointer to the create chunk or NULL if failure
 *
 * @note This routine will initialized the chunk manager if not already
 *       done.
 */
chunk_t *
chunk_create_caller (uint32_t cfg_size,
                     uint32_t cfg_max,
                     uint32_t flags,
                     uint32_t alignment,
                     CHUNK_PRIVATE_MEMPOOL_ARG
                     const char *name,
                     void *caller_pc)
{
    if ((flags & CHUNK_FLAGS_DYNAMIC) &&
        (cfg_size > CHUNK_MEMBLOCK_MAXSIZE)) {
        CPR_ERROR("Invalid chunk size = %u\n", cfg_size);
        return NULL;
    }

    /*
     * If we are called for first time and chunk_init was not done
     * then initialize global chunk variables. No need to worry
     * calling chunk_init for every chunk_create since chunk_init()
     * is reentrant now.
     */
    (void) chunk_init();

    return chunk_create_inline(cfg_size, cfg_max, flags, alignment,
                               CHUNK_PRIVATE_MEMPOOL_VAR
                               name, caller_pc);
}

/**
 * Determine if the chunk can be destroyed, i.e. freed.
 *
 * @param chunk  The chunk descriptor
 *
 * @return TRUE if all sibling chunks have been freed and the initial
 *         chunk can be destroyed; otherwise, FALSE
 */
boolean
chunk_is_destroyable (chunk_t *chunk)
{
    if (!chunk) {
        return FALSE;
    }

    /* Can't destroy if there's anything still allocated from this guy. */
    if (chunk->index != chunk->real_max) {
        return FALSE;
    }

    /* Can't destroy if there are any siblings. */
    if (chunk->next_sibling != NULL) {
        return FALSE;
    }

    return TRUE;
}

/**
 * Remove sibling chunk from chunk list
 *
 * Remove @c chunk from sibling list defined in @c head_chunk
 * @c chunk can be any sibling chunk but can't be @c head_chunk
 *
 * @param head_chunk    The root chunk descriptor
 * @param chunk         The chunk descriptor to remove
 *
 * @return none
 *
 * @pre (head_chunk != NULL)
 * @pre (chunk != NULL)
 *
 * @note The calling function must provide mutex protection
 */
static void
chunk_remove_sibling (chunk_t *head_chunk, chunk_t *chunk)
{
    if (chunk == head_chunk->p.tail) {
        head_chunk->p.tail = chunk->prev_sibling;
    }

    chunk->prev_sibling->next_sibling = chunk->next_sibling;
    if (chunk->next_sibling) {
        chunk->next_sibling->prev_sibling = chunk->prev_sibling;
    }
    chunk->next_sibling = NULL;
    chunk->prev_sibling = NULL;
}

/**
 * Insert sibling chunk into chunk list
 *
 * Insert @c chunk into @c head_chunk's sibling list after @c append
 * which can be the head_chunk, middle sibling chunk or the tail
 * chunk.
 *
 * @param head_chunk    The root chunk descriptor
 * @param append        The chunk descriptor to append after
 * @param chunk         The chunk descriptor to insert
 *
 * @return none
 *
 * @pre (head_chunk != NULL)
 * @pre (append != NULL)
 * @pre (chunk != NULL)
 *
 * @note The caller must provide mutex protection
 */
static void
chunk_insert_sibling (chunk_t *head_chunk, chunk_t *append,
                      chunk_t *chunk)
{
    if (append->next_sibling) {
        chunk->next_sibling = append->next_sibling;
        chunk->prev_sibling = append;
        append->next_sibling->prev_sibling = chunk;
        append->next_sibling = chunk;
    } else {
        if (append == head_chunk) {
            head_chunk->p.tail = chunk;
        }
        chunk->next_sibling = NULL;
        chunk->prev_sibling = append;
        append->next_sibling = chunk;
    }

    if (append == head_chunk->p.tail) {
        head_chunk->p.tail = chunk;
    }
}

/**
 * Rotate chunk list when a specific chunk is fully allocated
 *
 * When a chunk becomes empty (no free space) after chunk_malloc,
 * it is removed from its current position at the front part of
 * the sibling list and appended at the end of the list so that
 * next chunk_malloc can find a free chunk element from the front
 * part of the sibling list without going through this empty
 * sibling chunk.
 *
 * @param head_chunk    The root chunk descriptor
 * @param chunk         The chunk descriptor to move
 *
 * @return none
 *
 * @pre (head_chunk != NULL)
 * @pre (chunk != NULL)
 */
static INLINE void
chunk_turn_empty (chunk_t *head_chunk, chunk_t *chunk)
{
    if (chunk == head_chunk) {
        return;
    }

    /*
     * only one sibling, no need to shuffle list. and don't shuffle
     * when checkchunk_for_scheduler is called for validating chunks.
     */
    if (head_chunk->next_sibling == head_chunk->p.tail ||
        chunk_siblings_stop_shuffling) {
        return;
    }

    (void) cprGetMutex(chunk_mutex);
    chunk_remove_sibling(head_chunk, chunk);
    chunk_insert_sibling(head_chunk, head_chunk->p.tail, chunk);
    (void) cprReleaseMutex(chunk_mutex);
    return;
}

/**
 * Rotate chunk list when a specific chunk has space available
 *
 * When a chunk becomes non-empty after chunk_free, it is removed
 * from its current position in the rear part of the sibling list
 * and appended right after head_chunk if its next_sibling is empty
 * or after head_chunk->next_sibling if next_sibling is not empty
 * so that the next chunk_malloc can find a free chunk element from
 * the front part of sibling list.
 *
 * The reason the non-empty chunk is not always appended right
 * after head_chunk is because a newly created sibling chunk, which
 * contains many free chunk elements is placed right after
 * head_chunk. The chunk which just turns non-empty should be
 * placed after such newly created sibling chunk.
 *
 * @param head_chunk    The root chunk descriptor
 * @param chunk         The chunk descriptor to move
 *
 * @return none
 *
 * @pre (head_chunk != NULL)
 * @pre (chunk != NULL)
 */
static INLINE void
chunk_turn_not_empty (chunk_t *head_chunk, chunk_t *chunk)
{
    chunk_t *append;

    if (chunk == head_chunk) {
        return;
    }

    /*
     * Only one sibling, no need to shuffle list. and don't shuffle when
     * checkchunk_for_scheduler is called for validating chunks.
     */
    if (head_chunk->next_sibling == head_chunk->p.tail ||
        chunk_siblings_stop_shuffling) {
        return;
    }

    (void) cprGetMutex(chunk_mutex);
    chunk_remove_sibling(head_chunk, chunk);
    append = head_chunk->next_sibling;
    if (!append->index) {
        append = head_chunk;
    }
    chunk_insert_sibling(head_chunk, append, chunk);
    (void) cprReleaseMutex(chunk_mutex);
    return;
}

/**
 * Destroy chunk if force is enabled; otherwise, only free if the entire
 * chunk is empty, i.e. none of the chunk elements are being used.
 *
 * @param chunk      The chunk descriptor
 * @param caller_pc  The calling program counter
 * @param force      Whether to force removal when siblings exist
 *
 * @return TRUE or FALSE
 */
boolean
chunk_destroy_internal (chunk_t *chunk, void *caller_pc, boolean force)
{
    chunk_t *head_chunk = NULL;

    /*
     * Make sure it's a live one
     */
    if (!chunk) {
        return FALSE;
    }

    /*
     * All present and accounted for?
     */
    (void) cprGetMutex(chunk_mutex);
    if ((chunk->index != chunk->real_max) && (force == FALSE)) {
        /*
         * chunk_malloc from interrupt level to enter this case
         */
        (void) cprReleaseMutex(chunk_mutex);
        return FALSE;
    }

    /*
     * Decide if it's a sibling free or the main man.
     */
    if (chunk_is_sibling(chunk)) {
        /*
         * update tail pointer and total_free in head_chunk check
         */
        head_chunk = chunk->p.head_chunk;

        chunk_remove_sibling(head_chunk, chunk);
        head_chunk->total_free -= chunk->index;
        (void) cprReleaseMutex(chunk_mutex);

        chunk_siblings_trimmed++;
    } else {
        /*
         * It's a wipeout. Only allow the main structure to be deleted
         * if there are no siblings.
         */
        head_chunk = chunk;
        if (chunk->next_sibling != NULL) {
            (void) cprReleaseMutex(chunk_mutex);
            CPR_ERROR("Can not destroy chunk due to siblings, 0x%x", chunk);
            return FALSE;
        }

        /*
         * Release protection
         */
        (void) cprReleaseMutex(chunk_mutex);
    }
    chunks_destroyed++;

    /*
     * Hand back that memory
     */
    if (chunk_is_independent(chunk)) {
        FREE(chunk->data);
    }

    CHUNK_DEBUG("Free chunk 0x%x, ", chunk);
    CHUNK_DEBUG("caller_pc %p\n", caller_pc);
    FREE(chunk);

    return TRUE;
}

/**
 * Destroys a chunk structure depending on it's sibling status
 *
 * @param chunk  The chunk descriptor
 *
 * @return TRUE is destroyed; otherwise, FALSE
 */
boolean
chunk_destroy (chunk_t *chunk)
{
    void *caller_pc = CHUNK_CALLER_PC;

    if (chunk && chunk_is_resident(chunk) && !chunk_is_sibling(chunk) &&
        chunk->total_inuse == 0) {
        while (chunk->next_sibling) {
            if (!chunk_destroy_internal(chunk->next_sibling, caller_pc,
                                        FALSE)) {
                return FALSE;
            }
        }
    }
    return chunk_destroy_internal(chunk, caller_pc, FALSE);
}

/**
 * Forcefully destroy the chunk.
 *
 * Destroys a master chunk "forcefully", even if it has elements
 * or siblings that are not free.
 *
 * @param chunk  The chunk descriptor
 *
 * @return TRUE if destroyed; otherwise, FALSE
 *
 * @note You should know what you're doing before using this function.
 *       If you aren't sure, use chunk_destroy() instead.  A crash will
 *       occur if some code continues to refer to the chunk that has
 *       been destroyed or an element of it.
 */
boolean
chunk_destroy_forced (chunk_t *chunk)
{
    void *caller_pc = CHUNK_CALLER_PC;

    if (!chunk) {
        return FALSE;
    }

    if (chunk_is_sibling(chunk)) {
        CPR_ERROR("Chunk is a sibling, 0x%x, name = %s\n", chunk, chunk->name);
        return FALSE;
    }

    while (chunk->next_sibling) {
        if (!chunk_destroy_internal(chunk->next_sibling, caller_pc,
                                    TRUE)) {
            return FALSE;
        }
    }
    return chunk_destroy_internal(chunk, caller_pc, TRUE);
}

/**
 * Prepare the chunk data for allocate chunk element
 *
 * Fill-in the chunk header and zero the memory if configured
 * to do so.
 *
 * @param data          Start of the chunk element's data
 * @param chunk         The chunk containing the element
 * @param head_chunk    The head chunk descriptor
 *
 * @return none
 *
 * @pre (data != NULL)
 * @pre (chunk != NULL)
 * @pre (head_chunk != NULL)
 */
static void
chunk_prepare_data (void *data, chunk_t *chunk, chunk_t *head_chunk)
{
    chunk_header_t *hdr;

    cpr_assert_debug(data != NULL);
    cpr_assert_debug(chunk != NULL);
    cpr_assert_debug(head_chunk != NULL);

    /*
     * Check the high water mark.
     */
    if (head_chunk->total_inuse > head_chunk->inuse_hwm) {
        head_chunk->inuse_hwm = head_chunk->total_inuse;
    }

    /*
     * Zero out the memory
     */
    hdr = data_to_chunk_hdr(data);
    hdr->root_chunk = chunk; /* to be on safer side */

    if (!chunk_is_unzeroed(chunk)) {
        memset(data, 0, chunk->cfg_size);
    } else {
        ((free_chunk_t *)data)->last_deallocator = NULL;
    }
}


/**
 * Internal routine to allocate a chunk element
 *
 * Grabs a chunk of free memory from the specified chunk. If there isn't
 * enough memory, it'll try and create a sibling chunk if it can.
 *
 * @param chunk      The chunk descriptor
 * @param caller_pc  The caller's program counter
 *
 * @return pointer to the allocated chunk element or NULL
 *
 * @pre (chunk_mutex is initialized)
 *
 * @note If the chunk manager can not pass internal sanity check
 *       of negative number of available chunk elements, a forced
 *       crashdump will occur.
 */
static void *
chunk_malloc_inline (chunk_t *chunk, void *caller_pc)
{
    int index;
    void *data;
    chunk_t *head_chunk;

    /*
     * Make sure we have a chunk to malloc from
     */
    if (!chunk) {
        CPR_ERROR("chunk_malloc: chunk is (NULL)\n");
        return NULL;
    }

    head_chunk = chunk;

    

    if (!head_chunk->total_free && !chunk_is_dynamic(head_chunk)){
        /*
         * Tough luck.
         */
        chunk->flags |= CHUNK_FLAGS_MALLOCFAILED;
        return NULL;
    }
    
    /*
     * Protect section
     */
    (void) cprGetMutex(chunk_mutex);

    /*
     * Check to see whether there is any available chunk in the whole chunk
     */
    if (head_chunk->total_free) {

        /* Sanity check for wrap condition */
        if (head_chunk->total_free & (1 << 31)) {
            /* Be nice to the OS, release the mutex before crashing */
            (void) cprReleaseMutex(chunk_mutex);
            cpr_crashdump();
        }

        while (chunk) {
            /*
             * Check to see if this chunk's got some memory spare...
             */
            if (chunk->index > 0) {
                /*
                 * Fetch the next free chunk
                 */
                index = --chunk->index;
                head_chunk->total_free--;

                /*
                 * move empty chunk to the end of sibling linked list
                 */
                if ((index == 0) && head_chunk->total_free) {
                    chunk_turn_empty(head_chunk, chunk);
                }

                /*
                 * Count it
                 */
                head_chunk->total_inuse++;

                /*
                 * This parent chunk has space. Allocate from it.
                 */
                data = chunk->freelist[index];

                /*
                 * Release protection
                 */
                (void) cprReleaseMutex(chunk_mutex);

                /*
                 * Fill in the data header if there is any.
                 */
                chunk_prepare_data(data, chunk, head_chunk);

                CHUNK_DEBUG("Allocate chunk node: data = 0x%x\n", data);
                CHUNK_DEBUG("Allocate chunk node: pc = %p\n", caller_pc);
                return data;
            }

            chunk = chunk->next_sibling;
        }
        /*
         * No more siblings to carve out memory from.
         */
        CPR_ERROR("No more chunk siblings available for %s: free = %u,"
                  " inuse = %u\n", head_chunk->name, head_chunk->total_free,
                  head_chunk->total_inuse);
    }

    /*
     * Release protection
     */
    (void) cprReleaseMutex(chunk_mutex);

    /*
     * We're out of memory. See if we're allowed to grab some more.
     */
    if (chunk_is_dynamic(head_chunk)) {
        chunk_t *new_chunk = NULL;

        /*
         * Grab another chunk of memory
         */
        new_chunk = chunk_create_inline(head_chunk->cfg_size,
                                        head_chunk->cfg_max,
                                        (head_chunk->flags |
                                         CHUNK_FLAGS_SIBLING),
                                        head_chunk->alignment,
                                        CHUNK_PRIVATE_MEMPOOL_ARG_REF(head_chunk)
                                        head_chunk->name, caller_pc);
        /*
         * If we managed to grab another big chunk, carve a slice
         * from the top of it and return that.
         */
        if (new_chunk) {

            new_chunk->p.head_chunk = head_chunk;

            /*
             * Protect section
             */
            (void) cprGetMutex(chunk_mutex);

            chunk_insert_sibling(head_chunk, head_chunk, new_chunk);
            head_chunk->total_free += new_chunk->real_max;

            /*
             * Fetch the next free chunk
             */
            index = --new_chunk->index;
            head_chunk->total_free--;

            /*
             * Count it
             */
            head_chunk->total_inuse++;

            /*
             * This parent chunk has space. Allocate from it.
             */
            data = new_chunk->freelist[index];

            /*
             * Release protection
             */
            (void) cprReleaseMutex(chunk_mutex);

            /*
             * Check alignment (don't do this til interrupts are re-enabled.)
             */
            MEMASSERT(data);

            /*
             * Fill in the data header if there is any.
             */
            chunk_prepare_data(data, new_chunk, head_chunk);

            CHUNK_DEBUG("Allocate chunk node: data = 0x%x\n", data);
            CHUNK_DEBUG("Allocate chunk node: pc = %p\n", caller_pc);
            chunk_siblings_created++;
            return data;
        }
    }

    return NULL;
}

/**
 * Allocate a chunk element
 *
 * @param chunk  The chunk descriptor
 *
 * @return pointer to the allocated chunk element or NULL
 *
 * @note The calling routine's program counter will be saved
 *
 * @see cpr_malloc_inline
 */
void *
chunk_malloc (chunk_t *chunk)
{
    void *caller_pc = CHUNK_CALLER_PC;

    return chunk_malloc_inline(chunk, caller_pc);
}

/**
 * Allocate a chunk element with caller PC given
 *
 * @param chunk      The chunk descriptor
 * @param caller_pc  The caller's program counter
 *
 * @return pointer to the allocated chunk element or NULL
 *
 * @see cpr_malloc_inline
 */
void *
chunk_malloc_caller (chunk_t *chunk, void *caller_pc)
{
    return chunk_malloc_inline(chunk, caller_pc);
}


/**
 * Determine the size of the given chunk element data
 *
 * @param data  Pointer to chunk element's data
 *
 * @return Size of chunk element, if data is NULL then
 *         zero(0) is returned
 *
 * @note if @c data is bogus, return value will be bogus
 */
uint32_t
get_chunk_size (void *data)
{
    chunk_header_t *hdr;

    if (!data) {
        return 0;
    }

    /*
     * Find out the chunk the data belongs to.
     */
    hdr = data_to_chunk_hdr(data);

    return hdr->root_chunk->cfg_size;
}

/**
 * Poison the chunk element
 *
 * Write the poison pattern on the freed chunk data which is
 * 4-byte aligned.
 *
 * @param chunk  The chunk descriptor
 * @param data   The chunk element
 * @param size   The size of the chunk element
 *
 * @return none
 *
 * @note The beginning of freed chunk data is free_chunk_t which
 *       doesn't need poison pattern
 */
static void
poison_chunk (chunk_t *chunk, void *data, uint32_t size)
{
    uint32_t i;
    uint32_t *data2;

    data2 = (uint32_t *)((uintptr_t)data + sizeof(free_chunk_t));
    size -= sizeof(free_chunk_t);
    if (size > MAX_CHUNK_POISON) {
        size = MAX_CHUNK_POISON;
    }
    size /= 4;
    for (i = 0; i < size; i++) {
        *data2++ = CHUNK_POISON_PATTERN;
    }
}

/**
 * Free the chunk element
 *
 * The chunk will mark the element as free by returning the element
 * to the chunk's free list. Also the chunk element data will then
 * contain a free magic value with the caller's program counter
 *
 * @param chunk       The chunk descriptor
 * @param head_chunk  The root chunk's descriptor
 * @param data        The chunk elmement's data pointer
 * @param caller_pc   The calling routine's program counter
 *
 * @return TRUE if freed; otherwise, FALSE
 *
 * @pre (chunk != NULL)
 * @pre (head_chunk != NULL)
 * @pre (data != NULL)
 */
static boolean
chunk_free_body (chunk_t *chunk, chunk_t *head_chunk, void *data,
                 void *caller_pc)
{
    cpr_assert_debug(chunk != NULL);
    cpr_assert_debug(head_chunk != NULL);
    cpr_assert_debug(data != NULL);

    /*
     * Add a little sanity checking
     */
    if (chunk->index >= chunk->real_max) {
        /*
         * Something's gone horribly wrong.
         */
        CPR_ERROR("Attempt to free non-empty chunk (0x%x), %s: free = %u,"
                  " max_free = %u\n", chunk, head_chunk->name, chunk->index,
                  chunk->real_max);
        return FALSE;
    }

    ((free_chunk_t *)data)->magic = FREECHUNKMAGIC;
    ((free_chunk_t *)data)->last_deallocator = (void *)caller_pc;

    /*
     * Put the memory back in the freelist safely
     */
    (void) cprGetMutex(chunk_mutex);

    /*
     * move this chunk to front part of sibling linked list if
     * it was empty and just gets one free chunk element back.
     */
    if (!chunk->index) {
        chunk_turn_not_empty(head_chunk, chunk);
    }
    chunk->freelist[chunk->index++] = data;
    head_chunk->total_free++;
    head_chunk->total_inuse--;

    (void) cprReleaseMutex(chunk_mutex);

    CHUNK_DEBUG("Free chunk node: data = 0x%x,", data);
    CHUNK_DEBUG("Free chunk node: pc = 0x%x\n", caller_pc);

    /*
     * Okay. Is this chunk now full and a sibling? If it is, it's
     * time to hand this puppy back.
     */
    if (chunk->index == chunk->real_max) {
        if (chunk_destroy_sibling_ok(chunk)) {
            chunk_destroy_internal(chunk, caller_pc, FALSE);
        }
    }
    return TRUE;
}

/**
 * Free the chunk element
 *
 * This will identify the chunk data header, thus accesses the sibling to
 * which the data belongs. Calls chunk_free_body to return to the free pool.
 *
 * It steps back by four(4) bytes and uses it to access the chunk.
 *
 * @param chunk      The chunk descriptor
 * @param data       The chunk element to free
 * @param caller_pc  The caller's program counter
 *
 * @return TRUE if chunk element is freed; otherwise, FALSE
 *
 * @note This routine will force a crash if chunk is NULL and the data
 *       pointer is bogus which results in chunk remaining NULL
 */
static boolean
chunk_free_inline (chunk_t *chunk, void *data, void *caller_pc)
{
    chunk_t *head_chunk;
    chunk_header_t *hdr;

    /*
     * Make sure we have a data to work with
     */
    if (!data) {
        return FALSE;
    }

    /*
     * Find out the parent chunk for this data.
     */
    if (!chunk) {
        hdr = data_to_chunk_hdr(data);
        chunk = hdr->root_chunk;
    }

    /*
     * Check for corruption.
     */
    if (!chunk) {
        CPR_ERROR("chunk_free: no root chunk\n");
        cpr_crashdump();
    }

    head_chunk = NULL;
    hdr = data_to_chunk_hdr(data);
    chunk = hdr->root_chunk;

    /*
     * Get the head chunk.
     */
    if (chunk_is_sibling(chunk)) {
        head_chunk = chunk->p.head_chunk;
    } else {
        head_chunk = chunk;
    }

    if (((uint8_t *)data < chunk->data) ||
        ((uint8_t *)data >= chunk->end)) {
        /*
         * Something's gone horribly wrong.
         */
        CPR_ERROR("chunk_free: data node not in chunk (0x%x, %s) data range,"
                  " free=%d, max_free=%d\n", chunk, head_chunk->name,
                  chunk->index, chunk->real_max);
        return FALSE;
    }

    poison_chunk(chunk, data, chunk->cfg_size);
    return chunk_free_body(chunk, head_chunk, data, caller_pc);
}

/**
 * Free the chunk element
 *
 * @param chunk      The chunk descriptor
 * @param data       The chunk element to free
 *
 * @return TRUE if chunk element is freed; otherwise, FALSE
 *
 * @note After determining the caller's program counter, calls
 *       chunk_free_inline to do the work
 */
boolean
chunk_free (chunk_t *chunk, void *data)
{
    void *caller_pc = CHUNK_CALLER_PC;

    return chunk_free_inline(chunk, data, caller_pc);
}

/**
 * Free the chunk element
 *
 * @param chunk      The chunk descriptor
 * @param data       The chunk element to free
 * @param caller_pc  The caller's program counter
 *
 * @return TRUE if chunk element is freed; otherwise, FALSE
 *
 * @note Calls chunk_free_inline to do the work
 */
boolean
chunk_free_caller (chunk_t *chunk, void *data, void *caller_pc)
{
    return chunk_free_inline(chunk, data, caller_pc);
}

/**
 * Returns the unused chunk elements present in the chunk pool.
 *
 * @param chunk  Pointer to chunk
 *
 * @return the number of available chunk elements or zero(0) if
 *         chunk is NULL
 */
long
chunk_totalfree_count (chunk_t *chunk)
{
    if (!chunk) {
        return 0;
    }

    return chunk->total_free;
}

/** @} */ /* End of chunk memory routines */

