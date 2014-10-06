/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsIFile.h"
#include "nsTArray.h"

#include "mozilla/Preferences.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace mozilla;

#ifdef PR_LOGGING 
static PRLogModuleInfo *
GetDeviceContextSpecGTKLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("DeviceContextSpecGTK");
  return sLog;
}
#endif /* PR_LOGGING */
/* Macro to make lines shorter */
#define DO_PR_DEBUG_LOG(x) PR_LOG(GetDeviceContextSpecGTKLog(), PR_LOG_DEBUG, x)

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

  bool      PrintersAreAllocated()       { return mGlobalPrinterList != nullptr; }
  uint32_t  GetNumPrinters()
    { return mGlobalPrinterList ? mGlobalPrinterList->Length() : 0; }
  nsString* GetStringAt(int32_t aInx)    { return &mGlobalPrinterList->ElementAt(aInx); }
  void      GetDefaultPrinterName(char16_t **aDefaultPrinterName);

protected:
  GlobalPrinters() {}

  static GlobalPrinters mGlobalPrinters;
  static nsTArray<nsString>* mGlobalPrinterList;
};

//---------------
// static members
GlobalPrinters GlobalPrinters::mGlobalPrinters;
nsTArray<nsString>* GlobalPrinters::mGlobalPrinterList = nullptr;
//---------------

nsDeviceContextSpecGTK::nsDeviceContextSpecGTK()
  : mPrintJob(nullptr)
  , mGtkPrinter(nullptr)
  , mGtkPrintSettings(nullptr)
  , mGtkPageSetup(nullptr)
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

NS_IMPL_ISUPPORTS(nsDeviceContextSpecGTK,
                  nsIDeviceContextSpec)

#include "gfxPDFSurface.h"
#include "gfxPSSurface.h"
NS_IMETHODIMP nsDeviceContextSpecGTK::GetSurfaceForPrinter(gfxASurface **aSurface)
{
  *aSurface = nullptr;

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
  gint fd = g_file_open_tmp("XXXXXX.tmp", &buf, nullptr);
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

  int16_t format;
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
            static_cast<int16_t>(nsIPrintSettings::kOutputFormatPDF) :
            static_cast<int16_t>(nsIPrintSettings::kOutputFormatPS);
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
    int32_t orientation;
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
#if (MOZ_WIDGET_GTK == 3)
print_callback(GtkPrintJob *aJob, gpointer aData, const GError *aError) {
#else
print_callback(GtkPrintJob *aJob, gpointer aData, GError *aError) {
#endif
  g_object_unref(aJob);
  ((nsIFile*) aData)->Remove(false);
}

static void
ns_release_macro(gpointer aData) {
  nsIFile* spoolFile = (nsIFile*) aData;
  NS_RELEASE(spoolFile);
}

NS_IMETHODIMP nsDeviceContextSpecGTK::BeginDocument(const nsAString& aTitle, char16_t * aPrintToFileName,
                                                    int32_t aStartPage, int32_t aEndPage)
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

    if (!gtk_print_job_set_source_file(mPrintJob, mSpoolName.get(), nullptr))
      return NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE;

    NS_ADDREF(mSpoolFile.get());
    gtk_print_job_send(mPrintJob, print_callback, mSpoolFile, ns_release_macro);
  } else {
    // Handle print-to-file ourselves for the benefit of embedders
    nsXPIDLString targetPath;
    nsCOMPtr<nsIFile> destFile;
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

NS_IMPL_ISUPPORTS(nsPrinterEnumeratorGTK, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorGTK::GetPrinterNameList(nsIStringEnumerator **aPrinterNameList)
{
  NS_ENSURE_ARG_POINTER(aPrinterNameList);
  *aPrinterNameList = nullptr;
  
  nsresult rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();
  nsTArray<nsString> *printers = new nsTArray<nsString>(numPrinters);
  if (!printers) {
    GlobalPrinters::GetInstance()->FreeGlobalPrinters();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  uint32_t count = 0;
  while( count < numPrinters )
  {
    printers->AppendElement(*GlobalPrinters::GetInstance()->GetStringAt(count++));
  }
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();

  return NS_NewAdoptingStringEnumerator(aPrinterNameList, printers);
}

/* readonly attribute wstring defaultPrinterName; */
NS_IMETHODIMP nsPrinterEnumeratorGTK::GetDefaultPrinterName(char16_t **aDefaultPrinterName)
{
  DO_PR_DEBUG_LOG(("nsPrinterEnumeratorGTK::GetDefaultPrinterName()\n"));
  NS_ENSURE_ARG_POINTER(aDefaultPrinterName);

  GlobalPrinters::GetInstance()->GetDefaultPrinterName(aDefaultPrinterName);

  DO_PR_DEBUG_LOG(("GetDefaultPrinterName(): default printer='%s'.\n", NS_ConvertUTF16toUTF8(*aDefaultPrinterName).get()));
  return NS_OK;
}

/* void initPrintSettingsFromPrinter (in wstring aPrinterName, in nsIPrintSettings aPrintSettings); */
NS_IMETHODIMP nsPrinterEnumeratorGTK::InitPrintSettingsFromPrinter(const char16_t *aPrinterName, nsIPrintSettings *aPrintSettings)
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
    int32_t slash = printerName.FindChar('/');
    if (kNotFound != slash)
      printerName.Cut(0, slash + 1);
  }
  
  /* Set filename */
  nsAutoCString filename;
  if (NS_FAILED(CopyPrinterCharPref(nullptr, printerName, "filename", filename))) {
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
     
    nsAutoCString orientation;
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

    /* PostScript module does not support changing the plex mode... */
    DO_PR_DEBUG_LOG(("setting default plex to '%s'\n", "default"));
    aPrintSettings->SetPlexName(MOZ_UTF16("default"));

    /* PostScript module does not support changing the resolution mode... */
    DO_PR_DEBUG_LOG(("setting default resolution to '%s'\n", "default"));
    aPrintSettings->SetResolutionName(MOZ_UTF16("default"));

    /* PostScript module does not support changing the colorspace... */
    DO_PR_DEBUG_LOG(("setting default colorspace to '%s'\n", "default"));
    aPrintSettings->SetColorspace(MOZ_UTF16("default"));

    nsAutoCString papername;
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
    }

    bool hasSpoolerCmd = (nsPSPrinterList::kTypePS ==
        nsPSPrinterList::GetPrinterType(fullPrinterName));

    if (hasSpoolerCmd) {
      nsAutoCString command;
      if (NS_SUCCEEDED(CopyPrinterCharPref("postscript",
            printerName, "print_command", command))) {
        DO_PR_DEBUG_LOG(("setting default print command to '%s'\n",
            command.get()));
        aPrintSettings->SetPrintCommand(NS_ConvertUTF8toUTF16(command).get());
      }
    }
    
    return NS_OK;    
  }

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsPrinterEnumeratorGTK::DisplayPropertiesDlg(const char16_t *aPrinter, nsIPrintSettings *aPrintSettings)
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
    for (uint32_t i = 0; i < printerList.Length(); i++)
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
    mGlobalPrinterList = nullptr;
  }  
}

void 
GlobalPrinters::GetDefaultPrinterName(char16_t **aDefaultPrinterName)
{
  *aDefaultPrinterName = nullptr;
  
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

