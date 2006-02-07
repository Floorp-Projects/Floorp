/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDeviceContextSpecG.h"

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIPrintOptions.h"
#include "nsReadableUtils.h"
#include "nsGfxCIID.h"

#include "nsIPref.h"
#include "prenv.h" /* for PR_GetEnv */

#include "nsIDOMWindow.h"
#include "nsIServiceManager.h"
#include "nsIDialogParamBlock.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindowInternal.h"

#ifdef USE_XPRINT
#include "xprintutil.h"
#endif /* USE_XPRINT */

static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);

nsStringArray* nsDeviceContextSpecGTK::globalPrinterList = nsnull;
int nsDeviceContextSpecGTK::globalNumPrinters = 0;

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecGTK
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecGTK :: nsDeviceContextSpecGTK()
{
  NS_INIT_REFCNT();
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecGTK
 *  @update   dc 2/15/98
 */
nsDeviceContextSpecGTK :: ~nsDeviceContextSpecGTK()
{
}

static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
static NS_DEFINE_IID(kIDeviceContextSpecPSIID, NS_IDEVICE_CONTEXT_SPEC_PS_IID);
#ifdef USE_XPRINT
static NS_DEFINE_IID(kIDeviceContextSpecXPIID, NS_IDEVICE_CONTEXT_SPEC_XP_IID);
#endif

NS_IMETHODIMP nsDeviceContextSpecGTK :: QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(kIDeviceContextSpecIID))
  {
    nsIDeviceContextSpec* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(kIDeviceContextSpecPSIID))
  {
    nsIDeviceContextSpecPS* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }

#ifdef USE_XPRINT
  if (aIID.Equals(kIDeviceContextSpecXPIID))
  {
    nsIDeviceContextSpecXp *tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
#endif /* USE_XPRINT */

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(kISupportsIID))
  {
    nsIDeviceContextSpec* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDeviceContextSpecGTK)
NS_IMPL_RELEASE(nsDeviceContextSpecGTK)


int nsDeviceContextSpecGTK::InitializeGlobalPrinters ()
{
  globalNumPrinters = 0;
  globalPrinterList = new nsStringArray();
  if (!globalPrinterList) 
    return NS_ERROR_OUT_OF_MEMORY;
      
#ifdef USE_XPRINT   
  XPPrinterList plist = XpuGetPrinterList(nsnull, &globalNumPrinters);
  
  if (plist && (globalNumPrinters > 0))
  {  
    int i;
    for(  i = 0 ; i < globalNumPrinters ; i++ )
    {
      globalPrinterList->AppendString(nsString(NS_ConvertASCIItoUCS2(plist[i].name)));
    }
    
    XpuFreePrinterList(plist);
  }  
#endif /* USE_XPRINT */

  /* add an entry for the default printer (see nsPostScriptObj.cpp) */
  globalPrinterList->AppendString(
    nsString(NS_ConvertASCIItoUCS2(NS_POSTSCRIPT_DRIVER_NAME "default")));
  globalNumPrinters++;

  /* get the list of printers */
  char *printerList = nsnull;
  
  /* the env var MOZILLA_PRINTER_LIST can "override" the prefs */
  printerList = PR_GetEnv("MOZILLA_PRINTER_LIST");
  
  if (!printerList) {
    nsresult rv;
    nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      (void) pPrefs->CopyCharPref("print.printer_list", &printerList);
    }
  }  

  if (printerList) {
    char *tok_lasts;
    char *name;
    
    /* PL_strtok_r() will modify the string - copy it! */
    printerList = strdup(printerList);
    if (!printerList)
      return NS_ERROR_OUT_OF_MEMORY;    
    
    for( name = PL_strtok_r(printerList, " ", &tok_lasts) ; 
         name != nsnull ; 
         name = PL_strtok_r(nsnull, " ", &tok_lasts) )
    {
      globalPrinterList->AppendString(
        nsString(NS_ConvertASCIItoUCS2(NS_POSTSCRIPT_DRIVER_NAME)) + 
        nsString(NS_ConvertASCIItoUCS2(name)));
      globalNumPrinters++;      
    }
    
    free(printerList);
  }
      
  if (globalNumPrinters == 0)
    return NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAIULABLE; 

  return NS_OK;
}

void nsDeviceContextSpecGTK::FreeGlobalPrinters()
{
  delete globalPrinterList;
  globalPrinterList = nsnull;
  globalNumPrinters = 0;
}


/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecGTK
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 *
 * gisburn: Please note that this function exists as 1:1 copy in other
 * toolkits including:
 * - GTK+-toolkit:
 *   file:     mozilla/gfx/src/gtk/nsDeviceContextSpecG.cpp
 *   function: NS_IMETHODIMP nsDeviceContextSpecGTK::Init(PRBool aQuiet)
 * - Xlib-toolkit: 
 *   file:     mozilla/gfx/src/xlib/nsDeviceContextSpecXlib.cpp 
 *   function: NS_IMETHODIMP nsDeviceContextSpecXlib::Init(PRBool aQuiet)
 * - Qt-toolkit:
 *   file:     mozilla/gfx/src/qt/nsDeviceContextSpecQT.cpp
 *   function: NS_IMETHODIMP nsDeviceContextSpecQT::Init(PRBool aQuiet)
 * 
 * ** Please update the other toolkits when changing this function.
 */
NS_IMETHODIMP nsDeviceContextSpecGTK::Init(PRBool aQuiet)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIPrintOptions> printService(do_GetService(kPrintOptionsCID, &rv));
  NS_ASSERTION(nsnull != printService, "No print service.");
  
  // if there is a current selection then enable the "Selection" radio button
  if (NS_SUCCEEDED(rv) && printService) {
    PRBool isOn;
    printService->GetPrintOptions(nsIPrintOptions::kPrintOptionsEnableSelectionRB, &isOn);
    nsCOMPtr<nsIPref> pPrefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      (void) pPrefs->SetBoolPref("print.selection_radio_enabled", isOn);
    }
  }

  char      *path;
  PRBool     canPrint       = PR_FALSE;
  PRBool     reversed       = PR_FALSE;
  PRBool     color          = PR_FALSE;
  PRBool     tofile         = PR_FALSE;
  PRInt16    printRange     = nsIPrintOptions::kRangeAllPages;
  PRInt32    paper_size     = NS_LETTER_SIZE;
  PRInt32    orientation    = NS_PORTRAIT;
  PRInt32    fromPage       = 1;
  PRInt32    toPage         = 1;
  PRUnichar *command        = nsnull;
  PRInt32    copies         = 1;
  PRUnichar *printer        = nsnull;
  PRUnichar *printfile      = nsnull;
  double     dleft          = 0.5;
  double     dright         = 0.5;
  double     dtop           = 0.5;
  double     dbottom        = 0.5; 

  if( !globalPrinterList )
    if (InitializeGlobalPrinters())
       return NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAIULABLE;
  if( globalNumPrinters && !globalPrinterList->Count() ) 
     return NS_ERROR_OUT_OF_MEMORY;

  if (!aQuiet ) {
    rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsIDialogParamBlock> ioParamBlock(do_CreateInstance("@mozilla.org/embedcomp/dialogparam;1"));

    nsCOMPtr<nsISupportsInterfacePointer> paramBlockWrapper;
    if (ioParamBlock)
      paramBlockWrapper = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID);

    if (paramBlockWrapper) {
      paramBlockWrapper->SetData(ioParamBlock);
      paramBlockWrapper->SetDataIID(&NS_GET_IID(nsIDialogParamBlock));

      nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
      if (wwatch) {
        nsCOMPtr<nsIDOMWindow> active;
        wwatch->GetActiveWindow(getter_AddRefs(active));      
        nsCOMPtr<nsIDOMWindowInternal> parent = do_QueryInterface(active);

        nsCOMPtr<nsIDOMWindow> newWindow;
        rv = wwatch->OpenWindow(parent, "chrome://global/content/printdialog.xul",
                      "_blank", "chrome,modal", paramBlockWrapper,
                      getter_AddRefs(newWindow));
      }
    }
    if (NS_SUCCEEDED(rv)) {
      PRInt32 buttonPressed = 0;
      ioParamBlock->GetInt(0, &buttonPressed);
      if (buttonPressed == 0) {
        canPrint = PR_TRUE;
      }
      else {
        rv = NS_ERROR_ABORT;
      }
    }
  }
  else {
    canPrint = PR_TRUE;
  }
  
  FreeGlobalPrinters();

  if (canPrint) {
    if (printService) {
      printService->GetPrinter(&printer);
      printService->GetPrintReversed(&reversed);
      printService->GetPrintInColor(&color);
      printService->GetPaperSize(&paper_size);
      printService->GetOrientation(&orientation);
      printService->GetPrintCommand(&command);
      printService->GetPrintRange(&printRange);
      printService->GetToFileName(&printfile);
      printService->GetPrintToFile(&tofile);
      printService->GetStartPageRange(&fromPage);
      printService->GetEndPageRange(&toPage);
      printService->GetNumCopies(&copies);
      printService->GetMarginTop(&dtop);
      printService->GetMarginLeft(&dleft);
      printService->GetMarginBottom(&dbottom);
      printService->GetMarginRight(&dright);

      if (command != nsnull && printfile != nsnull) {
        // ToDo: Use LocalEncoding instead of UTF-8 (see bug 73446)
        strcpy(mPrData.command, NS_ConvertUCS2toUTF8(command).get());  
        strcpy(mPrData.path,    NS_ConvertUCS2toUTF8(printfile).get());
      }
      if (printer != nsnull) 
        strcpy(mPrData.printer, NS_ConvertUCS2toUTF8(printer).get());        
#ifdef DEBUG_rods
      printf("margins:       %5.2f,%5.2f,%5.2f,%5.2f\n", dtop, dleft, dbottom, dright);
      printf("printRange     %d\n", printRange);
      printf("fromPage       %d\n", fromPage);
      printf("toPage         %d\n", toPage);
#endif /* DEBUG_rods */
    } else {
#ifdef VMS
      // Note to whoever puts the "lpr" into the prefs file. Please contact me
      // as I need to make the default be "print" instead of "lpr" for OpenVMS.
      strcpy(mPrData.command, "print");
#else
      strcpy(mPrData.command, "lpr ${MOZ_PRINTER_NAME:+'-P'}${MOZ_PRINTER_NAME}");
#endif /* VMS */
    }

    mPrData.top       = dtop;
    mPrData.bottom    = dbottom;
    mPrData.left      = dleft;
    mPrData.right     = dright;
    mPrData.fpf       = !reversed;
    mPrData.grayscale = !color;
    mPrData.size      = paper_size;
    mPrData.orientation = orientation;
    mPrData.toPrinter = !tofile;
    mPrData.copies = copies;

    // PWD, HOME, or fail 
    
    if (!printfile) {
      if ( ( path = PR_GetEnv( "PWD" ) ) == (char *) nsnull ) 
        if ( ( path = PR_GetEnv( "HOME" ) ) == (char *) nsnull )
          strcpy(mPrData.path, "mozilla.ps");
          
      if ( path != (char *) nsnull )
        sprintf(mPrData.path, "%s/mozilla.ps", path);
      else
        return NS_ERROR_FAILURE;
    }
    
#ifdef NOT_IMPLEMENTED_YET
    if (globalNumPrinters) {
       for(int i = 0; (i < globalNumPrinters) && !mQueue; i++) {
          if (!(globalPrinterList->StringAt(i)->CompareWithConversion(mPrData.printer, TRUE, -1)))
             mQueue = PrnDlg.SetPrinterQueue(i);
       }
    }
#endif /* NOT_IMPLEMENTED_YET */
    
    if (command != nsnull) {
      nsMemory::Free(command);
    }
    if (printfile != nsnull) {
      nsMemory::Free(printfile);
    }

    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetToPrinter(PRBool &aToPrinter)
{
  aToPrinter = mPrData.toPrinter;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetPrinter ( char **aPrinter )
{
   *aPrinter = &mPrData.printer[0];
   return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetCopies ( int &aCopies )
{
   aCopies = mPrData.copies;
   return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetFirstPageFirst ( PRBool &aFpf )      
{
  aFpf = mPrData.fpf;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetGrayscale ( PRBool &aGrayscale )      
{
  aGrayscale = mPrData.grayscale;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetSize ( int &aSize )      
{
  aSize = mPrData.size;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetPageDimensions ( float &aWidth, float &aHeight )      
{
    if ( mPrData.size == NS_LETTER_SIZE ) {
        aWidth = 8.5;
        aHeight = 11.0;
    } else if ( mPrData.size == NS_LEGAL_SIZE ) {
        aWidth = 8.5;
        aHeight = 14.0;
    } else if ( mPrData.size == NS_EXECUTIVE_SIZE ) {
        aWidth = 7.5;
        aHeight = 10.0;
    } else if ( mPrData.size == NS_A4_SIZE ) {
        // 210mm X 297mm == 8.27in X 11.69in
        aWidth = 8.27;
        aHeight = 11.69;
  } else if ( mPrData.size == NS_A3_SIZE ) {
        // 297mm X 420mm == 11.69in X 16.53in
        aWidth = 11.69;
        aHeight = 16.53;    }

    if (mPrData.orientation == NS_LANDSCAPE) {
      float temp;
      temp = aWidth;
      aWidth = aHeight;
      aHeight = temp;
    }

    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetLandscape ( PRBool &landscape )
{
  landscape = (mPrData.orientation == NS_LANDSCAPE);
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetTopMargin ( float &value )      
{
  value = mPrData.top;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetBottomMargin ( float &value )      
{
  value = mPrData.bottom;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetRightMargin ( float &value )      
{
  value = mPrData.right;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetLeftMargin ( float &value )      
{
  value = mPrData.left;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetCommand ( char **aCommand )      
{
  *aCommand = &mPrData.command[0];
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetPath ( char **aPath )      
{
  *aPath = &mPrData.path[0];
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK :: GetUserCancelled( PRBool &aCancel )     
{
  aCancel = mPrData.cancel;
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::GetPrintMethod(PrintMethod &aMethod)
{
  /* printer names for the PostScript module alwas start with 
   * the NS_POSTSCRIPT_DRIVER_NAME string */
  if (strncmp(mPrData.printer, NS_POSTSCRIPT_DRIVER_NAME, 
              NS_POSTSCRIPT_DRIVER_NAME_LEN) != 0)
    aMethod = pmXprint;
  else
    aMethod = pmPostScript;
    
  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::ClosePrintManager()
{
  return NS_OK;
}


//  Printer Enumerator
nsPrinterEnumeratorGTK::nsPrinterEnumeratorGTK()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS1(nsPrinterEnumeratorGTK, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorGTK::EnumeratePrinters(PRUint32* aCount, PRUnichar*** aResult)
{
  if (aCount) 
    *aCount = 0;
  else 
    return NS_ERROR_NULL_POINTER;
  
  if (aResult) 
    *aResult = nsnull;
  else 
    return NS_ERROR_NULL_POINTER;
  

  PRUnichar** array = (PRUnichar**) nsMemory::Alloc(nsDeviceContextSpecGTK::globalNumPrinters * sizeof(PRUnichar*));
  if (!array && nsDeviceContextSpecGTK::globalNumPrinters) 
    return NS_ERROR_OUT_OF_MEMORY;
  
  int count = 0;
  while( count < nsDeviceContextSpecGTK::globalNumPrinters )
  {
    PRUnichar *str = ToNewUnicode(*nsDeviceContextSpecGTK::globalPrinterList->StringAt(count));

    if (!str) {
      for (int i = count - 1; i >= 0; i--) 
        nsMemory::Free(array[i]);
      
      nsMemory::Free(array);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    array[count++] = str;
    
  }
  *aCount = count;
  *aResult = array;

  return NS_OK;
}

NS_IMETHODIMP nsPrinterEnumeratorGTK::DisplayPropertiesDlg(const PRUnichar *aPrinter)
{
  nsresult rv = NS_ERROR_FAILURE;
  
  /* fixme: We simply ignore the |aPrinter| argument here
   * We should get the supported printer attributes from the printer and 
   * populate the print job options dialog with these data instead of using 
   * the "default set" here.
   * However, this requires changes on all platforms and is another big chunk
   * of patches ... ;-(
   */

  nsCOMPtr<nsIPrintOptions> printService(do_GetService(kPrintOptionsCID, &rv));

  rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIDialogParamBlock> ioParamBlock(do_CreateInstance("@mozilla.org/embedcomp/dialogparam;1"));

  nsCOMPtr<nsISupportsInterfacePointer> paramBlockWrapper;
  if (ioParamBlock)
    paramBlockWrapper = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID);

  if (paramBlockWrapper) {
    paramBlockWrapper->SetData(ioParamBlock);
    paramBlockWrapper->SetDataIID(&NS_GET_IID(nsIDialogParamBlock));

    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
    if (wwatch) {
      nsCOMPtr<nsIDOMWindow> active;
      wwatch->GetActiveWindow(getter_AddRefs(active));
      nsCOMPtr<nsIDOMWindowInternal> parent = do_QueryInterface(active);

      nsCOMPtr<nsIDOMWindow> newWindow;
      rv = wwatch->OpenWindow(parent, "chrome://global/content/printjoboptions.xul",
                    "_blank", "chrome,modal", paramBlockWrapper,
                    getter_AddRefs(newWindow));
    }
  }

  return rv;
}

