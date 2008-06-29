#ifndef MOZ_MEMORY_WINDOWS
#  include <stdbool.h>
#else
#  include <windows.h>
#  ifndef bool
#    define bool BOOL
#  endif
#endif

extern const char	*_malloc_options;

/*
 * jemalloc_stats() is not a stable interface.  When using jemalloc_stats_t, be
 * sure that the compiled results of jemalloc.c are in sync with this header
 * file.
 */
typedef struct {
	/*
	 * Run-time configuration settings.
	 */
	bool	opt_abort;	/* abort(3) on error? */
	bool	opt_dss;	/* Use sbrk(2) to map memory? */
	bool	opt_junk;	/* Fill allocated/free memory with 0xa5/0x5a? */
	bool	opt_mmap;	/* Use mmap(2) to map memory? */
	bool	opt_utrace;	/* Trace all allocation events? */
	bool	opt_sysv;	/* SysV semantics? */
	bool	opt_xmalloc;	/* abort(3) on OOM? */
	bool	opt_zero;	/* Fill allocated memory with 0x0? */
	size_t	narenas;	/* Number of arenas. */
	size_t	balance_threshold; /* Arena contention rebalance threshold. */
	size_t	quantum;	/* Allocation quantum. */
	size_t	small_max;	/* Max quantum-spaced allocation size. */
	size_t	large_max;	/* Max sub-chunksize allocation size. */
	size_t	chunksize;	/* Size of each virtual memory mapping. */
	size_t	dirty_max;	/* Max dirty pages per arena. */

	/*
	 * Current memory usage statistics.
	 */
	size_t	mapped;		/* Bytes mapped (not necessarily committed). */
	size_t	committed;	/* Bytes committed (readable/writable). */
	size_t	allocated;	/* Bytes allocted (in use by application). */
	size_t	dirty;		/* Bytes dirty (committed unused pages). */
} jemalloc_stats_t;

#ifndef MOZ_MEMORY_DARWIN
void	*malloc(size_t size);
void	*valloc(size_t size);
void	*calloc(size_t num, size_t size);
void	*realloc(void *ptr, size_t size);
void	free(void *ptr);
#endif

int	posix_memalign(void **memptr, size_t alignment, size_t size);
void	*memalign(size_t alignment, size_t size);
size_t	malloc_usable_size(const void *ptr);
void	jemalloc_stats(jemalloc_stats_t *stats);
