/***************************************************************************/
/*                                                                         */
/*  psauxmod.c                                                             */
/*                                                                         */
/*    FreeType auxiliary PostScript module implementation (body).          */
/*                                                                         */
/*  Copyright 2000-2001, 2002 by                                           */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/


#include <ft2build.h>
#include "psauxmod.h"
#include "psobjs.h"
#include "t1decode.h"
#include "t1cmap.h"


  FT_CALLBACK_TABLE_DEF
  const PS_Table_FuncsRec  ps_table_funcs =
  {
    PS_Table_New,
    PS_Table_Done,
    PS_Table_Add,
    PS_Table_Release
  };


  FT_CALLBACK_TABLE_DEF
  const PS_Parser_FuncsRec  ps_parser_funcs =
  {
    PS_Parser_Init,
    PS_Parser_Done,
    PS_Parser_SkipSpaces,
    PS_Parser_SkipAlpha,
    PS_Parser_ToInt,
    PS_Parser_ToFixed,
    PS_Parser_ToCoordArray,
    PS_Parser_ToFixedArray,
    PS_Parser_ToToken,
    PS_Parser_ToTokenArray,
    PS_Parser_LoadField,
    PS_Parser_LoadFieldTable
  };


  FT_CALLBACK_TABLE_DEF
  const T1_Builder_FuncsRec  t1_builder_funcs =
  {
    T1_Builder_Init,
    T1_Builder_Done,
    T1_Builder_Check_Points,
    T1_Builder_Add_Point,
    T1_Builder_Add_Point1,
    T1_Builder_Add_Contour,
    T1_Builder_Start_Point,
    T1_Builder_Close_Contour
  };


  FT_CALLBACK_TABLE_DEF
  const T1_Decoder_FuncsRec  t1_decoder_funcs =
  {
    T1_Decoder_Init,
    T1_Decoder_Done,
    T1_Decoder_Parse_Charstrings
  };


  FT_CALLBACK_TABLE_DEF
  const T1_CMap_ClassesRec  t1_cmap_classes =
  {
    &t1_cmap_standard_class_rec,
    &t1_cmap_expert_class_rec,
    &t1_cmap_custom_class_rec,
    &t1_cmap_unicode_class_rec
  };


  static
  const PSAux_Interface  psaux_interface =
  {
    &ps_table_funcs,
    &ps_parser_funcs,
    &t1_builder_funcs,
    &t1_decoder_funcs,

    T1_Decrypt,
    
    (const T1_CMap_ClassesRec*) &t1_cmap_classes,
  };


  FT_CALLBACK_TABLE_DEF
  const FT_Module_Class  psaux_module_class =
  {
    0,
    sizeof( FT_ModuleRec ),
    "psaux",
    0x10000L,
    0x20000L,

    &psaux_interface,  /* module-specific interface */

    (FT_Module_Constructor)0,
    (FT_Module_Destructor) 0,
    (FT_Module_Requester)  0
  };


/* END */
