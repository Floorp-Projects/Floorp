// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef PUBLIC_FPDF_TEXT_H_
#define PUBLIC_FPDF_TEXT_H_

// NOLINTNEXTLINE(build/include)
#include "fpdfview.h"

// Exported Functions
#ifdef __cplusplus
extern "C" {
#endif

// Function: FPDFText_LoadPage
//          Prepare information about all characters in a page.
// Parameters:
//          page    -   Handle to the page. Returned by FPDF_LoadPage function
//          (in FPDFVIEW module).
// Return value:
//          A handle to the text page information structure.
//          NULL if something goes wrong.
// Comments:
//          Application must call FPDFText_ClosePage to release the text page
//          information.
//
DLLEXPORT FPDF_TEXTPAGE STDCALL FPDFText_LoadPage(FPDF_PAGE page);

// Function: FPDFText_ClosePage
//          Release all resources allocated for a text page information
//          structure.
// Parameters:
//          text_page   -   Handle to a text page information structure.
//          Returned by FPDFText_LoadPage function.
// Return Value:
//          None.
//
DLLEXPORT void STDCALL FPDFText_ClosePage(FPDF_TEXTPAGE text_page);

// Function: FPDFText_CountChars
//          Get number of characters in a page.
// Parameters:
//          text_page   -   Handle to a text page information structure.
//          Returned by FPDFText_LoadPage function.
// Return value:
//          Number of characters in the page. Return -1 for error.
//          Generated characters, like additional space characters, new line
//          characters, are also counted.
// Comments:
//          Characters in a page form a "stream", inside the stream, each
//          character has an index.
//          We will use the index parameters in many of FPDFTEXT functions. The
//          first character in the page
//          has an index value of zero.
//
DLLEXPORT int STDCALL FPDFText_CountChars(FPDF_TEXTPAGE text_page);

// Function: FPDFText_GetUnicode
//          Get Unicode of a character in a page.
// Parameters:
//          text_page   -   Handle to a text page information structure.
//          Returned by FPDFText_LoadPage function.
//          index       -   Zero-based index of the character.
// Return value:
//          The Unicode of the particular character.
//          If a character is not encoded in Unicode and Foxit engine can't
//          convert to Unicode,
//          the return value will be zero.
//
DLLEXPORT unsigned int STDCALL FPDFText_GetUnicode(FPDF_TEXTPAGE text_page,
                                                   int index);

// Function: FPDFText_GetFontSize
//          Get the font size of a particular character.
// Parameters:
//          text_page   -   Handle to a text page information structure.
//          Returned by FPDFText_LoadPage function.
//          index       -   Zero-based index of the character.
// Return value:
//          The font size of the particular character, measured in points (about
//          1/72 inch).
//          This is the typographic size of the font (so called "em size").
//
DLLEXPORT double STDCALL FPDFText_GetFontSize(FPDF_TEXTPAGE text_page,
                                              int index);

// Function: FPDFText_GetCharBox
//          Get bounding box of a particular character.
// Parameters:
//          text_page   -   Handle to a text page information structure.
//          Returned by FPDFText_LoadPage function.
//          index       -   Zero-based index of the character.
//          left        -   Pointer to a double number receiving left position
//          of the character box.
//          right       -   Pointer to a double number receiving right position
//          of the character box.
//          bottom      -   Pointer to a double number receiving bottom position
//          of the character box.
//          top         -   Pointer to a double number receiving top position of
//          the character box.
// Return Value:
//          None.
// Comments:
//          All positions are measured in PDF "user space".
//
DLLEXPORT void STDCALL FPDFText_GetCharBox(FPDF_TEXTPAGE text_page,
                                           int index,
                                           double* left,
                                           double* right,
                                           double* bottom,
                                           double* top);

// Function: FPDFText_GetCharIndexAtPos
//          Get the index of a character at or nearby a certain position on the
//          page.
// Parameters:
//          text_page   -   Handle to a text page information structure.
//          Returned by FPDFText_LoadPage function.
//          x           -   X position in PDF "user space".
//          y           -   Y position in PDF "user space".
//          xTolerance  -   An x-axis tolerance value for character hit
//          detection, in point unit.
//          yTolerance  -   A y-axis tolerance value for character hit
//          detection, in point unit.
// Return Value:
//          The zero-based index of the character at, or nearby the point (x,y).
//          If there is no character at or nearby the point, return value will
//          be -1.
//          If an error occurs, -3 will be returned.
//
DLLEXPORT int STDCALL FPDFText_GetCharIndexAtPos(FPDF_TEXTPAGE text_page,
                                                 double x,
                                                 double y,
                                                 double xTolerance,
                                                 double yTolerance);

// Function: FPDFText_GetText
//          Extract unicode text string from the page.
// Parameters:
//          text_page   -   Handle to a text page information structure.
//          Returned by FPDFText_LoadPage function.
//          start_index -   Index for the start characters.
//          count       -   Number of characters to be extracted.
//          result      -   A buffer (allocated by application) receiving the
//          extracted unicodes.
//                          The size of the buffer must be able to hold the
//                          number of characters plus a terminator.
// Return Value:
//          Number of characters written into the result buffer, including the
//          trailing terminator.
// Comments:
//          This function ignores characters without unicode information.
//
DLLEXPORT int STDCALL FPDFText_GetText(FPDF_TEXTPAGE text_page,
                                       int start_index,
                                       int count,
                                       unsigned short* result);

// Function: FPDFText_CountRects
//          Count number of rectangular areas occupied by a segment of texts.
// Parameters:
//          text_page   -   Handle to a text page information structure.
//          Returned by FPDFText_LoadPage function.
//          start_index -   Index for the start characters.
//          count       -   Number of characters.
// Return value:
//          Number of rectangles. Zero for error.
// Comments:
//          This function, along with FPDFText_GetRect can be used by
//          applications to detect the position
//          on the page for a text segment, so proper areas can be highlighted
//          or something.
//          FPDFTEXT will automatically merge small character boxes into bigger
//          one if those characters
//          are on the same line and use same font settings.
//
DLLEXPORT int STDCALL FPDFText_CountRects(FPDF_TEXTPAGE text_page,
                                          int start_index,
                                          int count);

// Function: FPDFText_GetRect
//          Get a rectangular area from the result generated by
//          FPDFText_CountRects.
// Parameters:
//          text_page   -   Handle to a text page information structure.
//          Returned by FPDFText_LoadPage function.
//          rect_index  -   Zero-based index for the rectangle.
//          left        -   Pointer to a double value receiving the rectangle
//          left boundary.
//          top         -   Pointer to a double value receiving the rectangle
//          top boundary.
//          right       -   Pointer to a double value receiving the rectangle
//          right boundary.
//          bottom      -   Pointer to a double value receiving the rectangle
//          bottom boundary.
// Return Value:
//          None.
//
DLLEXPORT void STDCALL FPDFText_GetRect(FPDF_TEXTPAGE text_page,
                                        int rect_index,
                                        double* left,
                                        double* top,
                                        double* right,
                                        double* bottom);

// Function: FPDFText_GetBoundedText
//          Extract unicode text within a rectangular boundary on the page.
// Parameters:
//          text_page   -   Handle to a text page information structure.
//          Returned by FPDFText_LoadPage function.
//          left        -   Left boundary.
//          top         -   Top boundary.
//          right       -   Right boundary.
//          bottom      -   Bottom boundary.
//          buffer      -   A unicode buffer.
//          buflen      -   Number of characters (not bytes) for the buffer,
//          excluding an additional terminator.
// Return Value:
//          If buffer is NULL or buflen is zero, return number of characters
//          (not bytes) of text present within
//          the rectangle, excluding a terminating NUL.  Generally you should
//          pass a buffer at least one larger
//          than this if you want a terminating NUL, which will be provided if
//          space is available.
//          Otherwise, return number of characters copied into the buffer,
//          including the terminating NUL
//          when space for it is available.
// Comment:
//          If the buffer is too small, as much text as will fit is copied into
//          it.
//
DLLEXPORT int STDCALL FPDFText_GetBoundedText(FPDF_TEXTPAGE text_page,
                                              double left,
                                              double top,
                                              double right,
                                              double bottom,
                                              unsigned short* buffer,
                                              int buflen);

// Flags used by FPDFText_FindStart function.
#define FPDF_MATCHCASE \
  0x00000001  // If not set, it will not match case by default.
#define FPDF_MATCHWHOLEWORD \
  0x00000002  // If not set, it will not match the whole word by default.

// Function: FPDFText_FindStart
//          Start a search.
// Parameters:
//          text_page   -   Handle to a text page information structure.
//          Returned by FPDFText_LoadPage function.
//          findwhat    -   A unicode match pattern.
//          flags       -   Option flags.
//          start_index -   Start from this character. -1 for end of the page.
// Return Value:
//          A handle for the search context. FPDFText_FindClose must be called
//          to release this handle.
//
DLLEXPORT FPDF_SCHHANDLE STDCALL FPDFText_FindStart(FPDF_TEXTPAGE text_page,
                                                    FPDF_WIDESTRING findwhat,
                                                    unsigned long flags,
                                                    int start_index);

// Function: FPDFText_FindNext
//          Search in the direction from page start to end.
// Parameters:
//          handle      -   A search context handle returned by
//          FPDFText_FindStart.
// Return Value:
//          Whether a match is found.
//
DLLEXPORT FPDF_BOOL STDCALL FPDFText_FindNext(FPDF_SCHHANDLE handle);

// Function: FPDFText_FindPrev
//          Search in the direction from page end to start.
// Parameters:
//          handle      -   A search context handle returned by
//          FPDFText_FindStart.
// Return Value:
//          Whether a match is found.
//
DLLEXPORT FPDF_BOOL STDCALL FPDFText_FindPrev(FPDF_SCHHANDLE handle);

// Function: FPDFText_GetSchResultIndex
//          Get the starting character index of the search result.
// Parameters:
//          handle      -   A search context handle returned by
//          FPDFText_FindStart.
// Return Value:
//          Index for the starting character.
//
DLLEXPORT int STDCALL FPDFText_GetSchResultIndex(FPDF_SCHHANDLE handle);

// Function: FPDFText_GetSchCount
//          Get the number of matched characters in the search result.
// Parameters:
//          handle      -   A search context handle returned by
//          FPDFText_FindStart.
// Return Value:
//          Number of matched characters.
//
DLLEXPORT int STDCALL FPDFText_GetSchCount(FPDF_SCHHANDLE handle);

// Function: FPDFText_FindClose
//          Release a search context.
// Parameters:
//          handle      -   A search context handle returned by
//          FPDFText_FindStart.
// Return Value:
//          None.
//
DLLEXPORT void STDCALL FPDFText_FindClose(FPDF_SCHHANDLE handle);

// Function: FPDFLink_LoadWebLinks
//          Prepare information about weblinks in a page.
// Parameters:
//          text_page   -   Handle to a text page information structure.
//          Returned by FPDFText_LoadPage function.
// Return Value:
//          A handle to the page's links information structure.
//          NULL if something goes wrong.
// Comments:
//          Weblinks are those links implicitly embedded in PDF pages. PDF also
//          has a type of
//          annotation called "link", FPDFTEXT doesn't deal with that kind of
//          link.
//          FPDFTEXT weblink feature is useful for automatically detecting links
//          in the page
//          contents. For example, things like "http://www.foxitsoftware.com"
//          will be detected,
//          so applications can allow user to click on those characters to
//          activate the link,
//          even the PDF doesn't come with link annotations.
//
//          FPDFLink_CloseWebLinks must be called to release resources.
//
DLLEXPORT FPDF_PAGELINK STDCALL FPDFLink_LoadWebLinks(FPDF_TEXTPAGE text_page);

// Function: FPDFLink_CountWebLinks
//          Count number of detected web links.
// Parameters:
//          link_page   -   Handle returned by FPDFLink_LoadWebLinks.
// Return Value:
//          Number of detected web links.
//
DLLEXPORT int STDCALL FPDFLink_CountWebLinks(FPDF_PAGELINK link_page);

// Function: FPDFLink_GetURL
//          Fetch the URL information for a detected web link.
// Parameters:
//          link_page   -   Handle returned by FPDFLink_LoadWebLinks.
//          link_index  -   Zero-based index for the link.
//          buffer      -   A unicode buffer for the result.
//          buflen      -   Number of characters (not bytes) for the buffer,
//                          including an additional terminator.
// Return Value:
//          If |buffer| is NULL or |buflen| is zero, return the number of
//          characters (not bytes) needed to buffer the result (an additional
//          terminator is included in this count).
//          Otherwise, copy the result into |buffer|, truncating at |buflen| if
//          the result is too large to fit, and return the number of characters
//          actually copied into the buffer (the additional terminator is also
//          included in this count).
//          If |link_index| does not correspond to a valid link, then the result
//          is an empty string.
//
DLLEXPORT int STDCALL FPDFLink_GetURL(FPDF_PAGELINK link_page,
                                      int link_index,
                                      unsigned short* buffer,
                                      int buflen);

// Function: FPDFLink_CountRects
//          Count number of rectangular areas for the link.
// Parameters:
//          link_page   -   Handle returned by FPDFLink_LoadWebLinks.
//          link_index  -   Zero-based index for the link.
// Return Value:
//          Number of rectangular areas for the link.  If |link_index| does
//          not correspond to a valid link, then 0 is returned.
//
DLLEXPORT int STDCALL FPDFLink_CountRects(FPDF_PAGELINK link_page,
                                          int link_index);

// Function: FPDFLink_GetRect
//          Fetch the boundaries of a rectangle for a link.
// Parameters:
//          link_page   -   Handle returned by FPDFLink_LoadWebLinks.
//          link_index  -   Zero-based index for the link.
//          rect_index  -   Zero-based index for a rectangle.
//          left        -   Pointer to a double value receiving the rectangle
//                          left boundary.
//          top         -   Pointer to a double value receiving the rectangle
//                          top boundary.
//          right       -   Pointer to a double value receiving the rectangle
//                          right boundary.
//          bottom      -   Pointer to a double value receiving the rectangle
//                          bottom boundary.
// Return Value:
//          None.  If |link_index| does not correspond to a valid link, then
//          |left|, |top|, |right|, and |bottom| remain unmodified.
//
DLLEXPORT void STDCALL FPDFLink_GetRect(FPDF_PAGELINK link_page,
                                        int link_index,
                                        int rect_index,
                                        double* left,
                                        double* top,
                                        double* right,
                                        double* bottom);

// Function: FPDFLink_CloseWebLinks
//          Release resources used by weblink feature.
// Parameters:
//          link_page   -   Handle returned by FPDFLink_LoadWebLinks.
// Return Value:
//          None.
//
DLLEXPORT void STDCALL FPDFLink_CloseWebLinks(FPDF_PAGELINK link_page);

#ifdef __cplusplus
}
#endif

#endif  // PUBLIC_FPDF_TEXT_H_
