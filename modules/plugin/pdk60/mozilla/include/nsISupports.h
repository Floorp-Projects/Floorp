/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsISupports.idl
 */

#ifndef __gen_nsISupports_h__
#define __gen_nsISupports_h__

#include "nsrootidl.h"

/* 
 * Start commenting out the C++ versions of the below in the output header
 */
#if 0

/* starting interface:    nsISupports */
#define NS_ISUPPORTS_IID_STR "00000000-0000-0000-c000-000000000046"

#define NS_ISUPPORTS_IID \
  {0x00000000, 0x0000, 0x0000, \
    { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }}

class nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISUPPORTS_IID)

  /* void QueryInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
  NS_IMETHOD QueryInterface(const nsIID & uuid, void * *result) = 0;

  /* [noscript, notxpcom] nsrefcnt AddRef (); */
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;

  /* [noscript, notxpcom] nsrefcnt Release (); */
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSISUPPORTS \
  NS_IMETHOD QueryInterface(const nsIID & uuid, void * *result); \
  NS_IMETHOD_(nsrefcnt) AddRef(void); \
  NS_IMETHOD_(nsrefcnt) Release(void); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSISUPPORTS(_to) \
  NS_IMETHOD QueryInterface(const nsIID & uuid, void * *result) { return _to ## QueryInterface(uuid, result); } \
  NS_IMETHOD_(nsrefcnt) AddRef(void) { return _to ## AddRef(); } \
  NS_IMETHOD_(nsrefcnt) Release(void) { return _to ## Release(); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsSupports : public nsISupports
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISUPPORTS

  nsSupports();
  virtual ~nsSupports();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsSupports, nsISupports)

nsSupports::nsSupports()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsSupports::~nsSupports()
{
  /* destructor code */
}

/* void QueryInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP nsSupports::QueryInterface(const nsIID & uuid, void * *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript, notxpcom] nsrefcnt AddRef (); */
NS_IMETHODIMP_(nsrefcnt) nsSupports::AddRef()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript, notxpcom] nsrefcnt Release (); */
NS_IMETHODIMP_(nsrefcnt) nsSupports::Release()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif

/* 
 * End commenting out the C++ versions of the above in the output header
 */
#endif
#include "nsISupportsUtils.h"

#endif /* __gen_nsISupports_h__ */
