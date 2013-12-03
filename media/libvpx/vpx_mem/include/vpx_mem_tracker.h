/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __VPX_MEM_TRACKER_H__
#define __VPX_MEM_TRACKER_H__

/* vpx_mem_tracker version info */
#define vpx_mem_tracker_version "2.5.1.1"

#define VPX_MEM_TRACKER_VERSION_CHIEF 2
#define VPX_MEM_TRACKER_VERSION_MAJOR 5
#define VPX_MEM_TRACKER_VERSION_MINOR 1
#define VPX_MEM_TRACKER_VERSION_PATCH 1
/* END - vpx_mem_tracker version info */

#include <stdarg.h>

struct mem_block
{
    size_t addr;
    unsigned int size,
             line;
    char *file;
    struct mem_block *prev,
            * next;

    int padded; // This mem_block has padding for integrity checks.
    // As of right now, this should only be 0 if
    // using vpx_mem_alloc to allocate cache memory.
    // 2005-01-11 tjf
};

#if defined(__cplusplus)
extern "C" {
#endif

    /*
        vpx_memory_tracker_init(int padding_size, int pad_value)
          padding_size - the size of the padding before and after each mem addr.
                         Values > 0 indicate that integrity checks can be performed
                         by inspecting these areas.
          pad_value - the initial value within the padding area before and after
                      each mem addr.

        Initializes the memory tracker interface. Should be called before any
        other calls to the memory tracker.
    */
    int vpx_memory_tracker_init(int padding_size, int pad_value);

    /*
        vpx_memory_tracker_destroy()
        Deinitializes the memory tracker interface
    */
    void vpx_memory_tracker_destroy();

    /*
        vpx_memory_tracker_add(size_t addr, unsigned int size,
                             char * file, unsigned int line)
          addr - memory address to be added to list
          size - size of addr
          file - the file addr was referenced from
          line - the line in file addr was referenced from
        Adds memory address addr, it's size, file and line it came from
        to the memory tracker allocation table
    */
    void vpx_memory_tracker_add(size_t addr, unsigned int size,
                                char *file, unsigned int line,
                                int padded);

    /*
        vpx_memory_tracker_add(size_t addr, unsigned int size, char * file, unsigned int line)
          addr - memory address to be added to be removed
          padded - if 0, disables bounds checking on this memory block even if bounds
          checking is enabled. (for example, when allocating cache memory, we still want
          to check for memory leaks, but we do not waste cache space for bounds check padding)
        Removes the specified address from the memory tracker's allocation
        table
        Return:
          0: on success
          -1: if memory allocation table's mutex could not be locked
          -2: if the addr was not found in the list
    */
    int vpx_memory_tracker_remove(size_t addr);

    /*
        vpx_memory_tracker_find(unsigned int addr)
          addr - address to be found in the memory tracker's
                 allocation table
        Return:
            If found, pointer to the memory block that matches addr
            NULL otherwise
    */
    struct mem_block *vpx_memory_tracker_find(size_t addr);

    /*
        vpx_memory_tracker_dump()
        Dumps the current contents of the memory
        tracker allocation table
    */
    void vpx_memory_tracker_dump();

    /*
        vpx_memory_tracker_check_integrity()
        If a padding_size was provided to vpx_memory_tracker_init()
        This function will verify that the region before and after each
        memory address contains the specified pad_value. Should the check
        fail, the filename and line of the check will be printed out.
    */
    void vpx_memory_tracker_check_integrity(char *file, unsigned int line);

    /*
        vpx_memory_tracker_set_log_type
          type - value representing the logging type to use
          option - type specific option. This will be interpreted differently
                   based on the type.
        Sets the logging type for the memory tracker.
        Values currently supported:
          0: if option is NULL, log to stderr, otherwise interpret option as a
             filename and attempt to open it.
          1: Use output_debug_string (WIN32 only), option ignored
        Return:
          0: on success
          -1: if the logging type could not be set, because the value was invalid
              or because a file could not be opened
    */
    int vpx_memory_tracker_set_log_type(int type, char *option);

    /*
        vpx_memory_tracker_set_log_func
          userdata - ptr to be passed to the supplied logfunc, can be NULL
          logfunc - the logging function to be used to output data from
                    vpx_memory_track_dump/check_integrity
        Sets a logging function to be used by the memory tracker.
        Return:
          0: on success
          -1: if the logging type could not be set because logfunc was NULL
    */
    int vpx_memory_tracker_set_log_func(void *userdata,
                                        void(*logfunc)(void *userdata,
                                                const char *fmt, va_list args));

    /* Wrappers to standard library functions. */
    typedef void*(* mem_track_malloc_func)(size_t);
    typedef void*(* mem_track_calloc_func)(size_t, size_t);
    typedef void*(* mem_track_realloc_func)(void *, size_t);
    typedef void (* mem_track_free_func)(void *);
    typedef void*(* mem_track_memcpy_func)(void *, const void *, size_t);
    typedef void*(* mem_track_memset_func)(void *, int, size_t);
    typedef void*(* mem_track_memmove_func)(void *, const void *, size_t);

    /*
        vpx_memory_tracker_set_functions

        Sets the function pointers for the standard library functions.

        Return:
          0: on success
          -1: if the use global function pointers is not set.
    */
    int vpx_memory_tracker_set_functions(mem_track_malloc_func g_malloc_l
                                         , mem_track_calloc_func g_calloc_l
                                         , mem_track_realloc_func g_realloc_l
                                         , mem_track_free_func g_free_l
                                         , mem_track_memcpy_func g_memcpy_l
                                         , mem_track_memset_func g_memset_l
                                         , mem_track_memmove_func g_memmove_l);

#if defined(__cplusplus)
}
#endif

#endif //__VPX_MEM_TRACKER_H__
