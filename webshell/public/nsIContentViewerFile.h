/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIContentViewerFile.idl
 */

#ifndef __gen_nsIContentViewerFile_h__
#define __gen_nsIContentViewerFile_h__

#include "nsISupports.h"
#include "nsrootidl.h"
#include "nsIWebShell.h"
#include "nsIDOMWindow.h"

class nsIDeviceContext;
class nsIPrintSettings;
class nsIWebProgressListener;

/* starting interface:    nsIContentViewerFile */

#define NS_ICONTENTVIEWERFILE_IID_STR "6317f32c-9bc7-11d3-bccc-0060b0fc76bd"

#define NS_ICONTENTVIEWERFILE_IID \
  {0x6317f32c, 0x9bc7, 0x11d3, \
    { 0xbc, 0xcc, 0x00, 0x60, 0xb0, 0xfc, 0x76, 0xbd }}

class nsIContentViewerFile : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICONTENTVIEWERFILE_IID)

  /* void Save (); */
  NS_IMETHOD Save(void) = 0;

  /* readonly attribute boolean saveable; */
  NS_IMETHOD GetSaveable(PRBool *aSaveable) = 0;

  /**
   * Print the current document
   * @param aSilent         -- if true, the print settings dialog will be suppressed
   * @param aPrintSettings  -- PrintSettings used during print
   * @param aProgressListener  -- Listens to the progress of printing
   * @return error status
   */
  NS_IMETHOD Print(PRBool            aSilent,
                   nsIPrintSettings* aPrintSettings,
                   nsIWebProgressListener * aProgressListener) = 0;
#ifdef NS_DEBUG
  /**
   * Print the current document
   * @param aSilent        -- if true, the print settings dialog will be suppressed
   * @param aDebugFile     -- Regression testing debug file
   * @param aPrintSettings -- PrintSettings used during print
   * @return error status
   */
  NS_IMETHOD Print(PRBool            aSilent,
                   FILE *            aDebugFile, 
                   nsIPrintSettings* aPrintSettings = nsnull) = 0;
#endif

  /**
   * PrintPreview the current document
   * @param aPrintSettings  -- PrintSettings used during print
   * @return error status
   */
  NS_IMETHOD PrintPreview(nsIPrintSettings* aPrintSettings) = 0;

  /* [noscript] void PrintContent (in nsIWebShell parent, in nsIDeviceContext DContext, in nsIDOMWindow aDOMWin, PRBool aIsSubDoc); */
  NS_IMETHOD PrintContent(nsIWebShell *      aParent,
                          nsIDeviceContext * aDContext,
                          nsIDOMWindow     * aDOMWin,
                          PRBool             aIsSubDoc) = 0;

  /* readonly attribute boolean printable; */
  NS_IMETHOD GetPrintable(PRBool *aPrintable) = 0;
};

#ifdef NS_DEBUG
/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSICONTENTVIEWERFILE \
  NS_IMETHOD Save(void); \
  NS_IMETHOD GetSaveable(PRBool *aSaveable); \
  NS_IMETHOD Print(PRBool aSilent, nsIPrintSettings* aPrintSettings, nsIWebProgressListener * aProgressListener); \
  NS_IMETHOD Print(PRBool aSilent, FILE * aDebugFile, nsIPrintSettings* aPrintSettings); \
  NS_IMETHOD PrintPreview(nsIPrintSettings* aPrintSettings); \
  NS_IMETHOD PrintContent(nsIWebShell * parent, nsIDeviceContext * DContext, nsIDOMWindow * aDOMWin, PRBool aIsSubDoc); \
  NS_IMETHOD GetPrintable(PRBool *aPrintable); 
#else
/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSICONTENTVIEWERFILE \
  NS_IMETHOD Save(void); \
  NS_IMETHOD GetSaveable(PRBool *aSaveable); \
  NS_IMETHOD Print(PRBool aSilent, nsIPrintSettings* aPrintSettings, nsIWebProgressListener * aProgressListener); \
  NS_IMETHOD PrintPreview(nsIPrintSettings* aPrintSettings); \
  NS_IMETHOD PrintContent(nsIWebShell * parent, nsIDeviceContext * DContext, nsIDOMWindow * aDOMWin, PRBool aIsSubDoc); \
  NS_IMETHOD GetPrintable(PRBool *aPrintable); 
#endif

#endif /* __gen_nsIContentViewerFile_h__ */
