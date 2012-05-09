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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Ken Herron <kherron+mozilla@fmailbox.com>
 *   Julien Lafon <julien.lafon@gmail.com>
 *   Michael Ventnor <m.ventnor@gmail.com>
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
 
/* Store per-printer features in temp. prefs vars that the
 * print dialog can pick them up... */
#define SET_PRINTER_FEATURES_VIA_PREFS 1 
#define PRINTERFEATURES_PREF "print.tmp.printerfeatures"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1 /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#include "plstr.h"

#include "nsDeviceContextSpecG.h"

#include "prenv.h" /* for PR_GetEnv */

#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsStringEnumerator.h"
#include "nsIServiceManager.h" 

#include "nsPSPrinters.h"
#include "nsPaperPS.h"  /* Paper size list */

#include "nsPrintSettingsGTK.h"

#include "nsIFileStreams.h"
#include "nsILocalFile.h"
#include "nsTArray.h"

#include "mozilla/Preferences.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace mozilla;

#ifdef PR_LOGGING 
static PRLogModuleInfo *DeviceContextSpecGTKLM = PR_NewLogModule("DeviceContextSpecGTK");
#endif /* PR_LOGGING */
/* Macro to make lines shorter */
#define DO_PR_DEBUG_LOG(x) PR_LOG(DeviceContextSpecGTKLM, PR_LOG_DEBUG, x)

//----------------------------------------------------------------------------------
// The printer data is shared between the PrinterEnumerator and the nsDeviceContextSpecGTK
// The PrinterEnumerator creates the printer info
// but the nsDeviceContextSpecGTK cleans it up
// If it gets created (via the Page Setup Dialog) but the user never prints anything
// then it will never be delete, so this class takes care of that.
class GlobalPrinters {
public:
  static GlobalPrinters* GetInstance()   { return &mGlobalPrinters; }
  ~GlobalPrinters()                      { FreeGlobalPrinters(); }

  void      FreeGlobalPrinters();
  nsresult  InitializeGlobalPrinters();

  bool      PrintersAreAllocated()       { return mGlobalPrinterList != nsnull; }
  PRUint32  GetNumPrinters()
    { return mGlobalPrinterList ? mGlobalPrinterList->Length() : 0; }
  nsString* GetStringAt(PRInt32 aInx)    { return &mGlobalPrinterList->ElementAt(aInx); }
  void      GetDefaultPrinterName(PRUnichar **aDefaultPrinterName);

protected:
  GlobalPrinters() {}

  static GlobalPrinters mGlobalPrinters;
  static nsTArray<nsString>* mGlobalPrinterList;
};

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
/* "Prototype" for the new nsPrinterFeatures service */
class nsPrinterFeatures {
public:
  nsPrinterFeatures( const char *printername );
  ~nsPrinterFeatures() {}

  /* Does this printer allow to set/change the paper size ? */
  void SetCanChangePaperSize( bool aCanSetPaperSize );
  /* Does this Mozilla print module allow set/change the paper size ? */
  void SetSupportsPaperSizeChange( bool aSupportsPaperSizeChange );
  /* Set number of paper size records and the records itself */
  void SetNumPaperSizeRecords( PRInt32 aCount );
  void SetPaperRecord( PRInt32 aIndex, const char *aName, PRInt32 aWidthMM, PRInt32 aHeightMM, bool aIsInch );

  /* Does this printer allow to set/change the content orientation ? */
  void SetCanChangeOrientation( bool aCanSetOrientation );
  /* Does this Mozilla print module allow set/change the content orientation ? */
  void SetSupportsOrientationChange( bool aSupportsOrientationChange );
  /* Set number of orientation records and the records itself */
  void SetNumOrientationRecords( PRInt32 aCount );
  void SetOrientationRecord( PRInt32 aIndex, const char *aName );

  /* Does this printer allow to set/change the plex mode ? */
  void SetCanChangePlex( bool aCanSetPlex );
  /* Does this Mozilla print module allow set/change the plex mode ? */
  void SetSupportsPlexChange( bool aSupportsPlexChange );
  /* Set number of plex records and the records itself */
  void SetNumPlexRecords( PRInt32 aCount );
  void SetPlexRecord( PRInt32 aIndex, const char *aName );

  /* Does this printer allow to set/change the resolution name ? */
  void SetCanChangeResolutionName( bool aCanSetResolutionName );
  /* Does this Mozilla print module allow set/change the resolution name ? */
  void SetSupportsResolutionNameChange( bool aSupportsResolutionChange );
  /* Set number of resolution records and the records itself */
  void SetNumResolutionNameRecords( PRInt32 aCount );
  void SetResolutionNameRecord( PRInt32 aIndex, const char *aName );

  /* Does this printer allow to set/change the colorspace ? */
  void SetCanChangeColorspace( bool aCanSetColorspace );
  /* Does this Mozilla print module allow set/change the colorspace ? */
  void SetSupportsColorspaceChange( bool aSupportsColorspace );
  /* Set number of colorspace records and the records itself */
  void SetNumColorspaceRecords( PRInt32 aCount );
  void SetColorspaceRecord( PRInt32 aIndex, const char *aName );

  /* Does this device allow to set/change the usage of the internal grayscale mode ? */
  void SetCanChangePrintInColor( bool aCanSetPrintInColor );
  /* Does this printer allow to set/change the usage of the internal grayscale mode ? */
  void SetSupportsPrintInColorChange( bool aSupportPrintInColorChange );

  /* Does this device allow to set/change the usage of font download to the printer? */
  void SetCanChangeDownloadFonts( bool aCanSetDownloadFonts );
  /* Does this printer allow to set/change the usage of font download to the printer? */
  void SetSupportsDownloadFontsChange( bool aSupportDownloadFontsChange );

  /* Does this device allow to set/change the job title ? */
  void SetCanChangeJobTitle( bool aCanSetJobTitle );
  /* Does this printer allow to set/change the job title ? */
  void SetSupportsJobTitleChange( bool aSupportJobTitleChange );
    
  /* Does this device allow to set/change the spooler command ? */
  void SetCanChangeSpoolerCommand( bool aCanSetSpoolerCommand );
  /* Does this printer allow to set/change the spooler command ? */
  void SetSupportsSpoolerCommandChange( bool aSupportSpoolerCommandChange );
  
  /* Does this device allow to set/change number of copies for an document ? */
  void SetCanChangeNumCopies( bool aCanSetNumCopies );

private:
  /* private helper methods */
  void SetBoolValue( const char *tagname, bool value );
  void SetIntValue(  const char *tagname, PRInt32 value );
  void SetCharValue(  const char *tagname, const char *value );

  nsXPIDLCString          mPrinterName;
};

void nsPrinterFeatures::SetBoolValue( const char *tagname, bool value )
{
  nsPrintfCString prefName(PRINTERFEATURES_PREF ".%s.%s",
                           mPrinterName.get(), tagname);
  Preferences::SetBool(prefName.get(), value);
}

void nsPrinterFeatures::SetIntValue(  const char *tagname, PRInt32 value )
{
  nsPrintfCString prefName(PRINTERFEATURES_PREF ".%s.%s",
                           mPrinterName.get(), tagname);
  Preferences::SetInt(prefName.get(), value);
}

void nsPrinterFeatures::SetCharValue(  const char *tagname, const char *value )
{
  nsPrintfCString prefName(PRINTERFEATURES_PREF ".%s.%s",
                           mPrinterName.get(), tagname);
  Preferences::SetCString(prefName.get(), value);
}

nsPrinterFeatures::nsPrinterFeatures( const char *printername )
{
  DO_PR_DEBUG_LOG(("nsPrinterFeatures::nsPrinterFeatures('%s')\n", printername));
  mPrinterName.Assign(printername);

  SetBoolValue("has_special_printerfeatures", true);
}

void nsPrinterFeatures::SetCanChangePaperSize( bool aCanSetPaperSize )
{
  SetBoolValue("can_change_paper_size", aCanSetPaperSize);
}

void nsPrinterFeatures::SetSupportsPaperSizeChange( bool aSupportsPaperSizeChange )
{
  SetBoolValue("supports_paper_size_change", aSupportsPaperSizeChange);
}

/* Set number of paper size records and the records itself */
void nsPrinterFeatures::SetNumPaperSizeRecords( PRInt32 aCount )
{
  SetIntValue("paper.count", aCount);          
}

void nsPrinterFeatures::SetPaperRecord(PRInt32 aIndex, const char *aPaperName, PRInt32 aWidthMM, PRInt32 aHeightMM, bool aIsInch)
{
  SetCharValue(nsPrintfCString("paper.%d.name",      aIndex).get(), aPaperName);
  SetIntValue( nsPrintfCString("paper.%d.width_mm",  aIndex).get(), aWidthMM);
  SetIntValue( nsPrintfCString("paper.%d.height_mm", aIndex).get(), aHeightMM);
  SetBoolValue(nsPrintfCString("paper.%d.is_inch",   aIndex).get(), aIsInch);
}

void nsPrinterFeatures::SetCanChangeOrientation( bool aCanSetOrientation )
{
  SetBoolValue("can_change_orientation", aCanSetOrientation);
}

void nsPrinterFeatures::SetSupportsOrientationChange( bool aSupportsOrientationChange )
{
  SetBoolValue("supports_orientation_change", aSupportsOrientationChange);
}

void nsPrinterFeatures::SetNumOrientationRecords( PRInt32 aCount )
{
  SetIntValue("orientation.count", aCount);          
}

void nsPrinterFeatures::SetOrientationRecord( PRInt32 aIndex, const char *aOrientationName )
{
  SetCharValue(nsPrintfCString("orientation.%d.name", aIndex).get(), aOrientationName);
}

void nsPrinterFeatures::SetCanChangePlex( bool aCanSetPlex )
{
  SetBoolValue("can_change_plex", aCanSetPlex);
}

void nsPrinterFeatures::SetSupportsPlexChange( bool aSupportsPlexChange )
{
  SetBoolValue("supports_plex_change", aSupportsPlexChange);
}

void nsPrinterFeatures::SetNumPlexRecords( PRInt32 aCount )
{
  SetIntValue("plex.count", aCount);          
}

void nsPrinterFeatures::SetPlexRecord( PRInt32 aIndex, const char *aPlexName )
{
  SetCharValue(nsPrintfCString("plex.%d.name", aIndex).get(), aPlexName);
}

void nsPrinterFeatures::SetCanChangeResolutionName( bool aCanSetResolutionName )
{
  SetBoolValue("can_change_resolution", aCanSetResolutionName);
}

void nsPrinterFeatures::SetSupportsResolutionNameChange( bool aSupportsResolutionNameChange )
{
  SetBoolValue("supports_resolution_change", aSupportsResolutionNameChange);
}

void nsPrinterFeatures::SetNumResolutionNameRecords( PRInt32 aCount )
{
  SetIntValue("resolution.count", aCount);          
}

void nsPrinterFeatures::SetResolutionNameRecord( PRInt32 aIndex, const char *aResolutionName )
{
  SetCharValue(nsPrintfCString("resolution.%d.name", aIndex).get(), aResolutionName);
}

void nsPrinterFeatures::SetCanChangeColorspace( bool aCanSetColorspace )
{
  SetBoolValue("can_change_colorspace", aCanSetColorspace);
}

void nsPrinterFeatures::SetSupportsColorspaceChange( bool aSupportsColorspaceChange )
{
  SetBoolValue("supports_colorspace_change", aSupportsColorspaceChange);
}

void nsPrinterFeatures::SetNumColorspaceRecords( PRInt32 aCount )
{
  SetIntValue("colorspace.count", aCount);          
}

void nsPrinterFeatures::SetColorspaceRecord( PRInt32 aIndex, const char *aColorspace )
{
  SetCharValue(nsPrintfCString("colorspace.%d.name", aIndex).get(), aColorspace);
}

void nsPrinterFeatures::SetCanChangeDownloadFonts( bool aCanSetDownloadFonts )
{
  SetBoolValue("can_change_downloadfonts", aCanSetDownloadFonts);
}

void nsPrinterFeatures::SetSupportsDownloadFontsChange( bool aSupportDownloadFontsChange )
{
  SetBoolValue("supports_downloadfonts_change", aSupportDownloadFontsChange);
}

void nsPrinterFeatures::SetCanChangePrintInColor( bool aCanSetPrintInColor )
{
  SetBoolValue("can_change_printincolor", aCanSetPrintInColor);
}

void nsPrinterFeatures::SetSupportsPrintInColorChange( bool aSupportPrintInColorChange )
{
  SetBoolValue("supports_printincolor_change", aSupportPrintInColorChange);
}

void nsPrinterFeatures::SetCanChangeSpoolerCommand( bool aCanSetSpoolerCommand )
{
  SetBoolValue("can_change_spoolercommand", aCanSetSpoolerCommand);
}

void nsPrinterFeatures::SetSupportsSpoolerCommandChange( bool aSupportSpoolerCommandChange )
{
  SetBoolValue("supports_spoolercommand_change", aSupportSpoolerCommandChange);
}

void nsPrinterFeatures::SetCanChangeJobTitle( bool aCanSetJobTitle )
{
  SetBoolValue("can_change_jobtitle", aCanSetJobTitle);
}

void nsPrinterFeatures::SetSupportsJobTitleChange( bool aSupportsJobTitle )
{
  SetBoolValue("supports_jobtitle_change", aSupportsJobTitle);
}

void nsPrinterFeatures::SetCanChangeNumCopies( bool aCanSetNumCopies )
{
  SetBoolValue("can_change_num_copies", aCanSetNumCopies);
}

#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

//---------------
// static members
GlobalPrinters GlobalPrinters::mGlobalPrinters;
nsTArray<nsString>* GlobalPrinters::mGlobalPrinterList = nsnull;
//---------------

nsDeviceContextSpecGTK::nsDeviceContextSpecGTK()
  : mPrintJob(NULL)
  , mGtkPrinter(NULL)
  , mGtkPrintSettings(NULL)
  , mGtkPageSetup(NULL)
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecGTK::nsDeviceContextSpecGTK()\n"));
}

nsDeviceContextSpecGTK::~nsDeviceContextSpecGTK()
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecGTK::~nsDeviceContextSpecGTK()\n"));

  if (mGtkPageSetup) {
    g_object_unref(mGtkPageSetup);
  }

  if (mGtkPrintSettings) {
    g_object_unref(mGtkPrintSettings);
  }
}

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecGTK,
                   nsIDeviceContextSpec)

#include "gfxPDFSurface.h"
#include "gfxPSSurface.h"
NS_IMETHODIMP nsDeviceContextSpecGTK::GetSurfaceForPrinter(gfxASurface **aSurface)
{
  *aSurface = nsnull;

  const char *path;
  GetPath(&path);

  double width, height;
  mPrintSettings->GetEffectivePageSize(&width, &height);

  // convert twips to points
  width  /= TWIPS_PER_POINT_FLOAT;
  height /= TWIPS_PER_POINT_FLOAT;

  DO_PR_DEBUG_LOG(("\"%s\", %f, %f\n", path, width, height));
  nsresult rv;

  // Spool file. Use Glib's temporary file function since we're
  // already dependent on the gtk software stack.
  gchar *buf;
  gint fd = g_file_open_tmp("XXXXXX.tmp", &buf, nsnull);
  if (-1 == fd)
    return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
  close(fd);

  rv = NS_NewNativeLocalFile(nsDependentCString(buf), false,
                             getter_AddRefs(mSpoolFile));
  if (NS_FAILED(rv)) {
    unlink(buf);
    return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;
  }

  mSpoolName = buf;
  g_free(buf);

  mSpoolFile->SetPermissions(0600);

  nsCOMPtr<nsIFileOutputStream> stream = do_CreateInstance("@mozilla.org/network/file-output-stream;1");
  rv = stream->Init(mSpoolFile, -1, -1, 0);
  if (NS_FAILED(rv))
    return rv;

  PRInt16 format;
  mPrintSettings->GetOutputFormat(&format);

  nsRefPtr<gfxASurface> surface;
  gfxSize surfaceSize(width, height);

  // Determine the real format with some GTK magic
  if (format == nsIPrintSettings::kOutputFormatNative) {
    if (mIsPPreview) {
      // There is nothing to detect on Print Preview, use PS.
      format = nsIPrintSettings::kOutputFormatPS;
    } else {
      const gchar* fmtGTK = gtk_print_settings_get(mGtkPrintSettings, GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT);
      if (!fmtGTK && GTK_IS_PRINTER(mGtkPrinter)) {
        // Likely not print-to-file, check printer's capabilities

        // Prior to gtk 2.24, gtk_printer_accepts_pdf() and
        // gtk_printer_accepts_ps() always returned true regardless of the
        // printer's capability.
        if (gtk_major_version > 2 ||
            (gtk_major_version == 2 && gtk_minor_version >= 24)) {
          format =
            gtk_printer_accepts_pdf(mGtkPrinter) ?
            static_cast<PRInt16>(nsIPrintSettings::kOutputFormatPDF) :
            static_cast<PRInt16>(nsIPrintSettings::kOutputFormatPS);
        } else {
          format = nsIPrintSettings::kOutputFormatPS;
        }

      } else if (nsDependentCString(fmtGTK).EqualsIgnoreCase("pdf")) {
        format = nsIPrintSettings::kOutputFormatPDF;
      } else {
        format = nsIPrintSettings::kOutputFormatPS;
      }
    }
  }

  if (format == nsIPrintSettings::kOutputFormatPDF) {
    surface = new gfxPDFSurface(stream, surfaceSize);
  } else {
    PRInt32 orientation;
    mPrintSettings->GetOrientation(&orientation);
    if (nsIPrintSettings::kPortraitOrientation == orientation) {
      surface = new gfxPSSurface(stream, surfaceSize, gfxPSSurface::PORTRAIT);
    } else {
      surface = new gfxPSSurface(stream, surfaceSize, gfxPSSurface::LANDSCAPE);
    }
  }

  if (!surface)
    return NS_ERROR_OUT_OF_MEMORY;

  surface.swap(*aSurface);

  return NS_OK;
}

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecGTK
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 */
NS_IMETHODIMP nsDeviceContextSpecGTK::Init(nsIWidget *aWidget,
                                           nsIPrintSettings* aPS,
                                           bool aIsPrintPreview)
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecGTK::Init(aPS=%p)\n", aPS));

  if (gtk_major_version < 2 ||
      (gtk_major_version == 2 && gtk_minor_version < 10))
    return NS_ERROR_NOT_AVAILABLE;  // I'm so sorry bz

  mPrintSettings = aPS;
  mIsPPreview = aIsPrintPreview;

  // This is only set by embedders
  bool toFile;
  aPS->GetPrintToFile(&toFile);

  mToPrinter = !toFile && !aIsPrintPreview;

  nsCOMPtr<nsPrintSettingsGTK> printSettingsGTK(do_QueryInterface(aPS));
  if (!printSettingsGTK)
    return NS_ERROR_NO_INTERFACE;

  mGtkPrinter = printSettingsGTK->GetGtkPrinter();
  mGtkPrintSettings = printSettingsGTK->GetGtkPrintSettings();
  mGtkPageSetup = printSettingsGTK->GetGtkPageSetup();

  // This is a horrible workaround for some printer driver bugs that treat custom page sizes different
  // to standard ones. If our paper object matches one of a standard one, use a standard paper size
  // object instead. See bug 414314 for more info.
  GtkPaperSize* geckosHackishPaperSize = gtk_page_setup_get_paper_size(mGtkPageSetup);
  GtkPaperSize* standardGtkPaperSize = gtk_paper_size_new(gtk_paper_size_get_name(geckosHackishPaperSize));

  mGtkPageSetup = gtk_page_setup_copy(mGtkPageSetup);
  mGtkPrintSettings = gtk_print_settings_copy(mGtkPrintSettings);

  GtkPaperSize* properPaperSize;
  if (gtk_paper_size_is_equal(geckosHackishPaperSize, standardGtkPaperSize)) {
    properPaperSize = standardGtkPaperSize;
  } else {
    properPaperSize = geckosHackishPaperSize;
  }
  gtk_print_settings_set_paper_size(mGtkPrintSettings, properPaperSize);
  gtk_page_setup_set_paper_size_and_default_margins(mGtkPageSetup, properPaperSize);
  gtk_paper_size_free(standardGtkPaperSize);

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetPath(const char **aPath)      
{
  *aPath = mPath;
  return NS_OK;
}

/* static !! */
nsresult nsDeviceContextSpecGTK::GetPrintMethod(const char *aPrinter, PrintMethod &aMethod)
{
  aMethod = pmPostScript;
  return NS_OK;
}

static void
print_callback(GtkPrintJob *aJob, gpointer aData, GError *aError) {
  g_object_unref(aJob);
  ((nsILocalFile*) aData)->Remove(false);
}

static void
ns_release_macro(gpointer aData) {
  nsILocalFile* spoolFile = (nsILocalFile*) aData;
  NS_RELEASE(spoolFile);
}

NS_IMETHODIMP nsDeviceContextSpecGTK::BeginDocument(PRUnichar * aTitle, PRUnichar * aPrintToFileName,
                                                    PRInt32 aStartPage, PRInt32 aEndPage)
{
  if (mToPrinter) {
    if (!GTK_IS_PRINTER(mGtkPrinter))
      return NS_ERROR_FAILURE;

    mPrintJob = gtk_print_job_new(NS_ConvertUTF16toUTF8(aTitle).get(), mGtkPrinter,
                                  mGtkPrintSettings, mGtkPageSetup);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::EndDocument()
{
  if (mToPrinter) {
    if (!mPrintJob)
      return NS_OK; // The operation was aborted.

    if (!gtk_print_job_set_source_file(mPrintJob, mSpoolName.get(), nsnull))
      return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;

    NS_ADDREF(mSpoolFile.get());
    gtk_print_job_send(mPrintJob, print_callback, mSpoolFile, ns_release_macro);
  } else {
    // Handle print-to-file ourselves for the benefit of embedders
    nsXPIDLString targetPath;
    nsCOMPtr<nsILocalFile> destFile;
    mPrintSettings->GetToFileName(getter_Copies(targetPath));

    nsresult rv = NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(targetPath),
                                        false, getter_AddRefs(destFile));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString destLeafName;
    rv = destFile->GetLeafName(destLeafName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> destDir;
    rv = destFile->GetParent(getter_AddRefs(destDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mSpoolFile->MoveTo(destDir, destLeafName);
    NS_ENSURE_SUCCESS(rv, rv);

    // This is the standard way to get the UNIX umask. Ugh.
    mode_t mask = umask(0);
    umask(mask);
    // If you're not familiar with umasks, they contain the bits of what NOT to set in the permissions
    // (thats because files and directories have different numbers of bits for their permissions)
    destFile->SetPermissions(0666 & ~(mask));
  }
  return NS_OK;
}

/* Get prefs for printer
 * Search order:
 * - Get prefs per printer name and module name
 * - Get prefs per printer name
 * - Get prefs per module name
 * - Get prefs
 */
static
nsresult CopyPrinterCharPref(const char *modulename, const char *printername,
                             const char *prefname, nsCString &return_buf)
{
  DO_PR_DEBUG_LOG(("CopyPrinterCharPref('%s', '%s', '%s')\n", modulename, printername, prefname));

  nsresult rv = NS_ERROR_FAILURE;
 
  if (printername && modulename) {
    /* Get prefs per printer name and module name */
    nsPrintfCString name("print.%s.printer_%s.%s", modulename, printername, prefname);
    DO_PR_DEBUG_LOG(("trying to get '%s'\n", name.get()));
    rv = Preferences::GetCString(name.get(), &return_buf);
  }
  
  if (NS_FAILED(rv)) { 
    if (printername) {
      /* Get prefs per printer name */
      nsPrintfCString name("print.printer_%s.%s", printername, prefname);
      DO_PR_DEBUG_LOG(("trying to get '%s'\n", name.get()));
      rv = Preferences::GetCString(name.get(), &return_buf);
    }

    if (NS_FAILED(rv)) {
      if (modulename) {
        /* Get prefs per module name */
        nsPrintfCString name("print.%s.%s", modulename, prefname);
        DO_PR_DEBUG_LOG(("trying to get '%s'\n", name.get()));
        rv = Preferences::GetCString(name.get(), &return_buf);
      }
      
      if (NS_FAILED(rv)) {
        /* Get prefs */
        nsPrintfCString name("print.%s", prefname);
        DO_PR_DEBUG_LOG(("trying to get '%s'\n", name.get()));
        rv = Preferences::GetCString(name.get(), &return_buf);
      }
    }
  }

#ifdef PR_LOG  
  if (NS_SUCCEEDED(rv)) {
    DO_PR_DEBUG_LOG(("CopyPrinterCharPref returning '%s'.\n", return_buf.get()));
  }
  else
  {
    DO_PR_DEBUG_LOG(("CopyPrinterCharPref failure.\n"));
  }
#endif /* PR_LOG */

  return rv;
}

//  Printer Enumerator
nsPrinterEnumeratorGTK::nsPrinterEnumeratorGTK()
{
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorGTK, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorGTK::GetPrinterNameList(nsIStringEnumerator **aPrinterNameList)
{
  NS_ENSURE_ARG_POINTER(aPrinterNameList);
  *aPrinterNameList = nsnull;
  
  nsresult rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUint32 numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();
  nsTArray<nsString> *printers = new nsTArray<nsString>(numPrinters);
  if (!printers) {
    GlobalPrinters::GetInstance()->FreeGlobalPrinters();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  PRUint32 count = 0;
  while( count < numPrinters )
  {
    printers->AppendElement(*GlobalPrinters::GetInstance()->GetStringAt(count++));
  }
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();

  return NS_NewAdoptingStringEnumerator(aPrinterNameList, printers);
}

/* readonly attribute wstring defaultPrinterName; */
NS_IMETHODIMP nsPrinterEnumeratorGTK::GetDefaultPrinterName(PRUnichar **aDefaultPrinterName)
{
  DO_PR_DEBUG_LOG(("nsPrinterEnumeratorGTK::GetDefaultPrinterName()\n"));
  NS_ENSURE_ARG_POINTER(aDefaultPrinterName);

  GlobalPrinters::GetInstance()->GetDefaultPrinterName(aDefaultPrinterName);

  DO_PR_DEBUG_LOG(("GetDefaultPrinterName(): default printer='%s'.\n", NS_ConvertUTF16toUTF8(*aDefaultPrinterName).get()));
  return NS_OK;
}

/* void initPrintSettingsFromPrinter (in wstring aPrinterName, in nsIPrintSettings aPrintSettings); */
NS_IMETHODIMP nsPrinterEnumeratorGTK::InitPrintSettingsFromPrinter(const PRUnichar *aPrinterName, nsIPrintSettings *aPrintSettings)
{
  DO_PR_DEBUG_LOG(("nsPrinterEnumeratorGTK::InitPrintSettingsFromPrinter()"));
  nsresult rv;

  NS_ENSURE_ARG_POINTER(aPrinterName);
  NS_ENSURE_ARG_POINTER(aPrintSettings);
  
  NS_ENSURE_TRUE(*aPrinterName, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(aPrintSettings, NS_ERROR_FAILURE);

  nsXPIDLCString fullPrinterName, /* Full name of printer incl. driver-specific prefix */ 
                 printerName;     /* "Stripped" name of printer */
  fullPrinterName.Assign(NS_ConvertUTF16toUTF8(aPrinterName));
  printerName.Assign(NS_ConvertUTF16toUTF8(aPrinterName));
  DO_PR_DEBUG_LOG(("printerName='%s'\n", printerName.get()));
  
  PrintMethod type = pmInvalid;
  rv = nsDeviceContextSpecGTK::GetPrintMethod(printerName, type);
  if (NS_FAILED(rv))
    return rv;

  /* "Demangle" postscript printer name */
  if (type == pmPostScript) {
    /* Strip the printing method name from the printer,
     * e.g. turn "PostScript/foobar" to "foobar" */
    PRInt32 slash = printerName.FindChar('/');
    if (kNotFound != slash)
      printerName.Cut(0, slash + 1);
  }

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
  /* Defaults to FALSE */
  nsPrintfCString  prefName(
    PRINTERFEATURES_PREF ".%s.has_special_printerfeatures",
    fullPrinterName.get());
  Preferences::SetBool(prefName.get(), false);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

  
  /* Set filename */
  nsCAutoString filename;
  if (NS_FAILED(CopyPrinterCharPref(nsnull, printerName, "filename", filename))) {
    const char *path;
  
    if (!(path = PR_GetEnv("PWD")))
      path = PR_GetEnv("HOME");
  
    if (path)
      filename = nsPrintfCString("%s/mozilla.pdf", path);
    else
      filename.AssignLiteral("mozilla.pdf");
  }  
  DO_PR_DEBUG_LOG(("Setting default filename to '%s'\n", filename.get()));
  aPrintSettings->SetToFileName(NS_ConvertUTF8toUTF16(filename).get());

  aPrintSettings->SetIsInitializedFromPrinter(true);

  if (type == pmPostScript) {
    DO_PR_DEBUG_LOG(("InitPrintSettingsFromPrinter() for PostScript printer\n"));

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    nsPrinterFeatures printerFeatures(fullPrinterName);

    printerFeatures.SetSupportsPaperSizeChange(true);
    printerFeatures.SetSupportsOrientationChange(true);
    printerFeatures.SetSupportsPlexChange(false);
    printerFeatures.SetSupportsResolutionNameChange(false);
    printerFeatures.SetSupportsColorspaceChange(false);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */ 
      
#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetCanChangeOrientation(true);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    nsCAutoString orientation;
    if (NS_SUCCEEDED(CopyPrinterCharPref("postscript", printerName,
                                         "orientation", orientation))) {
      if (orientation.LowerCaseEqualsLiteral("portrait")) {
        DO_PR_DEBUG_LOG(("setting default orientation to 'portrait'\n"));
        aPrintSettings->SetOrientation(nsIPrintSettings::kPortraitOrientation);
      }
      else if (orientation.LowerCaseEqualsLiteral("landscape")) {
        DO_PR_DEBUG_LOG(("setting default orientation to 'landscape'\n"));
        aPrintSettings->SetOrientation(nsIPrintSettings::kLandscapeOrientation);  
      }
      else {
        DO_PR_DEBUG_LOG(("Unknown default orientation '%s'\n", orientation.get()));
      }
    }

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetOrientationRecord(0, "portrait");
    printerFeatures.SetOrientationRecord(1, "landscape");
    printerFeatures.SetNumOrientationRecords(2);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    /* PostScript module does not support changing the plex mode... */
#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetCanChangePlex(false);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */
    DO_PR_DEBUG_LOG(("setting default plex to '%s'\n", "default"));
    aPrintSettings->SetPlexName(NS_LITERAL_STRING("default").get());
#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetPlexRecord(0, "default");
    printerFeatures.SetNumPlexRecords(1);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    /* PostScript module does not support changing the resolution mode... */
#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetCanChangeResolutionName(false);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */
    DO_PR_DEBUG_LOG(("setting default resolution to '%s'\n", "default"));
    aPrintSettings->SetResolutionName(NS_LITERAL_STRING("default").get());
#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetResolutionNameRecord(0, "default");
    printerFeatures.SetNumResolutionNameRecords(1);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    /* PostScript module does not support changing the colorspace... */
#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetCanChangeColorspace(false);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */
    DO_PR_DEBUG_LOG(("setting default colorspace to '%s'\n", "default"));
    aPrintSettings->SetColorspace(NS_LITERAL_STRING("default").get());
#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetColorspaceRecord(0, "default");
    printerFeatures.SetNumColorspaceRecords(1);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */   

#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetCanChangePaperSize(true);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */
    nsCAutoString papername;
    if (NS_SUCCEEDED(CopyPrinterCharPref("postscript", printerName,
                                         "paper_size", papername))) {
      nsPaperSizePS paper;

      if (paper.Find(papername.get())) {
        DO_PR_DEBUG_LOG(("setting default paper size to '%s' (%g mm/%g mm)\n",
              paper.Name(), paper.Width_mm(), paper.Height_mm()));
	aPrintSettings->SetPaperSizeUnit(nsIPrintSettings::kPaperSizeMillimeters);
        aPrintSettings->SetPaperWidth(paper.Width_mm());
        aPrintSettings->SetPaperHeight(paper.Height_mm());
        aPrintSettings->SetPaperName(NS_ConvertASCIItoUTF16(paper.Name()).get());
      }
      else {
        DO_PR_DEBUG_LOG(("Unknown paper size '%s' given.\n", papername.get()));
      }
#ifdef SET_PRINTER_FEATURES_VIA_PREFS
      paper.First();
      int count = 0;
      while (!paper.AtEnd())
      {
        printerFeatures.SetPaperRecord(count++, paper.Name(),
            (int)paper.Width_mm(), (int)paper.Height_mm(), !paper.IsMetric());
        paper.Next();
      }
      printerFeatures.SetNumPaperSizeRecords(count);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */
    }

    bool hasSpoolerCmd = (nsPSPrinterList::kTypePS ==
        nsPSPrinterList::GetPrinterType(fullPrinterName));
#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetSupportsSpoolerCommandChange(hasSpoolerCmd);
    printerFeatures.SetCanChangeSpoolerCommand(hasSpoolerCmd);

    /* Postscript module does not pass the job title to lpr */
    printerFeatures.SetSupportsJobTitleChange(false);
    printerFeatures.SetCanChangeJobTitle(false);
    /* Postscript module has no control over builtin fonts yet */
    printerFeatures.SetSupportsDownloadFontsChange(false);
    printerFeatures.SetCanChangeDownloadFonts(false);
    /* Postscript module does not support multiple colorspaces
     * so it has to use the old way */
    printerFeatures.SetSupportsPrintInColorChange(true);
    printerFeatures.SetCanChangePrintInColor(true);
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    if (hasSpoolerCmd) {
      nsCAutoString command;
      if (NS_SUCCEEDED(CopyPrinterCharPref("postscript",
            printerName, "print_command", command))) {
        DO_PR_DEBUG_LOG(("setting default print command to '%s'\n",
            command.get()));
        aPrintSettings->SetPrintCommand(NS_ConvertUTF8toUTF16(command).get());
      }
    }
    
#ifdef SET_PRINTER_FEATURES_VIA_PREFS
    printerFeatures.SetCanChangeNumCopies(true);   
#endif /* SET_PRINTER_FEATURES_VIA_PREFS */

    return NS_OK;    
  }

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsPrinterEnumeratorGTK::DisplayPropertiesDlg(const PRUnichar *aPrinter, nsIPrintSettings *aPrintSettings)
{
  return NS_OK;
}

//----------------------------------------------------------------------
nsresult GlobalPrinters::InitializeGlobalPrinters ()
{
  if (PrintersAreAllocated()) {
    return NS_OK;
  }

  mGlobalPrinterList = new nsTArray<nsString>();

  nsPSPrinterList psMgr;
  if (psMgr.Enabled()) {
    /* Get the list of PostScript-module printers */
    // XXX: this function is the only user of GetPrinterList
    // So it may be interesting to convert the nsCStrings
    // in this function, we would save one loop here
    nsTArray<nsCString> printerList;
    psMgr.GetPrinterList(printerList);
    for (PRUint32 i = 0; i < printerList.Length(); i++)
    {
      mGlobalPrinterList->AppendElement(NS_ConvertUTF8toUTF16(printerList[i]));
    }
  }

  /* If there are no printers available after all checks, return an error */
  if (!mGlobalPrinterList->Length())
  {
    /* Make sure we do not cache an empty printer list */
    FreeGlobalPrinters();

    return NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE;
  }

  return NS_OK;
}

//----------------------------------------------------------------------
void GlobalPrinters::FreeGlobalPrinters()
{
  if (mGlobalPrinterList) {
    delete mGlobalPrinterList;
    mGlobalPrinterList = nsnull;
  }  
}

void 
GlobalPrinters::GetDefaultPrinterName(PRUnichar **aDefaultPrinterName)
{
  *aDefaultPrinterName = nsnull;
  
  bool allocate = !GlobalPrinters::GetInstance()->PrintersAreAllocated();
  
  if (allocate) {
    nsresult rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
    if (NS_FAILED(rv)) {
      return;
    }
  }
  NS_ASSERTION(GlobalPrinters::GetInstance()->PrintersAreAllocated(), "no GlobalPrinters");

  if (GlobalPrinters::GetInstance()->GetNumPrinters() == 0)
    return;
  
  *aDefaultPrinterName = ToNewUnicode(*GlobalPrinters::GetInstance()->GetStringAt(0));

  if (allocate) {  
    GlobalPrinters::GetInstance()->FreeGlobalPrinters();
  }  
}

