/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include "glib.h"

/* #define ENABLE_MEM_PROFILE */
/* #define ENABLE_MEM_PROFILE_EXCLUDES_MEM_CHUNKS */
/* #define ENABLE_MEM_CHECK */
#define MEM_PROFILE_TABLE_SIZE 8192

/*
 * This library can check for some attempts to do illegal things to
 * memory (ENABLE_MEM_CHECK), and can do profiling
 * (ENABLE_MEM_PROFILE).  Both features are implemented by storing
 * words before the start of the memory chunk.
 *
 * The first, at offset -2*SIZEOF_LONG, is used only if
 * ENABLE_MEM_CHECK is set, and stores 0 after the memory has been
 * allocated and 1 when it has been freed.  The second, at offset
 * -SIZEOF_LONG, is used if either flag is set and stores the size of
 * the block.
 *
 * The MEM_CHECK flag is checked when memory is realloc'd and free'd,
 * and it can be explicitly checked before using a block by calling
 * g_mem_check().
 */

#if defined(ENABLE_MEM_PROFILE) && defined(ENABLE_MEM_PROFILE_EXCLUDES_MEM_CHUNKS)
#define ENTER_MEM_CHUNK_ROUTINE() \
  g_private_set (allocating_for_mem_chunk, \
		g_private_get (allocating_for_mem_chunk) + 1)
#define LEAVE_MEM_CHUNK_ROUTINE() \
  g_private_set (allocating_for_mem_chunk, \
		g_private_get (allocating_for_mem_chunk) - 1) 
#else
#define ENTER_MEM_CHUNK_ROUTINE()
#define LEAVE_MEM_CHUNK_ROUTINE()
#endif


#define MAX_MEM_AREA  65536L
#define MEM_AREA_SIZE 4L

#if SIZEOF_VOID_P > SIZEOF_LONG
#define MEM_ALIGN     SIZEOF_VOID_P
#else
#define MEM_ALIGN     SIZEOF_LONG
#endif


typedef struct _GFreeAtom      GFreeAtom;
typedef struct _GMemArea       GMemArea;
typedef struct _GRealMemChunk  GRealMemChunk;

struct _GFreeAtom
{
  GFreeAtom *next;
};

struct _GMemArea
{
  GMemArea *next;            /* the next mem area */
  GMemArea *prev;            /* the previous mem area */
  gulong index;              /* the current index into the "mem" array */
  gulong free;               /* the number of free bytes in this mem area */
  gulong allocated;          /* the number of atoms allocated from this area */
  gulong mark;               /* is this mem area marked for deletion */
  gchar mem[MEM_AREA_SIZE];  /* the mem array from which atoms get allocated
			      * the actual size of this array is determined by
			      *  the mem chunk "area_size". ANSI says that it
			      *  must be declared to be the maximum size it
			      *  can possibly be (even though the actual size
			      *  may be less).
			      */
};

struct _GRealMemChunk
{
  gchar *name;               /* name of this MemChunk...used for debugging output */
  gint type;                 /* the type of MemChunk: ALLOC_ONLY or ALLOC_AND_FREE */
  gint num_mem_areas;        /* the number of memory areas */
  gint num_marked_areas;     /* the number of areas marked for deletion */
  guint atom_size;           /* the size of an atom */
  gulong area_size;          /* the size of a memory area */
  GMemArea *mem_area;        /* the current memory area */
  GMemArea *mem_areas;       /* a list of all the mem areas owned by this chunk */
  GMemArea *free_mem_area;   /* the free area...which is about to be destroyed */
  GFreeAtom *free_atoms;     /* the free atoms list */
  GTree *mem_tree;           /* tree of mem areas sorted by memory address */
  GRealMemChunk *next;       /* pointer to the next chunk */
  GRealMemChunk *prev;       /* pointer to the previous chunk */
};


static gulong g_mem_chunk_compute_size (gulong    size);
static gint   g_mem_chunk_area_compare (GMemArea *a,
					GMemArea *b);
static gint   g_mem_chunk_area_search  (GMemArea *a,
					gchar    *addr);


/* here we can't use StaticMutexes, as they depend upon a working
 * g_malloc, the same holds true for StaticPrivate */
static GMutex* mem_chunks_lock = NULL;
static GRealMemChunk *mem_chunks = NULL;

#ifdef ENABLE_MEM_PROFILE
static GMutex* mem_profile_lock;
static gulong allocations[MEM_PROFILE_TABLE_SIZE] = { 0 };
static gulong allocated_mem = 0;
static gulong freed_mem = 0;
static GPrivate* allocating_for_mem_chunk = NULL;
#define IS_IN_MEM_CHUNK_ROUTINE() \
  GPOINTER_TO_UINT (g_private_get (allocating_for_mem_chunk))
#endif /* ENABLE_MEM_PROFILE */


#ifndef USE_DMALLOC

gpointer
g_malloc (gulong size)
{
  gpointer p;
  
  
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  gulong *t;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */
  
  
  if (size == 0)
    return NULL;
  
  
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  size += SIZEOF_LONG;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */
  
#ifdef ENABLE_MEM_CHECK
  size += SIZEOF_LONG;
#endif /* ENABLE_MEM_CHECK */
  
  
  p = (gpointer) malloc (size);
  if (!p)
    g_error ("could not allocate %ld bytes", size);
  
  
#ifdef ENABLE_MEM_CHECK
  size -= SIZEOF_LONG;
  
  t = p;
  p = ((guchar*) p + SIZEOF_LONG);
  *t = 0;
#endif /* ENABLE_MEM_CHECK */
  
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  size -= SIZEOF_LONG;
  
  t = p;
  p = ((guchar*) p + SIZEOF_LONG);
  *t = size;
  
#ifdef ENABLE_MEM_PROFILE
  g_mutex_lock (mem_profile_lock);
#  ifdef ENABLE_MEM_PROFILE_EXCLUDES_MEM_CHUNKS
  if(!IS_IN_MEM_CHUNK_ROUTINE()) {
#  endif
    if (size <= MEM_PROFILE_TABLE_SIZE - 1)
      allocations[size-1] += 1;
    else
      allocations[MEM_PROFILE_TABLE_SIZE - 1] += 1;
    allocated_mem += size;
#  ifdef ENABLE_MEM_PROFILE_EXCLUDES_MEM_CHUNKS
  }
#  endif
  g_mutex_unlock (mem_profile_lock);
#endif /* ENABLE_MEM_PROFILE */
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */
  
  
  return p;
}

gpointer
g_malloc0 (gulong size)
{
  gpointer p;
  
  
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  gulong *t;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */
  
  
  if (size == 0)
    return NULL;
  
  
#if defined (ENABLE_MEM_PROFILE) || defined (ENABLE_MEM_CHECK)
  size += SIZEOF_LONG;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */
  
#ifdef ENABLE_MEM_CHECK
  size += SIZEOF_LONG;
#endif /* ENABLE_MEM_CHECK */
  
  
  p = (gpointer) calloc (size, 1);
  if (!p)
    g_error ("could not allocate %ld bytes", size);
  
  
#ifdef ENABLE_MEM_CHECK
  size -= SIZEOF_LONG;
  
  t = p;
  p = ((guchar*) p + SIZEOF_LONG);
  *t = 0;
#endif /* ENABLE_MEM_CHECK */
  
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  size -= SIZEOF_LONG;
  
  t = p;
  p = ((guchar*) p + SIZEOF_LONG);
  *t = size;
  
#  ifdef ENABLE_MEM_PROFILE
  g_mutex_lock (mem_profile_lock);
#    ifdef ENABLE_MEM_PROFILE_EXCLUDES_MEM_CHUNKS
  if(!IS_IN_MEM_CHUNK_ROUTINE()) {
#    endif
    if (size <= (MEM_PROFILE_TABLE_SIZE - 1))
      allocations[size-1] += 1;
    else
      allocations[MEM_PROFILE_TABLE_SIZE - 1] += 1;
    allocated_mem += size;
#    ifdef ENABLE_MEM_PROFILE_EXCLUDES_MEM_CHUNKS
  }
#    endif
  g_mutex_unlock (mem_profile_lock);
#  endif /* ENABLE_MEM_PROFILE */
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */
  
  
  return p;
}

gpointer
g_realloc (gpointer mem,
	   gulong   size)
{
  gpointer p;
  
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  gulong *t;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */
  
  
  if (size == 0)
    {
      g_free (mem);
    
      return NULL;
    }
  
  
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  size += SIZEOF_LONG;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */
  
#ifdef ENABLE_MEM_CHECK
  size += SIZEOF_LONG;
#endif /* ENABLE_MEM_CHECK */
  
  
  if (!mem)
    {
#ifdef REALLOC_0_WORKS
      p = (gpointer) realloc (NULL, size);
#else /* !REALLOC_0_WORKS */
      p = (gpointer) malloc (size);
#endif /* !REALLOC_0_WORKS */
    }
  else
    {
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
      t = (gulong*) ((guchar*) mem - SIZEOF_LONG);
#ifdef ENABLE_MEM_PROFILE
      g_mutex_lock (mem_profile_lock);
      freed_mem += *t;
      g_mutex_unlock (mem_profile_lock);
#endif /* ENABLE_MEM_PROFILE */
      mem = t;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */
      
#ifdef ENABLE_MEM_CHECK
      t = (gulong*) ((guchar*) mem - SIZEOF_LONG);
      if (*t >= 1)
	g_warning ("trying to realloc freed memory\n");
      mem = t;
#endif /* ENABLE_MEM_CHECK */
      
      p = (gpointer) realloc (mem, size);
    }
  
  if (!p)
    g_error ("could not reallocate %lu bytes", (gulong) size);
  
  
#ifdef ENABLE_MEM_CHECK
  size -= SIZEOF_LONG;
  
  t = p;
  p = ((guchar*) p + SIZEOF_LONG);
  *t = 0;
#endif /* ENABLE_MEM_CHECK */
  
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
  size -= SIZEOF_LONG;
  
  t = p;
  p = ((guchar*) p + SIZEOF_LONG);
  *t = size;
  
#ifdef ENABLE_MEM_PROFILE
  g_mutex_lock (mem_profile_lock);
#ifdef ENABLE_MEM_PROFILE_EXCLUDES_MEM_CHUNKS
  if(!IS_IN_MEM_CHUNK_ROUTINE()) {
#endif
    if (size <= (MEM_PROFILE_TABLE_SIZE - 1))
      allocations[size-1] += 1;
    else
      allocations[MEM_PROFILE_TABLE_SIZE - 1] += 1;
    allocated_mem += size;
#ifdef ENABLE_MEM_PROFILE_EXCLUDES_MEM_CHUNKS
  }
#endif
  g_mutex_unlock (mem_profile_lock);
#endif /* ENABLE_MEM_PROFILE */
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */
  
  
  return p;
}

void
g_free (gpointer mem)
{
  if (mem)
    {
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
      gulong *t;
      gulong size;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */
      
#if defined(ENABLE_MEM_PROFILE) || defined(ENABLE_MEM_CHECK)
      t = (gulong*) ((guchar*) mem - SIZEOF_LONG);
      size = *t;
#ifdef ENABLE_MEM_PROFILE     
      g_mutex_lock (mem_profile_lock);
      freed_mem += size;
      g_mutex_unlock (mem_profile_lock);
#endif /* ENABLE_MEM_PROFILE */
      mem = t;
#endif /* ENABLE_MEM_PROFILE || ENABLE_MEM_CHECK */
      
#ifdef ENABLE_MEM_CHECK
      t = (gulong*) ((guchar*) mem - SIZEOF_LONG);
      if (*t >= 1)
	g_warning ("freeing previously freed memory\n");
      *t += 1;
      mem = t;
      
      memset ((guchar*) mem + 8, 0, size);
#else /* ENABLE_MEM_CHECK */
      free (mem);
#endif /* ENABLE_MEM_CHECK */
    }
}

#endif /* ! USE_DMALLOC */


void
g_mem_profile (void)
{
#ifdef ENABLE_MEM_PROFILE
  gint i;
  gulong local_allocations[MEM_PROFILE_TABLE_SIZE];
  gulong local_allocated_mem;
  gulong local_freed_mem;  

  g_mutex_lock (mem_profile_lock);
  for (i = 0; i < MEM_PROFILE_TABLE_SIZE; i++)
    local_allocations[i] = allocations[i];
  local_allocated_mem = allocated_mem;
  local_freed_mem = freed_mem;
  g_mutex_unlock (mem_profile_lock);

  for (i = 0; i < (MEM_PROFILE_TABLE_SIZE - 1); i++)
    if (local_allocations[i] > 0)
      g_log (g_log_domain_glib, G_LOG_LEVEL_INFO,
	     "%lu allocations of %d bytes", local_allocations[i], i + 1);
  
  if (local_allocations[MEM_PROFILE_TABLE_SIZE - 1] > 0)
    g_log (g_log_domain_glib, G_LOG_LEVEL_INFO,
	   "%lu allocations of greater than %d bytes",
	   local_allocations[MEM_PROFILE_TABLE_SIZE - 1], MEM_PROFILE_TABLE_SIZE - 1);
  g_log (g_log_domain_glib, G_LOG_LEVEL_INFO, "%lu bytes allocated", local_allocated_mem);
  g_log (g_log_domain_glib, G_LOG_LEVEL_INFO, "%lu bytes freed", local_freed_mem);
  g_log (g_log_domain_glib, G_LOG_LEVEL_INFO, "%lu bytes in use", local_allocated_mem - local_freed_mem);
#endif /* ENABLE_MEM_PROFILE */
}

void
g_mem_check (gpointer mem)
{
#ifdef ENABLE_MEM_CHECK
  gulong *t;
  
  t = (gulong*) ((guchar*) mem - SIZEOF_LONG - SIZEOF_LONG);
  
  if (*t >= 1)
    g_warning ("mem: 0x%08x has been freed %lu times\n", (gulong) mem, *t);
#endif /* ENABLE_MEM_CHECK */
}

GMemChunk*
g_mem_chunk_new (gchar  *name,
		 gint    atom_size,
		 gulong  area_size,
		 gint    type)
{
  GRealMemChunk *mem_chunk;
  gulong rarea_size;

  ENTER_MEM_CHUNK_ROUTINE();

  mem_chunk = g_new (struct _GRealMemChunk, 1);
  mem_chunk->name = name;
  mem_chunk->type = type;
  mem_chunk->num_mem_areas = 0;
  mem_chunk->num_marked_areas = 0;
  mem_chunk->mem_area = NULL;
  mem_chunk->free_mem_area = NULL;
  mem_chunk->free_atoms = NULL;
  mem_chunk->mem_tree = NULL;
  mem_chunk->mem_areas = NULL;
  mem_chunk->atom_size = atom_size;
  
  if (mem_chunk->type == G_ALLOC_AND_FREE)
    mem_chunk->mem_tree = g_tree_new ((GCompareFunc) g_mem_chunk_area_compare);
  
  if (mem_chunk->atom_size % MEM_ALIGN)
    mem_chunk->atom_size += MEM_ALIGN - (mem_chunk->atom_size % MEM_ALIGN);
  
  mem_chunk->area_size = area_size;
  if (mem_chunk->area_size > MAX_MEM_AREA)
    mem_chunk->area_size = MAX_MEM_AREA;
  while (mem_chunk->area_size < mem_chunk->atom_size)
    mem_chunk->area_size *= 2;
  
  rarea_size = mem_chunk->area_size + sizeof (GMemArea) - MEM_AREA_SIZE;
  rarea_size = g_mem_chunk_compute_size (rarea_size);
  mem_chunk->area_size = rarea_size - (sizeof (GMemArea) - MEM_AREA_SIZE);
  
  /*
    mem_chunk->area_size -= (sizeof (GMemArea) - MEM_AREA_SIZE);
    if (mem_chunk->area_size < mem_chunk->atom_size)
    {
    mem_chunk->area_size = (mem_chunk->area_size + sizeof (GMemArea) - MEM_AREA_SIZE) * 2;
    mem_chunk->area_size -= (sizeof (GMemArea) - MEM_AREA_SIZE);
    }
    
    if (mem_chunk->area_size % mem_chunk->atom_size)
    mem_chunk->area_size += mem_chunk->atom_size - (mem_chunk->area_size % mem_chunk->atom_size);
  */
  
  g_mutex_lock (mem_chunks_lock);
  mem_chunk->next = mem_chunks;
  mem_chunk->prev = NULL;
  if (mem_chunks)
    mem_chunks->prev = mem_chunk;
  mem_chunks = mem_chunk;
  g_mutex_unlock (mem_chunks_lock);

  LEAVE_MEM_CHUNK_ROUTINE();

  return ((GMemChunk*) mem_chunk);
}

void
g_mem_chunk_destroy (GMemChunk *mem_chunk)
{
  GRealMemChunk *rmem_chunk;
  GMemArea *mem_areas;
  GMemArea *temp_area;
  
  g_return_if_fail (mem_chunk != NULL);

  ENTER_MEM_CHUNK_ROUTINE();

  rmem_chunk = (GRealMemChunk*) mem_chunk;
  
  mem_areas = rmem_chunk->mem_areas;
  while (mem_areas)
    {
      temp_area = mem_areas;
      mem_areas = mem_areas->next;
      g_free (temp_area);
    }
  
  if (rmem_chunk->next)
    rmem_chunk->next->prev = rmem_chunk->prev;
  if (rmem_chunk->prev)
    rmem_chunk->prev->next = rmem_chunk->next;
  
  g_mutex_lock (mem_chunks_lock);
  if (rmem_chunk == mem_chunks)
    mem_chunks = mem_chunks->next;
  g_mutex_unlock (mem_chunks_lock);
  
  if (rmem_chunk->type == G_ALLOC_AND_FREE)
    g_tree_destroy (rmem_chunk->mem_tree);
  
  g_free (rmem_chunk);

  LEAVE_MEM_CHUNK_ROUTINE();
}

gpointer
g_mem_chunk_alloc (GMemChunk *mem_chunk)
{
  GRealMemChunk *rmem_chunk;
  GMemArea *temp_area;
  gpointer mem;

  ENTER_MEM_CHUNK_ROUTINE();

  g_return_val_if_fail (mem_chunk != NULL, NULL);
  
  rmem_chunk = (GRealMemChunk*) mem_chunk;
  
  while (rmem_chunk->free_atoms)
    {
      /* Get the first piece of memory on the "free_atoms" list.
       * We can go ahead and destroy the list node we used to keep
       *  track of it with and to update the "free_atoms" list to
       *  point to its next element.
       */
      mem = rmem_chunk->free_atoms;
      rmem_chunk->free_atoms = rmem_chunk->free_atoms->next;
      
      /* Determine which area this piece of memory is allocated from */
      temp_area = g_tree_search (rmem_chunk->mem_tree,
				 (GSearchFunc) g_mem_chunk_area_search,
				 mem);
      
      /* If the area has been marked, then it is being destroyed.
       *  (ie marked to be destroyed).
       * We check to see if all of the segments on the free list that
       *  reference this area have been removed. This occurs when
       *  the ammount of free memory is less than the allocatable size.
       * If the chunk should be freed, then we place it in the "free_mem_area".
       * This is so we make sure not to free the mem area here and then
       *  allocate it again a few lines down.
       * If we don't allocate a chunk a few lines down then the "free_mem_area"
       *  will be freed.
       * If there is already a "free_mem_area" then we'll just free this mem area.
       */
      if (temp_area->mark)
        {
          /* Update the "free" memory available in that area */
          temp_area->free += rmem_chunk->atom_size;
	  
          if (temp_area->free == rmem_chunk->area_size)
            {
              if (temp_area == rmem_chunk->mem_area)
                rmem_chunk->mem_area = NULL;
	      
              if (rmem_chunk->free_mem_area)
                {
                  rmem_chunk->num_mem_areas -= 1;
		  
                  if (temp_area->next)
                    temp_area->next->prev = temp_area->prev;
                  if (temp_area->prev)
                    temp_area->prev->next = temp_area->next;
                  if (temp_area == rmem_chunk->mem_areas)
                    rmem_chunk->mem_areas = rmem_chunk->mem_areas->next;
		  
		  if (rmem_chunk->type == G_ALLOC_AND_FREE)
		    g_tree_remove (rmem_chunk->mem_tree, temp_area);
                  g_free (temp_area);
                }
              else
                rmem_chunk->free_mem_area = temp_area;
	      
	      rmem_chunk->num_marked_areas -= 1;
	    }
	}
      else
        {
          /* Update the number of allocated atoms count.
	   */
          temp_area->allocated += 1;
	  
          /* The area wasn't marked...return the memory
	   */
	  goto outa_here;
        }
    }
  
  /* If there isn't a current mem area or the current mem area is out of space
   *  then allocate a new mem area. We'll first check and see if we can use
   *  the "free_mem_area". Otherwise we'll just malloc the mem area.
   */
  if ((!rmem_chunk->mem_area) ||
      ((rmem_chunk->mem_area->index + rmem_chunk->atom_size) > rmem_chunk->area_size))
    {
      if (rmem_chunk->free_mem_area)
        {
          rmem_chunk->mem_area = rmem_chunk->free_mem_area;
	  rmem_chunk->free_mem_area = NULL;
        }
      else
        {
	  rmem_chunk->mem_area = (GMemArea*) g_malloc (sizeof (GMemArea) -
						       MEM_AREA_SIZE +
						       rmem_chunk->area_size);
	  
	  rmem_chunk->num_mem_areas += 1;
	  rmem_chunk->mem_area->next = rmem_chunk->mem_areas;
	  rmem_chunk->mem_area->prev = NULL;
	  
	  if (rmem_chunk->mem_areas)
	    rmem_chunk->mem_areas->prev = rmem_chunk->mem_area;
	  rmem_chunk->mem_areas = rmem_chunk->mem_area;
	  
	  if (rmem_chunk->type == G_ALLOC_AND_FREE)
	    g_tree_insert (rmem_chunk->mem_tree, rmem_chunk->mem_area, rmem_chunk->mem_area);
        }
      
      rmem_chunk->mem_area->index = 0;
      rmem_chunk->mem_area->free = rmem_chunk->area_size;
      rmem_chunk->mem_area->allocated = 0;
      rmem_chunk->mem_area->mark = 0;
    }
  
  /* Get the memory and modify the state variables appropriately.
   */
  mem = (gpointer) &rmem_chunk->mem_area->mem[rmem_chunk->mem_area->index];
  rmem_chunk->mem_area->index += rmem_chunk->atom_size;
  rmem_chunk->mem_area->free -= rmem_chunk->atom_size;
  rmem_chunk->mem_area->allocated += 1;

outa_here:

  LEAVE_MEM_CHUNK_ROUTINE();

  return mem;
}

gpointer
g_mem_chunk_alloc0 (GMemChunk *mem_chunk)
{
  gpointer mem;

  mem = g_mem_chunk_alloc (mem_chunk);
  if (mem)
    {
      GRealMemChunk *rmem_chunk = (GRealMemChunk*) mem_chunk;

      memset (mem, 0, rmem_chunk->atom_size);
    }

  return mem;
}

void
g_mem_chunk_free (GMemChunk *mem_chunk,
		  gpointer   mem)
{
  GRealMemChunk *rmem_chunk;
  GMemArea *temp_area;
  GFreeAtom *free_atom;
  
  g_return_if_fail (mem_chunk != NULL);
  g_return_if_fail (mem != NULL);

  ENTER_MEM_CHUNK_ROUTINE();

  rmem_chunk = (GRealMemChunk*) mem_chunk;
  
  /* Don't do anything if this is an ALLOC_ONLY chunk
   */
  if (rmem_chunk->type == G_ALLOC_AND_FREE)
    {
      /* Place the memory on the "free_atoms" list
       */
      free_atom = (GFreeAtom*) mem;
      free_atom->next = rmem_chunk->free_atoms;
      rmem_chunk->free_atoms = free_atom;
      
      temp_area = g_tree_search (rmem_chunk->mem_tree,
				 (GSearchFunc) g_mem_chunk_area_search,
				 mem);
      
      temp_area->allocated -= 1;
      
      if (temp_area->allocated == 0)
	{
	  temp_area->mark = 1;
	  rmem_chunk->num_marked_areas += 1;
	}
    }

  LEAVE_MEM_CHUNK_ROUTINE();
}

/* This doesn't free the free_area if there is one */
void
g_mem_chunk_clean (GMemChunk *mem_chunk)
{
  GRealMemChunk *rmem_chunk;
  GMemArea *mem_area;
  GFreeAtom *prev_free_atom;
  GFreeAtom *temp_free_atom;
  gpointer mem;
  
  g_return_if_fail (mem_chunk != NULL);
  
  rmem_chunk = (GRealMemChunk*) mem_chunk;
  
  if (rmem_chunk->type == G_ALLOC_AND_FREE)
    {
      prev_free_atom = NULL;
      temp_free_atom = rmem_chunk->free_atoms;
      
      while (temp_free_atom)
	{
	  mem = (gpointer) temp_free_atom;
	  
	  mem_area = g_tree_search (rmem_chunk->mem_tree,
				    (GSearchFunc) g_mem_chunk_area_search,
				    mem);
	  
          /* If this mem area is marked for destruction then delete the
	   *  area and list node and decrement the free mem.
           */
	  if (mem_area->mark)
	    {
	      if (prev_free_atom)
		prev_free_atom->next = temp_free_atom->next;
	      else
		rmem_chunk->free_atoms = temp_free_atom->next;
	      temp_free_atom = temp_free_atom->next;
	      
	      mem_area->free += rmem_chunk->atom_size;
	      if (mem_area->free == rmem_chunk->area_size)
		{
		  rmem_chunk->num_mem_areas -= 1;
		  rmem_chunk->num_marked_areas -= 1;
		  
		  if (mem_area->next)
		    mem_area->next->prev = mem_area->prev;
		  if (mem_area->prev)
		    mem_area->prev->next = mem_area->next;
		  if (mem_area == rmem_chunk->mem_areas)
		    rmem_chunk->mem_areas = rmem_chunk->mem_areas->next;
		  if (mem_area == rmem_chunk->mem_area)
		    rmem_chunk->mem_area = NULL;
		  
		  if (rmem_chunk->type == G_ALLOC_AND_FREE)
		    g_tree_remove (rmem_chunk->mem_tree, mem_area);
		  g_free (mem_area);
		}
	    }
	  else
	    {
	      prev_free_atom = temp_free_atom;
	      temp_free_atom = temp_free_atom->next;
	    }
	}
    }
}

void
g_mem_chunk_reset (GMemChunk *mem_chunk)
{
  GRealMemChunk *rmem_chunk;
  GMemArea *mem_areas;
  GMemArea *temp_area;
  
  g_return_if_fail (mem_chunk != NULL);
  
  rmem_chunk = (GRealMemChunk*) mem_chunk;
  
  mem_areas = rmem_chunk->mem_areas;
  rmem_chunk->num_mem_areas = 0;
  rmem_chunk->mem_areas = NULL;
  rmem_chunk->mem_area = NULL;
  
  while (mem_areas)
    {
      temp_area = mem_areas;
      mem_areas = mem_areas->next;
      g_free (temp_area);
    }
  
  rmem_chunk->free_atoms = NULL;
  
  if (rmem_chunk->mem_tree)
    g_tree_destroy (rmem_chunk->mem_tree);
  rmem_chunk->mem_tree = g_tree_new ((GCompareFunc) g_mem_chunk_area_compare);
}

void
g_mem_chunk_print (GMemChunk *mem_chunk)
{
  GRealMemChunk *rmem_chunk;
  GMemArea *mem_areas;
  gulong mem;
  
  g_return_if_fail (mem_chunk != NULL);
  
  rmem_chunk = (GRealMemChunk*) mem_chunk;
  mem_areas = rmem_chunk->mem_areas;
  mem = 0;
  
  while (mem_areas)
    {
      mem += rmem_chunk->area_size - mem_areas->free;
      mem_areas = mem_areas->next;
    }
  
  g_log (g_log_domain_glib, G_LOG_LEVEL_INFO,
	 "%s: %ld bytes using %d mem areas",
	 rmem_chunk->name, mem, rmem_chunk->num_mem_areas);
}

void
g_mem_chunk_info (void)
{
  GRealMemChunk *mem_chunk;
  gint count;
  
  count = 0;
  g_mutex_lock (mem_chunks_lock);
  mem_chunk = mem_chunks;
  while (mem_chunk)
    {
      count += 1;
      mem_chunk = mem_chunk->next;
    }
  g_mutex_unlock (mem_chunks_lock);
  
  g_log (g_log_domain_glib, G_LOG_LEVEL_INFO, "%d mem chunks", count);
  
  g_mutex_lock (mem_chunks_lock);
  mem_chunk = mem_chunks;
  g_mutex_unlock (mem_chunks_lock);

  while (mem_chunk)
    {
      g_mem_chunk_print ((GMemChunk*) mem_chunk);
      mem_chunk = mem_chunk->next;
    }  
}

void
g_blow_chunks (void)
{
  GRealMemChunk *mem_chunk;
  
  g_mutex_lock (mem_chunks_lock);
  mem_chunk = mem_chunks;
  g_mutex_unlock (mem_chunks_lock);
  while (mem_chunk)
    {
      g_mem_chunk_clean ((GMemChunk*) mem_chunk);
      mem_chunk = mem_chunk->next;
    }
}


static gulong
g_mem_chunk_compute_size (gulong size)
{
  gulong power_of_2;
  gulong lower, upper;
  
  power_of_2 = 16;
  while (power_of_2 < size)
    power_of_2 <<= 1;
  
  lower = power_of_2 >> 1;
  upper = power_of_2;
  
  if ((size - lower) < (upper - size))
    return lower;
  return upper;
}

static gint
g_mem_chunk_area_compare (GMemArea *a,
			  GMemArea *b)
{
  return (a->mem - b->mem);
}

static gint
g_mem_chunk_area_search (GMemArea *a,
			 gchar    *addr)
{
  if (a->mem <= addr)
    {
      if (addr < &a->mem[a->index])
	return 0;
      return 1;
    }
  return -1;
}

/* generic allocators
 */
struct _GAllocator /* from gmem.c */
{
  gchar		*name;
  guint16	 n_preallocs;
  guint		 is_unused : 1;
  guint		 type : 4;
  GAllocator	*last;
  GMemChunk	*mem_chunk;
  gpointer	 dummy; /* implementation specific */
};

GAllocator*
g_allocator_new (const gchar *name,
		 guint        n_preallocs)
{
  GAllocator *allocator;

  g_return_val_if_fail (name != NULL, NULL);

  allocator = g_new0 (GAllocator, 1);
  allocator->name = g_strdup (name);
  allocator->n_preallocs = CLAMP (n_preallocs, 1, 65535);
  allocator->is_unused = TRUE;
  allocator->type = 0;
  allocator->last = NULL;
  allocator->mem_chunk = NULL;
  allocator->dummy = NULL;

  return allocator;
}

void
g_allocator_free (GAllocator *allocator)
{
  g_return_if_fail (allocator != NULL);
  g_return_if_fail (allocator->is_unused == TRUE);

  g_free (allocator->name);
  if (allocator->mem_chunk)
    g_mem_chunk_destroy (allocator->mem_chunk);

  g_free (allocator);
}

void
g_mem_init (void)
{
  mem_chunks_lock = g_mutex_new();
#ifdef ENABLE_MEM_PROFILE
  mem_profile_lock = g_mutex_new();
  allocating_for_mem_chunk = g_private_new(NULL);
#endif
}
