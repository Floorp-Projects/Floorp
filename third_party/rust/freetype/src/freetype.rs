// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.


use libc::*;

pub type FT_Int16 = c_short;
pub type FT_UInt16 = c_ushort;
pub type FT_Int32 = c_int;
pub type FT_UInt32 = c_uint;
pub type FT_Fast = c_int;
pub type FT_UFast = c_uint;

pub type FT_Memory = *mut struct_FT_MemoryRec_;
pub type FT_Alloc_Func = extern fn(mem: FT_Memory, size: c_long) -> *mut c_void;
pub type FT_Free_Func = extern fn(mem: FT_Memory, block: *mut c_void);
pub type FT_Realloc_Func = extern fn(mem: FT_Memory,
                                     cur_size: c_long,
                                     new_size: c_long,
                                     block: *mut c_void) -> *mut c_void;

#[repr(C)]
pub struct struct_FT_MemoryRec_ {
    pub user: *mut c_void,
    pub alloc: FT_Alloc_Func,
    pub free: FT_Free_Func,
    pub realloc: FT_Realloc_Func,
}

pub type FT_Stream = *mut struct_FT_StreamRec_;
pub type union_FT_StreamDesc_ = c_void /* FIXME: union type */;
pub type FT_StreamDesc = union_FT_StreamDesc_;
pub type FT_Stream_IoFunc = *mut u8;
pub type FT_Stream_CloseFunc = *mut u8;

#[repr(C)]
pub struct struct_FT_StreamRec_ {
    pub base: *mut c_uchar,
    pub size: c_ulong,
    pub pos: c_ulong,
    pub descriptor: FT_StreamDesc,
    pub pathname: FT_StreamDesc,
    pub read: FT_Stream_IoFunc,
    pub close: FT_Stream_CloseFunc,
    pub memory: *mut c_void /* FT_Memory */,
    pub cursor: *mut c_uchar,
    pub limit: *mut c_uchar,
}

pub type FT_StreamRec = struct_FT_StreamRec_;
pub type FT_Pos = c_long;

#[repr(C)]
pub struct struct_FT_Vector_ {
    pub x: FT_Pos,
    pub y: FT_Pos,
}

pub type FT_Vector = struct_FT_Vector_;

#[repr(C)]
pub struct struct_FT_BBox_ {
    pub xMin: FT_Pos,
    pub yMin: FT_Pos,
    pub xMax: FT_Pos,
    pub yMax: FT_Pos,
}

pub type FT_BBox = struct_FT_BBox_;

#[repr(C)]
pub enum FT_LcdFilter {
    FT_LCD_FILTER_NONE    = 0,
    FT_LCD_FILTER_DEFAULT = 1,
    FT_LCD_FILTER_LIGHT   = 2,
    FT_LCD_FILTER_LEGACY1 = 3,
    FT_LCD_FILTER_LEGACY  = 16,
}

pub type enum_FT_Sfnt_Tag_ = c_uint;
pub const ft_sfnt_head: u32 = 0_u32;
pub const ft_sfnt_maxp: u32 = 1_u32;
pub const ft_sfnt_os2: u32 = 2_u32;
pub const ft_sfnt_hhea: u32 = 3_u32;
pub const ft_sfnt_vhea: u32 = 4_u32;
pub const ft_sfnt_post: u32 = 5_u32;
pub const ft_sfnt_pclt: u32 = 6_u32;
pub const ft_sfnt_max: u32 = 7_u32;
pub type FT_Sfnt_Tag = enum_FT_Sfnt_Tag_;

pub type enum_FT_Pixel_Mode_ = c_uint;
pub const FT_PIXEL_MODE_NONE: u32 = 0_u32;
pub const FT_PIXEL_MODE_MONO: u32 = 1_u32;
pub const FT_PIXEL_MODE_GRAY: u32 = 2_u32;
pub const FT_PIXEL_MODE_GRAY2: u32 = 3_u32;
pub const FT_PIXEL_MODE_GRAY4: u32 = 4_u32;
pub const FT_PIXEL_MODE_LCD: u32 = 5_u32;
pub const FT_PIXEL_MODE_LCD_V: u32 = 6_u32;
pub const FT_PIXEL_MODE_MAX: u32 = 7_u32;

pub type FT_Pixel_Mode = enum_FT_Pixel_Mode_;

#[repr(C)]
pub struct struct_FT_Bitmap_ {
    pub rows: c_int,
    pub width: c_int,
    pub pitch: c_int,
    pub buffer: *mut c_uchar,
    pub num_grays: c_short,
    pub pixel_mode: c_char,
    pub palette_mode: c_char,
    pub palette: *mut c_void,
}

pub type FT_Bitmap = struct_FT_Bitmap_;

#[repr(C)]
pub struct struct_FT_Outline_ {
    pub n_contours: c_short,
    pub n_points: c_short,
    pub points: *mut FT_Vector,
    pub tags: *mut c_char,
    pub contours: *mut c_short,
    pub flags: c_int,
}

pub type FT_Outline = struct_FT_Outline_;
pub type FT_Outline_MoveToFunc = *mut u8;
pub type FT_Outline_LineToFunc = *mut u8;
pub type FT_Outline_ConicToFunc = *mut u8;
pub type FT_Outline_CubicToFunc = *mut u8;

#[repr(C)]
pub struct struct_FT_Outline_Funcs_ {
    pub move_to: FT_Outline_MoveToFunc,
    pub line_to: FT_Outline_LineToFunc,
    pub conic_to: FT_Outline_ConicToFunc,
    pub cubic_to: FT_Outline_CubicToFunc,
    pub shift: c_int,
    pub delta: FT_Pos,
}

pub type FT_Outline_Funcs = struct_FT_Outline_Funcs_;

pub type enum_FT_Glyph_Format_ = c_uint;
pub const FT_GLYPH_FORMAT_NONE: u32 = 0_u32;
pub const FT_GLYPH_FORMAT_COMPOSITE: u32 = 1668246896_u32;
pub const FT_GLYPH_FORMAT_BITMAP: u32 = 1651078259_u32;
pub const FT_GLYPH_FORMAT_OUTLINE: u32 = 1869968492_u32;
pub const FT_GLYPH_FORMAT_PLOTTER: u32 = 1886154612_u32;

pub type FT_Glyph_Format = enum_FT_Glyph_Format_;
pub type struct_FT_RasterRec_ = c_void;
pub type FT_Raster = *mut struct_FT_RasterRec_;

#[repr(C)]
pub struct struct_FT_Span_ {
    pub x: c_short,
    pub len: c_ushort,
    pub coverage: c_uchar,
}

pub type FT_Span = struct_FT_Span_;
pub type FT_SpanFunc = *mut u8;
pub type FT_Raster_BitTest_Func = *mut u8;
pub type FT_Raster_BitSet_Func = *mut u8;

#[repr(C)]
pub struct struct_FT_Raster_Params_ {
    pub target: *mut FT_Bitmap,
    pub source: *mut c_void,
    pub flags: c_int,
    pub gray_spans: FT_SpanFunc,
    pub black_spans: FT_SpanFunc,
    pub bit_test: FT_Raster_BitTest_Func,
    pub bit_set: FT_Raster_BitSet_Func,
    pub user: *mut c_void,
    pub clip_box: FT_BBox,
}

pub type FT_Raster_Params = struct_FT_Raster_Params_;
pub type FT_Raster_NewFunc = *mut u8;
pub type FT_Raster_DoneFunc = *mut u8;
pub type FT_Raster_ResetFunc = *mut u8;
pub type FT_Raster_SetModeFunc = *mut u8;
pub type FT_Raster_RenderFunc = *mut u8;

#[repr(C)]
pub struct struct_FT_Raster_Funcs_ {
    pub glyph_format: FT_Glyph_Format,
    pub raster_new: FT_Raster_NewFunc,
    pub raster_reset: FT_Raster_ResetFunc,
    pub raster_set_mode: FT_Raster_SetModeFunc,
    pub raster_render: FT_Raster_RenderFunc,
    pub raster_done: FT_Raster_DoneFunc,
}

pub type FT_Raster_Funcs = struct_FT_Raster_Funcs_;
pub type FT_Bool = c_uchar;
pub type FT_FWord = c_short;
pub type FT_UFWord = c_ushort;
pub type FT_Char = c_char;
pub type FT_Byte = c_uchar;
pub type FT_Bytes = *mut FT_Byte;
pub type FT_Tag = FT_UInt32;
pub type FT_String = c_char;
pub type FT_Short = c_short;
pub type FT_UShort = c_ushort;
pub type FT_Int = c_int;
pub type FT_UInt = c_uint;
pub type FT_Long = c_long;
pub type FT_ULong = c_ulong;
pub type FT_F2Dot14 = c_short;
pub type FT_F26Dot6 = c_long;
pub type FT_Fixed = c_long;
pub type FT_Error = c_int;

pub trait FTErrorMethods {
    fn succeeded(&self) -> bool;
}

impl FTErrorMethods for FT_Error {
    fn succeeded(&self) -> bool { *self == 0 as FT_Error }
}

pub type FT_Pointer = *mut c_void;
pub type FT_Offset = size_t;
pub type FT_PtrDist = ptrdiff_t;

#[repr(C)]
pub struct struct_FT_UnitVector_ {
    pub x: FT_F2Dot14,
    pub y: FT_F2Dot14,
}

pub type FT_UnitVector = struct_FT_UnitVector_;

#[repr(C)]
pub struct struct_FT_Matrix_ {
    pub xx: FT_Fixed,
    pub xy: FT_Fixed,
    pub yx: FT_Fixed,
    pub yy: FT_Fixed,
}

pub type FT_Matrix = struct_FT_Matrix_;

#[repr(C)]
pub struct struct_FT_Data_ {
    pub pointer: *mut FT_Byte,
    pub length: FT_Int,
}

pub type FT_Data = struct_FT_Data_;
pub type FT_Generic_Finalizer = *mut u8;

#[repr(C)]
pub struct struct_FT_Generic_ {
    pub data: *mut c_void,
    pub finalizer: FT_Generic_Finalizer,
}

pub type FT_Generic = struct_FT_Generic_;
pub type FT_ListNode = *mut struct_FT_ListNodeRec_;
pub type FT_List = *mut struct_FT_ListRec_;

#[repr(C)]
pub struct struct_FT_ListNodeRec_ {
    pub prev: *mut c_void /* FT_ListNode */,
    pub next: *mut c_void /* FT_ListNode */,
    pub data: *mut c_void,
}

pub type FT_ListNodeRec = struct_FT_ListNodeRec_;

#[repr(C)]
pub struct struct_FT_ListRec_ {
    pub head: *mut c_void /* FT_ListNode */,
    pub tail: *mut c_void /* FT_ListNode */,
}

pub type FT_ListRec = struct_FT_ListRec_;

#[repr(C)]
pub struct struct_FT_Glyph_Metrics_ {
    pub width: FT_Pos,
    pub height: FT_Pos,
    pub horiBearingX: FT_Pos,
    pub horiBearingY: FT_Pos,
    pub horiAdvance: FT_Pos,
    pub vertBearingX: FT_Pos,
    pub vertBearingY: FT_Pos,
    pub vertAdvance: FT_Pos,
}

pub type FT_Glyph_Metrics = struct_FT_Glyph_Metrics_;

#[repr(C)]
pub struct struct_FT_Bitmap_Size_ {
    pub height: FT_Short,
    pub width: FT_Short,
    pub size: FT_Pos,
    pub x_ppem: FT_Pos,
    pub y_ppem: FT_Pos,
}

pub type FT_Bitmap_Size = struct_FT_Bitmap_Size_;

pub type struct_FT_LibraryRec_ = c_void;
pub type FT_Library = *mut struct_FT_LibraryRec_;

pub type struct_FT_ModuleRec_ = c_void;
pub type FT_Module = *mut struct_FT_ModuleRec_;

pub type struct_FT_DriverRec_ = c_void;
pub type FT_Driver = *mut struct_FT_DriverRec_;

pub type struct_FT_RendererRec_ = c_void;
pub type FT_Renderer = *mut struct_FT_RendererRec_;

pub type FT_Face = *mut struct_FT_FaceRec_;
pub type FT_Size = *mut struct_FT_SizeRec_;
pub type FT_GlyphSlot = *mut struct_FT_GlyphSlotRec_;
pub type FT_CharMap = *mut struct_FT_CharMapRec_;

pub type enum_FT_Encoding_ = c_uint;
pub const FT_ENCODING_NONE: u32 = 0_u32;
pub const FT_ENCODING_MS_SYMBOL: u32 = 1937337698_u32;
pub const FT_ENCODING_UNICODE: u32 = 1970170211_u32;
pub const FT_ENCODING_SJIS: u32 = 1936353651_u32;
pub const FT_ENCODING_GB2312: u32 = 1734484000_u32;
pub const FT_ENCODING_BIG5: u32 = 1651074869_u32;
pub const FT_ENCODING_WANSUNG: u32 = 2002873971_u32;
pub const FT_ENCODING_JOHAB: u32 = 1785686113_u32;
pub const FT_ENCODING_MS_SJIS: u32 = 1936353651_u32;
pub const FT_ENCODING_MS_GB2312: u32 = 1734484000_u32;
pub const FT_ENCODING_MS_BIG5: u32 = 1651074869_u32;
pub const FT_ENCODING_MS_WANSUNG: u32 = 2002873971_u32;
pub const FT_ENCODING_MS_JOHAB: u32 = 1785686113_u32;
pub const FT_ENCODING_ADOBE_STANDARD: u32 = 1094995778_u32;
pub const FT_ENCODING_ADOBE_EXPERT: u32 = 1094992453_u32;
pub const FT_ENCODING_ADOBE_CUSTOM: u32 = 1094992451_u32;
pub const FT_ENCODING_ADOBE_LATIN_1: u32 = 1818326065_u32;
pub const FT_ENCODING_OLD_LATIN_2: u32 = 1818326066_u32;
pub const FT_ENCODING_APPLE_ROMAN: u32 = 1634889070_u32;

pub const FT_LOAD_DEFAULT: i32 = 0x0;
pub const FT_LOAD_NO_SCALE: i32 = 0x1 << 0;
pub const FT_LOAD_NO_HINTING: i32 = 0x1 << 1;
pub const FT_LOAD_RENDER: i32 = 0x1 << 2;
pub const FT_LOAD_NO_BITMAP: i32 = 0x1 << 3;
pub const FT_LOAD_VERTICAL_LAYOUT: i32 = 0x1 << 4;
pub const FT_LOAD_FORCE_AUTOHINT: i32 = 0x1 << 5;
pub const FT_LOAD_CROP_BITMAP: i32 = 0x1 << 6;
pub const FT_LOAD_PENDANTIC: i32 = 0x1 << 7;
pub const FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH: i32 = 0x1 << 9;
pub const FT_LOAD_NO_RECURSE: i32 = 0x1 << 10;
pub const FT_LOAD_IGNORE_TRANSFORM: i32 = 0x1 << 11;
pub const FT_LOAD_MONOCHROME: i32 = 0x1 << 12;
pub const FT_LOAD_LINEAR_DESIGN: i32 = 0x1 << 13;
pub const FT_LOAD_NO_AUTOHINT: i32 = 0x1 << 15;

pub type FT_Encoding = enum_FT_Encoding_;

#[repr(C)]
pub struct struct_FT_CharMapRec_ {
    pub face: *mut c_void /* FT_Face */,
    pub encoding: FT_Encoding,
    pub platform_id: FT_UShort,
    pub encoding_id: FT_UShort,
}

pub type FT_CharMapRec = struct_FT_CharMapRec_;

pub type struct_FT_Face_InternalRec_ = c_void;
pub type FT_Face_Internal = *mut struct_FT_Face_InternalRec_;

pub const FT_STYLE_FLAG_ITALIC: FT_Long = (1 << 0);
pub const FT_STYLE_FLAG_BOLD: FT_Long = (1 << 1);

#[repr(C)]
pub struct struct_FT_FaceRec_ {
    pub num_faces: FT_Long,
    pub face_index: FT_Long,
    pub face_flags: FT_Long,
    pub style_flags: FT_Long,
    pub num_glyphs: FT_Long,
    pub family_name: *mut FT_String,
    pub style_name: *mut FT_String,
    pub num_fixed_sizes: FT_Int,
    pub available_sizes: *mut FT_Bitmap_Size,
    pub num_charmaps: FT_Int,
    pub charmaps: *mut *mut c_void /* FT_CharMap */,
    pub generic: FT_Generic,
    pub bbox: FT_BBox,
    pub units_per_EM: FT_UShort,
    pub ascender: FT_Short,
    pub descender: FT_Short,
    pub height: FT_Short,
    pub max_advance_width: FT_Short,
    pub max_advance_height: FT_Short,
    pub underline_position: FT_Short,
    pub underline_thickness: FT_Short,
    pub glyph: *mut c_void /* FT_GlyphSlot */,
    pub size: *mut c_void /* FT_Size */,
    pub charmap: *mut c_void /* FT_CharMap */,
    pub driver: *mut c_void /* FT_Driver */,
    pub memory: *mut c_void /* FT_Memory */,
    pub stream: *mut c_void /* FT_Stream */,
    pub sizes_list: FT_ListRec,
    pub autohint: FT_Generic,
    pub extensions: *mut c_void,
    pub internal: *mut c_void /* FT_Face_Internal */,
}

pub type FT_FaceRec = struct_FT_FaceRec_;

pub type struct_FT_Size_InternalRec_ = c_void;
pub type FT_Size_Internal = *mut struct_FT_Size_InternalRec_;

#[repr(C)]
pub struct struct_FT_Size_Metrics_ {
    pub x_ppem: FT_UShort,
    pub y_ppem: FT_UShort,
    pub x_scale: FT_Fixed,
    pub y_scale: FT_Fixed,
    pub ascender: FT_Pos,
    pub descender: FT_Pos,
    pub height: FT_Pos,
    pub max_advance: FT_Pos,
}

pub type FT_Size_Metrics = struct_FT_Size_Metrics_;

#[repr(C)]
pub struct struct_FT_SizeRec_ {
    pub face: *mut c_void /* FT_Face */,
    pub generic: FT_Generic,
    pub metrics: FT_Size_Metrics,
    pub internal: *mut c_void /* FT_Size_Internal */,
}

pub type FT_SizeRec = struct_FT_SizeRec_;

pub type struct_FT_SubGlyphRec_ = c_void;
pub type FT_SubGlyph = *mut struct_FT_SubGlyphRec_;

pub type struct_FT_Slot_InternalRec_ = c_void;
pub type FT_Slot_Internal = *mut struct_FT_Slot_InternalRec_;

#[repr(C)]
pub struct struct_FT_GlyphSlotRec_ {
    pub library: *mut c_void /* FT_Library */,
    pub face: *mut c_void /* FT_Face */,
    pub next: *mut c_void /* FT_GlyphSlot */,
    pub reserved: FT_UInt,
    pub generic: FT_Generic,
    pub metrics: FT_Glyph_Metrics,
    pub linearHoriAdvance: FT_Fixed,
    pub linearVertAdvance: FT_Fixed,
    pub advance: FT_Vector,
    pub format: FT_Glyph_Format,
    pub bitmap: FT_Bitmap,
    pub bitmap_left: FT_Int,
    pub bitmap_top: FT_Int,
    pub outline: FT_Outline,
    pub num_subglyphs: FT_UInt,
    pub subglyphs: *mut c_void /* FT_SubGlyph */,
    pub control_data: *mut c_void,
    pub control_len: c_long,
    pub lsb_delta: FT_Pos,
    pub rsb_delta: FT_Pos,
    pub other: *mut c_void,
    pub internal: *mut c_void /* FT_Slot_Internal */,
}

pub type FT_GlyphSlotRec = struct_FT_GlyphSlotRec_;

#[repr(C)]
pub struct struct_FT_Parameter_ {
    pub tag: FT_ULong,
    pub data: FT_Pointer,
}

pub type FT_Parameter = struct_FT_Parameter_;

#[repr(C)]
pub struct struct_FT_Open_Args_ {
    pub flags: FT_UInt,
    pub memory_base: *mut FT_Byte,
    pub memory_size: FT_Long,
    pub pathname: *mut FT_String,
    pub stream: *mut c_void /* FT_Stream */,
    pub driver: *mut c_void /* FT_Module */,
    pub num_params: FT_Int,
    pub params: *mut FT_Parameter,
}

pub type FT_Open_Args = struct_FT_Open_Args_;

pub type enum_FT_Size_Request_Type_ = c_uint;
pub const FT_SIZE_REQUEST_TYPE_NOMINAL: u32 = 0_u32;
pub const FT_SIZE_REQUEST_TYPE_REAL_DIM: u32 = 1_u32;
pub const FT_SIZE_REQUEST_TYPE_BBOX: u32 = 2_u32;
pub const FT_SIZE_REQUEST_TYPE_CELL: u32 = 3_u32;
pub const FT_SIZE_REQUEST_TYPE_SCALES: u32 = 4_u32;
pub const FT_SIZE_REQUEST_TYPE_MAX: u32 = 5_u32;

pub type FT_Size_Request_Type = enum_FT_Size_Request_Type_;

#[repr(C)]
pub struct struct_FT_Size_RequestRec_ {
    pub _type: FT_Size_Request_Type,
    pub width: FT_Long,
    pub height: FT_Long,
    pub horiResolution: FT_UInt,
    pub vertResolution: FT_UInt,
}

pub type FT_Size_RequestRec = struct_FT_Size_RequestRec_;
pub type FT_Size_Request = *mut struct_FT_Size_RequestRec_;

pub type enum_FT_Render_Mode_ = c_uint;
pub const FT_RENDER_MODE_NORMAL: u32 = 0_u32;
pub const FT_RENDER_MODE_LIGHT: u32 = 1_u32;
pub const FT_RENDER_MODE_MONO: u32 = 2_u32;
pub const FT_RENDER_MODE_LCD: u32 = 3_u32;
pub const FT_RENDER_MODE_LCD_V: u32 = 4_u32;
pub const FT_RENDER_MODE_MAX: u32 = 5_u32;

pub type FT_Render_Mode = enum_FT_Render_Mode_;

pub type enum_FT_Kerning_Mode_ = c_uint;
pub const FT_KERNING_DEFAULT: u32 = 0_u32;
pub const FT_KERNING_UNFITTED: u32 = 1_u32;
pub const FT_KERNING_UNSCALED: u32 = 2_u32;

pub type FT_Kerning_Mode = enum_FT_Kerning_Mode_;

pub type enum_unnamed1 = c_uint;
pub const FT_Mod_Err_Base: u32 = 0_u32;
pub const FT_Mod_Err_Autofit: u32 = 0_u32;
pub const FT_Mod_Err_BDF: u32 = 0_u32;
pub const FT_Mod_Err_Cache: u32 = 0_u32;
pub const FT_Mod_Err_CFF: u32 = 0_u32;
pub const FT_Mod_Err_CID: u32 = 0_u32;
pub const FT_Mod_Err_Gzip: u32 = 0_u32;
pub const FT_Mod_Err_LZW: u32 = 0_u32;
pub const FT_Mod_Err_OTvalid: u32 = 0_u32;
pub const FT_Mod_Err_PCF: u32 = 0_u32;
pub const FT_Mod_Err_PFR: u32 = 0_u32;
pub const FT_Mod_Err_PSaux: u32 = 0_u32;
pub const FT_Mod_Err_PShinter: u32 = 0_u32;
pub const FT_Mod_Err_PSnames: u32 = 0_u32;
pub const FT_Mod_Err_Raster: u32 = 0_u32;
pub const FT_Mod_Err_SFNT: u32 = 0_u32;
pub const FT_Mod_Err_Smooth: u32 = 0_u32;
pub const FT_Mod_Err_TrueType: u32 = 0_u32;
pub const FT_Mod_Err_Type1: u32 = 0_u32;
pub const FT_Mod_Err_Type42: u32 = 0_u32;
pub const FT_Mod_Err_Winfonts: u32 = 0_u32;
pub const FT_Mod_Err_Max: u32 = 1_u32;

pub type enum_unnamed2 = c_uint;
pub const FT_Err_Ok: u32 = 0_u32;
pub const FT_Err_Cannot_Open_Resource: u32 = 1_u32;
pub const FT_Err_Unknown_File_Format: u32 = 2_u32;
pub const FT_Err_Invalid_File_Format: u32 = 3_u32;
pub const FT_Err_Invalid_Version: u32 = 4_u32;
pub const FT_Err_Lower_Module_Version: u32 = 5_u32;
pub const FT_Err_Invalid_Argument: u32 = 6_u32;
pub const FT_Err_Unimplemented_Feature: u32 = 7_u32;
pub const FT_Err_Invalid_Table: u32 = 8_u32;
pub const FT_Err_Invalid_Offset: u32 = 9_u32;
pub const FT_Err_Array_Too_Large: u32 = 10_u32;
pub const FT_Err_Invalid_Glyph_Index: u32 = 16_u32;
pub const FT_Err_Invalid_Character_Code: u32 = 17_u32;
pub const FT_Err_Invalid_Glyph_Format: u32 = 18_u32;
pub const FT_Err_Cannot_Render_Glyph: u32 = 19_u32;
pub const FT_Err_Invalid_Outline: u32 = 20_u32;
pub const FT_Err_Invalid_Composite: u32 = 21_u32;
pub const FT_Err_Too_Many_Hints: u32 = 22_u32;
pub const FT_Err_Invalid_Pixel_Size: u32 = 23_u32;
pub const FT_Err_Invalid_Handle: u32 = 32_u32;
pub const FT_Err_Invalid_Library_Handle: u32 = 33_u32;
pub const FT_Err_Invalid_Driver_Handle: u32 = 34_u32;
pub const FT_Err_Invalid_Face_Handle: u32 = 35_u32;
pub const FT_Err_Invalid_Size_Handle: u32 = 36_u32;
pub const FT_Err_Invalid_Slot_Handle: u32 = 37_u32;
pub const FT_Err_Invalid_CharMap_Handle: u32 = 38_u32;
pub const FT_Err_Invalid_Cache_Handle: u32 = 39_u32;
pub const FT_Err_Invalid_Stream_Handle: u32 = 40_u32;
pub const FT_Err_Too_Many_Drivers: u32 = 48_u32;
pub const FT_Err_Too_Many_Extensions: u32 = 49_u32;
pub const FT_Err_Out_Of_Memory: u32 = 64_u32;
pub const FT_Err_Unlisted_Object: u32 = 65_u32;
pub const FT_Err_Cannot_Open_Stream: u32 = 81_u32;
pub const FT_Err_Invalid_Stream_Seek: u32 = 82_u32;
pub const FT_Err_Invalid_Stream_Skip: u32 = 83_u32;
pub const FT_Err_Invalid_Stream_Read: u32 = 84_u32;
pub const FT_Err_Invalid_Stream_Operation: u32 = 85_u32;
pub const FT_Err_Invalid_Frame_Operation: u32 = 86_u32;
pub const FT_Err_Nested_Frame_Access: u32 = 87_u32;
pub const FT_Err_Invalid_Frame_Read: u32 = 88_u32;
pub const FT_Err_Raster_Uninitialized: u32 = 96_u32;
pub const FT_Err_Raster_Corrupted: u32 = 97_u32;
pub const FT_Err_Raster_Overflow: u32 = 98_u32;
pub const FT_Err_Raster_Negative_Height: u32 = 99_u32;
pub const FT_Err_Too_Many_Caches: u32 = 112_u32;
pub const FT_Err_Invalid_Opcode: u32 = 128_u32;
pub const FT_Err_Too_Few_Arguments: u32 = 129_u32;
pub const FT_Err_Stack_Overflow: u32 = 130_u32;
pub const FT_Err_Code_Overflow: u32 = 131_u32;
pub const FT_Err_Bad_Argument: u32 = 132_u32;
pub const FT_Err_Divide_By_Zero: u32 = 133_u32;
pub const FT_Err_Invalid_Reference: u32 = 134_u32;
pub const FT_Err_Debug_OpCode: u32 = 135_u32;
pub const FT_Err_ENDF_In_Exec_Stream: u32 = 136_u32;
pub const FT_Err_Nested_DEFS: u32 = 137_u32;
pub const FT_Err_Invalid_CodeRange: u32 = 138_u32;
pub const FT_Err_Execution_Too_Long: u32 = 139_u32;
pub const FT_Err_Too_Many_Function_Defs: u32 = 140_u32;
pub const FT_Err_Too_Many_Instruction_Defs: u32 = 141_u32;
pub const FT_Err_Table_Missing: u32 = 142_u32;
pub const FT_Err_Horiz_Header_Missing: u32 = 143_u32;
pub const FT_Err_Locations_Missing: u32 = 144_u32;
pub const FT_Err_Name_Table_Missing: u32 = 145_u32;
pub const FT_Err_CMap_Table_Missing: u32 = 146_u32;
pub const FT_Err_Hmtx_Table_Missing: u32 = 147_u32;
pub const FT_Err_Post_Table_Missing: u32 = 148_u32;
pub const FT_Err_Invalid_Horiz_Metrics: u32 = 149_u32;
pub const FT_Err_Invalid_CharMap_Format: u32 = 150_u32;
pub const FT_Err_Invalid_PPem: u32 = 151_u32;
pub const FT_Err_Invalid_Vert_Metrics: u32 = 152_u32;
pub const FT_Err_Could_Not_Find_Context: u32 = 153_u32;
pub const FT_Err_Invalid_Post_Table_Format: u32 = 154_u32;
pub const FT_Err_Invalid_Post_Table: u32 = 155_u32;
pub const FT_Err_Syntax_Error: u32 = 160_u32;
pub const FT_Err_Stack_Underflow: u32 = 161_u32;
pub const FT_Err_Ignore: u32 = 162_u32;
pub const FT_Err_No_Unicode_Glyph_Name: u32 = 163_u32;
pub const FT_Err_Missing_Startfont_Field: u32 = 176_u32;
pub const FT_Err_Missing_Font_Field: u32 = 177_u32;
pub const FT_Err_Missing_Size_Field: u32 = 178_u32;
pub const FT_Err_Missing_Fontboundingbox_Field: u32 = 179_u32;
pub const FT_Err_Missing_Chars_Field: u32 = 180_u32;
pub const FT_Err_Missing_Startchar_Field: u32 = 181_u32;
pub const FT_Err_Missing_Encoding_Field: u32 = 182_u32;
pub const FT_Err_Missing_Bbx_Field: u32 = 183_u32;
pub const FT_Err_Bbx_Too_Big: u32 = 184_u32;
pub const FT_Err_Corrupted_Font_Header: u32 = 185_u32;
pub const FT_Err_Corrupted_Font_Glyphs: u32 = 186_u32;
pub const FT_Err_Max: u32 = 187_u32;

extern {

pub fn FT_Init_FreeType(alibrary: *mut FT_Library) -> FT_Error;

pub fn FT_New_Library(memory: FT_Memory, alibrary: *mut FT_Library) -> FT_Error;

pub fn FT_Done_Library(library: FT_Library) -> FT_Error;

pub fn FT_Add_Default_Modules(library: FT_Library);

pub fn FT_Done_FreeType(library: FT_Library) -> FT_Error;

pub fn FT_New_Face(library: FT_Library,
                   filepathname: *mut c_char,
                   face_index: FT_Long, aface: *mut FT_Face) -> FT_Error;

pub fn FT_New_Memory_Face(library: FT_Library,
                          file_base: *const FT_Byte,
                          file_size: FT_Long,
                          face_index: FT_Long,
                          aface: *mut FT_Face) -> FT_Error;

pub fn FT_Open_Face(library: FT_Library,
                    args: *mut FT_Open_Args,
                    face_index: FT_Long,
                    aface: *mut FT_Face) -> FT_Error;

pub fn FT_Library_SetLcdFilter(library: FT_Library, filter: FT_LcdFilter) -> FT_Error;

pub fn FT_Attach_File(face: FT_Face, filepathname: *mut c_char) -> FT_Error;

pub fn FT_Attach_Stream(face: FT_Face, parameters: *mut FT_Open_Args) -> FT_Error;

pub fn FT_Reference_Face(face: FT_Face) -> FT_Error;

pub fn FT_Done_Face(face: FT_Face) -> FT_Error;

pub fn FT_Select_Size(face: FT_Face, strike_index: FT_Int) -> FT_Error;

pub fn FT_Request_Size(face: FT_Face, req: FT_Size_Request) -> FT_Error;

pub fn FT_Set_Char_Size(face: FT_Face,
                        char_width: FT_F26Dot6,
                        char_height: FT_F26Dot6,
                        horz_resolution: FT_UInt,
                        vert_resolution: FT_UInt) -> FT_Error;

pub fn FT_Set_Pixel_Sizes(face: FT_Face, pixel_width: FT_UInt, pixel_height: FT_UInt) -> FT_Error;

pub fn FT_Load_Glyph(face: FT_Face, glyph_index: FT_UInt, load_flags: FT_Int32) -> FT_Error;

pub fn FT_Load_Char(face: FT_Face, char_code: FT_ULong, load_flags: FT_Int32) -> FT_Error;

pub fn FT_Set_Transform(face: FT_Face, matrix: *mut FT_Matrix, delta: *mut FT_Vector);

pub fn FT_Render_Glyph(slot: FT_GlyphSlot, render_mode: FT_Render_Mode) -> FT_Error;

pub fn FT_Get_Kerning(face: FT_Face,
                      left_glyph: FT_UInt,
                      right_glyph: FT_UInt,
                      kern_mode: FT_UInt,
                      akerning: *mut FT_Vector) -> FT_Error;

pub fn FT_Get_Track_Kerning(face: FT_Face, point_size: FT_Fixed, degree: FT_Int, akerning: *mut FT_Fixed) -> FT_Error;

pub fn FT_Get_Glyph_Name(face: FT_Face, glyph_index: FT_UInt, buffer: FT_Pointer, buffer_max: FT_UInt) -> FT_Error;

pub fn FT_Get_Postscript_Name(face: FT_Face) -> *mut c_char;

pub fn FT_Select_Charmap(face: FT_Face, encoding: FT_Encoding) -> FT_Error;

pub fn FT_Set_Charmap(face: FT_Face, charmap: FT_CharMap) -> FT_Error;

pub fn FT_Get_Charmap_Index(charmap: FT_CharMap) -> FT_Int;

pub fn FT_Get_Char_Index(face: FT_Face, charcode: FT_ULong) -> FT_UInt;

pub fn FT_Get_First_Char(face: FT_Face, agindex: *mut FT_UInt) -> FT_ULong;

pub fn FT_Get_Next_Char(face: FT_Face, char_code: FT_ULong, agindex: *mut FT_UInt) -> FT_ULong;

pub fn FT_Get_Name_Index(face: FT_Face, glyph_name: *mut FT_String) -> FT_UInt;

pub fn FT_Get_SubGlyph_Info(glyph: FT_GlyphSlot,
                            sub_index: FT_UInt,
                            p_index: *mut FT_Int,
                            p_flags: *mut FT_UInt,
                            p_arg1: *mut FT_Int,
                            p_arg2: *mut FT_Int,
                            p_transform: *mut FT_Matrix) -> FT_Error;

pub fn FT_Get_FSType_Flags(face: FT_Face) -> FT_UShort;

pub fn FT_Face_GetCharVariantIndex(face: FT_Face, charcode: FT_ULong, variantSelector: FT_ULong) -> FT_UInt;

pub fn FT_Face_GetCharVariantIsDefault(face: FT_Face, charcode: FT_ULong, variantSelector: FT_ULong) -> FT_Int;

pub fn FT_Face_GetVariantSelectors(face: FT_Face) -> *mut FT_UInt32;

pub fn FT_Face_GetVariantsOfChar(face: FT_Face, charcode: FT_ULong) -> *mut FT_UInt32;

pub fn FT_Face_GetCharsOfVariant(face: FT_Face, variantSelector: FT_ULong) -> *mut FT_UInt32;

pub fn FT_MulDiv(a: FT_Long, b: FT_Long, c: FT_Long) -> FT_Long;

pub fn FT_MulFix(a: FT_Long, b: FT_Long) -> FT_Long;

pub fn FT_DivFix(a: FT_Long, b: FT_Long) -> FT_Long;

pub fn FT_RoundFix(a: FT_Fixed) -> FT_Fixed;

pub fn FT_CeilFix(a: FT_Fixed) -> FT_Fixed;

pub fn FT_FloorFix(a: FT_Fixed) -> FT_Fixed;

pub fn FT_Vector_Transform(vec: *mut FT_Vector, matrix: *mut FT_Matrix);

pub fn FT_Library_Version(library: FT_Library, amajor: *mut FT_Int, aminor: *mut FT_Int, apatch: *mut FT_Int);

pub fn FT_Face_CheckTrueTypePatents(face: FT_Face) -> FT_Bool;

pub fn FT_Face_SetUnpatentedHinting(face: FT_Face, value: FT_Bool) -> FT_Bool;

pub fn FT_Get_Sfnt_Table(face: FT_Face, tag: FT_Sfnt_Tag) -> *mut c_void;

pub fn FT_Load_Sfnt_Table(face: FT_Face,
                          tag: FT_ULong,
                          offset: FT_Long,
                          buffer: *mut FT_Byte,
                          length: *mut FT_ULong) -> FT_Error;
}
