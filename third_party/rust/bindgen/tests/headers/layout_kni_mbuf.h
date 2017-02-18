
#define RTE_CACHE_LINE_MIN_SIZE 64	/**< Minimum Cache line size. */

#define RTE_CACHE_LINE_SIZE 64

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

/*
 * The kernel image of the rte_mbuf struct, with only the relevant fields.
 * Padding is necessary to assure the offsets of these fields
 */
struct rte_kni_mbuf {
	void *buf_addr __attribute__((__aligned__(RTE_CACHE_LINE_SIZE)));
	uint64_t buf_physaddr;
	char pad0[2];
	uint16_t data_off;      /**< Start address of data in segment buffer. */
	char pad1[2];
	uint8_t nb_segs;        /**< Number of segments. */
	char pad4[1];
	uint64_t ol_flags;      /**< Offload features. */
	char pad2[4];
	uint32_t pkt_len;       /**< Total pkt len: sum of all segment data_len. */
	uint16_t data_len;      /**< Amount of data in segment buffer. */

	/* fields on second cache line */
	char pad3[8] __attribute__((__aligned__(RTE_CACHE_LINE_MIN_SIZE)));
	void *pool;
	void *next;
};
