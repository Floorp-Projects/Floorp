// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

// NOTE: External docs refer to this file as "fpdfview.h", so do not rename
// despite lack of consistency with other public files.

#ifndef PUBLIC_FPDFVIEW_H_
#define PUBLIC_FPDFVIEW_H_

#if defined(_WIN32) && !defined(__WINDOWS__)
#include <windows.h>
#endif

#ifdef PDF_ENABLE_XFA
// PDF_USE_XFA is set in confirmation that this version of PDFium can support
// XFA forms as requested by the PDF_ENABLE_XFA setting.
#define PDF_USE_XFA
#endif  // PDF_ENABLE_XFA

// PDF types
typedef void* FPDF_ACTION;
typedef void* FPDF_BITMAP;
typedef void* FPDF_BOOKMARK;
typedef void* FPDF_CLIPPATH;
typedef void* FPDF_DEST;
typedef void* FPDF_DOCSCHHANDLE;
typedef void* FPDF_DOCUMENT;
typedef void* FPDF_FONT;
typedef void* FPDF_HMODULE;
typedef void* FPDF_LINK;
typedef void* FPDF_MODULEMGR;
typedef void* FPDF_PAGE;
typedef void* FPDF_PAGELINK;
typedef void* FPDF_PAGEOBJECT;  // Page object(text, path, etc)
typedef void* FPDF_PAGERANGE;
typedef void* FPDF_PATH;
typedef void* FPDF_RECORDER;
typedef void* FPDF_SCHHANDLE;
typedef void* FPDF_STRUCTELEMENT;
typedef void* FPDF_STRUCTTREE;
typedef void* FPDF_TEXTPAGE;

#ifdef PDF_ENABLE_XFA
typedef void* FPDF_STRINGHANDLE;
typedef void* FPDF_WIDGET;
#endif  // PDF_ENABLE_XFA

// Basic data types
typedef int FPDF_BOOL;
typedef int FPDF_ERROR;
typedef unsigned long FPDF_DWORD;
typedef float FS_FLOAT;

#ifdef PDF_ENABLE_XFA
typedef void* FPDF_LPVOID;
typedef void const* FPDF_LPCVOID;
typedef char const* FPDF_LPCSTR;
typedef int FPDF_RESULT;
#endif

// Duplex types
typedef enum _FPDF_DUPLEXTYPE_ {
  DuplexUndefined = 0,
  Simplex,
  DuplexFlipShortEdge,
  DuplexFlipLongEdge
} FPDF_DUPLEXTYPE;

// String types
typedef unsigned short FPDF_WCHAR;
typedef unsigned char const* FPDF_LPCBYTE;

// FPDFSDK may use three types of strings: byte string, wide string (UTF-16LE
// encoded), and platform dependent string
typedef const char* FPDF_BYTESTRING;

// FPDFSDK always uses UTF-16LE encoded wide strings, each character uses 2
// bytes (except surrogation), with the low byte first.
typedef const unsigned short* FPDF_WIDESTRING;

#ifdef PDF_ENABLE_XFA
// Structure for a byte string.
// Note, a byte string commonly means a UTF-16LE formated string.
typedef struct _FPDF_BSTR {
  // String buffer.
  char* str;
  // Length of the string, in bytes.
  int len;
} FPDF_BSTR;
#endif  // PDF_ENABLE_XFA

// For Windows programmers: In most cases it's OK to treat FPDF_WIDESTRING as a
// Windows unicode string, however, special care needs to be taken if you
// expect to process Unicode larger than 0xffff.
//
// For Linux/Unix programmers: most compiler/library environments use 4 bytes
// for a Unicode character, and you have to convert between FPDF_WIDESTRING and
// system wide string by yourself.
typedef const char* FPDF_STRING;

// Matrix for transformation.
typedef struct _FS_MATRIX_ {
  float a;
  float b;
  float c;
  float d;
  float e;
  float f;
} FS_MATRIX;

// Rectangle area(float) in device or page coordinate system.
typedef struct _FS_RECTF_ {
  // The x-coordinate of the left-top corner.
  float left;
  // The y-coordinate of the left-top corner.
  float top;
  // The x-coordinate of the right-bottom corner.
  float right;
  // The y-coordinate of the right-bottom corner.
  float bottom;
} * FS_LPRECTF, FS_RECTF;

// Const Pointer to FS_RECTF structure.
typedef const FS_RECTF* FS_LPCRECTF;

#if defined(_WIN32) && defined(FPDFSDK_EXPORTS)
// On Windows system, functions are exported in a DLL
#define DLLEXPORT __declspec(dllexport)
#define STDCALL __stdcall
#else
#define DLLEXPORT
#define STDCALL
#endif

// Exported Functions
#ifdef __cplusplus
extern "C" {
#endif

// Function: FPDF_InitLibrary
//          Initialize the FPDFSDK library
// Parameters:
//          None
// Return value:
//          None.
// Comments:
//          Convenience function to call FPDF_InitLibraryWithConfig() for
//          backwards comatibility purposes.
DLLEXPORT void STDCALL FPDF_InitLibrary();

// Process-wide options for initializing the library.
typedef struct FPDF_LIBRARY_CONFIG_ {
  // Version number of the interface. Currently must be 2.
  int version;

  // Array of paths to scan in place of the defaults when using built-in
  // FXGE font loading code. The array is terminated by a NULL pointer.
  // The Array may be NULL itself to use the default paths. May be ignored
  // entirely depending upon the platform.
  const char** m_pUserFontPaths;

  // Version 2.

  // pointer to the v8::Isolate to use, or NULL to force PDFium to create one.
  void* m_pIsolate;

  // The embedder data slot to use in the v8::Isolate to store PDFium's
  // per-isolate data. The value needs to be between 0 and
  // v8::Internals::kNumIsolateDataLots (exclusive). Note that 0 is fine
  // for most embedders.
  unsigned int m_v8EmbedderSlot;
} FPDF_LIBRARY_CONFIG;

// Function: FPDF_InitLibraryWithConfig
//          Initialize the FPDFSDK library
// Parameters:
//          config - configuration information as above.
// Return value:
//          None.
// Comments:
//          You have to call this function before you can call any PDF
//          processing functions.
DLLEXPORT void STDCALL FPDF_InitLibraryWithConfig(
    const FPDF_LIBRARY_CONFIG* config);

// Function: FPDF_DestroyLibary
//          Release all resources allocated by the FPDFSDK library.
// Parameters:
//          None.
// Return value:
//          None.
// Comments:
//          You can call this function to release all memory blocks allocated by
//          the library.
//          After this function is called, you should not call any PDF
//          processing functions.
DLLEXPORT void STDCALL FPDF_DestroyLibrary();

// Policy for accessing the local machine time.
#define FPDF_POLICY_MACHINETIME_ACCESS 0

// Function: FPDF_SetSandBoxPolicy
//          Set the policy for the sandbox environment.
// Parameters:
//          policy -   The specified policy for setting, for example:
//                     FPDF_POLICY_MACHINETIME_ACCESS.
//          enable -   True to enable, false to disable the policy.
// Return value:
//          None.
DLLEXPORT void STDCALL FPDF_SetSandBoxPolicy(FPDF_DWORD policy,
                                             FPDF_BOOL enable);

#if defined(_WIN32)
#if defined(PDFIUM_PRINT_TEXT_WITH_GDI)
// Pointer to a helper function to make |font| with |text| of |text_length|
// accessible when printing text with GDI. This is useful in sandboxed
// environments where PDFium's access to GDI may be restricted.
typedef void (*PDFiumEnsureTypefaceCharactersAccessible)(const LOGFONT* font,
                                                         const wchar_t* text,
                                                         size_t text_length);

// Function: FPDF_SetTypefaceAccessibleFunc
//          Set the function pointer that makes GDI fonts available in sandboxed
//          environments. Experimental API.
// Parameters:
//          func -   A function pointer. See description above.
// Return value:
//          None.
DLLEXPORT void STDCALL
FPDF_SetTypefaceAccessibleFunc(PDFiumEnsureTypefaceCharactersAccessible func);

// Function: FPDF_SetPrintTextWithGDI
//          Set whether to use GDI to draw fonts when printing on Windows.
//          Experimental API.
// Parameters:
//          use_gdi -   Set to true to enable printing text with GDI.
// Return value:
//          None.
DLLEXPORT void STDCALL FPDF_SetPrintTextWithGDI(FPDF_BOOL use_gdi);
#endif  // PDFIUM_PRINT_TEXT_WITH_GDI

// Function: FPDF_SetPrintPostscriptLevel
//          Set postscript printing level when printing on Windows.
//          Experimental API.
// Parameters:
//          postscript_level -  0 to disable postscript printing,
//                              2 to print with postscript level 2,
//                              3 to print with postscript level 3.
//                              All other values are invalid.
// Return value:
//          True if successful, false if unsucessful (typically invalid input).
DLLEXPORT FPDF_BOOL STDCALL
FPDF_SetPrintPostscriptLevel(FPDF_BOOL postscript_level);
#endif  // defined(_WIN32)

// Function: FPDF_LoadDocument
//          Open and load a PDF document.
// Parameters:
//          file_path -  Path to the PDF file (including extension).
//          password  -  A string used as the password for the PDF file.
//                       If no password is needed, empty or NULL can be used.
// Return value:
//          A handle to the loaded document, or NULL on failure.
// Comments:
//          Loaded document can be closed by FPDF_CloseDocument().
//          If this function fails, you can use FPDF_GetLastError() to retrieve
//          the reason why it failed.
DLLEXPORT FPDF_DOCUMENT STDCALL FPDF_LoadDocument(FPDF_STRING file_path,
                                                  FPDF_BYTESTRING password);

// Function: FPDF_LoadMemDocument
//          Open and load a PDF document from memory.
// Parameters:
//          data_buf    -   Pointer to a buffer containing the PDF document.
//          size        -   Number of bytes in the PDF document.
//          password    -   A string used as the password for the PDF file.
//                          If no password is needed, empty or NULL can be used.
// Return value:
//          A handle to the loaded document, or NULL on failure.
// Comments:
//          The memory buffer must remain valid when the document is open.
//          The loaded document can be closed by FPDF_CloseDocument.
//          If this function fails, you can use FPDF_GetLastError() to retrieve
//          the reason why it failed.
// Notes:
//          If PDFium is built with the XFA module, the application should call
//          FPDF_LoadXFA() function after the PDF document loaded to support XFA
//          fields defined in the fpdfformfill.h file.
DLLEXPORT FPDF_DOCUMENT STDCALL FPDF_LoadMemDocument(const void* data_buf,
                                                     int size,
                                                     FPDF_BYTESTRING password);

// Structure for custom file access.
typedef struct {
  // File length, in bytes.
  unsigned long m_FileLen;

  // A function pointer for getting a block of data from a specific position.
  // Position is specified by byte offset from the beginning of the file.
  // The position and size will never go out of range of the file length.
  // It may be possible for FPDFSDK to call this function multiple times for
  // the same position.
  // Return value: should be non-zero if successful, zero for error.
  int (*m_GetBlock)(void* param,
                    unsigned long position,
                    unsigned char* pBuf,
                    unsigned long size);

  // A custom pointer for all implementation specific data.  This pointer will
  // be used as the first parameter to the m_GetBlock callback.
  void* m_Param;
} FPDF_FILEACCESS;

#ifdef PDF_ENABLE_XFA
/**
 * @brief Structure for file reading or writing (I/O).
 *
 * @note This is a handler and should be implemented by callers.
 */
typedef struct _FPDF_FILEHANDLER {
  /**
   * @brief User-defined data.
   * @note Callers can use this field to track controls.
   */
  FPDF_LPVOID clientData;
  /**
   * @brief Callback function to release the current file stream object.
   *
   * @param[in] clientData    Pointer to user-defined data.
   *
   * @return None.
   */
  void (*Release)(FPDF_LPVOID clientData);
  /**
   * @brief Callback function to retrieve the current file stream size.
   *
   * @param[in] clientData    Pointer to user-defined data.
   *
   * @return Size of file stream.
   */
  FPDF_DWORD (*GetSize)(FPDF_LPVOID clientData);
  /**
   * @brief Callback function to read data from the current file stream.
   *
   * @param[in]   clientData  Pointer to user-defined data.
   * @param[in]   offset      Offset position starts from the beginning of file
   * stream. This parameter indicates reading position.
   * @param[in]   buffer      Memory buffer to store data which are read from
   * file stream. This parameter should not be <b>NULL</b>.
   * @param[in]   size        Size of data which should be read from file
   * stream, in bytes. The buffer indicated by the parameter <i>buffer</i>
   * should be enough to store specified data.
   *
   * @return 0 for success, other value for failure.
   */
  FPDF_RESULT (*ReadBlock)(FPDF_LPVOID clientData,
                           FPDF_DWORD offset,
                           FPDF_LPVOID buffer,
                           FPDF_DWORD size);
  /**
   * @brief   Callback function to write data into the current file stream.
   *
   * @param[in]   clientData  Pointer to user-defined data.
   * @param[in]   offset      Offset position starts from the beginning of file
   * stream. This parameter indicates writing position.
   * @param[in]   buffer      Memory buffer contains data which is written into
   * file stream. This parameter should not be <b>NULL</b>.
   * @param[in]   size        Size of data which should be written into file
   * stream, in bytes.
   *
   * @return 0 for success, other value for failure.
   */
  FPDF_RESULT (*WriteBlock)(FPDF_LPVOID clientData,
                            FPDF_DWORD offset,
                            FPDF_LPCVOID buffer,
                            FPDF_DWORD size);
  /**
   * @brief   Callback function to flush all internal accessing buffers.
   *
   * @param[in]   clientData  Pointer to user-defined data.
   *
   * @return 0 for success, other value for failure.
   */
  FPDF_RESULT (*Flush)(FPDF_LPVOID clientData);
  /**
   * @brief   Callback function to change file size.
   *
   * @details This function is called under writing mode usually. Implementer
   * can determine whether to realize it based on application requests.
   *
   * @param[in]   clientData  Pointer to user-defined data.
   * @param[in]   size        New size of file stream, in bytes.
   *
   * @return 0 for success, other value for failure.
   */
  FPDF_RESULT (*Truncate)(FPDF_LPVOID clientData, FPDF_DWORD size);
} FPDF_FILEHANDLER, *FPDF_LPFILEHANDLER;

#endif
// Function: FPDF_LoadCustomDocument
//          Load PDF document from a custom access descriptor.
// Parameters:
//          pFileAccess -   A structure for accessing the file.
//          password    -   Optional password for decrypting the PDF file.
// Return value:
//          A handle to the loaded document, or NULL on failure.
// Comments:
//          The application must keep the file resources valid until the PDF
//          document is closed.
//
//          The loaded document can be closed with FPDF_CloseDocument.
// Notes:
//          If PDFium is built with the XFA module, the application should call
//          FPDF_LoadXFA() function after the PDF document loaded to support XFA
//          fields defined in the fpdfformfill.h file.
DLLEXPORT FPDF_DOCUMENT STDCALL
FPDF_LoadCustomDocument(FPDF_FILEACCESS* pFileAccess, FPDF_BYTESTRING password);

// Function: FPDF_GetFileVersion
//          Get the file version of the given PDF document.
// Parameters:
//          doc         -   Handle to a document.
//          fileVersion -   The PDF file version. File version: 14 for 1.4, 15
//                          for 1.5, ...
// Return value:
//          True if succeeds, false otherwise.
// Comments:
//          If the document was created by FPDF_CreateNewDocument,
//          then this function will always fail.
DLLEXPORT FPDF_BOOL STDCALL FPDF_GetFileVersion(FPDF_DOCUMENT doc,
                                                int* fileVersion);

#define FPDF_ERR_SUCCESS 0    // No error.
#define FPDF_ERR_UNKNOWN 1    // Unknown error.
#define FPDF_ERR_FILE 2       // File not found or could not be opened.
#define FPDF_ERR_FORMAT 3     // File not in PDF format or corrupted.
#define FPDF_ERR_PASSWORD 4   // Password required or incorrect password.
#define FPDF_ERR_SECURITY 5   // Unsupported security scheme.
#define FPDF_ERR_PAGE 6       // Page not found or content error.
#ifdef PDF_ENABLE_XFA
#define FPDF_ERR_XFALOAD 7    // Load XFA error.
#define FPDF_ERR_XFALAYOUT 8  // Layout XFA error.
#endif  // PDF_ENABLE_XFA

// Function: FPDF_GetLastError
//          Get last error code when a function fails.
// Parameters:
//          None.
// Return value:
//          A 32-bit integer indicating error code as defined above.
// Comments:
//          If the previous SDK call succeeded, the return value of this
//          function is not defined.
DLLEXPORT unsigned long STDCALL FPDF_GetLastError();

// Function: FPDF_GetDocPermission
//          Get file permission flags of the document.
// Parameters:
//          document    -   Handle to a document. Returned by FPDF_LoadDocument.
// Return value:
//          A 32-bit integer indicating permission flags. Please refer to the
//          PDF Reference for detailed descriptions. If the document is not
//          protected, 0xffffffff will be returned.
DLLEXPORT unsigned long STDCALL FPDF_GetDocPermissions(FPDF_DOCUMENT document);

// Function: FPDF_GetSecurityHandlerRevision
//          Get the revision for the security handler.
// Parameters:
//          document    -   Handle to a document. Returned by FPDF_LoadDocument.
// Return value:
//          The security handler revision number. Please refer to the PDF
//          Reference for a detailed description. If the document is not
//          protected, -1 will be returned.
DLLEXPORT int STDCALL FPDF_GetSecurityHandlerRevision(FPDF_DOCUMENT document);

// Function: FPDF_GetPageCount
//          Get total number of pages in the document.
// Parameters:
//          document    -   Handle to document. Returned by FPDF_LoadDocument.
// Return value:
//          Total number of pages in the document.
DLLEXPORT int STDCALL FPDF_GetPageCount(FPDF_DOCUMENT document);

// Function: FPDF_LoadPage
//          Load a page inside the document.
// Parameters:
//          document    -   Handle to document. Returned by FPDF_LoadDocument
//          page_index  -   Index number of the page. 0 for the first page.
// Return value:
//          A handle to the loaded page, or NULL if page load fails.
// Comments:
//          The loaded page can be rendered to devices using FPDF_RenderPage.
//          The loaded page can be closed using FPDF_ClosePage.
DLLEXPORT FPDF_PAGE STDCALL FPDF_LoadPage(FPDF_DOCUMENT document,
                                          int page_index);

// Function: FPDF_GetPageWidth
//          Get page width.
// Parameters:
//          page        -   Handle to the page. Returned by FPDF_LoadPage.
// Return value:
//          Page width (excluding non-displayable area) measured in points.
//          One point is 1/72 inch (around 0.3528 mm).
DLLEXPORT double STDCALL FPDF_GetPageWidth(FPDF_PAGE page);

// Function: FPDF_GetPageHeight
//          Get page height.
// Parameters:
//          page        -   Handle to the page. Returned by FPDF_LoadPage.
// Return value:
//          Page height (excluding non-displayable area) measured in points.
//          One point is 1/72 inch (around 0.3528 mm)
DLLEXPORT double STDCALL FPDF_GetPageHeight(FPDF_PAGE page);

// Function: FPDF_GetPageSizeByIndex
//          Get the size of the page at the given index.
// Parameters:
//          document    -   Handle to document. Returned by FPDF_LoadDocument.
//          page_index  -   Page index, zero for the first page.
//          width       -   Pointer to a double to receive the page width
//                          (in points).
//          height      -   Pointer to a double to receive the page height
//                          (in points).
// Return value:
//          Non-zero for success. 0 for error (document or page not found).
DLLEXPORT int STDCALL FPDF_GetPageSizeByIndex(FPDF_DOCUMENT document,
                                              int page_index,
                                              double* width,
                                              double* height);

// Page rendering flags. They can be combined with bit-wise OR.
//
// Set if annotations are to be rendered.
#define FPDF_ANNOT 0x01
// Set if using text rendering optimized for LCD display.
#define FPDF_LCD_TEXT 0x02
// Don't use the native text output available on some platforms
#define FPDF_NO_NATIVETEXT 0x04
// Grayscale output.
#define FPDF_GRAYSCALE 0x08
// Set if you want to get some debug info.
#define FPDF_DEBUG_INFO 0x80
// Set if you don't want to catch exceptions.
#define FPDF_NO_CATCH 0x100
// Limit image cache size.
#define FPDF_RENDER_LIMITEDIMAGECACHE 0x200
// Always use halftone for image stretching.
#define FPDF_RENDER_FORCEHALFTONE 0x400
// Render for printing.
#define FPDF_PRINTING 0x800
// Set to disable anti-aliasing on text.
#define FPDF_RENDER_NO_SMOOTHTEXT 0x1000
// Set to disable anti-aliasing on images.
#define FPDF_RENDER_NO_SMOOTHIMAGE 0x2000
// Set to disable anti-aliasing on paths.
#define FPDF_RENDER_NO_SMOOTHPATH 0x4000
// Set whether to render in a reverse Byte order, this flag is only used when
// rendering to a bitmap.
#define FPDF_REVERSE_BYTE_ORDER 0x10

#ifdef _WIN32
// Function: FPDF_RenderPage
//          Render contents of a page to a device (screen, bitmap, or printer).
//          This function is only supported on Windows.
// Parameters:
//          dc          -   Handle to the device context.
//          page        -   Handle to the page. Returned by FPDF_LoadPage.
//          start_x     -   Left pixel position of the display area in
//                          device coordinates.
//          start_y     -   Top pixel position of the display area in device
//                          coordinates.
//          size_x      -   Horizontal size (in pixels) for displaying the page.
//          size_y      -   Vertical size (in pixels) for displaying the page.
//          rotate      -   Page orientation:
//                            0 (normal)
//                            1 (rotated 90 degrees clockwise)
//                            2 (rotated 180 degrees)
//                            3 (rotated 90 degrees counter-clockwise)
//          flags       -   0 for normal display, or combination of flags
//                          defined above.
// Return value:
//          None.
DLLEXPORT void STDCALL FPDF_RenderPage(HDC dc,
                                       FPDF_PAGE page,
                                       int start_x,
                                       int start_y,
                                       int size_x,
                                       int size_y,
                                       int rotate,
                                       int flags);
#endif

// Function: FPDF_RenderPageBitmap
//          Render contents of a page to a device independent bitmap.
// Parameters:
//          bitmap      -   Handle to the device independent bitmap (as the
//                          output buffer). The bitmap handle can be created
//                          by FPDFBitmap_Create.
//          page        -   Handle to the page. Returned by FPDF_LoadPage
//          start_x     -   Left pixel position of the display area in
//                          bitmap coordinates.
//          start_y     -   Top pixel position of the display area in bitmap
//                          coordinates.
//          size_x      -   Horizontal size (in pixels) for displaying the page.
//          size_y      -   Vertical size (in pixels) for displaying the page.
//          rotate      -   Page orientation:
//                            0 (normal)
//                            1 (rotated 90 degrees clockwise)
//                            2 (rotated 180 degrees)
//                            3 (rotated 90 degrees counter-clockwise)
//          flags       -   0 for normal display, or combination of the Page
//                          Rendering flags defined above. With the FPDF_ANNOT
//                          flag, it renders all annotations that do not require
//                          user-interaction, which are all annotations except
//                          widget and popup annotations.
// Return value:
//          None.
DLLEXPORT void STDCALL FPDF_RenderPageBitmap(FPDF_BITMAP bitmap,
                                             FPDF_PAGE page,
                                             int start_x,
                                             int start_y,
                                             int size_x,
                                             int size_y,
                                             int rotate,
                                             int flags);

// Function: FPDF_RenderPageBitmapWithMatrix
//          Render contents of a page to a device independent bitmap.
// Parameters:
//          bitmap      -   Handle to the device independent bitmap (as the
//                          output buffer). The bitmap handle can be created
//                          by FPDFBitmap_Create.
//          page        -   Handle to the page. Returned by FPDF_LoadPage
//          matrix      -   The transform matrix.
//          clipping    -   The rect to clip to.
//          flags       -   0 for normal display, or combination of the Page
//                          Rendering flags defined above. With the FPDF_ANNOT
//                          flag, it renders all annotations that do not require
//                          user-interaction, which are all annotations except
//                          widget and popup annotations.
// Return value:
//          None.
DLLEXPORT void STDCALL FPDF_RenderPageBitmapWithMatrix(FPDF_BITMAP bitmap,
                                                       FPDF_PAGE page,
                                                       const FS_MATRIX* matrix,
                                                       const FS_RECTF* clipping,
                                                       int flags);

#ifdef _SKIA_SUPPORT_
DLLEXPORT FPDF_RECORDER STDCALL FPDF_RenderPageSkp(FPDF_PAGE page,
                                                   int size_x,
                                                   int size_y);
#endif

// Function: FPDF_ClosePage
//          Close a loaded PDF page.
// Parameters:
//          page        -   Handle to the loaded page.
// Return value:
//          None.
DLLEXPORT void STDCALL FPDF_ClosePage(FPDF_PAGE page);

// Function: FPDF_CloseDocument
//          Close a loaded PDF document.
// Parameters:
//          document    -   Handle to the loaded document.
// Return value:
//          None.
DLLEXPORT void STDCALL FPDF_CloseDocument(FPDF_DOCUMENT document);

// Function: FPDF_DeviceToPage
//          Convert the screen coordinates of a point to page coordinates.
// Parameters:
//          page        -   Handle to the page. Returned by FPDF_LoadPage.
//          start_x     -   Left pixel position of the display area in
//                          device coordinates.
//          start_y     -   Top pixel position of the display area in device
//                          coordinates.
//          size_x      -   Horizontal size (in pixels) for displaying the page.
//          size_y      -   Vertical size (in pixels) for displaying the page.
//          rotate      -   Page orientation:
//                            0 (normal)
//                            1 (rotated 90 degrees clockwise)
//                            2 (rotated 180 degrees)
//                            3 (rotated 90 degrees counter-clockwise)
//          device_x    -   X value in device coordinates to be converted.
//          device_y    -   Y value in device coordinates to be converted.
//          page_x      -   A pointer to a double receiving the converted X
//                          value in page coordinates.
//          page_y      -   A pointer to a double receiving the converted Y
//                          value in page coordinates.
// Return value:
//          None.
// Comments:
//          The page coordinate system has its origin at the left-bottom corner
//          of the page, with the X-axis on the bottom going to the right, and
//          the Y-axis on the left side going up.
//
//          NOTE: this coordinate system can be altered when you zoom, scroll,
//          or rotate a page, however, a point on the page should always have
//          the same coordinate values in the page coordinate system.
//
//          The device coordinate system is device dependent. For screen device,
//          its origin is at the left-top corner of the window. However this
//          origin can be altered by the Windows coordinate transformation
//          utilities.
//
//          You must make sure the start_x, start_y, size_x, size_y
//          and rotate parameters have exactly same values as you used in
//          the FPDF_RenderPage() function call.
DLLEXPORT void STDCALL FPDF_DeviceToPage(FPDF_PAGE page,
                                         int start_x,
                                         int start_y,
                                         int size_x,
                                         int size_y,
                                         int rotate,
                                         int device_x,
                                         int device_y,
                                         double* page_x,
                                         double* page_y);

// Function: FPDF_PageToDevice
//          Convert the page coordinates of a point to screen coordinates.
// Parameters:
//          page        -   Handle to the page. Returned by FPDF_LoadPage.
//          start_x     -   Left pixel position of the display area in
//                          device coordinates.
//          start_y     -   Top pixel position of the display area in device
//                          coordinates.
//          size_x      -   Horizontal size (in pixels) for displaying the page.
//          size_y      -   Vertical size (in pixels) for displaying the page.
//          rotate      -   Page orientation:
//                            0 (normal)
//                            1 (rotated 90 degrees clockwise)
//                            2 (rotated 180 degrees)
//                            3 (rotated 90 degrees counter-clockwise)
//          page_x      -   X value in page coordinates.
//          page_y      -   Y value in page coordinate.
//          device_x    -   A pointer to an integer receiving the result X
//                          value in device coordinates.
//          device_y    -   A pointer to an integer receiving the result Y
//                          value in device coordinates.
// Return value:
//          None.
// Comments:
//          See comments for FPDF_DeviceToPage().
DLLEXPORT void STDCALL FPDF_PageToDevice(FPDF_PAGE page,
                                         int start_x,
                                         int start_y,
                                         int size_x,
                                         int size_y,
                                         int rotate,
                                         double page_x,
                                         double page_y,
                                         int* device_x,
                                         int* device_y);

// Function: FPDFBitmap_Create
//          Create a device independent bitmap (FXDIB).
// Parameters:
//          width       -   The number of pixels in width for the bitmap.
//                          Must be greater than 0.
//          height      -   The number of pixels in height for the bitmap.
//                          Must be greater than 0.
//          alpha       -   A flag indicating whether the alpha channel is used.
//                          Non-zero for using alpha, zero for not using.
// Return value:
//          The created bitmap handle, or NULL if a parameter error or out of
//          memory.
// Comments:
//          The bitmap always uses 4 bytes per pixel. The first byte is always
//          double word aligned.
//
//          The byte order is BGRx (the last byte unused if no alpha channel) or
//          BGRA.
//
//          The pixels in a horizontal line are stored side by side, with the
//          left most pixel stored first (with lower memory address).
//          Each line uses width * 4 bytes.
//
//          Lines are stored one after another, with the top most line stored
//          first. There is no gap between adjacent lines.
//
//          This function allocates enough memory for holding all pixels in the
//          bitmap, but it doesn't initialize the buffer. Applications can use
//          FPDFBitmap_FillRect to fill the bitmap using any color.
DLLEXPORT FPDF_BITMAP STDCALL FPDFBitmap_Create(int width,
                                                int height,
                                                int alpha);

// More DIB formats
// Gray scale bitmap, one byte per pixel.
#define FPDFBitmap_Gray 1
// 3 bytes per pixel, byte order: blue, green, red.
#define FPDFBitmap_BGR 2
// 4 bytes per pixel, byte order: blue, green, red, unused.
#define FPDFBitmap_BGRx 3
// 4 bytes per pixel, byte order: blue, green, red, alpha.
#define FPDFBitmap_BGRA 4

// Function: FPDFBitmap_CreateEx
//          Create a device independent bitmap (FXDIB)
// Parameters:
//          width       -   The number of pixels in width for the bitmap.
//                          Must be greater than 0.
//          height      -   The number of pixels in height for the bitmap.
//                          Must be greater than 0.
//          format      -   A number indicating for bitmap format, as defined
//                          above.
//          first_scan  -   A pointer to the first byte of the first line if
//                          using an external buffer. If this parameter is NULL,
//                          then the a new buffer will be created.
//          stride      -   Number of bytes for each scan line, for external
//                          buffer only.
// Return value:
//          The bitmap handle, or NULL if parameter error or out of memory.
// Comments:
//          Similar to FPDFBitmap_Create function, but allows for more formats
//          and an external buffer is supported. The bitmap created by this
//          function can be used in any place that a FPDF_BITMAP handle is
//          required.
//
//          If an external buffer is used, then the application should destroy
//          the buffer by itself. FPDFBitmap_Destroy function will not destroy
//          the buffer.
DLLEXPORT FPDF_BITMAP STDCALL FPDFBitmap_CreateEx(int width,
                                                  int height,
                                                  int format,
                                                  void* first_scan,
                                                  int stride);

// Function: FPDFBitmap_FillRect
//          Fill a rectangle in a bitmap.
// Parameters:
//          bitmap      -   The handle to the bitmap. Returned by
//                          FPDFBitmap_Create.
//          left        -   The left position. Starting from 0 at the
//                          left-most pixel.
//          top         -   The top position. Starting from 0 at the
//                          top-most line.
//          width       -   Width in pixels to be filled.
//          height      -   Height in pixels to be filled.
//          color       -   A 32-bit value specifing the color, in 8888 ARGB
//                          format.
// Return value:
//          None.
// Comments:
//          This function sets the color and (optionally) alpha value in the
//          specified region of the bitmap.
//
//          NOTE: If the alpha channel is used, this function does NOT
//          composite the background with the source color, instead the
//          background will be replaced by the source color and the alpha.
//
//          If the alpha channel is not used, the alpha parameter is ignored.
DLLEXPORT void STDCALL FPDFBitmap_FillRect(FPDF_BITMAP bitmap,
                                           int left,
                                           int top,
                                           int width,
                                           int height,
                                           FPDF_DWORD color);

// Function: FPDFBitmap_GetBuffer
//          Get data buffer of a bitmap.
// Parameters:
//          bitmap      -   Handle to the bitmap. Returned by FPDFBitmap_Create.
// Return value:
//          The pointer to the first byte of the bitmap buffer.
// Comments:
//          The stride may be more than width * number of bytes per pixel
//
//          Applications can use this function to get the bitmap buffer pointer,
//          then manipulate any color and/or alpha values for any pixels in the
//          bitmap.
//
//          The data is in BGRA format. Where the A maybe unused if alpha was
//          not specified.
DLLEXPORT void* STDCALL FPDFBitmap_GetBuffer(FPDF_BITMAP bitmap);

// Function: FPDFBitmap_GetWidth
//          Get width of a bitmap.
// Parameters:
//          bitmap      -   Handle to the bitmap. Returned by FPDFBitmap_Create.
// Return value:
//          The width of the bitmap in pixels.
DLLEXPORT int STDCALL FPDFBitmap_GetWidth(FPDF_BITMAP bitmap);

// Function: FPDFBitmap_GetHeight
//          Get height of a bitmap.
// Parameters:
//          bitmap      -   Handle to the bitmap. Returned by FPDFBitmap_Create.
// Return value:
//          The height of the bitmap in pixels.
DLLEXPORT int STDCALL FPDFBitmap_GetHeight(FPDF_BITMAP bitmap);

// Function: FPDFBitmap_GetStride
//          Get number of bytes for each line in the bitmap buffer.
// Parameters:
//          bitmap      -   Handle to the bitmap. Returned by FPDFBitmap_Create.
// Return value:
//          The number of bytes for each line in the bitmap buffer.
// Comments:
//          The stride may be more than width * number of bytes per pixel.
DLLEXPORT int STDCALL FPDFBitmap_GetStride(FPDF_BITMAP bitmap);

// Function: FPDFBitmap_Destroy
//          Destroy a bitmap and release all related buffers.
// Parameters:
//          bitmap      -   Handle to the bitmap. Returned by FPDFBitmap_Create.
// Return value:
//          None.
// Comments:
//          This function will not destroy any external buffers provided when
//          the bitmap was created.
DLLEXPORT void STDCALL FPDFBitmap_Destroy(FPDF_BITMAP bitmap);

// Function: FPDF_VIEWERREF_GetPrintScaling
//          Whether the PDF document prefers to be scaled or not.
// Parameters:
//          document    -   Handle to the loaded document.
// Return value:
//          None.
DLLEXPORT FPDF_BOOL STDCALL
FPDF_VIEWERREF_GetPrintScaling(FPDF_DOCUMENT document);

// Function: FPDF_VIEWERREF_GetNumCopies
//          Returns the number of copies to be printed.
// Parameters:
//          document    -   Handle to the loaded document.
// Return value:
//          The number of copies to be printed.
DLLEXPORT int STDCALL FPDF_VIEWERREF_GetNumCopies(FPDF_DOCUMENT document);

// Function: FPDF_VIEWERREF_GetPrintPageRange
//          Page numbers to initialize print dialog box when file is printed.
// Parameters:
//          document    -   Handle to the loaded document.
// Return value:
//          The print page range to be used for printing.
DLLEXPORT FPDF_PAGERANGE STDCALL
FPDF_VIEWERREF_GetPrintPageRange(FPDF_DOCUMENT document);

// Function: FPDF_VIEWERREF_GetDuplex
//          Returns the paper handling option to be used when printing from
//          the print dialog.
// Parameters:
//          document    -   Handle to the loaded document.
// Return value:
//          The paper handling option to be used when printing.
DLLEXPORT FPDF_DUPLEXTYPE STDCALL
FPDF_VIEWERREF_GetDuplex(FPDF_DOCUMENT document);

// Function: FPDF_VIEWERREF_GetName
//          Gets the contents for a viewer ref, with a given key. The value must
//          be of type "name".
// Parameters:
//          document    -   Handle to the loaded document.
//          key         -   Name of the key in the viewer pref dictionary.
//          buffer      -   A string to write the contents of the key to.
//          length      -   Length of the buffer.
// Return value:
//          The number of bytes in the contents, including the NULL terminator.
//          Thus if the return value is 0, then that indicates an error, such
//          as when |document| is invalid or |buffer| is NULL. If |length| is
//          less than the returned length, or |buffer| is NULL, |buffer| will
//          not be modified.
DLLEXPORT unsigned long STDCALL FPDF_VIEWERREF_GetName(FPDF_DOCUMENT document,
                                                       FPDF_BYTESTRING key,
                                                       char* buffer,
                                                       unsigned long length);

// Function: FPDF_CountNamedDests
//          Get the count of named destinations in the PDF document.
// Parameters:
//          document    -   Handle to a document
// Return value:
//          The count of named destinations.
DLLEXPORT FPDF_DWORD STDCALL FPDF_CountNamedDests(FPDF_DOCUMENT document);

// Function: FPDF_GetNamedDestByName
//          Get a the destination handle for the given name.
// Parameters:
//          document    -   Handle to the loaded document.
//          name        -   The name of a destination.
// Return value:
//          The handle to the destination.
DLLEXPORT FPDF_DEST STDCALL FPDF_GetNamedDestByName(FPDF_DOCUMENT document,
                                                    FPDF_BYTESTRING name);

// Function: FPDF_GetNamedDest
//          Get the named destination by index.
// Parameters:
//          document        -   Handle to a document
//          index           -   The index of a named destination.
//          buffer          -   The buffer to store the destination name,
//                              used as wchar_t*.
//          buflen [in/out] -   Size of the buffer in bytes on input,
//                              length of the result in bytes on output
//                              or -1 if the buffer is too small.
// Return value:
//          The destination handle for a given index, or NULL if there is no
//          named destination corresponding to |index|.
// Comments:
//          Call this function twice to get the name of the named destination:
//            1) First time pass in |buffer| as NULL and get buflen.
//            2) Second time pass in allocated |buffer| and buflen to retrieve
//               |buffer|, which should be used as wchar_t*.
//
//         If buflen is not sufficiently large, it will be set to -1 upon
//         return.
DLLEXPORT FPDF_DEST STDCALL FPDF_GetNamedDest(FPDF_DOCUMENT document,
                                              int index,
                                              void* buffer,
                                              long* buflen);

#ifdef PDF_ENABLE_XFA
// Function: FPDF_BStr_Init
//          Helper function to initialize a byte string.
DLLEXPORT FPDF_RESULT STDCALL FPDF_BStr_Init(FPDF_BSTR* str);

// Function: FPDF_BStr_Set
//          Helper function to set string data.
DLLEXPORT FPDF_RESULT STDCALL FPDF_BStr_Set(FPDF_BSTR* str,
                                            FPDF_LPCSTR bstr,
                                            int length);

// Function: FPDF_BStr_Clear
//          Helper function to clear a byte string.
DLLEXPORT FPDF_RESULT STDCALL FPDF_BStr_Clear(FPDF_BSTR* str);
#endif  // PDF_ENABLE_XFA

#ifdef __cplusplus
}
#endif

#endif  // PUBLIC_FPDFVIEW_H_
