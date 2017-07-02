// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef PUBLIC_FPDF_DOC_H_
#define PUBLIC_FPDF_DOC_H_

// NOLINTNEXTLINE(build/include)
#include "fpdfview.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Unsupported action type.
#define PDFACTION_UNSUPPORTED 0
// Go to a destination within current document.
#define PDFACTION_GOTO 1
// Go to a destination within another document.
#define PDFACTION_REMOTEGOTO 2
// URI, including web pages and other Internet resources.
#define PDFACTION_URI 3
// Launch an application or open a file.
#define PDFACTION_LAUNCH 4

typedef struct _FS_QUADPOINTSF {
  FS_FLOAT x1;
  FS_FLOAT y1;
  FS_FLOAT x2;
  FS_FLOAT y2;
  FS_FLOAT x3;
  FS_FLOAT y3;
  FS_FLOAT x4;
  FS_FLOAT y4;
} FS_QUADPOINTSF;

// Get the first child of |bookmark|, or the first top-level bookmark item.
//
//   document - handle to the document.
//   bookmark - handle to the current bookmark. Pass NULL for the first top
//              level item.
//
// Returns a handle to the first child of |bookmark| or the first top-level
// bookmark item. NULL if no child or top-level bookmark found.
DLLEXPORT FPDF_BOOKMARK STDCALL
FPDFBookmark_GetFirstChild(FPDF_DOCUMENT document, FPDF_BOOKMARK bookmark);

// Get the next sibling of |bookmark|.
//
//   document - handle to the document.
//   bookmark - handle to the current bookmark.
//
// Returns a handle to the next sibling of |bookmark|, or NULL if this is the
// last bookmark at this level.
DLLEXPORT FPDF_BOOKMARK STDCALL
FPDFBookmark_GetNextSibling(FPDF_DOCUMENT document, FPDF_BOOKMARK bookmark);

// Get the title of |bookmark|.
//
//   bookmark - handle to the bookmark.
//   buffer   - buffer for the title. May be NULL.
//   buflen   - the length of the buffer in bytes. May be 0.
//
// Returns the number of bytes in the title, including the terminating NUL
// character. The number of bytes is returned regardless of the |buffer| and
// |buflen| parameters.
//
// Regardless of the platform, the |buffer| is always in UTF-16LE encoding. The
// string is terminated by a UTF16 NUL character. If |buflen| is less than the
// required length, or |buffer| is NULL, |buffer| will not be modified.
DLLEXPORT unsigned long STDCALL FPDFBookmark_GetTitle(FPDF_BOOKMARK bookmark,
                                                      void* buffer,
                                                      unsigned long buflen);

// Find the bookmark with |title| in |document|.
//
//   document - handle to the document.
//   title    - the UTF-16LE encoded Unicode title for which to search.
//
// Returns the handle to the bookmark, or NULL if |title| can't be found.
//
// |FPDFBookmark_Find| will always return the first bookmark found even if
// multiple bookmarks have the same |title|.
DLLEXPORT FPDF_BOOKMARK STDCALL FPDFBookmark_Find(FPDF_DOCUMENT document,
                                                  FPDF_WIDESTRING title);

// Get the destination associated with |bookmark|.
//
//   document - handle to the document.
//   bookmark - handle to the bookmark.
//
// Returns the handle to the destination data,  NULL if no destination is
// associated with |bookmark|.
DLLEXPORT FPDF_DEST STDCALL FPDFBookmark_GetDest(FPDF_DOCUMENT document,
                                                 FPDF_BOOKMARK bookmark);

// Get the action associated with |bookmark|.
//
//   bookmark - handle to the bookmark.
//
// Returns the handle to the action data, or NULL if no action is associated
// with |bookmark|. When NULL is returned, |FPDFBookmark_GetDest| should be
// called to get the |bookmark| destination data.
DLLEXPORT FPDF_ACTION STDCALL FPDFBookmark_GetAction(FPDF_BOOKMARK bookmark);

// Get the type of |action|.
//
//   action - handle to the action.
//
// Returns one of:
//   PDFACTION_UNSUPPORTED
//   PDFACTION_GOTO
//   PDFACTION_REMOTEGOTO
//   PDFACTION_URI
//   PDFACTION_LAUNCH
DLLEXPORT unsigned long STDCALL FPDFAction_GetType(FPDF_ACTION action);

// Get the destination of |action|.
//
//   document - handle to the document.
//   action   - handle to the action. |action| must be a |PDFACTION_GOTO| or
//              |PDFACTION_REMOTEGOTO|.
//
// Returns a handle to the destination data.
//
// In the case of |PDFACTION_REMOTEGOTO|, you should first call
// |FPDFAction_GetFilePath| then load that document, the document handle from
// that document should pass as |document| to |FPDFAction_GetDest|.
DLLEXPORT FPDF_DEST STDCALL FPDFAction_GetDest(FPDF_DOCUMENT document,
                                               FPDF_ACTION action);

// Get file path of a |PDFACTION_REMOTEGOTO| |action|.
//
//   action - handle to the action. |action| must be a |PDFACTION_LAUNCH| or
//            |PDFACTION_REMOTEGOTO|
//   buffer - a buffer for output the path string. May be NULL.
//   buflen - the length of the buffer, in bytes. May be 0.
//
// Returns the number of bytes in the file path, including the trailing UTF16
// NUL character.
//
// Regardless of the platform, the |buffer| is always in UTF-16LE encoding.
// If |buflen| is less than the returned length, or |buffer| is NULL, |buffer|
// will not be modified.
DLLEXPORT unsigned long STDCALL
FPDFAction_GetFilePath(FPDF_ACTION action, void* buffer, unsigned long buflen);

// Get the URI path of a |PDFACTION_URI| |action|.
//
//   document - handle to the document.
//   action   - handle to the action. Must be a |PDFACTION_URI|.
//   buffer   - a buffer for the path string. May be NULL.
//   buflen   - the length of the buffer, in bytes. May be 0.
//
// Returns the number of bytes in the URI path, including trailing zeros.
//
// The |buffer| is always encoded in 7-bit ASCII. If |buflen| is less than the
// returned length, or |buffer| is NULL, |buffer| will not be modified.
DLLEXPORT unsigned long STDCALL FPDFAction_GetURIPath(FPDF_DOCUMENT document,
                                                      FPDF_ACTION action,
                                                      void* buffer,
                                                      unsigned long buflen);

// Get the page index of |dest|.
//
//   document - handle to the document.
//   dest     - handle to the destination.
//
// Returns the page index containing |dest|. Page indices start from 0.
DLLEXPORT unsigned long STDCALL FPDFDest_GetPageIndex(FPDF_DOCUMENT document,
                                                      FPDF_DEST dest);

// Get the (x, y, zoom) location of |dest| in the destination page, if the
// destination is in [page /XYZ x y zoom] syntax.
//
//   dest       - handle to the destination.
//   hasXVal    - out parameter; true if the x value is not null
//   hasYVal    - out parameter; true if the y value is not null
//   hasZoomVal - out parameter; true if the zoom value is not null
//   x          - out parameter; the x coordinate, in page coordinates.
//   y          - out parameter; the y coordinate, in page coordinates.
//   zoom       - out parameter; the zoom value.
// Returns TRUE on successfully reading the /XYZ value.
//
// Note the [x, y, zoom] values are only set if the corresponding hasXVal,
// hasYVal or hasZoomVal flags are true.
DLLEXPORT FPDF_BOOL STDCALL FPDFDest_GetLocationInPage(FPDF_DEST dest,
                                                       FPDF_BOOL* hasXCoord,
                                                       FPDF_BOOL* hasYCoord,
                                                       FPDF_BOOL* hasZoom,
                                                       FS_FLOAT* x,
                                                       FS_FLOAT* y,
                                                       FS_FLOAT* zoom);

// Find a link at point (|x|,|y|) on |page|.
//
//   page - handle to the document page.
//   x    - the x coordinate, in the page coordinate system.
//   y    - the y coordinate, in the page coordinate system.
//
// Returns a handle to the link, or NULL if no link found at the given point.
//
// You can convert coordinates from screen coordinates to page coordinates using
// |FPDF_DeviceToPage|.
DLLEXPORT FPDF_LINK STDCALL FPDFLink_GetLinkAtPoint(FPDF_PAGE page,
                                                    double x,
                                                    double y);

// Find the Z-order of link at point (|x|,|y|) on |page|.
//
//   page - handle to the document page.
//   x    - the x coordinate, in the page coordinate system.
//   y    - the y coordinate, in the page coordinate system.
//
// Returns the Z-order of the link, or -1 if no link found at the given point.
// Larger Z-order numbers are closer to the front.
//
// You can convert coordinates from screen coordinates to page coordinates using
// |FPDF_DeviceToPage|.
DLLEXPORT int STDCALL
FPDFLink_GetLinkZOrderAtPoint(FPDF_PAGE page, double x, double y);

// Get destination info for |link|.
//
//   document - handle to the document.
//   link     - handle to the link.
//
// Returns a handle to the destination, or NULL if there is no destination
// associated with the link. In this case, you should call |FPDFLink_GetAction|
// to retrieve the action associated with |link|.
DLLEXPORT FPDF_DEST STDCALL FPDFLink_GetDest(FPDF_DOCUMENT document,
                                             FPDF_LINK link);

// Get action info for |link|.
//
//   link - handle to the link.
//
// Returns a handle to the action associated to |link|, or NULL if no action.
DLLEXPORT FPDF_ACTION STDCALL FPDFLink_GetAction(FPDF_LINK link);

// Enumerates all the link annotations in |page|.
//
//   page      - handle to the page.
//   startPos  - the start position, should initially be 0 and is updated with
//               the next start position on return.
//   linkAnnot - the link handle for |startPos|.
//
// Returns TRUE on success.
DLLEXPORT FPDF_BOOL STDCALL FPDFLink_Enumerate(FPDF_PAGE page,
                                               int* startPos,
                                               FPDF_LINK* linkAnnot);

// Get the rectangle for |linkAnnot|.
//
//   linkAnnot - handle to the link annotation.
//   rect      - the annotation rectangle.
//
// Returns true on success.
DLLEXPORT FPDF_BOOL STDCALL FPDFLink_GetAnnotRect(FPDF_LINK linkAnnot,
                                                  FS_RECTF* rect);

// Get the count of quadrilateral points to the |linkAnnot|.
//
//   linkAnnot - handle to the link annotation.
//
// Returns the count of quadrilateral points.
DLLEXPORT int STDCALL FPDFLink_CountQuadPoints(FPDF_LINK linkAnnot);

// Get the quadrilateral points for the specified |quadIndex| in |linkAnnot|.
//
//   linkAnnot  - handle to the link annotation.
//   quadIndex  - the specified quad point index.
//   quadPoints - receives the quadrilateral points.
//
// Returns true on success.
DLLEXPORT FPDF_BOOL STDCALL FPDFLink_GetQuadPoints(FPDF_LINK linkAnnot,
                                                   int quadIndex,
                                                   FS_QUADPOINTSF* quadPoints);

// Get meta-data |tag| content from |document|.
//
//   document - handle to the document.
//   tag      - the tag to retrieve. The tag can be one of:
//                Title, Author, Subject, Keywords, Creator, Producer,
//                CreationDate, or ModDate.
//              For detailed explanations of these tags and their respective
//              values, please refer to PDF Reference 1.6, section 10.2.1,
//              'Document Information Dictionary'.
//   buffer   - a buffer for the tag. May be NULL.
//   buflen   - the length of the buffer, in bytes. May be 0.
//
// Returns the number of bytes in the tag, including trailing zeros.
//
// The |buffer| is always encoded in UTF-16LE. The |buffer| is followed by two
// bytes of zeros indicating the end of the string.  If |buflen| is less than
// the returned length, or |buffer| is NULL, |buffer| will not be modified.
DLLEXPORT unsigned long STDCALL FPDF_GetMetaText(FPDF_DOCUMENT document,
                                                 FPDF_BYTESTRING tag,
                                                 void* buffer,
                                                 unsigned long buflen);

// Get the page label for |page_index| from |document|.
//
//   document    - handle to the document.
//   page_index  - the 0-based index of the page.
//   buffer      - a buffer for the page label. May be NULL.
//   buflen      - the length of the buffer, in bytes. May be 0.
//
// Returns the number of bytes in the page label, including trailing zeros.
//
// The |buffer| is always encoded in UTF-16LE. The |buffer| is followed by two
// bytes of zeros indicating the end of the string.  If |buflen| is less than
// the returned length, or |buffer| is NULL, |buffer| will not be modified.
DLLEXPORT unsigned long STDCALL FPDF_GetPageLabel(FPDF_DOCUMENT document,
                                                  int page_index,
                                                  void* buffer,
                                                  unsigned long buflen);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // PUBLIC_FPDF_DOC_H_
