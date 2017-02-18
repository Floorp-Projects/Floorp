typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef long long size_t;

#define RTE_CACHE_LINE_SIZE 64

/**
 * Force alignment
 */
#define __rte_aligned(a) __attribute__((__aligned__(a)))

/**
 * Force alignment to cache line.
 */
#define __rte_cache_aligned __rte_aligned(RTE_CACHE_LINE_SIZE)

#define RTE_MEMPOOL_OPS_NAMESIZE 32 /**< Max length of ops struct name. */

/**
 * Prototype for implementation specific data provisioning function.
 *
 * The function should provide the implementation specific memory for
 * for use by the other mempool ops functions in a given mempool ops struct.
 * E.g. the default ops provides an instance of the rte_ring for this purpose.
 * it will most likely point to a different type of data structure, and
 * will be transparent to the application programmer.
 * This function should set mp->pool_data.
 */
typedef int (*rte_mempool_alloc_t)(struct rte_mempool *mp);

/**
 * Free the opaque private data pointed to by mp->pool_data pointer.
 */
typedef void (*rte_mempool_free_t)(struct rte_mempool *mp);

/**
 * Enqueue an object into the external pool.
 */
typedef int (*rte_mempool_enqueue_t)(struct rte_mempool *mp,
		void * const *obj_table, unsigned int n);

/**
 * Dequeue an object from the external pool.
 */
typedef int (*rte_mempool_dequeue_t)(struct rte_mempool *mp,
		void **obj_table, unsigned int n);

/**
 * Return the number of available objects in the external pool.
 */
typedef unsigned (*rte_mempool_get_count)(const struct rte_mempool *mp);
/** Structure defining mempool operations structure */
struct rte_mempool_ops {
	char name[RTE_MEMPOOL_OPS_NAMESIZE]; /**< Name of mempool ops struct. */
	rte_mempool_alloc_t alloc;       /**< Allocate private data. */
	rte_mempool_free_t free;         /**< Free the external pool. */
	rte_mempool_enqueue_t enqueue;   /**< Enqueue an object. */
	rte_mempool_dequeue_t dequeue;   /**< Dequeue an object. */
	rte_mempool_get_count get_count; /**< Get qty of available objs. */
} __rte_cache_aligned;

#define RTE_MEMPOOL_MAX_OPS_IDX 16  /**< Max registered ops structs */

/**
 * The rte_spinlock_t type.
 */
typedef struct {
	volatile int locked; /**< lock status 0 = unlocked, 1 = locked */
} rte_spinlock_t;

/**
 * Structure storing the table of registered ops structs, each of which contain
 * the function pointers for the mempool ops functions.
 * Each process has its own storage for this ops struct array so that
 * the mempools can be shared across primary and secondary processes.
 * The indices used to access the array are valid across processes, whereas
 * any function pointers stored directly in the mempool struct would not be.
 * This results in us simply having "ops_index" in the mempool struct.
 */
struct rte_mempool_ops_table {
	rte_spinlock_t sl;     /**< Spinlock for add/delete. */
	uint32_t num_ops;      /**< Number of used ops structs in the table. */
	/**
	 * Storage for all possible ops structs.
	 */
	struct rte_mempool_ops ops[RTE_MEMPOOL_MAX_OPS_IDX];
} __rte_cache_aligned;


/* Number of free lists per heap, grouped by size. */
#define RTE_HEAP_NUM_FREELISTS  13

#define LIST_HEAD(name, type)                                           \
struct name {                                                           \
		struct type *lh_first;  /* first element */                     \
}

/**
 * Structure to hold malloc heap
 */
struct malloc_heap {
	rte_spinlock_t lock;
	LIST_HEAD(, malloc_elem) free_head[RTE_HEAP_NUM_FREELISTS];
	unsigned alloc_count;
	size_t total_size;
} __rte_cache_aligned;
