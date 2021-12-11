/****************************************************************************
 *
 * ftsystem.c
 *
 *   Windows-specific FreeType low-level system interface (body).
 *
 * Copyright (C) 2021 by
 * David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */


#include <ft2build.h>
  /* we use our special ftconfig.h file, not the standard one */
#include FT_CONFIG_CONFIG_H
#include <freetype/internal/ftdebug.h>
#include <freetype/ftsystem.h>
#include <freetype/fterrors.h>
#include <freetype/fttypes.h>
#include <freetype/internal/ftstream.h>

  /* memory-mapping includes and definitions */
#include <windows.h>

#include <stdlib.h>


  /**************************************************************************
   *
   *                      MEMORY MANAGEMENT INTERFACE
   *
   */


  /**************************************************************************
   *
   * It is not necessary to do any error checking for the
   * allocation-related functions.  This will be done by the higher level
   * routines like ft_mem_alloc() or ft_mem_realloc().
   *
   */


  /**************************************************************************
   *
   * @Function:
   *   ft_alloc
   *
   * @Description:
   *   The memory allocation function.
   *
   * @Input:
   *   memory ::
   *     A pointer to the memory object.
   *
   *   size ::
   *     The requested size in bytes.
   *
   * @Return:
   *   The address of newly allocated block.
   */
  FT_CALLBACK_DEF( void* )
  ft_alloc( FT_Memory  memory,
            long       size )
  {
    FT_UNUSED( memory );

    return malloc( size );
  }


  /**************************************************************************
   *
   * @Function:
   *   ft_realloc
   *
   * @Description:
   *   The memory reallocation function.
   *
   * @Input:
   *   memory ::
   *     A pointer to the memory object.
   *
   *   cur_size ::
   *     The current size of the allocated memory block.
   *
   *   new_size ::
   *     The newly requested size in bytes.
   *
   *   block ::
   *     The current address of the block in memory.
   *
   * @Return:
   *   The address of the reallocated memory block.
   */
  FT_CALLBACK_DEF( void* )
  ft_realloc( FT_Memory  memory,
              long       cur_size,
              long       new_size,
              void*      block )
  {
    FT_UNUSED( memory );
    FT_UNUSED( cur_size );

    return realloc( block, new_size );
  }


  /**************************************************************************
   *
   * @Function:
   *   ft_free
   *
   * @Description:
   *   The memory release function.
   *
   * @Input:
   *   memory ::
   *     A pointer to the memory object.
   *
   *   block ::
   *     The address of block in memory to be freed.
   */
  FT_CALLBACK_DEF( void )
  ft_free( FT_Memory  memory,
           void*      block )
  {
    FT_UNUSED( memory );

    free( block );
  }


  /**************************************************************************
   *
   *                    RESOURCE MANAGEMENT INTERFACE
   *
   */


  /**************************************************************************
   *
   * The macro FT_COMPONENT is used in trace mode.  It is an implicit
   * parameter of the FT_TRACE() and FT_ERROR() macros, used to print/log
   * messages during execution.
   */
#undef  FT_COMPONENT
#define FT_COMPONENT  io

  /* We use the macro STREAM_FILE for convenience to extract the       */
  /* system-specific stream handle from a given FreeType stream object */
#define STREAM_FILE( stream )  ( (FILE*)stream->descriptor.pointer )


  /**************************************************************************
   *
   * @Function:
   *   ft_close_stream_by_munmap
   *
   * @Description:
   *   The function to close a stream which is opened by mmap.
   *
   * @Input:
   *   stream :: A pointer to the stream object.
   */
  FT_CALLBACK_DEF( void )
  ft_close_stream_by_munmap( FT_Stream  stream )
  {
    UnmapViewOfFile( (LPCVOID)stream->descriptor.pointer );

    stream->descriptor.pointer = NULL;
    stream->size               = 0;
    stream->base               = 0;
  }


  /**************************************************************************
   *
   * @Function:
   *   ft_close_stream_by_free
   *
   * @Description:
   *   The function to close a stream which is created by ft_alloc.
   *
   * @Input:
   *   stream :: A pointer to the stream object.
   */
  FT_CALLBACK_DEF( void )
  ft_close_stream_by_free( FT_Stream  stream )
  {
    ft_free( NULL, stream->descriptor.pointer );

    stream->descriptor.pointer = NULL;
    stream->size               = 0;
    stream->base               = 0;
  }


  /* documentation is in ftobjs.h */

  FT_BASE_DEF( FT_Error )
  FT_Stream_Open( FT_Stream    stream,
                  const char*  filepathname )
  {
    HANDLE         file;
    HANDLE         fm;
    LARGE_INTEGER  size;


    if ( !stream )
      return FT_THROW( Invalid_Stream_Handle );

    /* open the file */
    file = CreateFileA( filepathname, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
    if ( file == INVALID_HANDLE_VALUE )
    {
      FT_ERROR(( "FT_Stream_Open:" ));
      FT_ERROR(( " could not open `%s'\n", filepathname ));
      return FT_THROW( Cannot_Open_Resource );
    }

    if ( GetFileSizeEx( file, &size ) == FALSE )
    {
      FT_ERROR(( "FT_Stream_Open:" ));
      FT_ERROR(( " could not retrieve size of file `%s'\n", filepathname ));
      goto Fail_Open;
    }

    /* `stream->size' is typedef'd to unsigned long (in `ftsystem.h'); */
    /* So avoid overflow caused by fonts in huge files larger than     */
    /* 2GB, do a test.                                                 */
    if ( size.QuadPart > LONG_MAX )
    {
      FT_ERROR(( "FT_Stream_Open: file is too big\n" ));
      goto Fail_Open;
    }
    else if ( size.QuadPart == 0 )
    {
      FT_ERROR(( "FT_Stream_Open: zero-length file\n" ));
      goto Fail_Open;
    }

    fm = CreateFileMapping( file, NULL, PAGE_READONLY, 0, 0, NULL );
    if ( fm == NULL )
    {
      FT_ERROR(( "FT_Stream_Open: can not map file\n" ));
      goto Fail_Open;
    }

    /* Store only the low part of this 64 bits integer because long is */
    /* a 32 bits type. Anyway, a check has been done above to forbid   */
    /* a size greater than LONG_MAX                                    */
    stream->size = size.LowPart;
    stream->pos  = 0;
    stream->base = (unsigned char *)
                     MapViewOfFile( fm, FILE_MAP_READ, 0, 0, 0 );

    CloseHandle( fm );

    if ( stream->base != NULL )
      stream->close = ft_close_stream_by_munmap;
    else
    {
      DWORD  total_read_count;


      FT_ERROR(( "FT_Stream_Open:" ));
      FT_ERROR(( " could not `mmap' file `%s'\n", filepathname ));

      stream->base = (unsigned char*)ft_alloc( NULL, stream->size );

      if ( !stream->base )
      {
        FT_ERROR(( "FT_Stream_Open:" ));
        FT_ERROR(( " could not `alloc' memory\n" ));
        goto Fail_Map;
      }

      total_read_count = 0;
      do
      {
        DWORD  read_count;


        if ( ReadFile( file,
                       stream->base + total_read_count,
                       stream->size - total_read_count,
                       &read_count, NULL ) == FALSE )
        {
          FT_ERROR(( "FT_Stream_Open:" ));
          FT_ERROR(( " error while `read'ing file `%s'\n", filepathname ));
          goto Fail_Read;
        }

        total_read_count += read_count;

      } while ( total_read_count != stream->size );

      stream->close = ft_close_stream_by_free;
    }

    CloseHandle( file );

    stream->descriptor.pointer = stream->base;
    stream->pathname.pointer   = (char*)filepathname;

    stream->read = 0;

    FT_TRACE1(( "FT_Stream_Open:" ));
    FT_TRACE1(( " opened `%s' (%ld bytes) successfully\n",
                filepathname, stream->size ));

    return FT_Err_Ok;

  Fail_Read:
    ft_free( NULL, stream->base );

  Fail_Map:
    CloseHandle( file );

  Fail_Open:
    CloseHandle( file );

    stream->base = NULL;
    stream->size = 0;
    stream->pos  = 0;

    return FT_THROW( Cannot_Open_Stream );
  }


#ifdef FT_DEBUG_MEMORY

  extern FT_Int
  ft_mem_debug_init( FT_Memory  memory );

  extern void
  ft_mem_debug_done( FT_Memory  memory );

#endif


  /* documentation is in ftobjs.h */

  FT_BASE_DEF( FT_Memory )
  FT_New_Memory( void )
  {
    FT_Memory  memory;


    memory = (FT_Memory)malloc( sizeof ( *memory ) );
    if ( memory )
    {
      memory->user    = 0;
      memory->alloc   = ft_alloc;
      memory->realloc = ft_realloc;
      memory->free    = ft_free;
#ifdef FT_DEBUG_MEMORY
      ft_mem_debug_init( memory );
#endif
    }

    return memory;
  }


  /* documentation is in ftobjs.h */

  FT_BASE_DEF( void )
  FT_Done_Memory( FT_Memory  memory )
  {
#ifdef FT_DEBUG_MEMORY
    ft_mem_debug_done( memory );
#endif
    memory->free( memory, memory );
  }


/* END */
