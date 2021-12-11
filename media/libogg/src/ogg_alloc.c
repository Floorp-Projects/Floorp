/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 *********************************************************************/

#include <stdlib.h>
#include "ogg/os_types.h"

ogg_malloc_function_type *ogg_malloc_func = malloc;
ogg_calloc_function_type *ogg_calloc_func = calloc;
ogg_realloc_function_type *ogg_realloc_func = realloc;
ogg_free_function_type *ogg_free_func = free;

void
ogg_set_mem_functions(ogg_malloc_function_type *malloc_func,
                      ogg_calloc_function_type *calloc_func,
                      ogg_realloc_function_type *realloc_func,
                      ogg_free_function_type *free_func)
{
  ogg_malloc_func = malloc_func;
  ogg_calloc_func = calloc_func;
  ogg_realloc_func = realloc_func;
  ogg_free_func = free_func;
}
