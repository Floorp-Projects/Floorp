// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXGE_FX_FREETYPE_H_
#define CORE_FXGE_FX_FREETYPE_H_

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_LCD_FILTER_H
#include FT_MULTIPLE_MASTERS_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H

#ifdef __cplusplus
extern "C" {
#endif

#define FXFT_ENCODING_UNICODE FT_ENCODING_UNICODE
#define FXFT_ENCODING_ADOBE_STANDARD FT_ENCODING_ADOBE_STANDARD
#define FXFT_ENCODING_ADOBE_EXPERT FT_ENCODING_ADOBE_EXPERT
#define FXFT_ENCODING_ADOBE_LATIN_1 FT_ENCODING_ADOBE_LATIN_1
#define FXFT_ENCODING_APPLE_ROMAN FT_ENCODING_APPLE_ROMAN
#define FXFT_ENCODING_ADOBE_CUSTOM FT_ENCODING_ADOBE_CUSTOM
#define FXFT_ENCODING_MS_SYMBOL FT_ENCODING_MS_SYMBOL
#define FXFT_ENCODING_GB2312 FT_ENCODING_GB2312
#define FXFT_ENCODING_BIG5 FT_ENCODING_BIG5
#define FXFT_ENCODING_SJIS FT_ENCODING_SJIS
#define FXFT_ENCODING_JOHAB FT_ENCODING_JOHAB
#define FXFT_ENCODING_WANSUNG FT_ENCODING_WANSUNG
#define FXFT_LOAD_NO_SCALE FT_LOAD_NO_SCALE
#define FXFT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH \
  FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH
#define FXFT_RENDER_MODE_LCD FT_RENDER_MODE_LCD
#define FXFT_RENDER_MODE_MONO FT_RENDER_MODE_MONO
#define FXFT_RENDER_MODE_NORMAL FT_RENDER_MODE_NORMAL
#define FXFT_LOAD_IGNORE_TRANSFORM FT_LOAD_IGNORE_TRANSFORM
#define FXFT_LOAD_NO_BITMAP FT_LOAD_NO_BITMAP
#define FXFT_LOAD_NO_HINTING FT_LOAD_NO_HINTING
#define FXFT_PIXEL_MODE_MONO FT_PIXEL_MODE_MONO
#define FXFT_STYLE_FLAG_ITALIC FT_STYLE_FLAG_ITALIC
#define FXFT_STYLE_FLAG_BOLD FT_STYLE_FLAG_BOLD
#define FXFT_FACE_FLAG_SFNT FT_FACE_FLAG_SFNT
#define FXFT_FACE_FLAG_TRICKY (1L << 13)
typedef FT_MM_Var* FXFT_MM_Var;
typedef FT_Bitmap* FXFT_Bitmap;
#define FXFT_Matrix FT_Matrix
#define FXFT_Vector FT_Vector
#define FXFT_Outline_Funcs FT_Outline_Funcs
typedef FT_Open_Args FXFT_Open_Args;
typedef FT_StreamRec FXFT_StreamRec;
typedef FT_StreamRec* FXFT_Stream;
typedef FT_BBox FXFT_BBox;
typedef FT_Glyph FXFT_Glyph;
typedef FT_CharMap FXFT_CharMap;
#define FXFT_GLYPH_BBOX_PIXELS FT_GLYPH_BBOX_PIXELS
#define FXFT_Open_Face(library, args, index, face) \
  FT_Open_Face((FT_Library)(library), args, index, (FT_Face*)(face))
#define FXFT_Done_Face(face) FT_Done_Face((FT_Face)(face))
#define FXFT_Done_FreeType(library) FT_Done_FreeType((FT_Library)(library))
#define FXFT_Init_FreeType(library) FT_Init_FreeType((FT_Library*)(library))
#define FXFT_New_Memory_Face(library, base, size, index, face) \
  FT_New_Memory_Face((FT_Library)(library), base, size, index, (FT_Face*)(face))
#define FXFT_New_Face(library, filename, index, face) \
  FT_New_Face((FT_Library)(library), filename, index, (FT_Face*)(face))
#define FXFT_Get_Face_FreeType(face) ((FT_Face)face)->driver->root.library
#define FXFT_Select_Charmap(face, encoding) \
  FT_Select_Charmap((FT_Face)face, (FT_Encoding)encoding)
#define FXFT_Set_Charmap(face, charmap) \
  FT_Set_Charmap((FT_Face)face, (FT_CharMap)charmap)
#define FXFT_Load_Glyph(face, glyph_index, flags) \
  FT_Load_Glyph((FT_Face)face, glyph_index, flags)
#define FXFT_Get_Char_Index(face, code) FT_Get_Char_Index((FT_Face)face, code)
#define FXFT_Get_Glyph_Name(face, index, buffer, size) \
  FT_Get_Glyph_Name((FT_Face)face, index, buffer, size)
#define FXFT_Get_Name_Index(face, name) FT_Get_Name_Index((FT_Face)face, name)
#define FXFT_Has_Glyph_Names(face) \
  (((FT_Face)face)->face_flags & FT_FACE_FLAG_GLYPH_NAMES)
#define FXFT_Get_Postscript_Name(face) FT_Get_Postscript_Name((FT_Face)face)
#define FXFT_Load_Sfnt_Table(face, tag, offset, buffer, length) \
  FT_Load_Sfnt_Table((FT_Face)face, tag, offset, buffer, length)
#define FXFT_Get_First_Char(face, glyph_index) \
  FT_Get_First_Char((FT_Face)face, glyph_index)
#define FXFT_Get_Next_Char(face, code, glyph_index) \
  FT_Get_Next_Char((FT_Face)face, code, glyph_index)
#define FXFT_Clear_Face_External_Stream(face) \
  (((FT_Face)face)->face_flags &= ~FT_FACE_FLAG_EXTERNAL_STREAM)
#define FXFT_Get_Face_External_Stream(face) \
  (((FT_Face)face)->face_flags & FT_FACE_FLAG_EXTERNAL_STREAM)
#define FXFT_Is_Face_TT_OT(face) \
  (((FT_Face)face)->face_flags & FT_FACE_FLAG_SFNT)
#define FXFT_Is_Face_Tricky(face) \
  (((FT_Face)face)->face_flags & FXFT_FACE_FLAG_TRICKY)
#define FXFT_Is_Face_fixedwidth(face) \
  (((FT_Face)face)->face_flags & FT_FACE_FLAG_FIXED_WIDTH)
#define FXFT_Get_Face_Stream_Base(face) ((FT_Face)face)->stream->base
#define FXFT_Get_Face_Stream_Size(face) ((FT_Face)face)->stream->size
#define FXFT_Get_Face_Family_Name(face) ((FT_Face)face)->family_name
#define FXFT_Get_Face_Style_Name(face) ((FT_Face)face)->style_name
#define FXFT_Get_Face_Numfaces(face) ((FT_Face)face)->num_faces
#define FXFT_Get_Face_Faceindex(face) ((FT_Face)face)->face_index
#define FXFT_Is_Face_Italic(face) \
  (((FT_Face)face)->style_flags & FT_STYLE_FLAG_ITALIC)
#define FXFT_Is_Face_Bold(face) \
  (((FT_Face)face)->style_flags & FT_STYLE_FLAG_BOLD)
#define FXFT_Get_Face_Charmaps(face) ((FT_Face)face)->charmaps
#define FXFT_Get_Glyph_HoriBearingX(face) \
  ((FT_Face)face)->glyph->metrics.horiBearingX
#define FXFT_Get_Glyph_HoriBearingY(face) \
  ((FT_Face)face)->glyph->metrics.horiBearingY
#define FXFT_Get_Glyph_Width(face) ((FT_Face)face)->glyph->metrics.width
#define FXFT_Get_Glyph_Height(face) ((FT_Face)face)->glyph->metrics.height
#define FXFT_Get_Face_CharmapCount(face) ((FT_Face)face)->num_charmaps
#define FXFT_Get_Charmap_Encoding(charmap) ((FT_CharMap)charmap)->encoding
#define FXFT_Get_Face_Charmap(face) ((FT_Face)face)->charmap
#define FXFT_Get_Charmap_PlatformID(charmap) ((FT_CharMap)charmap)->platform_id
#define FXFT_Get_Charmap_EncodingID(charmap) ((FT_CharMap)charmap)->encoding_id
#define FXFT_Get_Face_UnitsPerEM(face) ((FT_Face)face)->units_per_EM
#define FXFT_Get_Face_xMin(face) ((FT_Face)face)->bbox.xMin
#define FXFT_Get_Face_xMax(face) ((FT_Face)face)->bbox.xMax
#define FXFT_Get_Face_yMin(face) ((FT_Face)face)->bbox.yMin
#define FXFT_Get_Face_yMax(face) ((FT_Face)face)->bbox.yMax
#define FXFT_Get_Face_Height(face) ((FT_Face)face)->height
#define FXFT_Get_Face_UnderLineThickness(face) \
  ((FT_Face)face)->underline_thickness
#define FXFT_Get_Face_UnderLinePosition(face) \
  ((FT_Face)face)->underline_position
#define FXFT_Get_Face_MaxAdvanceWidth(face) ((FT_Face)face)->max_advance_width
#define FXFT_Get_Face_Ascender(face) ((FT_Face)face)->ascender
#define FXFT_Get_Face_Descender(face) ((FT_Face)face)->descender
#define FXFT_Get_Glyph_HoriAdvance(face) \
  ((FT_Face)face)->glyph->metrics.horiAdvance
#define FXFT_Get_MM_Axis(var, index) &static_cast<FT_MM_Var*>(var)->axis[index]
#define FXFT_Get_MM_Axis_Min(axis) ((FT_Var_Axis*)axis)->minimum
#define FXFT_Get_MM_Axis_Max(axis) ((FT_Var_Axis*)axis)->maximum
#define FXFT_Get_MM_Axis_Def(axis) ((FT_Var_Axis*)axis)->def
#define FXFT_Alloc(library, size) \
  ((FT_Library)library)->memory->alloc(((FT_Library)library)->memory, size)
#define FXFT_Free(face, p) \
  ((FT_Face)face)->memory->free(((FT_Face)face)->memory, p)
#define FXFT_Get_Glyph_Outline(face) &static_cast<FT_Face>(face)->glyph->outline
#define FXFT_Get_Outline_Bbox(outline, cbox) FT_Outline_Get_CBox(outline, cbox)
#define FXFT_Render_Glyph(face, mode) \
  FT_Render_Glyph(((FT_Face)face)->glyph, (enum FT_Render_Mode_)mode)
#define FXFT_Get_MM_Var(face, p) FT_Get_MM_Var((FT_Face)face, p)
#define FXFT_Set_MM_Design_Coordinates(face, n, p) \
  FT_Set_MM_Design_Coordinates((FT_Face)face, n, p)
#define FXFT_Set_Pixel_Sizes(face, w, h) FT_Set_Pixel_Sizes((FT_Face)face, w, h)
#define FXFT_Set_Transform(face, m, d) FT_Set_Transform((FT_Face)face, m, d)
#define FXFT_Outline_Embolden(outline, s) FT_Outline_Embolden(outline, s)
#define FXFT_Get_Glyph_Bitmap(face) &static_cast<FT_Face>(face)->glyph->bitmap
#define FXFT_Get_Bitmap_Width(bitmap) ((FT_Bitmap*)bitmap)->width
#define FXFT_Get_Bitmap_Rows(bitmap) ((FT_Bitmap*)bitmap)->rows
#define FXFT_Get_Bitmap_PixelMode(bitmap) ((FT_Bitmap*)bitmap)->pixel_mode
#define FXFT_Get_Bitmap_Pitch(bitmap) ((FT_Bitmap*)bitmap)->pitch
#define FXFT_Get_Bitmap_Buffer(bitmap) ((FT_Bitmap*)bitmap)->buffer
#define FXFT_Get_Glyph_BitmapLeft(face) ((FT_Face)face)->glyph->bitmap_left
#define FXFT_Get_Glyph_BitmapTop(face) ((FT_Face)face)->glyph->bitmap_top
#define FXFT_Outline_Decompose(outline, funcs, params) \
  FT_Outline_Decompose(outline, funcs, params)
#define FXFT_Set_Char_Size(face, char_width, char_height, horz_resolution, \
                           vert_resolution)                                \
  FT_Set_Char_Size(face, char_width, char_height, horz_resolution,         \
                   vert_resolution)
#define FXFT_Get_Glyph(slot, aglyph) FT_Get_Glyph(slot, aglyph)
#define FXFT_Glyph_Get_CBox(glyph, bbox_mode, acbox) \
  FT_Glyph_Get_CBox(glyph, bbox_mode, acbox)
#define FXFT_Done_Glyph(glyph) FT_Done_Glyph(glyph)
#define FXFT_Library_SetLcdFilter(library, filter) \
  FT_Library_SetLcdFilter((FT_Library)(library), filter)
int FXFT_unicode_from_adobe_name(const char* glyph_name);
void FXFT_adobe_name_from_unicode(char* name, wchar_t unicode);
#ifdef __cplusplus
};
#endif

#endif  // CORE_FXGE_FX_FREETYPE_H_
