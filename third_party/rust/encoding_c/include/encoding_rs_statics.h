// Copyright 2016 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// THIS IS A GENERATED FILE. PLEASE DO NOT EDIT.
// Instead, please regenerate using generate-encoding-data.py

// This file is not meant to be included directly. Instead, encoding_rs.h
// includes this file.

#ifndef encoding_rs_statics_h_
#define encoding_rs_statics_h_

#ifndef ENCODING_RS_ENCODING
#define ENCODING_RS_ENCODING Encoding
#ifndef __cplusplus
typedef struct Encoding_ Encoding;
#endif
#endif

#ifndef ENCODING_RS_ENCODER
#define ENCODING_RS_ENCODER Encoder
#ifndef __cplusplus
typedef struct Encoder_ Encoder;
#endif
#endif

#ifndef ENCODING_RS_DECODER
#define ENCODING_RS_DECODER Decoder
#ifndef __cplusplus
typedef struct Decoder_ Decoder;
#endif
#endif

#define INPUT_EMPTY 0

#define OUTPUT_FULL 0xFFFFFFFF

// x-mac-cyrillic
#define ENCODING_NAME_MAX_LENGTH 14

/// The Big5 encoding.
extern const ENCODING_RS_ENCODING* const BIG5_ENCODING;

/// The EUC-JP encoding.
extern const ENCODING_RS_ENCODING* const EUC_JP_ENCODING;

/// The EUC-KR encoding.
extern const ENCODING_RS_ENCODING* const EUC_KR_ENCODING;

/// The GBK encoding.
extern const ENCODING_RS_ENCODING* const GBK_ENCODING;

/// The IBM866 encoding.
extern const ENCODING_RS_ENCODING* const IBM866_ENCODING;

/// The ISO-2022-JP encoding.
extern const ENCODING_RS_ENCODING* const ISO_2022_JP_ENCODING;

/// The ISO-8859-10 encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_10_ENCODING;

/// The ISO-8859-13 encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_13_ENCODING;

/// The ISO-8859-14 encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_14_ENCODING;

/// The ISO-8859-15 encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_15_ENCODING;

/// The ISO-8859-16 encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_16_ENCODING;

/// The ISO-8859-2 encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_2_ENCODING;

/// The ISO-8859-3 encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_3_ENCODING;

/// The ISO-8859-4 encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_4_ENCODING;

/// The ISO-8859-5 encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_5_ENCODING;

/// The ISO-8859-6 encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_6_ENCODING;

/// The ISO-8859-7 encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_7_ENCODING;

/// The ISO-8859-8 encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_8_ENCODING;

/// The ISO-8859-8-I encoding.
extern const ENCODING_RS_ENCODING* const ISO_8859_8_I_ENCODING;

/// The KOI8-R encoding.
extern const ENCODING_RS_ENCODING* const KOI8_R_ENCODING;

/// The KOI8-U encoding.
extern const ENCODING_RS_ENCODING* const KOI8_U_ENCODING;

/// The Shift_JIS encoding.
extern const ENCODING_RS_ENCODING* const SHIFT_JIS_ENCODING;

/// The UTF-16BE encoding.
extern const ENCODING_RS_ENCODING* const UTF_16BE_ENCODING;

/// The UTF-16LE encoding.
extern const ENCODING_RS_ENCODING* const UTF_16LE_ENCODING;

/// The UTF-8 encoding.
extern const ENCODING_RS_ENCODING* const UTF_8_ENCODING;

/// The gb18030 encoding.
extern const ENCODING_RS_ENCODING* const GB18030_ENCODING;

/// The macintosh encoding.
extern const ENCODING_RS_ENCODING* const MACINTOSH_ENCODING;

/// The replacement encoding.
extern const ENCODING_RS_ENCODING* const REPLACEMENT_ENCODING;

/// The windows-1250 encoding.
extern const ENCODING_RS_ENCODING* const WINDOWS_1250_ENCODING;

/// The windows-1251 encoding.
extern const ENCODING_RS_ENCODING* const WINDOWS_1251_ENCODING;

/// The windows-1252 encoding.
extern const ENCODING_RS_ENCODING* const WINDOWS_1252_ENCODING;

/// The windows-1253 encoding.
extern const ENCODING_RS_ENCODING* const WINDOWS_1253_ENCODING;

/// The windows-1254 encoding.
extern const ENCODING_RS_ENCODING* const WINDOWS_1254_ENCODING;

/// The windows-1255 encoding.
extern const ENCODING_RS_ENCODING* const WINDOWS_1255_ENCODING;

/// The windows-1256 encoding.
extern const ENCODING_RS_ENCODING* const WINDOWS_1256_ENCODING;

/// The windows-1257 encoding.
extern const ENCODING_RS_ENCODING* const WINDOWS_1257_ENCODING;

/// The windows-1258 encoding.
extern const ENCODING_RS_ENCODING* const WINDOWS_1258_ENCODING;

/// The windows-874 encoding.
extern const ENCODING_RS_ENCODING* const WINDOWS_874_ENCODING;

/// The x-mac-cyrillic encoding.
extern const ENCODING_RS_ENCODING* const X_MAC_CYRILLIC_ENCODING;

/// The x-user-defined encoding.
extern const ENCODING_RS_ENCODING* const X_USER_DEFINED_ENCODING;

#endif // encoding_rs_statics_h_
