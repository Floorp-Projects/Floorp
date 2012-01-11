/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIDeviceContextSpec_h___
#define nsIDeviceContextSpec_h___

#include "nsISupports.h"

class nsIWidget;
class nsIPrintSettings;
class gfxASurface;

#define NS_IDEVICE_CONTEXT_SPEC_IID   \
{ 0x205c614f, 0x39f8, 0x42e1, \
{ 0x92, 0x53, 0x04, 0x9b, 0x48, 0xc3, 0xcb, 0xd8 } }

class nsIDeviceContextSpec : public nsISupports
{
public:
   NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDEVICE_CONTEXT_SPEC_IID)

   /**
    * Initialize the device context spec.
    * @param aWidget         A widget a dialog can be hosted in
    * @param aPrintSettings  Print settings for the print operation
    * @param aIsPrintPreview True if creating Spec for PrintPreview
    * @return NS_OK or a suitable error code.
    */
   NS_IMETHOD Init(nsIWidget *aWidget,
                   nsIPrintSettings* aPrintSettings,
                   bool aIsPrintPreview) = 0;

   NS_IMETHOD GetSurfaceForPrinter(gfxASurface **nativeSurface) = 0;

   NS_IMETHOD BeginDocument(PRUnichar*  aTitle,
                            PRUnichar*  aPrintToFileName,
                            PRInt32     aStartPage,
                            PRInt32     aEndPage) = 0;

   NS_IMETHOD EndDocument() = 0;
   NS_IMETHOD BeginPage() = 0;
   NS_IMETHOD EndPage() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDeviceContextSpec,
                              NS_IDEVICE_CONTEXT_SPEC_IID)

/* error codes for printer device contexts */
#define NS_ERROR_GFX_PRINTER_BASE (1) /* adjustable :-) */
/* Unix: print command (lp/lpr) not found */
#define NS_ERROR_GFX_PRINTER_CMD_NOT_FOUND          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+1)
/* Unix: print command returned an error */
#define NS_ERROR_GFX_PRINTER_CMD_FAILURE            \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+2)
/* no printer available (e.g. cannot find _any_ printer) */
#define NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE  \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+3)
/* _specified_ (by name) printer not found */
#define NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND         \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+4)
/* access to printer denied */
#define NS_ERROR_GFX_PRINTER_ACCESS_DENIED          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+5)
/* invalid printer attribute (for example: unsupported paper size etc.) */
#define NS_ERROR_GFX_PRINTER_INVALID_ATTRIBUTE      \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+6)
/* printer not "ready" (offline ?) */
#define NS_ERROR_GFX_PRINTER_PRINTER_NOT_READY     \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+8)
/* printer out of paper */
#define NS_ERROR_GFX_PRINTER_OUT_OF_PAPER           \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+9)
/* generic printer I/O error */
#define NS_ERROR_GFX_PRINTER_PRINTER_IO_ERROR       \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+10)
/* print-to-file: could not open output file */
#define NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE    \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+11)
/* print-to-file: I/O error while printing to file */
#define NS_ERROR_GFX_PRINTER_FILE_IO_ERROR          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+12)
/* print preview: needs at least one printer */
#define NS_ERROR_GFX_PRINTER_PRINTPREVIEW          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+13)
/* print: starting document */
#define NS_ERROR_GFX_PRINTER_STARTDOC          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+14)
/* print: ending document */
#define NS_ERROR_GFX_PRINTER_ENDDOC          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+15)
/* print: starting page */
#define NS_ERROR_GFX_PRINTER_STARTPAGE          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+16)
/* print: ending page */
#define NS_ERROR_GFX_PRINTER_ENDPAGE          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+17)
/* print: print while in print preview */
#define NS_ERROR_GFX_PRINTER_PRINT_WHILE_PREVIEW          \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+18)
/* requested page size not supported by printer */
#define NS_ERROR_GFX_PRINTER_PAPER_SIZE_NOT_SUPPORTED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+19)
/* requested page orientation not supported */
#define NS_ERROR_GFX_PRINTER_ORIENTATION_NOT_SUPPORTED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+20)
/* requested colorspace not supported (like printing "color" on a
   "grayscale"-only printer) */
#define NS_ERROR_GFX_PRINTER_COLORSPACE_NOT_SUPPORTED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+21)
/* too many copies requested */
#define NS_ERROR_GFX_PRINTER_TOO_MANY_COPIES \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+22)
/* driver configuration error */
#define NS_ERROR_GFX_PRINTER_DRIVER_CONFIGURATION_ERROR \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+23)
/* The document is still being loaded, can't Print Preview */
#define NS_ERROR_GFX_PRINTER_DOC_IS_BUSY_PP \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+24)
/* The document was asked to be destroyed while we were preparing printing */
#define NS_ERROR_GFX_PRINTER_DOC_WAS_DESTORYED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+25)
/* Cannot Print or Print Preview XUL Documents */
#define NS_ERROR_GFX_PRINTER_NO_XUL \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+26)
/* The toolkit no longer supports the Print Dialog (for embedders) */
#define NS_ERROR_GFX_NO_PRINTDIALOG_IN_TOOLKIT \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+27)
/* The was wasn't any Print Prompt service registered (this shouldn't happen) */
#define NS_ERROR_GFX_NO_PRINTROMPTSERVICE \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+28)
/* requested plex mode not supported by printer */
#define NS_ERROR_GFX_PRINTER_PLEX_NOT_SUPPORTED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+29)
/* The document is still being loaded */
#define NS_ERROR_GFX_PRINTER_DOC_IS_BUSY \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+30)
/* Printing is not implemented */
#define NS_ERROR_GFX_PRINTING_NOT_IMPLEMENTED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+31)
/* Cannot load the matching print module */
#define NS_ERROR_GFX_COULD_NOT_LOAD_PRINT_MODULE \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+32)
/* requested resolution/quality mode not supported by printer */
#define NS_ERROR_GFX_PRINTER_RESOLUTION_NOT_SUPPORTED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_GFX,NS_ERROR_GFX_PRINTER_BASE+33)

#endif
