// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef PUBLIC_FPDF_SYSFONTINFO_H_
#define PUBLIC_FPDF_SYSFONTINFO_H_

// NOLINTNEXTLINE(build/include)
#include "fpdfview.h"

/* Character sets for the font */
#define FXFONT_ANSI_CHARSET 0
#define FXFONT_DEFAULT_CHARSET 1
#define FXFONT_SYMBOL_CHARSET 2
#define FXFONT_SHIFTJIS_CHARSET 128
#define FXFONT_HANGEUL_CHARSET 129
#define FXFONT_GB2312_CHARSET 134
#define FXFONT_CHINESEBIG5_CHARSET 136

/* Font pitch and family flags */
#define FXFONT_FF_FIXEDPITCH 1
#define FXFONT_FF_ROMAN (1 << 4)
#define FXFONT_FF_SCRIPT (4 << 4)

/* Typical weight values */
#define FXFONT_FW_NORMAL 400
#define FXFONT_FW_BOLD 700

// Exported Functions
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Interface: FPDF_SYSFONTINFO
 *          Interface for getting system font information and font mapping
 */
typedef struct _FPDF_SYSFONTINFO {
  /**
   * Version number of the interface. Currently must be 1.
   **/
  int version;

  /**
   * Method: Release
   *          Give implementation a chance to release any data after the
   * interface is no longer used
   * Interface Version:
   *          1
   * Implementation Required:
   *          No
   * Comments:
   *          Called by Foxit SDK during the final cleanup process.
   * Parameters:
   *          pThis       -   Pointer to the interface structure itself
   * Return Value:
   *          None
   */
  void (*Release)(struct _FPDF_SYSFONTINFO* pThis);

  /**
   * Method: EnumFonts
   *          Enumerate all fonts installed on the system
   * Interface Version:
   *          1
   * Implementation Required:
   *          No
   * Comments:
   *          Implementation should call FPDF_AddIntalledFont() function for
   * each font found.
   *          Only TrueType/OpenType and Type1 fonts are accepted by Foxit SDK.
   * Parameters:
   *          pThis       -   Pointer to the interface structure itself
   *          pMapper     -   An opaque pointer to internal font mapper, used
   * when calling FPDF_AddInstalledFont
   * Return Value:
   *          None
   */
  void (*EnumFonts)(struct _FPDF_SYSFONTINFO* pThis, void* pMapper);

  /**
   * Method: MapFont
   *          Use the system font mapper to get a font handle from requested
   *parameters
   * Interface Version:
   *          1
   * Implementation Required:
   *          Yes only if GetFont method is not implemented.
   * Comments:
   *          If the system supports native font mapper (like Windows),
   *implementation can implement this method to get a font handle.
   *          Otherwise, Foxit SDK will do the mapping and then call GetFont
   *method.
   *          Only TrueType/OpenType and Type1 fonts are accepted by Foxit SDK.
   * Parameters:
   *          pThis       -   Pointer to the interface structure itself
   *          weight      -   Weight of the requested font. 400 is normal and
   *700 is bold.
   *          bItalic     -   Italic option of the requested font, TRUE or
   *FALSE.
   *          charset     -   Character set identifier for the requested font.
   *See above defined constants.
   *          pitch_family -  A combination of flags. See above defined
   *constants.
   *          face        -   Typeface name. Currently use system local encoding
   *only.
   *          bExact      -   Pointer to a boolean value receiving the indicator
   *whether mapper found the exact match.
   *                          If mapper is not sure whether it's exact match,
   *ignore this paramter.
   * Return Value:
   *          An opaque pointer for font handle, or NULL if system mapping is
   *not supported.
   **/
  void* (*MapFont)(struct _FPDF_SYSFONTINFO* pThis,
                   int weight,
                   FPDF_BOOL bItalic,
                   int charset,
                   int pitch_family,
                   const char* face,
                   FPDF_BOOL* bExact);

  /**
   * Method: GetFont
   *          Get a handle to a particular font by its internal ID
   * Interface Version:
   *          1
   * Implementation Required:
   *          Yes only if MapFont method is not implemented.
   * Comments:
   *          If the system mapping not supported, Foxit SDK will do the font
   *mapping and use this method to get a font handle.
   * Parameters:
   *          pThis       -   Pointer to the interface structure itself
   *          face        -   Typeface name. Currently use system local encoding
   *only.
   * Return Value:
   *          An opaque pointer for font handle.
   **/
  void* (*GetFont)(struct _FPDF_SYSFONTINFO* pThis, const char* face);

  /**
   * Method: GetFontData
   *          Get font data from a font
   * Interface Version:
   *          1
   * Implementation Required:
   *          Yes
   * Comments:
   *          Can read either full font file, or a particular TrueType/OpenType
   *table
   * Parameters:
   *          pThis       -   Pointer to the interface structure itself
   *          hFont       -   Font handle returned by MapFont or GetFont method
   *          table       -   TrueType/OpenType table identifier (refer to
   *TrueType specification).
   *                          0 for the whole font file.
   *          buffer      -   The buffer receiving the font data. Can be NULL if
   *not provided
   *          buf_size    -   Buffer size, can be zero if not provided
   * Return Value:
   *          Number of bytes needed, if buffer not provided or not large
   *enough,
   *          or number of bytes written into buffer otherwise.
   **/
  unsigned long (*GetFontData)(struct _FPDF_SYSFONTINFO* pThis,
                               void* hFont,
                               unsigned int table,
                               unsigned char* buffer,
                               unsigned long buf_size);

  /**
   * Method: GetFaceName
   *          Get face name from a font handle
   * Interface Version:
   *          1
   * Implementation Required:
   *          No
   * Parameters:
   *          pThis       -   Pointer to the interface structure itself
   *          hFont       -   Font handle returned by MapFont or GetFont method
   *          buffer      -   The buffer receiving the face name. Can be NULL if
   *not provided
   *          buf_size    -   Buffer size, can be zero if not provided
   * Return Value:
   *          Number of bytes needed, if buffer not provided or not large
   *enough,
   *          or number of bytes written into buffer otherwise.
   **/
  unsigned long (*GetFaceName)(struct _FPDF_SYSFONTINFO* pThis,
                               void* hFont,
                               char* buffer,
                               unsigned long buf_size);

  /**
   * Method: GetFontCharset
   *          Get character set information for a font handle
   * Interface Version:
   *          1
   * Implementation Required:
   *          No
   * Parameters:
   *          pThis       -   Pointer to the interface structure itself
   *          hFont       -   Font handle returned by MapFont or GetFont method
   * Return Value:
   *          Character set identifier. See defined constants above.
   **/
  int (*GetFontCharset)(struct _FPDF_SYSFONTINFO* pThis, void* hFont);

  /**
   * Method: DeleteFont
   *          Delete a font handle
   * Interface Version:
   *          1
   * Implementation Required:
   *          Yes
   * Parameters:
   *          pThis       -   Pointer to the interface structure itself
   *          hFont       -   Font handle returned by MapFont or GetFont method
   * Return Value:
   *          None
   **/
  void (*DeleteFont)(struct _FPDF_SYSFONTINFO* pThis, void* hFont);
} FPDF_SYSFONTINFO;

/**
 * Struct: FPDF_CharsetFontMap
 *    Provides the name of a font to use for a given charset value.
 **/
typedef struct FPDF_CharsetFontMap_ {
  int charset;  // Character Set Enum value, see FXFONT_*_CHARSET above.
  const char* fontname;  // Name of default font to use with that charset.
} FPDF_CharsetFontMap;

/**
 * Function: FPDF_GetDefaultTTFMap
 *    Returns a pointer to the default character set to TT Font name map. The
 *    map is an array of FPDF_CharsetFontMap structs, with its end indicated
 *    by a { -1, NULL } entry.
 * Parameters:
 *     None.
 * Return Value:
 *     Pointer to the Charset Font Map.
 **/
DLLEXPORT const FPDF_CharsetFontMap* STDCALL FPDF_GetDefaultTTFMap();

/**
 * Function: FPDF_AddInstalledFont
 *          Add a system font to the list in Foxit SDK.
 * Comments:
 *          This function is only called during the system font list building
 *process.
 * Parameters:
 *          mapper          -   Opaque pointer to Foxit font mapper
 *          face            -   The font face name
 *          charset         -   Font character set. See above defined constants.
 * Return Value:
 *          None.
 **/
DLLEXPORT void STDCALL FPDF_AddInstalledFont(void* mapper,
                                             const char* face,
                                             int charset);

/**
 * Function: FPDF_SetSystemFontInfo
 *          Set the system font info interface into Foxit SDK
 * Comments:
 *          Platform support implementation should implement required methods of
 *FFDF_SYSFONTINFO interface,
 *          then call this function during SDK initialization process.
 * Parameters:
 *          pFontInfo       -   Pointer to a FPDF_SYSFONTINFO structure
 * Return Value:
 *          None
 **/
DLLEXPORT void STDCALL FPDF_SetSystemFontInfo(FPDF_SYSFONTINFO* pFontInfo);

/**
 * Function: FPDF_GetDefaultSystemFontInfo
 *          Get default system font info interface for current platform
 * Comments:
 *          For some platforms Foxit SDK implement a default version of system
 *font info interface.
 *          The default implementation can be used in FPDF_SetSystemFontInfo
 *function.
 * Parameters:
 *          None
 * Return Value:
 *          Pointer to a FPDF_SYSFONTINFO structure describing the default
 *interface.
 *          Or NULL if the platform doesn't have a default interface.
 *          Application should call FPDF_FreeDefaultSystemFontInfo to free the
 *returned pointer.
 **/
DLLEXPORT FPDF_SYSFONTINFO* STDCALL FPDF_GetDefaultSystemFontInfo();

/**
 * Function: FPDF_FreeDefaultSystemFontInfo
 *           Free a default system font info interface
 * Comments:
 *           This function should be called on the output from
 *FPDF_SetSystemFontInfo once it is no longer needed by the client.
 * Parameters:
 *           pFontInfo       -   Pointer to a FPDF_SYSFONTINFO structure
 * Return Value:
 *          None
 **/
DLLEXPORT void FPDF_FreeDefaultSystemFontInfo(FPDF_SYSFONTINFO* pFontInfo);

#ifdef __cplusplus
}
#endif

#endif  // PUBLIC_FPDF_SYSFONTINFO_H_
