/***************************************************************************/
/*                                                                         */
/*  psobjs.h                                                               */
/*                                                                         */
/*    Auxiliary functions for PostScript fonts (specification).            */
/*                                                                         */
/*  Copyright 1996-2001, 2002 by                                           */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#ifndef __PSOBJS_H__
#define __PSOBJS_H__


#include <ft2build.h>
#include FT_INTERNAL_POSTSCRIPT_AUX_H


FT_BEGIN_HEADER


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                             T1_TABLE                          *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_CALLBACK_TABLE
  const PS_Table_FuncsRec    ps_table_funcs;

  FT_CALLBACK_TABLE
  const PS_Parser_FuncsRec   ps_parser_funcs;

  FT_CALLBACK_TABLE
  const T1_Builder_FuncsRec  t1_builder_funcs;


  FT_LOCAL( FT_Error )
  PS_Table_New( PS_Table   table,
                FT_Int     count,
                FT_Memory  memory );

  FT_LOCAL( FT_Error )
  PS_Table_Add( PS_Table  table,
                FT_Int    idx,
                void*     object,
                FT_Int    length );

  FT_LOCAL( void )
  PS_Table_Done( PS_Table  table );


  FT_LOCAL( void )
  PS_Table_Release( PS_Table  table );


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                            T1 PARSER                          *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/


  FT_LOCAL( void )
  PS_Parser_SkipSpaces( PS_Parser  parser );

  FT_LOCAL( void )
  PS_Parser_SkipAlpha( PS_Parser  parser );

  FT_LOCAL( void )
  PS_Parser_ToToken( PS_Parser  parser,
                     T1_Token   token );

  FT_LOCAL( void )
  PS_Parser_ToTokenArray( PS_Parser  parser,
                          T1_Token   tokens,
                          FT_UInt    max_tokens,
                          FT_Int*    pnum_tokens );

  FT_LOCAL( FT_Error )
  PS_Parser_LoadField( PS_Parser       parser,
                       const T1_Field  field,
                       void**          objects,
                       FT_UInt         max_objects,
                       FT_ULong*       pflags );

  FT_LOCAL( FT_Error )
  PS_Parser_LoadFieldTable( PS_Parser       parser,
                            const T1_Field  field,
                            void**          objects,
                            FT_UInt         max_objects,
                            FT_ULong*       pflags );

  FT_LOCAL( FT_Long )
  PS_Parser_ToInt( PS_Parser  parser );


  FT_LOCAL( FT_Fixed )
  PS_Parser_ToFixed( PS_Parser  parser,
                     FT_Int     power_ten );


  FT_LOCAL( FT_Int )
  PS_Parser_ToCoordArray( PS_Parser  parser,
                          FT_Int     max_coords,
                          FT_Short*  coords );

  FT_LOCAL( FT_Int )
  PS_Parser_ToFixedArray( PS_Parser  parser,
                          FT_Int     max_values,
                          FT_Fixed*  values,
                          FT_Int     power_ten );


  FT_LOCAL( void )
  PS_Parser_Init( PS_Parser  parser,
                  FT_Byte*   base,
                  FT_Byte*   limit,
                  FT_Memory  memory );

  FT_LOCAL( void )
  PS_Parser_Done( PS_Parser  parser );


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                            T1 BUILDER                         *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/

  FT_LOCAL( void )
  T1_Builder_Init( T1_Builder    builder,
                   FT_Face       face,
                   FT_Size       size,
                   FT_GlyphSlot  glyph,
                   FT_Bool       hinting );

  FT_LOCAL( void )
  T1_Builder_Done( T1_Builder  builder );

  FT_LOCAL( FT_Error )
  T1_Builder_Check_Points( T1_Builder  builder,
                           FT_Int      count );

  FT_LOCAL( void )
  T1_Builder_Add_Point( T1_Builder  builder,
                        FT_Pos      x,
                        FT_Pos      y,
                        FT_Byte     flag );

  FT_LOCAL( FT_Error )
  T1_Builder_Add_Point1( T1_Builder  builder,
                         FT_Pos      x,
                         FT_Pos      y );

  FT_LOCAL( FT_Error )
  T1_Builder_Add_Contour( T1_Builder  builder );


  FT_LOCAL( FT_Error )
  T1_Builder_Start_Point( T1_Builder  builder,
                          FT_Pos      x,
                          FT_Pos      y );


  FT_LOCAL( void )
  T1_Builder_Close_Contour( T1_Builder  builder );


  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****                            OTHER                              *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/

  FT_LOCAL( void )
  T1_Decrypt( FT_Byte*   buffer,
              FT_Offset  length,
              FT_UShort  seed );


FT_END_HEADER

#endif /* __PSOBJS_H__ */


/* END */
