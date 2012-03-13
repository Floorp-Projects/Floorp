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
#include "cpr_types.h"
#include "cpr_debug.h"
#include "cpr_assert.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include "cpr_time.h"
#include "cpr_memory.h"
#include "cpr_locks.h"
#include "cpr_linux_align.h"
#include "cpr_linux_chunk.h"
#include "cpr_linux_memory_api.h"
#include "cpr_linux_string.h"
#include "plat_api.h"
#include <errno.h>
#include <sys/syslog.h>


/*-------------------------------------
 *
 * Constants
 *
 */

/**
 * @addtogroup MemoryConstants Linux memory constants (private)
 * @ingroup  Memory
 * @brief Internal constants for managing memory
 *
 * @{
 */

/** Size of memory header */
#define SIZE_OF_MEMORY_HEADER (offsetof(cpr_mem_t, mem))

/** Magic number used for memory tracking nodes' magic field */
#define CPR_TRACKING_MAGIC 0xabcdc0de

/** Poison value used to poison memory in cpr_free */
#define CPR_POISON 0xdeadc0de

/** The red-zone value used to check for memory overruns */
#define CPR_REDZONE 0xbeef00ee

/** The red-zone size (in bytes) being used */
#define SIZE_OF_REDZONE 4

/** The minimum supported memory allocation size */
#define MIN_MEMORY_SIZE 4

/** The number of bytes (including redzone) recorded for redzone violation */
#define REDZONE_RECORD_SIZE 16

/** Default number of bytes poisoned */
#define DEFAULT_POISON_SIZE 16

/** Maximum number of bytes that can by poisoned */
#define MAX_POISON_SIZE 128

/** The number of allocations that can be tracked */
#define DEFAULT_TRACKING_SIZE 1024

/** Maximum number of memory allocations that can be tracked */
#define MAX_TRACKING_SIZE (12*1024)

/** Size of memory heap guard in bytes */
#define PRIVATE_SYS_MEM_GUARD_SIZE 64

/** 16-bit magic value for the beginning guard */
#define BEGIN_GUARD_VALUE 0xBBAA

/** 16-bit magic value for the end guard */
#define END_GUARD_VALUE 0x0055

/** Memory tracking out-of-chunk condition flag */
#define MEM_TRACKING_FLAG_OOC 0x00000001

/** Memory tracking chunk_free failure condition flag */
#define MEM_TRACKING_FLAG_FREE_FAILED 0x00000002

/** Memory high-water mark condition, 87.5% of memory */
#define MEM_HIGH_WATER_MARK ((private_sys_mem_size >> 3) * 7)

/** Memory low-water mark condition, 75% of memory */
#define MEM_LOW_WATER_MARK ((private_sys_mem_size >> 2) * 3)

#define NUM_OF_CPR_SHOW_ARGUMENTS 3
/** @} */ /* End private constants */

/*-------------------------------------
 *
 * Structures
 *
 */

/**
 * @addtogroup MemoryStructures Linux memory structures (private)
 * @ingroup  Memory
 * @brief Internal structures for managing memory
 *
 * @{
 */

/**
 * CPR memory statistics
 */
typedef struct cpr_mem_statistics_s
{
    uint32_t total_failed_allocs;/**< Total failed memory allocation calls */
    uint32_t total_allocs;       /**< Total successful memory allocation calls */
    uint32_t mallocs;            /**< Total successful cpr_malloc calls */
    uint32_t callocs;            /**< Total successful cpr_calloc calls */
    uint32_t reallocs;           /**< Total successful cpr_realloc calls */
    uint32_t frees;              /**< Total successful cpr_free calls */
    uint32_t total_alloc_size;   /**< Total memory allocated in bytes */
    uint32_t in_use_count;       /**< Current number of allocations */
    uint32_t in_use_size;        /**< Current memory allocated in bytes */
    uint32_t max_use_size;       /**< Maximum memory allocated in bytes */
    uint32_t allocs_1_to_63;     /**< Total memory allocs from 1-63 bytes */
    uint32_t allocs_64_to_255;   /**< Total memory allocs from 64-255 bytes */
    uint32_t allocs_256_to_1k;   /**< Total memory allocs from 256-1k bytes */
    uint32_t allocs_1k_to_4k;    /**< Total memory allocs from 1k-4k bytes */
    uint32_t allocs_4k_to_32k;   /**< Total memory allocs from 4k-32k bytes */
    uint32_t allocs_32k_plus;    /**< Total memory allocs from 32k+ bytes */
    uint32_t redzone_violations; /**< Total red-zone violations */
    uint32_t redzone_size;       /**< Last redzone violation memory size */
    void *redzone_pc;            /**< Last redzone violation caller pc */
    uint8_t redzone_memory[REDZONE_RECORD_SIZE];  /**< Last redzone memory record */
} cpr_mem_statistics_t;

/**
 * Memory tracking node associated with an allocated piece of memory
 */
typedef struct cpr_mem_tracking_node_s
{
    struct cpr_mem_tracking_node_s *next; /**< Next node */
    struct cpr_mem_tracking_node_s *prev; /**< Previous node */
    void *mem;                   /**< Allocated memory */
    void *pc;                    /**< Allocator's program counter */
    size_t size;                 /**< Size of allocated memory */
    uint32_t magic;              /**< Magic number to validate node */
    cpr_time_t timestamp;        /**< Timestamp tick of allocation */
} cpr_mem_tracking_node_t;

/**
 * Memory tracking management
 */
typedef struct cpr_mem_tracking_list_s
{
    cpr_mem_tracking_node_t *head; /**< Head of list of tracked nodes */
    cpr_mem_tracking_node_t *tail; /**< Tail of list of tracked nodes */
    uint16_t max_size;             /**< Maximum size of memory tracking nodes */
    uint16_t real_size;            /**< Current number of tracked nodes */
    uint32_t missing_allocs;       /**< Number of memory allocations not tracked */
    boolean enabled;               /**< Whether memory tracking is enabled */
    uint32_t flags;                /**< Configuration and status of memory tracking */
    cpr_time_t timestamp;          /**< Start of when tracking was enabled */
} cpr_mem_tracking_t;

/**
 * CPR memory header
 *
 * The content of the union field is determined by the least significant
 * bit being one(1) for size and zero(0) for tracking node.  This is to
 * take advantage of the default memory alignment provided by the under-
 * lying memory management routines.
 */
typedef struct cpr_mem_s
{
    union {
        size_t size;             /**< Size of allocated memory */
        cpr_mem_tracking_node_t *node; /**< Memory tracking node */
    } u;
    void *pc;                    /**< Allocator's program counter */
    void *mem;                   /**< Start of memory */
} cpr_mem_t;

/** @} */ /* End private structures */


/*-------------------------------------
 *
 * Macros
 *
 */

/**
 * Macro to get a uint32_t pointer from a given memory pointer plus some
 * byte adjustment
 */
#define GET_UINT32_PTR(mem, adj) ((uint32_t *)((uint8_t *)(mem) + (adj)))

/**
 * Macros to safely read and write 4 bytes based on memory alignment setting,
 */
#define WRITE_4BYTES_MEM_ALIGNED WRITE_4BYTES_UNALIGNED
#define  READ_4BYTES_MEM_ALIGNED  READ_4BYTES_UNALIGNED

/**
 * Macro to get the Red Zone pointer from given internal memory pointer
 */
#define GET_REDZONE_PTR(cpr_mem, mem_size) \
    ((uint8_t *)(cpr_mem) + ((mem_size) - SIZE_OF_REDZONE))

/**
 * Macro to get the start of user memory pointer from given internal memory
 */
#define GET_USER_MEM_PTR(cpr_mem) \
    ((void *)((uint8_t *)(cpr_mem) + offsetof(cpr_mem_t, mem)))

/* This macro returns the caller_pc of the function */
#define CPR_CALLER_PC __builtin_return_address(0)


/*-------------------------------------
 *
 * Local variables
 *
 */

/** Memory tracking chunk node */
static chunk_t *tracking_chunk = NULL;

/** Memory statistics */
static cpr_mem_statistics_t memory_stats;
static cpr_mem_statistics_t saved_memory_stats;

/** Current poison size in bytes */
static uint32_t poison_size = DEFAULT_POISON_SIZE;

/** Memory tracking list */
static cpr_mem_tracking_t memory_tracking;

/** Mutex used for memory tracking */
static cprMutex_t mem_tracking_mutex;

/** Constant string prefix for CLI commands */
static const char *cmd_prefix = "CPR memory";

/** Track high/low memory water-mark condition */
static boolean high_mem_use_condition = FALSE;

/** The size of the memory allocated for the simulated heap */
static size_t private_size;
static size_t private_sys_mem_size;
/**
 * Next memory tracking node to display
 *
 * This variable @b MUST be volatile since the next node to display could
 * be freed/released before the displaying of current node's content is
 * complete which means this variable will be updated by the releasing
 * routine (and the variable content needs to be re-loaded).
 */
static volatile cpr_mem_tracking_node_t *next_display_node;

/*-------------------------------------
 *
 * Global variables
 *
 */
/** DL malloc special mutexes that need pre-initialization */
extern plat_os_thread_mutex_t morecore_mutex;
extern plat_os_thread_mutex_t magic_init_mutex;


/*-------------------------------------
 *
 * Forward declarations
 *
 */
static INLINE void
cpr_peg_alloc_size(size_t size);
static INLINE void
cpr_record_memory_size(cpr_mem_t *cpr_mem, size_t size);
static INLINE size_t
cpr_get_memory_size(cpr_mem_t *cpr_mem);
static INLINE void
cpr_poison_memory(cpr_mem_t *cpr_mem, size_t size);
static void
cpr_check_redzone(cpr_mem_t *cpr_mem, size_t size);
static void
cpr_assign_mem_tracking_node(cpr_mem_t *cpr_mem, size_t size);
static void
cpr_release_mem_tracking_node(cpr_mem_tracking_node_t *node);
static INLINE cpr_mem_tracking_node_t *
cpr_get_mem_tracking_node(cpr_mem_t *cpr_mem);
static void
cpr_debug_memory_cli_usage(void);
static boolean
cpr_set_memory_tracking(boolean debug, int32_t argc, const char *argv[]);
static INLINE void
cpr_enable_memory_tracking(void);
static INLINE void
cpr_disable_memory_tracking(void);
static boolean
cpr_set_poison_size(boolean debug, int32_t argc, const char *argv[]);
static void
cpr_clear_memory_tracking(void);
static void
cpr_show_memory_usage(void);
static void
cpr_show_memory_config(void);
static void
cpr_show_memory_statistics(void);
static void
cpr_show_memory_tracking(void);
static void
cpr_dump_memory(uint32_t *memory, size_t size); 

/* Fault insertion is not enabled */

/*-------------------------------------
 *
 * Constants
 *
 */
#define FAULT_INSERTION_CONFIG ""

/*-------------------------------------
 *
 * Macros
 *
 */
#define MEMORY_FAULT_INSERTION

#define PARSE_FAULT_INSERTION_CLI(token, debug, argc, argv)

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
 * APIs for managing buffers and memory for the application code
 * running on top of the Cisco Portable Runtime layer.
 *
 * This CPR layer adds an additional memory header to assist
 * with tracking memory and memory-related issued used by the
 * application.  The memory header is of the following format:
 *
   @verbatim
   v--------- 4 bytes ---------v
   +---------------------------+
   | node or size   | bit flag |
   +---------------------------+
   | allocator program counter |
   +---------------------------+
   | start of allocated memory |
   +---------------------------+
   | ...                       |
   +---------------------------+
   | red zone                  |
   +---------------------------+
   @endverbatim
 *
 * The bit flag(s) determine whether the initial field is a node
 * pointer used by memory tracking or the size requested memory. 
 *
 * @{
 */

/**
 * Initialize memory management before anything is started,
 * i.e. this routine is called by cprPreInit.
 *
 * @return TRUE or FALSE
 *
 * @warning This routine needs to be the @b first initialization call
 *          within CPR as even CPR layer initialization may require
 *          memory.
 *
 * @todo Create a separate function for DLmalloc init
 */
boolean
cpr_memory_mgmt_pre_init (size_t size)
{
    const char *fname = "cpr_memory_mgmt_pre_init";

    private_size = size + (2*PRIVATE_SYS_MEM_GUARD_SIZE);
    private_sys_mem_size = size;

    if (chunk_init() != TRUE) {
        return FALSE;
    }

    /* zero the statistics */
    memset(&memory_stats, 0, sizeof(cpr_mem_statistics_t));
    memset(&saved_memory_stats, 0, sizeof(cpr_mem_statistics_t));

    /* memory tracking starts disabled */
    memory_tracking.enabled = FALSE;

    /* set the default for the memory tracking size */
    memory_tracking.max_size = DEFAULT_TRACKING_SIZE;

    /* Create mutex for memory tracking */
    mem_tracking_mutex = cprCreateMutex("Memory tracking");
    if (mem_tracking_mutex == NULL) {
        CPR_ERROR("%s: init mem_tracking_mutex failure\n", fname);
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Allocate memory
 *
 * The cpr_malloc function attempts to malloc a memory block of at least size
 * bytes. 
 *
 * @param[in] size  size in bytes to allocate
 *
 * @return      pointer to allocated memory or #NULL
 *
 * @note        memory is NOT initialized
 */
void *
cpr_malloc(size_t size)
{
    cpr_mem_t *cpr_mem;
    uint8_t *p;
    size_t mem_size;
    void *caller_pc;

    /* Get caller PC before calling other functions */
    caller_pc = CPR_CALLER_PC;

    if (size == 0) {
        errno = ENOMEM;
        CPR_ERROR("ERROR: malloc %d bytes are not supported, pc=0x%x\n",
                  size, caller_pc);
        return NULL;
    }

    MEMORY_FAULT_INSERTION;

    /* Compute actual size to be allocated */
    size = MAX((size_t)MIN_MEMORY_SIZE, size);
    mem_size = size + SIZE_OF_MEMORY_HEADER + SIZE_OF_REDZONE;
    cpr_mem = MALLOC(mem_size);
    if (!cpr_mem) {
        errno = ENOMEM;
        CPR_ERROR("ERROR: malloc %d bytes failed, pc=0x%x\n", size, caller_pc);
        return NULL;
    }

    /*
     * Fill in red zone
     * 
     * cpr_mem is cast to a byte pointer so the pointer arithmetic
     * will be at a byte level and a macro is used to ensure the
     * write can be properly done if memory is not properly aligned.
     */
    p = GET_REDZONE_PTR(cpr_mem, mem_size);
    WRITE_4BYTES_UNALIGNED(p, CPR_REDZONE);

    /* Fill in the program counter */
    cpr_mem->pc = caller_pc;

    /* Is memory tracking enabled? */
    if (memory_tracking.enabled == TRUE) {
        /* Assign a tracking node */
        cpr_assign_mem_tracking_node(cpr_mem, size);
    } else {
        /* No memory tracking, just record memory size */
        cpr_record_memory_size(cpr_mem, size);
    }

    /* Peg statistics */
    memory_stats.total_allocs++;
    memory_stats.mallocs++;
    memory_stats.in_use_count++;
    memory_stats.in_use_size += size;
    memory_stats.total_alloc_size += size;
    if (memory_stats.max_use_size < memory_stats.in_use_size) {
        memory_stats.max_use_size = memory_stats.in_use_size;
    }
    cpr_peg_alloc_size(size);

    /* Set high-water mark condition */
    if (memory_stats.in_use_size > MEM_HIGH_WATER_MARK) {
        high_mem_use_condition = TRUE;
    }

    return GET_USER_MEM_PTR(cpr_mem);
}

/**
 * @brief Allocate and initialize memory
 *
 * The cpr_calloc function attempts to calloc "num" number of memory block of at
 * least size bytes. The code zeros out the memory block before passing 
 * the pointer back to the calling thread.
 *
 * @param[in] num   number of objects to allocate
 * @param[in] size  size in bytes of an object
 *
 * @return      pointer to allocated memory or #NULL
 *
 * @note        allocated memory is initialized to zero(0)
 */
void *
cpr_calloc (size_t num, size_t size)
{
    cpr_mem_t *cpr_mem;
    uint8_t *p;
    size_t mem_size;
    void *caller_pc;

    /* Get caller PC before calling other functions */
    caller_pc = CPR_CALLER_PC;

    if (size == 0 || num == 0) {
        CPR_ERROR("ERROR: calloc num %d of size %d bytes are not supported,"
                  " pc=0x%x\n", num, size, caller_pc);
        return NULL;
    }

    MEMORY_FAULT_INSERTION;

    /* Compute actual size to be allocated */
    if (num != 1) {
        /* NOT checking for wrap condition
         * ...that would be just too much anyways!
         */
        size = size * num;
    }
    size = MAX((size_t)MIN_MEMORY_SIZE, size);
    mem_size = size + SIZE_OF_MEMORY_HEADER + SIZE_OF_REDZONE;

    cpr_mem = CALLOC(1, mem_size);
    if (!cpr_mem) {
        CPR_ERROR("ERROR: calloc num %d of size %d bytes failed, pc=0x%x\n", 
                  num, size, caller_pc);
        return NULL;
    }

    /*
     * Fill in red zone
     * 
     * cpr_mem is cast to a byte pointer so the pointer arithmetic
     * will be at a byte level and a macro is used to ensure the
     * write can be properly done if memory is not properly aligned.
     */
    p = GET_REDZONE_PTR(cpr_mem, mem_size);
    WRITE_4BYTES_UNALIGNED(p, CPR_REDZONE);

    /* Fill in the program counter */
    cpr_mem->pc = caller_pc;

    /* Is memory tracking enabled? */
    if (memory_tracking.enabled == TRUE) {
        /* Assign a tracking node */
        cpr_assign_mem_tracking_node(cpr_mem, size);
    } else {
        /* No memory tracking, just record memory size */
        cpr_record_memory_size(cpr_mem, size);
    }

    /* Peg statistics */
    memory_stats.total_allocs++;
    memory_stats.callocs++;
    memory_stats.in_use_count++;
    memory_stats.in_use_size += size;
    memory_stats.total_alloc_size += size;
    if (memory_stats.max_use_size < memory_stats.in_use_size) {
        memory_stats.max_use_size = memory_stats.in_use_size;
    }
    cpr_peg_alloc_size(size);

    /* Set high-water mark condition */
    if (memory_stats.in_use_size > MEM_HIGH_WATER_MARK) {
        high_mem_use_condition = TRUE;
    }

    return GET_USER_MEM_PTR(cpr_mem);
}

/**
 * @brief Reallocate memory
 *
 * @param[in] mem  memory to reallocate
 * @param[in] size new memory size
 *
 * @return     pointer to reallocated memory or #NULL
 *
 * @note       if either mem is NULL or size is zero
 *             the return value will be NULL
 */
void *
cpr_realloc (void *mem, size_t size)
{
    cpr_mem_t *cpr_mem;
    size_t mem_size;
    size_t prev_size;
    cpr_mem_t *realloc_mem;
    uint32_t *p;
    void *caller_pc;

    /* Get caller PC before calling other functions */
    caller_pc = CPR_CALLER_PC;

    /* Return NULL for implementation defined behavior */
    if (mem == NULL) {
        return NULL;
    }
    if (size == 0) {
        /* Free the original memory, using cpr_free */
        memory_stats.total_allocs++;
        memory_stats.reallocs++;
        memory_stats.frees--;
        cpr_free(mem);
        return NULL;
    }

    MEMORY_FAULT_INSERTION;

    /* Determine start of actual memory */
    cpr_mem = (cpr_mem_t *)((uint8_t *)mem - SIZE_OF_MEMORY_HEADER);

    /* Check if memory has already been freed */
    if ((uint32_t)((uintptr_t) cpr_mem->pc) == CPR_POISON) {
        CPR_ERROR("ERROR: attempt to realloc free memory, 0x%x\n", mem);
        return NULL;
    }

    /* Remember previous size */
    prev_size = cpr_get_memory_size(cpr_mem);

    /* Compute actual size to be re-allocated */
    size = MAX((size_t)MIN_MEMORY_SIZE, size);
    mem_size = size + SIZE_OF_MEMORY_HEADER + SIZE_OF_REDZONE;

    /* Reallocate memory */
    realloc_mem = REALLOC(cpr_mem, mem_size);
    if (realloc_mem == NULL) {
        CPR_ERROR("ERROR: realloc %d bytes failed, pc=0x%x\n", size, caller_pc);
        return NULL;
    }

    /* Fill in new red zone */
    p = GET_UINT32_PTR(realloc_mem, mem_size - SIZE_OF_REDZONE);
    WRITE_4BYTES_UNALIGNED(p, CPR_REDZONE);

    /* Fill in the program counter */
    realloc_mem->pc = caller_pc;

    /* Is memory tracking enabled? */
    if (memory_tracking.enabled == TRUE) {
        cpr_mem_tracking_node_t *node;

        /* Determine if size or memory tracking node is present */
        node = (cpr_mem_tracking_node_t *)cpr_get_mem_tracking_node(realloc_mem);

        if (node && (node->magic == CPR_TRACKING_MAGIC)) {
            /* Node already exists, just update the content */
            node->pc = realloc_mem->pc;
            node->size = size;
            node->timestamp = time(NULL);
        } else {
            /* Currently just a size value, assign a tracking node */
            cpr_assign_mem_tracking_node(realloc_mem, size);
        }
    } else {
        /* No memory tracking, just record memory size */
        cpr_record_memory_size(realloc_mem, size);
    }

    /* Peg statistics */
    memory_stats.total_allocs++;
    memory_stats.reallocs++;
    memory_stats.in_use_size += size - prev_size;
    memory_stats.total_alloc_size += size;
    cpr_peg_alloc_size(size);
    if (memory_stats.max_use_size < memory_stats.in_use_size) {
        memory_stats.max_use_size = memory_stats.in_use_size;
    }

    /* Set high-water mark condition */
    if (memory_stats.in_use_size > MEM_HIGH_WATER_MARK) {
        high_mem_use_condition = TRUE;
    }

    return GET_USER_MEM_PTR(realloc_mem);
}

/**
 * @brief Free memory
 * The cpr_free function attempts to free the memory block passed in.
 *
 * @param[in] mem  memory to free
 *
 * @return     none
 */
void
cpr_free (void *mem)
{
    cpr_mem_tracking_node_t *node;
    cpr_mem_t *cpr_mem;
    size_t size = 0;

    if (!mem) {
        return;
    }

    /* Determine actual memory */
    cpr_mem = (cpr_mem_t *)((uint8_t *)mem - SIZE_OF_MEMORY_HEADER);

    /* Check if memory has already been freed */
    if ((uint32_t)((uintptr_t) cpr_mem->pc) == CPR_POISON) {
        CPR_ERROR("ERROR: attempt to double free memory, 0x%x\n", mem);
        return;
    }

    /* Determine size of memory */
    node = (cpr_mem_tracking_node_t *)cpr_get_mem_tracking_node(cpr_mem);
    if (!node) {
        /* Memory is not being tracked, so field is a size value */

        /* Record size for poisoning */
        size = cpr_get_memory_size(cpr_mem);

        cpr_check_redzone(cpr_mem, size);
    } else if (memory_tracking.enabled == TRUE) {
        /* Determine size from memory tracking node */

        if (node->magic == CPR_TRACKING_MAGIC) {
            size = node->size;
            cpr_check_redzone(cpr_mem, size);

            /* Free tracking node */
            cpr_release_mem_tracking_node(node);
        }
    } else if (node->magic == CPR_TRACKING_MAGIC) {
        /* Tracking node exists but memory tracking is off? */
        CPR_ERROR("Unexpected tracking node found while off, 0x%x\n", node);
#ifdef HELP
        if (chunk_free(tracking_chunk, node) == FALSE) {
            CPR_ERROR("Memory tracking is broken\n");
        }
#endif
    }
    /* else
     * Assume memory is no longer being tracked,
     * just skip the red-zone checking since the
     * size is not available.
     */

    /* Poison memory */
    cpr_poison_memory(cpr_mem, size);

    /* Actually free memory */
    FREE(cpr_mem);

    /* Peg statistics */
    memory_stats.frees++;
    memory_stats.in_use_count--;
    memory_stats.in_use_size -= size;

    /* Clear high-water mark condition if below low-water mark */
    if (memory_stats.in_use_size < MEM_LOW_WATER_MARK) {
        high_mem_use_condition = FALSE;
    }

    return;
}


/**
 * Force a crash dump which will allow a stack trace to be generated
 *
 * @return none
 *
 * @note crash dump is created by an illegal write to memory
 */
void
cpr_crashdump (void)
{
    *(volatile int *) 0xdeadbeef = 0x12345678;
}

/**
 * Determine if CPR is using more than the high-water mark of memory
 *
 * @return TRUE or FALSE
 *
 * @note The memory high-water mark is set to be 87.5% of
 *       total available CPR memory
 */
boolean
cpr_mem_high_water_mark (void)
{
    return high_mem_use_condition;
}



/**
 * Peg appropriate allocation counter bucket based on given size
 *
 * @param[in] size      Size of user requested memory
 *
 * @return none
 */
static INLINE void
cpr_peg_alloc_size (size_t size)
{
    if (size < 64) {
        memory_stats.allocs_1_to_63++;
    } else if (size < 256) {
        memory_stats.allocs_64_to_255++;
    } else if (size < 1024) {
        memory_stats.allocs_256_to_1k++;
    } else if (size < 4096) {
        memory_stats.allocs_1k_to_4k++;
    } else if (size < 32768) {
        memory_stats.allocs_4k_to_32k++;
    } else {
        memory_stats.allocs_32k_plus++;
    }
}

/**
 * Record user requested memory size in memory header
 *
 * @param[in] cpr_mem   CPR allocated memory
 * @param[in] size      Size of user requested memory
 *
 * @return none
 *
 * @pre (cpr_mem != NULL)
 * @pre ((size >> 31) == 0)
 *
 * @note This adjustment is done to merge the memory tracking node
 *       with the size field by taking advantage of the memory
 *       alignment of pointers being evenly aligned, i.e. the low
 *       order bit is zero.
 */
static void
cpr_record_memory_size (cpr_mem_t *cpr_mem, size_t size)
{
    cpr_assert_debug(cpr_mem != NULL);
    cpr_assert_debug((size >> 31) == 0);

    cpr_mem->u.size = size << 1 | 0x1;
}

/**
 * Get user requested memory size from memory header
 *
 * @param[in] cpr_mem   CPR allocated memory
 *
 * @return size of user-requested memory allocated
 *
 * @pre (cpr_mem != NULL)
 * @pre (cpr_mem->u.node != NULL)
 *
 * @see  cpr_record_memory_size
 */
static INLINE size_t
cpr_get_memory_size (cpr_mem_t *cpr_mem)
{
    size_t size;

    cpr_assert_debug(cpr_mem != NULL);

    if (cpr_mem->u.size & 0x1) {
        size = cpr_mem->u.size >> 1;
    } else {
        size = cpr_mem->u.node->size;
    }
    return size;
}

/**
 * Acquire and assign a memory tracking node to memory
 *
 * If a memory tracking node cannot be allocated, the
 * memory will be assigned the size as if memory tracking
 * was never enabled while the counter for missing memory
 * allocations is incremented.
 *
 * @param[in] cpr_mem   CPR allocated memory
 * @param[in] size      Size of user requested memory
 *
 * @return none
 * 
 * @pre (memory_tracking.enabled == TRUE)
 * @pre (cpr_mem != NULL) and (cpr_mem->pc != NULL)
 * @pre (size != 0)
 */
static void
cpr_assign_mem_tracking_node (cpr_mem_t *cpr_mem, size_t size)
{
    cpr_mem_tracking_node_t *node;

    cpr_assert_debug(memory_tracking.enabled == TRUE);
    cpr_assert_debug(cpr_mem != NULL);
    cpr_assert_debug(cpr_mem->pc != NULL);
    cpr_assert_debug(size != 0);

    node = chunk_malloc(tracking_chunk);
    if (!node) {
        /* Failed, so continue best effort */
        if (!(memory_tracking.flags & MEM_TRACKING_FLAG_OOC)) {
            memory_tracking.flags |= MEM_TRACKING_FLAG_OOC;
            CPR_ERROR("WARNING: Out-of-chunks for memory tracking!\n");
        }
        memory_tracking.missing_allocs++;

        /* Treat memory as if tracking is off */
        cpr_record_memory_size(cpr_mem, size);
        return;
    }

    /* Fill in node */
    node->mem       = cpr_mem;
    node->pc        = cpr_mem->pc;
    node->size      = size;
    node->timestamp = time(NULL);
    node->magic     = CPR_TRACKING_MAGIC;

    /* Get tracking lock */
    (void) cprGetMutex(mem_tracking_mutex);

    /* Add to tail of list (this keeps the oldest first) */
    node->next = NULL;
    node->prev = memory_tracking.tail;
    if (memory_tracking.tail != NULL) {
        memory_tracking.tail->next = node;
    } else {
        memory_tracking.head = node;
    }
    memory_tracking.tail = node;

    /* Release tracking lock */
    (void) cprReleaseMutex(mem_tracking_mutex);

    /* Attach node to memory */
    cpr_mem->u.node = node;
}

/**
 * Release a memory tracking element
 *
 * @param[in] node  Memory tracking element
 *
 * @return none
 * 
 * @pre (node != NULL)
 *
 * @note There is special handling to help with the displaying
 *       of memory tracking nodes, refer to cpr_show_memory_tracking
 *       for details.
 */
static void
cpr_release_mem_tracking_node (cpr_mem_tracking_node_t *node)
{
    static const char *fname = "cpr_release_mem_tracking_node";

    cpr_assert_debug(node != NULL);

    /* Acquire tracking lock */
    (void) cprGetMutex(mem_tracking_mutex);

    /* Check if this matches the next node to display */
    if (node == next_display_node) {
        /* Uh-oh...about to collide with the walking the list
         * of tracked nodes being displayed.  Need to update
         * the next display node to this node's next.
         */
        next_display_node = node->next;

        /* Flag condition */
        CPR_INFO("%s: Correcting node being displayed\n", fname);
    }

    /* Release node from active list */
    if (node->next) {
        node->next->prev = node->prev;
    } else if (node == memory_tracking.tail) {
        memory_tracking.tail = node->prev;
    }

    if (node->prev) {
        node->prev->next = node->next;
    } else if (node == memory_tracking.head) {
        memory_tracking.head = node->next;
    }
    node->next = NULL;
    node->prev = NULL;

    /* Release tracking lock */
    (void) cprReleaseMutex(mem_tracking_mutex);

    /* Free tracking node from chunk manager */
    if (chunk_free(tracking_chunk, node) == FALSE) {
        /* Not much can be done, just report the error */
        if (!(memory_tracking.flags & MEM_TRACKING_FLAG_FREE_FAILED)) {
            CPR_ERROR("Error: Memory tracking is broken\n");
            memory_tracking.flags |= MEM_TRACKING_FLAG_FREE_FAILED;
        }
    }
}

/**
 * Get the memory tracking node, if present, from the memory header
 *
 * @param[in] cpr_mem  CPR allocated memory
 *
 * @return Pointer to the memory tracking node or #NULL
 *
 * @pre (cpr_mem != NULL)
 */
static INLINE cpr_mem_tracking_node_t *
cpr_get_mem_tracking_node (cpr_mem_t *cpr_mem)
{
    cpr_assert_debug(cpr_mem != NULL);

    if ((cpr_mem->u.size & 0x1) == 0) {
        return cpr_mem->u.node;
    }
    return NULL;
}

/**
 * Check the red zone for the given memory
 *
 * @param[in] cpr_mem   CPR allocated memory
 * @param[in] size      Size of user requested memory
 *
 * @return none
 *
 * @pre    (cpr_mem != NULL)
 * @pre    (size > 0)
 *
 * @note   Any red zone error is recorded to syslog and the
 *         last instance is saved for later retrieval via
 *         the debug command shell
 * @note   While the size can be determined from cpr_mem,
 *         the calling routine is expected to have already
 *         determined the size; therefore, for efficiency
 *         reasons the value is passed
 */
static void
cpr_check_redzone (cpr_mem_t *cpr_mem, size_t size)
{
    uint32_t redzone;
    uint32_t *rz;

    cpr_assert_debug(cpr_mem != NULL);
    cpr_assert_debug(size > 0);

    /* Determine pointer to red-zone */
    rz = (uint32_t *)((uint8_t *)cpr_mem + offsetof(cpr_mem_t, mem) + size);

    /* Read red-zone */
    redzone = READ_4BYTES_UNALIGNED(rz, redzone);

    /* Determine if red-zone valid */
    if (redzone != CPR_REDZONE) {
        /* Record failure */
        memory_stats.redzone_pc = cpr_mem->pc;
        memory_stats.redzone_size = size;
        memcpy(memory_stats.redzone_memory, (uint8_t *)cpr_mem +
               offsetof(cpr_mem_t, mem) + size - REDZONE_RECORD_SIZE/2,
               REDZONE_RECORD_SIZE);

        CPR_ERROR("Red-zone failure: caller_pc = %p, size = %d, rz = %x\n",
                  cpr_mem->pc, size, redzone);
        memory_stats.redzone_violations++;
    }
}

/**
 * Poison memory with a fixed poison value up to the given size in bytes
 *
 * @param[in] cpr_mem   CPR allocated memory
 * @param[in] size      number of bytes to poison
 *
 * @return none
 *
 * @pre (cpr_mem != NULL)
 * @pre ((size >> 31) == 0)
 * @pre (((cpr_mem->u.size & 0x1) == 0) or
         ((cpr_mem->u.size >> 1) >= size))
 * @post (size < poison_size) or
 *       (*(mem + poison_size) != CPR_POISON)
 *
 * @note poison_size is configurable via the
 *       debug shell, config cpr-memory poison [0-64]
 * @note the number of bytes actually poisoned via size
 *       is rounded down to number divisible by 4,
 *       i.e. (size / 4) * 4 so a size of 15 will only
 *       poison 12 bytes
 */
static void
cpr_poison_memory (cpr_mem_t *cpr_mem, size_t size)
{
    uint32_t *p;
    uint32_t *end;
    const char *fname = "cpr_poison_memory";

    cpr_assert_debug(cpr_mem != NULL);
    cpr_assert_debug((size >> 31) == 0);
    /* Since the asserts are NOPed right now, have a check that will return from
     * this function if any of the above conditions are true.
     */
    if ((cpr_mem != NULL) && ((size >> 31) == 0)) {
        cpr_assert_debug(((cpr_mem->u.size & 0x1) == 0) or
                ((cpr_mem->u.size >> 1) >= size));

        /* Poison memory header */
        WRITE_4BYTES_MEM_ALIGNED(&cpr_mem->u.node, 0);
        WRITE_4BYTES_MEM_ALIGNED(&cpr_mem->pc, CPR_POISON);
        if (sizeof(cpr_mem->pc) >= 8)
            WRITE_4BYTES_MEM_ALIGNED(((uintptr_t)&cpr_mem->pc) + 4, CPR_POISON);

        /* Determine start of memory */
        p = (uint32_t *) GET_USER_MEM_PTR(cpr_mem);

        /* Determine amount to be poisoned, recording the end pointer */
        end = (uint32_t *)((uint8_t *)p + MIN(poison_size, size));

        /* Poison memory */
        while (p < end) {
            WRITE_4BYTES_MEM_ALIGNED(p, CPR_POISON);
            p++;
        }
    } else {
        CPR_INFO("%s: Parameters passed in are wrong. cpr_mem: 0x%x, size: %d.\n", fname, cpr_mem, size);
    }
}

/**
 * Display memory configuration "show cpr-memory"
 *
 * @return  none, all output set to terminal
 */
static void
cpr_show_memory_config (void)
{
    debugif_printf("SIP memory management configuration:\n");
    debugif_printf("\tMemory tracking %s\n",
            (memory_tracking.enabled == TRUE) ? "on" : "off");
    debugif_printf("\tMemory tracking size: %d\n",
            memory_tracking.max_size);
    debugif_printf("\tPoison memory size: %d\n",
            poison_size);
    // TODO: need to support
    //debugif_printf("\tTimestamp of last mark: %u\n",
    //         cpr_memory_stats_timestamp);
    if (strlen(FAULT_INSERTION_CONFIG)) {
        debugif_printf("\t");
        debugif_printf(FAULT_INSERTION_CONFIG);
        debugif_printf("\n");
    }

    debugif_printf("\n");
}

/**
 * Display memory statistics "show cpr-memory statistics"
 *
 * @return  none, all output set to terminal
 */
static void
cpr_show_memory_statistics (void)
{
    debugif_printf("SIP memory statistics:\n");
    debugif_printf("\tTotal memory allocated  : %u\n",
            memory_stats.total_alloc_size);
    debugif_printf("\tMaximum memory allocated: %u\n",
            memory_stats.max_use_size);
    debugif_printf("\tTotal memory allocations: %u\n",
            memory_stats.total_allocs);
    debugif_printf("\t 1 to 63 bytes   : %6u\n",
            memory_stats.allocs_1_to_63);
    debugif_printf("\t 64 to 255 bytes : %6u\n",
            memory_stats.allocs_64_to_255);
    debugif_printf("\t 256 to 1k bytes : %6u\n",
            memory_stats.allocs_256_to_1k);
    debugif_printf("\t 1k to 4k bytes  : %6u\n",
            memory_stats.allocs_1k_to_4k);
    debugif_printf("\t 4k to 32k bytes : %6u\n",
            memory_stats.allocs_4k_to_32k);
    debugif_printf("\t 32k plus bytes  : %6u\n",
            memory_stats.allocs_32k_plus);
    debugif_printf("\t Malloc : %6u\n",
            memory_stats.mallocs);
    debugif_printf("\t Calloc : %6u\n",
            memory_stats.callocs);
    debugif_printf("\t Realloc: %6u\n",
            memory_stats.reallocs);
    debugif_printf("\t Free   : %6u\n",
            memory_stats.frees);
    debugif_printf("\tIn-use count: %5u\n",
            memory_stats.in_use_count);
    debugif_printf("\tIn-use size: %6u\n",
            memory_stats.in_use_size);
    debugif_printf("\tRedzone violations: %u\n\n",
            memory_stats.redzone_violations);
    if (memory_stats.redzone_violations) {
        debugif_printf("Last Redzone Violation:\n");
        debugif_printf("\tSize of memory = %u\n", memory_stats.redzone_size);
        debugif_printf("\tAllocator PC = 0x%x\n", memory_stats.redzone_pc);
        cpr_dump_memory((uint32_t *)memory_stats.redzone_memory,
                        REDZONE_RECORD_SIZE);
    }
}

/**
 * Dump memory content to terminal
 *
 * @param[in] memory   Pointer to memory to dump
 * @param[in] size     Size to dump in bytes
 *
 * @return none, all output set to terminal
 *
 * @pre (memory != NULL)
 *
 * @warning This should not be widely used for dumping any memory
 *          for security reasons!!!
 */
static void
cpr_dump_memory (uint32_t *memory, size_t size)
{
    uint16_t i;
    uint16_t idx;

    /*
     * Recompute size to be a multiple of 4 which
     * is divided by 4.  This means any remainder is
     * rounded up.
     *
     * size = size / 4 + ((size % 4) ? 1 : 0);
     */
    size = (size >> 2) + ((size & 3) ? 1 : 0);

    for (i = 0; i < size; i++) {
        idx = i & 3;
        if (!idx) {
            debugif_printf("\n\t%x: ", memory);
            if (i != 0) {
                memory += 4;
            }
        }
        debugif_printf("%08x  ", memory[idx]);
    }
    debugif_printf("\n");
}


/**
 * Display memory management tracking "show cpr-memory tracking"
 *
 * @return  none, all output set to terminal
 *
 * @note The @c next_display_node variable is shared between this
 *       routine and cpr_release_mem_tracking_node.  The need is
 *       due to not wanting to block memory additions and removals
 *       while displaying the whole list which could be quite
 *       large.  So, this creates the problem while walking the
 *       list the next node to display is freed during the data
 *       dump of the current node.  The only one who knows this
 *       occurred is the release routine which will detect the
 *       problem and fix the next_display_node to skip over the
 *       freed node.
 *
 * @note If memory tracking is disabled during the display of
 *       tracking nodes, the displaying will stop.
 */
static void
cpr_show_memory_tracking (void)
{
    cpr_mem_tracking_node_t node;
    cpr_mem_tracking_node_t *display_node;
    cpr_time_t timestamp;

    if (memory_tracking.enabled == FALSE) {
        debugif_printf("CPR memory tracking not enabled\n");
        return;
    }

    /* Get current time */
    timestamp = time(NULL);

    debugif_printf("CPR memory tracking size = %u, missed allocations = %u\n",
                   memory_tracking.max_size, memory_tracking.missing_allocs);
    if (memory_tracking.flags & MEM_TRACKING_FLAG_FREE_FAILED) {
        debugif_printf("   problems occurred with freeing tracking nodes\n\n");
    }
    debugif_printf("__Memory__ TimeDelta  __Size__ ____PC____\n");

    if (memory_tracking.head) {
        /* Acquire tracking lock */
        (void) cprGetMutex(mem_tracking_mutex);

        /* Get the starting node */
        next_display_node = memory_tracking.head;

        /* Release tracking lock */
        (void) cprReleaseMutex(mem_tracking_mutex);
    } else {
        debugif_printf("Currently no memory allocation being tracked\n");
        return;
    }

    while ((memory_tracking.enabled == TRUE) &&
           (next_display_node != NULL)) {
        /* Acquire tracking lock */
        (void) cprGetMutex(mem_tracking_mutex);

        /* Force the check to see if the node has changed */
        display_node = (cpr_mem_tracking_node_t *) next_display_node;

        /* Copy node content */
        memcpy(&node, display_node, sizeof(cpr_mem_tracking_node_t));
        //node = *display_node;

        /* Save reference to next node */
        next_display_node = display_node->next;

        /* Release tracking lock */
        (void) cprReleaseMutex(mem_tracking_mutex);

        /* Dump node's content and print as string*/
        debugif_printf("0x%08x %10ld %8u 0x%08x: %s:!!!\n", node.mem,
                       timestamp - node.timestamp,
                       node.size, node.pc, (int *)node.mem + SIZE_OF_MEMORY_HEADER);
    }

    /* Done dump node content */
    next_display_node = NULL;
}

/**
 * Display memory configuration "show cpr-memory {keyword}"
 * where the keyword is: statistics, configuration, heap-guard
 * or tracking
 *
 * @return  zero(0) or one(1) if no match found
 *
 * @note    All output sent to debugif_printf()
 */
cc_int32_t
cpr_show_memory (cc_int32_t argc, const char *argv[])
{
    uint8_t i = 0;

    /* work even if the "show cpr-memory" is present */
    if (argc && strcasecmp("show", argv[i]) == 0) {
        argc--;
        i++;
    }
    if (argc && strcasecmp("cpr-memory", argv[i]) == 0) {
        argc--;
        i++;
    }

    /* parse the real stuff */
    if (argc == 0) {
        cpr_show_memory_usage();
    }
    else if ((strcasecmp(argv[i], "statistics") == 0) ||
             (strcasecmp(argv[i], "stats") == 0)) {
        cpr_show_memory_statistics();
    }
    else if ((strcasecmp(argv[i], "config") == 0) ||
             (strcasecmp(argv[i], "configuration") == 0)) {
        cpr_show_memory_config();
    }
    else if (strcasecmp(argv[i], "tracking") == 0) {
        cpr_show_memory_tracking();
    }
    else {
        cpr_show_memory_usage();
        /*
         * Normally would return the value of one(1), but this
         * just adds a useless "debug: Error 1" message that
         * really is not necessary, so zero(0) will just avoid it
         */
        return 0;
    }

    return 0;
}

/**
 * Display usage statement for "show cpr-memory"
 *
 * @return none, all output goes to the terminal
 */
static void
cpr_show_memory_usage (void)
{
    debugif_printf("\nshow cpr-memory [options]:\n");
    debugif_printf("\tconfig    \t- CPR memory configuration settings\n");
    debugif_printf("\tstatistics\t- CPR memory statistics\n");
    debugif_printf("\ttracking  \t- CPR memory tracked allocations\n\n");
}

/**
 * Clear the memory tracking nodes, i.e. reset
 *
 * @return none, all output goes to the terminal
 *
 * @note This could be made to be more sophisticated by not actually
 *       destroying the chunk, but releasing all of the chunk's
 *       contents
 */
static void
cpr_clear_memory_tracking (void)
{
    if (memory_tracking.enabled == TRUE) {
        cpr_disable_memory_tracking();
        cpr_enable_memory_tracking();
        debugif_printf("CPR memory tracking cleared/reset\n");
    } else {
        debugif_printf("CPR memory tracking is currently disabled\n");
    }
}

/**
 * Clear memory command "clear cpr-memory {statistics | tracking}"
 *
 * @return  zero(0) or one(1) if no match found
 *
 * @note    All output sent to debugif_printf()
 */
cc_int32_t
cpr_clear_memory (cc_int32_t argc, const char *argv[])
{
    uint8_t i = 0;

    /* work even if the "clear cpr-memory" is present */
    if (argc && strcasecmp("clear", argv[i]) == 0) {
        argc--;
        i++;
    }
    if (argc && strcasecmp("cpr-memory", argv[i]) == 0) {
        argc--;
        i++;
    }

    /* parse the real stuff */
    if (argc == 0) {
        debugif_printf("\nclear cpr-memory [options]:\n");
        debugif_printf("\tstatistics\t- clear cpr-memory statistics\n");
        debugif_printf("\ttracking  \t- clear cpr-memory tracking\n");
    }
    else if ((strcasecmp(argv[i], "statistics") == 0) ||
             (strcasecmp(argv[i], "stats") == 0)) {
        /* Copy the statistics */
        memcpy(&saved_memory_stats, &memory_stats,
               sizeof(cpr_mem_statistics_t));
        memset(&memory_stats, 0, sizeof(cpr_mem_statistics_t));
        debugif_printf("\ncleared cpr-memory statistics\n");
    }
    else if (strcasecmp(argv[i], "tracking") == 0) {
        cpr_clear_memory_tracking();
    }
    else {
        /*
         * Normally would return the value of one(1), but this
         * just adds a useless "debug: Error 1" message that
         * really is not necessary, so zero(0) will just avoid it
         */
        debugif_printf("\nNo match - no operation performed\n");
        debugif_printf("\nclear cpr-memory [options]:\n");
        debugif_printf("\tstatistics\t- clear cpr-memory statistics\n");
        debugif_printf("\ttracking  \t- clear cpr-memory tracking\n");
        return 0;
    }

    return 0;
}

/**
 * Parse the debug memory args and enable/disable the
 * appropriate settings.  The following commands are supported:
 * 
 * @li tracking [0-max size]
 * @li poison-size [0-max size]
 * @li fault-insertion [every] [#]
 *
 * @param[in] argc  The number of arguments, including 'debug'
 * @param[in] argv  The argument tokens
 *
 * @return zero(0) for success and one(1) for failure
 *
 * @note Additional arguments are ignored
 */
static int32_t
cpr_debug_memory_cli (int32_t argc, const char *argv[])
{
    boolean debug;
    int32_t rc = 0;

    /*
     * Enable or disable?
     */
    if (strcasecmp(argv[0], "debug") == 0)
    {
        debug = TRUE;
    }
    else if (strcasecmp(argv[0], "undebug") == 0)
    {
        debug = FALSE;
    }
    else
    {
        debugif_printf("Unknown prefix (%s) not 'debug' or 'undebug'\n");
        return 1;
    }

    if (debug) {
        /*
         * Enough args?
         */
        if (argc <= 2) {
            debugif_printf("\nMissing required args\n");
            debugif_printf("debug cpr-memory [options]:\n");
            debugif_printf("\ttracking - configuration of CPR memory tracking\n");
            debugif_printf("\tpoison-size - the number of bytes poisoned when"
                           " memory freed\n");
            /*
             * Normally would return the value of one(1), but this
             * just adds a useless "debug: Error 1" message that
             * really is not necessary, so zero(0) will just avoid it
             */
            return 0;
        }
    } else {
        if (argc <= 2 || (strcasecmp(argv[2], "all") == 0)) {
            rc = cpr_set_memory_tracking(debug, 0, NULL);
            rc = cpr_set_poison_size(debug, 0, NULL);
            return (int32_t)rc;
        }
    }
    /*
     * Parse command
     */
    if (strcasecmp(argv[2], "tracking") == 0) {
        rc = cpr_set_memory_tracking(debug, argc - 3, &argv[3]);
    }
    else if (strcasecmp(argv[2], "poison-size") == 0) {
        rc = cpr_set_poison_size(debug, argc - 3, &argv[3]);
    }
    /* Hook to allow for compile-time addition of 'fault-insertion' */
    PARSE_FAULT_INSERTION_CLI(argv[2], debug, argc - 3, &argv[3])
    else {
        debugif_printf("Error: Unable to parse command at token: %s\n", argv[2]);
        cpr_debug_memory_cli_usage();
        rc = 1;
    }

    return (int32_t) rc;
}

/**
 * Display usage statement for "debug cpr-memory"
 *
 * @return none, all output goes to the terminal
 */
static void
cpr_debug_memory_cli_usage (void)
{
    debugif_printf("debug cpr-memory [options]:\n");
    debugif_printf("\ttracking [options] - configuration of"
                   " CPR memory tracking\n");
    debugif_printf("\tpoison-size        - the number of bytes"
                   " poisoned when memory freed\n");
}

/**
 * Set the memory tracking appropriately based on command
 *
 * The command format:
 * [no] debug cmd-prefix tracking [size [0 - MAX_TRACKING_SIZE]]
 *
 * @param[in] debug   Indicate positive (debug) or negative (no debug/undebug)
 * @param[in] argc    The number of remaining args after "tracking"
 * @param[in] argv    The argument tokens after "tracking"
 *
 * @return zero(0) for success and one(1) for failure, in either case
 *         all output sent to terminal
 *
 * @note Additional arguments are ignored after those that are required
 *       have been parsed, i.e. when debug == FALSE all arguments
 *       are ignored.
 */
static boolean
cpr_set_memory_tracking (boolean debug, int32_t argc, const char *argv[])
{
    const char *cmd = "tracking";
    uint32_t size;
    boolean rc = 0;

    if (argc == 0) {
        if (debug == TRUE) {
            cpr_enable_memory_tracking();
        } else {
            cpr_disable_memory_tracking();
        }
        debugif_printf("%s %s %s\n", cmd_prefix, cmd,
                       (memory_tracking.enabled == TRUE) ?
                       "enabled" : "disabled");
    } else if (strcasecmp(argv[0], "?") == 0) {
        debugif_printf("debug cpr-memory tracking [size [0-%d]]\n",
                       MAX_TRACKING_SIZE);
    } else if (strcasecmp(argv[0], "size") == 0) {
        if ((argc == 1) || (debug == FALSE)) {
            memory_tracking.max_size = DEFAULT_TRACKING_SIZE;
            debugif_printf("%s %s size restored to default of %d\n",
                           cmd_prefix, cmd, memory_tracking.max_size);
        } else {
            size = atoi(argv[1]);

            if (size > MAX_TRACKING_SIZE) {
                debugif_printf("ERROR: %d is out-of-range, 0 - %d\n",
                               size, MAX_TRACKING_SIZE);
                rc = 1;
            } else {
                memory_tracking.max_size = (uint16_t) size;
            }
            debugif_printf("%s %s size set to %d\n", cmd_prefix,
                           cmd, memory_tracking.max_size);
        }
    } else {
        debugif_printf("Error: unabled to parse command at token, %s\n", argv[3]);
        debugif_printf("debug cpr-memory tracking [size [0-%d]]\n",
                       MAX_TRACKING_SIZE);
        rc = 1;
    }

    return rc;
}

/**
 * Enable debug memory tracking if not already enabled.
 *
 * @return none, all output sent to terminal
 */
static INLINE void
cpr_enable_memory_tracking (void)
{
    if (!tracking_chunk) {
        tracking_chunk = chunk_create(sizeof(cpr_mem_tracking_node_t),
                                             memory_tracking.max_size,
                                             CHUNK_FLAGS_NONE, 0,
                                             "CPR Memory Tracking");
        if (!tracking_chunk) {
            debugif_printf("Error: unable to initiate tracking of memory\n");
        } else {
            memory_tracking.enabled = TRUE;
            memory_tracking.timestamp = time(NULL);
        }
    } else if (memory_tracking.enabled == FALSE) {
        debugif_printf("Warning: memory_tracking not enabled,"
                       " but tracking chunk present\n");
        memory_tracking.enabled = TRUE;
        memory_tracking.timestamp = time(NULL);
    }
}

/**
 * Disable debug memory tracking and release memory tracking chunk
 * if not already freed.
 *
 * @return none, all output sent to terminal
 */
static INLINE void
cpr_disable_memory_tracking (void)
{
    memory_tracking.enabled = FALSE;
    memory_tracking.head = NULL;
    memory_tracking.tail = NULL;
    memory_tracking.real_size = 0;
    memory_tracking.flags &= ~(MEM_TRACKING_FLAG_OOC |
                               MEM_TRACKING_FLAG_FREE_FAILED);
    memory_tracking.missing_allocs = 0;

    if (tracking_chunk) {
         if (chunk_destroy_forced(tracking_chunk) == FALSE) {
             debugif_printf("Error: unable to release tracking chunk\n");
         } else {
             tracking_chunk = NULL;
         }
    }
}

/**
 * Set the poison size appropriately based on command
 *
 * The command format:
 * [no] debug cmd-prefix poison-size [0 - MAX_POISON_SIZE]
 *
 * @param[in] debug   Indicate positive (debug) or negative (no debug/undebug)
 * @param[in] argc    The number of remaining args after "poison-size"
 * @param[in] argv    The argument tokens after "poison-size"
 *
 * @return zero(0) for success and one(1) for failure, in either case
 *         all output sent to terminal
 *
 * @note Additional arguments are ignored after those that are required
 *       have been parsed, i.e. when debug == FALSE all arguments
 *       are ignored.
 */
static boolean
cpr_set_poison_size (boolean debug, int32_t argc, const char **argv)
{
    const char *cmd = "poison size";
    boolean rc = 0;

    if (debug == FALSE) {
        /* no debug / undebug scenario, restore to default */
        poison_size = DEFAULT_POISON_SIZE;
        debugif_printf("%s %s reset to default of %d\n", cmd_prefix, cmd,
                       poison_size);
    } else if (argc) {
        /* set to a new value, if possible */
        uint32_t size = atoi(argv[0]);

        if (size > MAX_POISON_SIZE) {
            debugif_printf("ERROR: %d is out-of-range, 0 - %d\n",
                               size, MAX_POISON_SIZE);
            rc = 1;
        } else {
            poison_size = size;
        }
        debugif_printf("%s %s set to %d\n", cmd_prefix, cmd, poison_size);
    } else {
        /* just report the current setting */
        debugif_printf("%s %s set to %d\n", cmd_prefix, cmd, poison_size);
    }

    return rc;
}

void debugCprMem(cc_debug_cpr_mem_options_e category, cc_debug_flag_e flag)
{
    /* Currently hardcoding to just enable "cpr-mem tracking" or "cpr-mem
     * poison-size". More options not being given to vendors for now.
     */
    static const char *debugArgs[NUM_OF_CPR_SHOW_ARGUMENTS];

    if (flag == CC_DEBUG_DISABLE) {
        debugArgs[0] = "undebug";
    } else if (flag == CC_DEBUG_ENABLE) {
        debugArgs[0] = "debug";
    } else {
        debugif_printf("Incorrect Arguments. The \"flag\" can be CC_DEBUG_DISABLE/ENABLE\n");
    }
    debugArgs[1] = "cpr-memory";

    if (category == CC_DEBUG_CPR_MEM_TRACKING) {
        debugArgs[2] = "tracking";
    } else if (category == CC_DEBUG_CPR_MEM_POISON) {
        debugArgs[2] = "poison-size";
    } else {
        debugif_printf("Incorrect Arguments. The \"category\" can be CC_DEBUG_CPR_MEM_TRACKING/POISON\n");
    }
    cpr_debug_memory_cli(NUM_OF_CPR_SHOW_ARGUMENTS, debugArgs);
}

void debugClearCprMem(cc_debug_clear_cpr_options_e category)
{
    static const char *debugArgs[NUM_OF_CPR_SHOW_ARGUMENTS];

    debugArgs[0] = "clear";
    debugArgs[1] = "cpr-memory";

    if (category == CC_DEBUG_CLEAR_CPR_TRACKING) {
        debugArgs[2] = "tracking";
    } else if (category == CC_DEBUG_CLEAR_CPR_STATISTICS) {
        debugArgs[2] = "statistics";
    } else {
        debugif_printf("Incorrect Arguments. The \"category\" can be CC_DEBUG_CLEAR_CPR_TRACKING/STATISTICS\n");
    }
    cpr_clear_memory(NUM_OF_CPR_SHOW_ARGUMENTS, debugArgs);
}

void debugShowCprMem(cc_debug_show_cpr_options_e category)
{
    static const char *debugArgs[NUM_OF_CPR_SHOW_ARGUMENTS];

    debugArgs[0] = "show";
    debugArgs[1] = "cpr-memory";

    if (category == CC_DEBUG_SHOW_CPR_CONFIG) {
        debugArgs[2] = "config";
    } else if (category == CC_DEBUG_SHOW_CPR_HEAP_GUARD) {
        debugArgs[2] = "heap-guard";
    } else if (category == CC_DEBUG_SHOW_CPR_STATISTICS) {
        debugArgs[2] = "statistics";
    } else if (category == CC_DEBUG_SHOW_CPR_TRACKING) {
        debugArgs[2] = "tracking";
    } else {
        debugif_printf("Incorrect Arguments. The \"options\" can be one of show-cpr-options\n");
    }

    cpr_show_memory(NUM_OF_CPR_SHOW_ARGUMENTS, debugArgs);
}
/** @} */ /* End private APIs */




