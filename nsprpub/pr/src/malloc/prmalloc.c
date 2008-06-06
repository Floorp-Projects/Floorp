/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "primpl.h"

/*
** We override malloc etc. on any platform which has preemption +
** nspr20 user level threads.  When we're debugging, we can make our
** version of malloc fail occasionally.
*/
#ifdef _PR_OVERRIDE_MALLOC

/*
** Thread safe version of malloc, calloc, realloc, free
*/
#include <stdarg.h>

#ifdef DEBUG
#define SANITY
#define EXTRA_SANITY
#else
#undef SANITY
#undef EXTRA_SANITY
#endif

/* Forward decls */
void *_PR_UnlockedMalloc(size_t size);
void _PR_UnlockedFree(void *ptr);
void *_PR_UnlockedRealloc(void *ptr, size_t size);
void *_PR_UnlockedCalloc(size_t n, size_t elsize);

/************************************************************************/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 */

/*
 * Defining SANITY will enable some checks which will tell you if the users
 * program did botch something
 */

/*
 * Defining EXTRA_SANITY will enable some checks which are mostly related
 * to internal conditions in malloc.c
 */

/*
 * Very verbose progress on stdout...
 */
#if 0
#  define TRACE(foo)    printf  foo
static int malloc_event;
#else
#  define TRACE(foo)	
#endif

/* XXX Pick a number, any number */
#   define malloc_pagesize		4096UL
#   define malloc_pageshift		12UL

#ifdef XP_UNIX
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#endif

/*
 * This structure describes a page's worth of chunks.
 */

struct pginfo {
    struct pginfo	*next;	/* next on the free list */
    char		*page;	/* Pointer to the page */
    u_short		size;	/* size of this page's chunks */
    u_short		shift;	/* How far to shift for this size chunks */
    u_short		free;	/* How many free chunks */
    u_short		total;	/* How many chunk */
    u_long		bits[1]; /* Which chunks are free */
};

struct pgfree {
    struct pgfree	*next;	/* next run of free pages */
    struct pgfree	*prev;	/* prev run of free pages */
    char		*page;	/* pointer to free pages */
    char		*end;	/* pointer to end of free pages */
    u_long		size;	/* number of bytes free */
};

/*
 * How many bits per u_long in the bitmap.
 * Change only if not 8 bits/byte
 */
#define	MALLOC_BITS	(8*sizeof(u_long))

/*
 * Magic values to put in the page_directory
 */
#define MALLOC_NOT_MINE	((struct pginfo*) 0)
#define MALLOC_FREE 	((struct pginfo*) 1)
#define MALLOC_FIRST	((struct pginfo*) 2)
#define MALLOC_FOLLOW	((struct pginfo*) 3)
#define MALLOC_MAGIC	((struct pginfo*) 4)

/*
 * Set to one when malloc_init has been called
 */
static	unsigned	initialized;

/*
 * The size of a page.
 * Must be a integral multiplum of the granularity of mmap(2).
 * Your toes will curl if it isn't a power of two
 */
#define malloc_pagemask	((malloc_pagesize)-1)

/*
 * The size of the largest chunk.
 * Half a page.
 */
#define malloc_maxsize	((malloc_pagesize)>>1)

/*
 * malloc_pagesize == 1 << malloc_pageshift
 */
#ifndef malloc_pageshift
static	unsigned	malloc_pageshift;
#endif /* malloc_pageshift */

/*
 * The smallest allocation we bother about.
 * Must be power of two
 */
#ifndef malloc_minsize
static	unsigned  malloc_minsize;
#endif /* malloc_minsize */

/*
 * The largest chunk we care about.
 * Must be smaller than pagesize
 * Must be power of two
 */
#ifndef malloc_maxsize
static	unsigned  malloc_maxsize;
#endif /* malloc_maxsize */

#ifndef malloc_cache
static	unsigned  malloc_cache;
#endif /* malloc_cache */

/*
 * The offset from pagenumber to index into the page directory
 */
static	u_long  malloc_origo;

/*
 * The last index in the page directory we care about
 */
static	u_long  last_index;

/*
 * Pointer to page directory.
 * Allocated "as if with" malloc
 */
static	struct	pginfo **page_dir;

/*
 * How many slots in the page directory
 */
static	unsigned	malloc_ninfo;

/*
 * Free pages line up here 
 */
static struct pgfree	free_list;

/*
 * Abort() if we fail to get VM ?
 */
static int malloc_abort;

#ifdef SANITY
/*
 * Are we trying to die ?
 */
static int suicide;
#endif

/*
 * dump statistics
 */
static int malloc_stats;

/*
 * always realloc ?
 */
static int malloc_realloc;

/*
 * my last break.
 */
static void *malloc_brk;

/*
 * one location cache for free-list holders
 */
static struct pgfree *px;

static int set_pgdir(void *ptr, struct  pginfo *info);
static int extend_page_directory(u_long index);

#ifdef SANITY
void
malloc_dump(FILE *fd)
{
    struct pginfo **pd;
    struct pgfree *pf;
    int j;

    pd = page_dir;

    /* print out all the pages */
    for(j=0;j<=last_index;j++) {
	fprintf(fd,"%08lx %5d ",(j+malloc_origo) << malloc_pageshift,j);
	if (pd[j] == MALLOC_NOT_MINE) {
	    for(j++;j<=last_index && pd[j] == MALLOC_NOT_MINE;j++)
		;
	    j--;
	    fprintf(fd,".. %5d not mine\n",	j);
	} else if (pd[j] == MALLOC_FREE) {
	    for(j++;j<=last_index && pd[j] == MALLOC_FREE;j++)
		;
	    j--;
	    fprintf(fd,".. %5d free\n", j);
	} else if (pd[j] == MALLOC_FIRST) {
	    for(j++;j<=last_index && pd[j] == MALLOC_FOLLOW;j++)
		;
	    j--;
	    fprintf(fd,".. %5d in use\n", j);
	} else if (pd[j] < MALLOC_MAGIC) {
	    fprintf(fd,"(%p)\n", pd[j]);
	} else {
	    fprintf(fd,"%p %d (of %d) x %d @ %p --> %p\n",
		pd[j],pd[j]->free, pd[j]->total, 
		pd[j]->size, pd[j]->page, pd[j]->next);
	}
    }

    for(pf=free_list.next; pf; pf=pf->next) {
	fprintf(fd,"Free: @%p [%p...%p[ %ld ->%p <-%p\n",
		pf,pf->page,pf->end,pf->size,pf->prev,pf->next);
	if (pf == pf->next) {
		fprintf(fd,"Free_list loops.\n");
		break;
	}
    }

    /* print out various info */
    fprintf(fd,"Minsize\t%d\n",malloc_minsize);
    fprintf(fd,"Maxsize\t%ld\n",malloc_maxsize);
    fprintf(fd,"Pagesize\t%ld\n",malloc_pagesize);
    fprintf(fd,"Pageshift\t%ld\n",malloc_pageshift);
    fprintf(fd,"FirstPage\t%ld\n",malloc_origo);
    fprintf(fd,"LastPage\t%ld %lx\n",last_index+malloc_pageshift,
	(last_index + malloc_pageshift) << malloc_pageshift);
    fprintf(fd,"Break\t%ld\n",(u_long)sbrk(0) >> malloc_pageshift);
}

static void wrterror(char *fmt, ...)
{
    char *q = "malloc() error: ";
    char buf[100];
    va_list ap;

    suicide = 1;

    va_start(ap, fmt);
    PR_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(q, stderr);
    fputs(buf, stderr);

    malloc_dump(stderr);
    PR_Abort();
}

static void wrtwarning(char *fmt, ...)
{
    char *q = "malloc() warning: ";
    char buf[100];
    va_list ap;

    va_start(ap, fmt);
    PR_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(q, stderr);
    fputs(buf, stderr);
}
#endif /* SANITY */


/*
 * Allocate a number of pages from the OS
 */
static caddr_t
map_pages(int pages, int update)
{
    caddr_t result,tail;

    result = ((caddr_t)sbrk(0)) + malloc_pagemask - 1;
    result = (caddr_t) ((u_long)result & ~malloc_pagemask);
    tail = result + (pages << malloc_pageshift);
    if (!brk(tail)) {
	last_index = ((u_long)tail >> malloc_pageshift) - malloc_origo -1;
	malloc_brk = tail;
	TRACE(("%6d S %p .. %p\n",malloc_event++, result, tail));
	if (!update || last_index < malloc_ninfo ||
	  extend_page_directory(last_index))
	    return result;
    }
    TRACE(("%6d s %d %p %d\n",malloc_event++,pages,sbrk(0),errno));
#ifdef EXTRA_SANITY
    wrterror("map_pages fails\n");
#endif
    return 0;
}

#define set_bit(_pi,_bit) \
    (_pi)->bits[(_bit)/MALLOC_BITS] |= 1L<<((_bit)%MALLOC_BITS)

#define clr_bit(_pi,_bit) \
    (_pi)->bits[(_bit)/MALLOC_BITS] &= ~(1L<<((_bit)%MALLOC_BITS));

#define tst_bit(_pi,_bit) \
    ((_pi)->bits[(_bit)/MALLOC_BITS] & (1L<<((_bit)%MALLOC_BITS)))

/*
 * Extend page directory
 */
static int
extend_page_directory(u_long index)
{
    struct  pginfo **young, **old;
    int i;

    TRACE(("%6d E %lu\n",malloc_event++,index));
    
    /* Make it this many pages */
    i = index * sizeof *page_dir;
    i /= malloc_pagesize;
    i += 2;

    /* Get new pages, if you used this much mem you don't care :-) */
    young = (struct pginfo**) map_pages(i,0);
    if (!young)
	return 0;

    /* Copy the old stuff */
    memset(young, 0, i * malloc_pagesize);
    memcpy(young, page_dir,
	    malloc_ninfo * sizeof *page_dir);

    /* register the new size */
    malloc_ninfo = i * malloc_pagesize / sizeof *page_dir;

    /* swap the pointers */
    old = page_dir;
    page_dir = young;

    /* Mark the pages */
    index = ((u_long)young >> malloc_pageshift) - malloc_origo;
    page_dir[index] = MALLOC_FIRST;
    while (--i) {
	page_dir[++index] = MALLOC_FOLLOW;
    }

    /* Now free the old stuff */
    _PR_UnlockedFree(old);
    return 1;
}

/*
 * Set entry in page directory.
 * Extend page directory if need be.
 */
static int
set_pgdir(void *ptr, struct  pginfo *info)
{
    u_long index = ((u_long)ptr >> malloc_pageshift) - malloc_origo;

    if (index >= malloc_ninfo && !extend_page_directory(index))
	return 0;
    page_dir[index] = info;
    return 1;
}

/*
 * Initialize the world
 */
static void
malloc_init (void)
{
    int i;
    char *p;

    TRACE(("%6d I\n",malloc_event++));
#ifdef DEBUG
    for (p=getenv("MALLOC_OPTIONS"); p && *p; p++) {
	switch (*p) {
	    case 'a': malloc_abort = 0; break;
	    case 'A': malloc_abort = 1; break;
	    case 'd': malloc_stats = 0; break;
	    case 'D': malloc_stats = 1; break;
	    case 'r': malloc_realloc = 0; break;
	    case 'R': malloc_realloc = 1; break;
	    default:
		wrtwarning("Unknown chars in MALLOC_OPTIONS\n");
		break;
	}
    }
#endif

#ifndef malloc_pagesize
    /* determine our pagesize */
    malloc_pagesize = getpagesize();
#endif /* malloc_pagesize */

#ifndef malloc_pageshift
    /* determine how much we shift by to get there */
    for (i = malloc_pagesize; i > 1; i >>= 1)
	malloc_pageshift++;
#endif /* malloc_pageshift */

#ifndef malloc_cache
    malloc_cache = 50 << malloc_pageshift;	
#endif /* malloc_cache */

#ifndef malloc_minsize
    /*
     * find the smallest size allocation we will bother about.
     * this is determined as the smallest allocation that can hold
     * it's own pginfo;
     */
    i = 2;
    for(;;) {
	int j;

	/* Figure out the size of the bits */
	j = malloc_pagesize/i;
	j /= 8;
	if (j < sizeof(u_long))
		j = sizeof (u_long);
	if (sizeof(struct pginfo) + j - sizeof (u_long) <= i)
		break;
	i += i;
    }
    malloc_minsize = i;
#endif /* malloc_minsize */


    /* Allocate one page for the page directory */
    page_dir = (struct pginfo **) map_pages(1,0);
#ifdef SANITY
    if (!page_dir)
	wrterror("fatal: my first mmap failed.  (check limits ?)\n");
#endif

    /*
     * We need a maximum of malloc_pageshift buckets, steal these from the
     * front of the page_directory;
     */
    malloc_origo = (u_long) page_dir >> malloc_pageshift;
    malloc_origo -= malloc_pageshift;

    /* Clear it */
    memset(page_dir,0,malloc_pagesize);

    /* Find out how much it tells us */
    malloc_ninfo = malloc_pagesize / sizeof *page_dir;

    /* Plug the page directory into itself */
    i = set_pgdir(page_dir,MALLOC_FIRST);
#ifdef SANITY
    if (!i)
	wrterror("fatal: couldn't set myself in the page directory\n");
#endif

    /* Been here, done that */
    initialized++;
}

/*
 * Allocate a number of complete pages
 */
static void *malloc_pages(size_t size)
{
    void *p,*delay_free = 0;
    int i;
    struct pgfree *pf;
    u_long index;

    /* How many pages ? */
    size += (malloc_pagesize-1);
    size &= ~malloc_pagemask;

    p = 0;
    /* Look for free pages before asking for more */
    for(pf = free_list.next; pf; pf = pf->next) {
#ifdef EXTRA_SANITY
	if (pf->page == pf->end)
	    wrterror("zero entry on free_list\n");
	if (pf->page > pf->end) {
	    TRACE(("%6d !s %p %p %p <%d>\n",malloc_event++,
		pf,pf->page,pf->end,__LINE__));
	    wrterror("sick entry on free_list\n");
	}
	if ((void*)pf->page >= (void*)sbrk(0))
	    wrterror("entry on free_list past brk\n");
	if (page_dir[((u_long)pf->page >> malloc_pageshift) - malloc_origo] 
	  != MALLOC_FREE) {
	    TRACE(("%6d !f %p %p %p <%d>\n",malloc_event++,
		pf,pf->page,pf->end,__LINE__));
	    wrterror("non-free first page on free-list\n");
	}
	if (page_dir[((u_long)pf->end >> malloc_pageshift) - 1 - malloc_origo] 
	  != MALLOC_FREE)
	    wrterror("non-free last page on free-list\n");
#endif /* EXTRA_SANITY */
	if (pf->size < size) 
	    continue;
	else if (pf->size == size) {
	    p = pf->page;
	    if (pf->next)
		    pf->next->prev = pf->prev;
	    pf->prev->next = pf->next;
	    delay_free = pf;
	    break;
	} else {
	    p = pf->page;
	    pf->page += size;
	    pf->size -= size;
	    break;
        }
    }
#ifdef EXTRA_SANITY
    if (p && page_dir[((u_long)p >> malloc_pageshift) - malloc_origo] 
      != MALLOC_FREE) {
	wrterror("allocated non-free page on free-list\n");
    }
#endif /* EXTRA_SANITY */

    size >>= malloc_pageshift;

    /* Map new pages */
    if (!p)
	p = map_pages(size,1);

    if (p) {
	/* Mark the pages in the directory */
	index = ((u_long)p >> malloc_pageshift) - malloc_origo;
	page_dir[index] = MALLOC_FIRST;
	for (i=1;i<size;i++)
	    page_dir[index+i] = MALLOC_FOLLOW;
    }
    if (delay_free) {
	if (!px) 
	    px = (struct pgfree*)delay_free;
	else
	    _PR_UnlockedFree(delay_free);
    }
    return p;
}

/*
 * Allocate a page of fragments
 */

static int
malloc_make_chunks(int bits)
{
    struct  pginfo *bp;
    void *pp;
    int i,k,l;

    /* Allocate a new bucket */
    pp = malloc_pages(malloc_pagesize);
    if (!pp)
	return 0;
    l = sizeof *bp - sizeof(u_long);
    l += sizeof(u_long) *
	(((malloc_pagesize >> bits)+MALLOC_BITS-1) / MALLOC_BITS);
    if ((1<<(bits)) <= l+l) {
	bp = (struct  pginfo *)pp;
    } else {
	bp = (struct  pginfo *)_PR_UnlockedMalloc(l);
    }
    if (!bp)
	return 0;
    bp->size = (1<<bits);
    bp->shift = bits;
    bp->total = bp->free = malloc_pagesize >> bits;
    bp->next = page_dir[bits];
    bp->page = (char*)pp;
    i = set_pgdir(pp,bp);
    if (!i)
	return 0;

    /* We can safely assume that there is nobody in this chain */
    page_dir[bits] = bp;

    /* set all valid bits in the bits */
    k = bp->total;
    i = 0;
/*
    for(;k-i >= MALLOC_BITS; i += MALLOC_BITS)
	bp->bits[i / MALLOC_BITS] = ~0;
*/
    for(; i < k; i++)
	set_bit(bp,i);

    if (bp != pp)
	return 1;

    /* We may have used the first ones already */
    for(i=0;l > 0;i++) {
	clr_bit(bp,i);
	bp->free--;
	bp->total--;
	l -= (1 << bits);
    }
    return 1;
}

/*
 * Allocate a fragment
 */
static void *malloc_bytes(size_t size)
{
    size_t s;
    int j;
    struct  pginfo *bp;
    int k;
    u_long *lp, bf;

    /* Don't bother with anything less than this */
    if (size < malloc_minsize) {
	size = malloc_minsize;
    }

    /* Find the right bucket */
    j = 1;
    s = size - 1;
    while (s >>= 1) {
        j++;
    }

    /* If it's empty, make a page more of that size chunks */
    if (!page_dir[j] && !malloc_make_chunks(j))
	return 0;

    /* Find first word of bitmap which isn't empty */
    bp = page_dir[j];
    for (lp = bp->bits; !*lp; lp++)
	;

    /* Find that bit */
    bf = *lp;
    k = 0;
    while ((bf & 1) == 0) {
	bf >>= 1;
	k++;
    }

    *lp ^= 1L<<k;                       /* clear it */
    bp->free--;
    if (!bp->free) {
	page_dir[j] = bp->next;
	bp->next = 0;
    }
    k += (lp - bp->bits)*MALLOC_BITS;
    return bp->page + (k << bp->shift);
}

void *_PR_UnlockedMalloc(size_t size)
{
    void *result;

    /* Round up to a multiple of 8 bytes */
    if (size & 7) {
	size = size + 8 - (size & 7);
    }

    if (!initialized)
	malloc_init();

#ifdef SANITY
    if (suicide)
	PR_Abort();
#endif

    if (size <= malloc_maxsize)
	result =  malloc_bytes(size);
    else
	result =  malloc_pages(size);
#ifdef SANITY
    if (malloc_abort && !result)
	wrterror("malloc() returns NULL\n");
#endif
    TRACE(("%6d M %p %d\n",malloc_event++,result,size));

    return result;
}

void *_PR_UnlockedMemalign(size_t alignment, size_t size)
{
    void *result;

    /*
     * alignment has to be a power of 2
     */

    if ((size <= alignment) && (alignment <= malloc_maxsize))
		size = alignment;	
    else
           	size += alignment - 1;	

    /* Round up to a multiple of 8 bytes */
    if (size & 7) {
	size = size + 8 - (size & 7);
    }

    if (!initialized)
	malloc_init();

#ifdef SANITY
    if (suicide)
	abort();
#endif

    if (size <= malloc_maxsize)
	result =  malloc_bytes(size);
    else
	result =  malloc_pages(size);
#ifdef SANITY
    if (malloc_abort && !result)
	wrterror("malloc() returns NULL\n");
#endif
    TRACE(("%6d A %p %d\n",malloc_event++,result,size));

    if ((u_long)result & (alignment - 1))
    	return ((void *)(((u_long)result + alignment)  & ~(alignment - 1)));
    else
    	return result;
}

void *_PR_UnlockedCalloc(size_t n, size_t nelem)
{
    void *p;

    /* Compute total size and then round up to a double word amount */
    n *= nelem;
    if (n & 7) {
	n = n + 8 - (n & 7);
    }

    /* Get the memory */
    p = _PR_UnlockedMalloc(n);
    if (p) {
	/* Zero it */
	memset(p, 0, n);
    }
    return p;
}

/*
 * Change an allocation's size
 */
void *_PR_UnlockedRealloc(void *ptr, size_t size)
{
    void *p;
    u_long osize,page,index,tmp_index;
    struct pginfo **mp;

    if (!initialized)
	malloc_init();

#ifdef SANITY
    if (suicide)
	PR_Abort();
#endif

    /* used as free() */
    TRACE(("%6d R %p %d\n",malloc_event++, ptr, size));
    if (ptr && !size) {
	_PR_UnlockedFree(ptr);
	return _PR_UnlockedMalloc (1);
    }

    /* used as malloc() */
    if (!ptr) {
	p = _PR_UnlockedMalloc(size);
	return p;
    }

    /* Find the page directory entry for the page in question */
    page = (u_long)ptr >> malloc_pageshift;
    index = page - malloc_origo;

    /*
     * check if memory was allocated by memalign
     */
    tmp_index = index;
    while (page_dir[tmp_index] == MALLOC_FOLLOW)
	tmp_index--;
    if (tmp_index != index) {
	/*
	 * memalign-allocated memory
	 */
	index = tmp_index;
	page = index + malloc_origo;			
	ptr = (void *) (page << malloc_pageshift);
    }
    TRACE(("%6d R2 %p %d\n",malloc_event++, ptr, size));

    /* make sure it makes sense in some fashion */
    if (index < malloc_pageshift || index > last_index) {
#ifdef SANITY
	wrtwarning("junk pointer passed to realloc()\n");
#endif
	return 0;
    }

    /* find the size of that allocation, and see if we need to relocate */
    mp = &page_dir[index];
    if (*mp == MALLOC_FIRST) {
	osize = malloc_pagesize;
	while (mp[1] == MALLOC_FOLLOW) {
	    osize += malloc_pagesize;
	    mp++;
	}
        if (!malloc_realloc && 
		size < osize && 
		size > malloc_maxsize &&
	    size > (osize - malloc_pagesize)) {
	    return ptr;
	}
    } else if (*mp >= MALLOC_MAGIC) {
	osize = (*mp)->size;
	if (!malloc_realloc &&
		size < osize && 
	    (size > (*mp)->size/2 || (*mp)->size == malloc_minsize)) {
	    return ptr;
	}
    } else {
#ifdef SANITY
	wrterror("realloc() of wrong page.\n");
#endif
    }

    /* try to reallocate */
    p = _PR_UnlockedMalloc(size);

    if (p) {
	/* copy the lesser of the two sizes */
	if (osize < size)
	    memcpy(p,ptr,osize);
	else
	    memcpy(p,ptr,size);
	_PR_UnlockedFree(ptr);
    }
#ifdef DEBUG
    else if (malloc_abort)
	wrterror("realloc() returns NULL\n");
#endif

    return p;
}

/*
 * Free a sequence of pages
 */

static void
free_pages(char *ptr, u_long page, int index, struct pginfo *info)
{
    int i;
    struct pgfree *pf,*pt;
    u_long l;
    char *tail;

    TRACE(("%6d FP %p %d\n",malloc_event++, ptr, page));
    /* Is it free already ? */
    if (info == MALLOC_FREE) {
#ifdef SANITY
	wrtwarning("freeing free page at %p.\n", ptr);
#endif
	return;
    }

#ifdef SANITY
    /* Is it not the right place to begin ? */
    if (info != MALLOC_FIRST)
	wrterror("freeing wrong page.\n");

    /* Is this really a pointer to a page ? */
    if ((u_long)ptr & malloc_pagemask)
	wrterror("freeing messed up page pointer.\n");
#endif

    /* Count how many pages it is anyway */
    page_dir[index] = MALLOC_FREE;
    for (i = 1; page_dir[index+i] == MALLOC_FOLLOW; i++)
	page_dir[index + i] = MALLOC_FREE;

    l = i << malloc_pageshift;

    tail = ptr+l;

    /* add to free-list */
    if (!px)
	px = (struct pgfree*)_PR_UnlockedMalloc(sizeof *pt);
    /* XXX check success */
    px->page = ptr;
    px->end =  tail;
    px->size = l;
    if (!free_list.next) {
	px->next = free_list.next;
	px->prev = &free_list;
	free_list.next = px;
	pf = px;
	px = 0;
    } else {
	tail = ptr+l;
	for(pf = free_list.next; pf->next && pf->end < ptr; pf = pf->next)
	    ;
	for(; pf; pf = pf->next) {
	    if (pf->end == ptr ) {
		/* append to entry */
		pf->end += l;
		pf->size += l;
		if (pf->next && pf->end == pf->next->page ) {
		    pt = pf->next;
		    pf->end = pt->end;
		    pf->size += pt->size;
		    pf->next = pt->next;
		    if (pf->next)
			pf->next->prev = pf;
		    _PR_UnlockedFree(pt);
		}
	    } else if (pf->page == tail) {
		/* prepend to entry */
		pf->size += l;
		pf->page = ptr;
	    } else if (pf->page > ptr) {
		px->next = pf;
		px->prev = pf->prev;
		pf->prev = px;
		px->prev->next = px;
		pf = px;
		px = 0;
	    } else if (!pf->next) {
		px->next = 0;
		px->prev = pf;
		pf->next = px;
		pf = px;
		px = 0;
	    } else {
		continue;
	    }
	    break;
	}
    }
    if (!pf->next &&
      pf->size > malloc_cache &&
      pf->end == malloc_brk &&
      malloc_brk == (void*)sbrk(0)) {
	pf->end = pf->page + malloc_cache;
	pf->size = malloc_cache;
	TRACE(("%6d U %p %d\n",malloc_event++,pf->end,pf->end - pf->page));
	brk(pf->end);
	malloc_brk = pf->end;
	/* Find the page directory entry for the page in question */
	page = (u_long)pf->end >> malloc_pageshift;
	index = page - malloc_origo;
	/* Now update the directory */
	for(i=index;i <= last_index;)
	    page_dir[i++] = MALLOC_NOT_MINE;
	last_index = index - 1;
    }
}

/*
 * Free a chunk, and possibly the page it's on, if the page becomes empty.
 */

static void
free_bytes(void *ptr, u_long page, int index, struct pginfo *info)
{
    int i;
    struct pginfo **mp;
    void *vp;

    /* Make sure that pointer is multiplum of chunk-size */
#ifdef SANITY
    if ((u_long)ptr & (info->size - 1))
	wrterror("freeing messed up chunk pointer\n");
#endif

    /* Find the chunk number on the page */
    i = ((u_long)ptr & malloc_pagemask) >> info->shift;

    /* See if it's free already */
    if (tst_bit(info,i)) {
#ifdef SANITY
	wrtwarning("freeing free chunk at %p\n", ptr);
#endif
	return;
    }

    /* Mark it free */
    set_bit(info,i);
    info->free++;

    /* If the page was full before, we need to put it on the queue now */
    if (info->free == 1) {
	mp = page_dir + info->shift;
	while (*mp && (*mp)->next && (*mp)->next->page < info->page)
	    mp = &(*mp)->next;
	info->next = *mp;
	*mp = info;
	return;
    }

    /* If this page isn't empty, don't do anything. */
    if (info->free != info->total)
	return;

    /* We may want to keep at least one page of each size chunks around.  */
    mp = page_dir + info->shift;
    if (0 && (*mp == info) && !info->next)
	return;

    /* Find & remove this page in the queue */
    while (*mp != info) {
	mp = &((*mp)->next);
#ifdef EXTRA_SANITY
	if (!*mp) {
		TRACE(("%6d !q %p\n",malloc_event++,info));
		wrterror("Not on queue\n");
	}
#endif
    }
    *mp = info->next;

    /* Free the page & the info structure if need be */
    set_pgdir(info->page,MALLOC_FIRST);
    if((void*)info->page == (void*)info) {
	_PR_UnlockedFree(info->page);
    } else {
	vp = info->page;
	_PR_UnlockedFree(info);
	_PR_UnlockedFree(vp);
    }
}

void _PR_UnlockedFree(void *ptr)
{
    u_long page;
    struct pginfo *info;
    int index, tmp_index;

    TRACE(("%6d F %p\n",malloc_event++,ptr));
    /* This is legal */
    if (!ptr)
	return;

#ifdef SANITY
    /* There wouldn't be anything to free */
    if (!initialized) {
	wrtwarning("free() called before malloc() ever got called\n");
	return;
    }
#endif

#ifdef SANITY
    if (suicide)
	PR_Abort();
#endif

    /* Find the page directory entry for the page in question */
    page = (u_long)ptr >> malloc_pageshift;
    index = page - malloc_origo;

    /*
     * check if memory was allocated by memalign
     */
    tmp_index = index;
    while (page_dir[tmp_index] == MALLOC_FOLLOW)
	tmp_index--;
    if (tmp_index != index) {
	/*
	 * memalign-allocated memory
	 */
	index = tmp_index;
	page = index + malloc_origo;			
	ptr = (void *) (page << malloc_pageshift);
    }
    /* make sure it makes sense in some fashion */
    if (index < malloc_pageshift) {
#ifdef SANITY
	wrtwarning("junk pointer %p (low) passed to free()\n", ptr);
#endif
	return;
    }
    if (index > last_index) {
#ifdef SANITY
	wrtwarning("junk pointer %p (high) passed to free()\n", ptr);
#endif
	return;
    }

    /* handle as page-allocation or chunk allocation */
    info = page_dir[index];
    if (info < MALLOC_MAGIC)
        free_pages((char*)ptr, page, index, info);
    else 
	free_bytes(ptr,page,index,info);
    return;
}
#endif /* _PR_OVERRIDE_MALLOC */
