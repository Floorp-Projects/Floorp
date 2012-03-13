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

#ifndef __CPR_DARWIN_CHUNK_H__
#define __CPR_DARWIN_CHUNK_H__


#define CHUNK_ALIGN_BYTES 4


/**
 * Primary chunk header and siblings header
 */
typedef struct chunk_s {
    struct chunk_s *next;          /**< Next mast chunk in chunkQ           */
    struct chunk_s *next_sibling;  /**< Next sibling in chunk family        */
    struct chunk_s *prev_sibling;  /**< Previous sibling in family          */
    union {
        struct chunk_s *head_chunk;/**< ptr to head in sibling              */
        struct chunk_s *tail;      /**< ptr to tail of sibling in head      */
    } p;

    uint32_t    cfg_max;           /**< User req max # of elements          */
    uint32_t    real_max;          /**< Actual max # of chunk elements      */
    uint32_t    cfg_size;          /**< User requested chunk element size   */
    uint32_t    real_size;         /**< Actual size of each element         */
    uint32_t    index;             /**< Index to the first free element     */
    uint32_t    total_inuse;       /**< Total chunks currently in use       */
    uint32_t    inuse_hwm;         /**< HighWaterMark of total chunks used  */
    uint32_t    total_free;        /**< total number of free chunks         */
    uint16_t    alignment;         /**< Fundamental alignment for chunks    */
    uint16_t    overhead;          /**< Overhead to avoid internal mem frag */
    uint32_t    flags;             /**< Chunk status flags                  */
    char        name[15];          /**< Name of this chunk user             */
    uint8_t    *data;              /**< Start of the actual element data    */
    uint8_t    *end;               /**< End of the chunk                    */
    void       *freelist[0];       /**< Start of the free list              */
} chunk_t;

/**
 * Free chunk node
 */
typedef struct free_chunk_s {
    uint32_t magic;
    void *last_deallocator;    /**< Who deallocated this block? */
} free_chunk_t;

/**
 * Chunk node header
 */
typedef struct chunk_header_s {
    chunk_t *root_chunk;
    union {
        uint8_t *data;          /**< Start of data area       */
        free_chunk_t free_info; /**< Start of free block info */
    } data_area;
} chunk_header_t;


/**
 * Chunk template
 *
 * The chunk looks like this in memory
 *
   @verbatim
   +------------+  <---- chunk
   |            |
   | chunk_type |
   |            |
   +------------+  <---- chunk->freelist
   |            |
   | free list  |
   |            |
   +------------+
   | |||||||||| |
   +------------+  <---- chunk->data (discontiguous if alignment is used)
   | chunk 0    |
   Z            Z
   | chunk n    |
   +------------+  <---- chunk->end
   @endverbatim
 *
 * Caveat
 * ------
 *
 * You should go and read the comments in os/chunk.c before using this
 * code for the first time.
 *
 */

#define FREECHUNKMAGIC          0xEF4321CD      /* Magic value for freechunkmagic */
#define CHUNK_POISON_PATTERN    0xb0d0b0d

/**
 * Upper limit of memory block that is allocated by the chunk manager.
 */
#define CHUNK_MEMBLOCK_MAXSIZE  65536

/*
 * Chunk Manager defines
 *
 * WARNING!
 *
 * CHUNK_FLAGS_DYNAMIC is only valid when chunk_malloc() and chunk_free()
 * are called from process level. Interrupt level growing of a chunk is
 * *not* supported.
 */

#define CHUNK_FLAGS_NONE         0x00000000
#define CHUNK_FLAGS_DYNAMIC      0x00000001
#define CHUNK_FLAGS_SIBLING      0x00000002
#define CHUNK_FLAGS_INDEPENDENT  0x00000004
#define CHUNK_FLAGS_RESIDENT     0x00000008
#define CHUNK_FLAGS_UNZEROED     0x00000010
#define CHUNK_FLAGS_MALLOCFAILED 0x00000080

/* Non data region, Chunk should not trigger an access to the allocate area. */
//#define CHUNK_FLAGS_NONDATA     0x00000100

/*
 * This flag indicates that the memory pool is private
 * specified by the application. chunks will be created from
 * this private memory pool. To specify this the application
 * has to call chunk_create_private with this flag.
 */
#define CHUNK_FLAGS_PRIVATE     0x00000200

/*
 * This flag indicates that all the siblings should be
 * allocated from Transient Mempool. This flag should be
 * passed only if the elements are transient in behavior.
 */
//#define CHUNK_FLAGS_TRANSIENT 0x00000800

#define CHUNK_FLAGS_DATA_HEADER (CHUNK_FLAGS_SMALLHEADER)




/*
 * Prototypes
 */

extern boolean chunk_init(void);
extern void chunk_exit(void);
extern chunk_t *chunk_create(uint32_t cfg_size, uint32_t cfg_max,
                             uint32_t flags, uint32_t alignment,
                             const char *name);
extern boolean chunk_destroy(chunk_t *chunk);
extern boolean chunk_destroy_forced(chunk_t *chunk);
extern void *chunk_malloc(chunk_t *chunk);
extern void *chunk_malloc_caller(chunk_t *chunk, uint32_t caller_pc);
extern boolean chunk_free(chunk_t *chunk, void *data);
extern boolean chunk_free_caller(chunk_t *chunk, void *data,
                                 uint32_t caller_pc);
extern boolean chunk_is_destroyable(chunk_t *chunk);
extern boolean chunk_did_malloc_fail(chunk_t *chunk);
extern uint32_t get_chunk_size(void *data);
extern long chunk_totalfree_count(chunk_t *chunk);

#endif
