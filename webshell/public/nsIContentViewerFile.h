/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIContentViewerFile.idl
 */

#ifndef __gen_nsIContentViewerFile_h__
#define __gen_nsIContentViewerFile_h__

#include "nsISupports.h"
#include "nsrootidl.h"
#include "nsIWebShell.h"
#include "nsIDOMWindow.h"
#include "nsIPrintListener.h"

class nsIDeviceContext;

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

  /* void Print (); 
  
  */
  /**
   * Print the current document
   * @param aSilent -- if true, the print settings dialog will be suppressed
   * @param aFileName -- a file pointer to output regression tests or print to a file
   * @return error status
   */
  NS_IMETHOD Print(PRBool aSilent,FILE *aFile, nsIPrintListener *aPrintListener = nsnull) = 0;

  NS_IMETHOD PrintPreview() = 0;

  /* [noscript] void PrintContent (in nsIWebShell parent, in nsIDeviceContext DContext, in nsIDOMWindow aDOMWin, PRBool aIsSubDoc); */
  NS_IMETHOD PrintContent(nsIWebShell *      aParent,
                          nsIDeviceContext * aDContext,
                          nsIDOMWindow     * aDOMWin,
                          PRBool             aIsSubDoc) = 0;

  /* readonly attribute boolean printable; */
  NS_IMETHOD GetPrintable(PRBool *aPrintable) = 0;
};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSICONTENTVIEWERFILE \
  NS_IMETHOD Save(void); \
  NS_IMETHOD GetSaveable(PRBool *aSaveable); \
  NS_IMETHOD Print(PRBool aSilent,FILE *aFile, nsIPrintListener *aPrintListener); \
  NS_IMETHOD PrintPreview(); \
  NS_IMETHOD PrintContent(nsIWebShell * parent, nsIDeviceContext * DContext, nsIDOMWindow * aDOMWin, PRBool aIsSubDoc); \
  NS_IMETHOD GetPrintable(PRBool *aPrintable); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSICONTENTVIEWERFILE(_to) \
  NS_IMETHOD Save(void) { return _to ## Save(); } \
  NS_IMETHOD GetSaveable(PRBool *aSaveable) { return _to ## GetSaveable(aSaveable); } \
  NS_IMETHOD Print(PRBool aSilent,FILE *aFile, nsIPrintListener *aPrintListener) { return _to ## Print(); } \
  NS_IMETHOD PrintPreview() { return _to ## Print(); } \
  NS_IMETHOD PrintContent(nsIWebShell * parent, nsIDeviceContext * DContext, nsIDOMWindow * aDOMWin, PRBool aIsSubDoc) { return _to ## PrintContent(parent, DContext, aDOMWin, aIsSubDoc); } \
  NS_IMETHOD GetPrintable(PRBool *aPrintable) { return _to ## GetPrintable(aPrintable); } 


#endif /* __gen_nsIContentViewerFile_h__ */
