/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIChromeRegistry.idl
 */

#ifndef __gen_nsIChromeRegistry_h__
#define __gen_nsIChromeRegistry_h__

#include "nsISupports.h"
#include "nsIURI.h"
#include "nsrootidl.h"
#include "nsIURL.h"

/* starting interface:    nsIChromeRegistry */

#define NS_ICHROMEREGISTRY_IID_STR "d8c7d8a1-e84c-11d2-bf87-00105a1b0627"

#define NS_ICHROMEREGISTRY_IID \
  {0xd8c7d8a1, 0xe84c, 0x11d2, \
    { 0xbf, 0x87, 0x00, 0x10, 0x5a, 0x1b, 0x06, 0x27 }}

class nsIChromeRegistry : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICHROMEREGISTRY_IID)

  /* void convertChromeURL (in nsIURI aChromeURL); */
  NS_IMETHOD ConvertChromeURL(nsIURI *aChromeURL) = 0;

  /* long getOverlayCount (in wstring aChromeURL); */
  NS_IMETHOD GetOverlayCount(const PRUnichar *aChromeURL, PRInt32 *_retval) = 0;

  /* wstring getOverlayAt (in wstring aChromeURL, in long aIndex); */
  NS_IMETHOD GetOverlayAt(const PRUnichar *aChromeURL, PRInt32 aIndex, PRUnichar **_retval) = 0;
};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSICHROMEREGISTRY \
  NS_IMETHOD ConvertChromeURL(nsIURI *aChromeURL); \
  NS_IMETHOD GetOverlayCount(const PRUnichar *aChromeURL, PRInt32 *_retval); \
  NS_IMETHOD GetOverlayAt(const PRUnichar *aChromeURL, PRInt32 aIndex, PRUnichar **_retval); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSICHROMEREGISTRY(_to) \
  NS_IMETHOD ConvertChromeURL(nsIURI *aChromeURL) { return _to ## ConvertChromeURL(aChromeURL); } \
  NS_IMETHOD GetOverlayCount(const PRUnichar *aChromeURL, PRInt32 *_retval) { return _to ## GetOverlayCount(aChromeURL, _retval); } \
  NS_IMETHOD GetOverlayAt(const PRUnichar *aChromeURL, PRInt32 aIndex, PRUnichar **_retval) { return _to ## GetOverlayAt(aChromeURL, aIndex, _retval); } 

// for component registration
// {D8C7D8A2-E84C-11d2-BF87-00105A1B0627}
#define NS_CHROMEREGISTRY_CID \
{ 0xd8c7d8a2, 0xe84c, 0x11d2, { 0xbf, 0x87, 0x0, 0x10, 0x5a, 0x1b, 0x6, 0x27 } }
extern nsresult
NS_NewChromeRegistry(nsIChromeRegistry* *aResult);

#endif /* __gen_nsIChromeRegistry_h__ */
