/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIFactory.idl
 */

#ifndef __gen_nsIFactory_h__
#define __gen_nsIFactory_h__

#include "nsISupports.h"


/* starting interface:    nsIFactory */
#define NS_IFACTORY_IID_STR "00000001-0000-0000-c000-000000000046"

#define NS_IFACTORY_IID \
  {0x00000001, 0x0000, 0x0000, \
    { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }}

class nsIFactory : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFACTORY_IID)

  /* void CreateInstance (in nsISupports aOuter, in nsIIDRef iid, [iid_is (iid), retval] out nsQIResult result); */
  NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID & iid, void * *result) = 0;

  /* void LockFactory (in PRBool lock); */
  NS_IMETHOD LockFactory(PRBool lock) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIFACTORY \
  NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID & iid, void * *result); \
  NS_IMETHOD LockFactory(PRBool lock); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIFACTORY(_to) \
  NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID & iid, void * *result) { return _to ## CreateInstance(aOuter, iid, result); } \
  NS_IMETHOD LockFactory(PRBool lock) { return _to ## LockFactory(lock); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsFactory : public nsIFactory
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFACTORY

  nsFactory();
  virtual ~nsFactory();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsFactory, nsIFactory)

nsFactory::nsFactory()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsFactory::~nsFactory()
{
  /* destructor code */
}

/* void CreateInstance (in nsISupports aOuter, in nsIIDRef iid, [iid_is (iid), retval] out nsQIResult result); */
NS_IMETHODIMP nsFactory::CreateInstance(nsISupports *aOuter, const nsIID & iid, void * *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void LockFactory (in PRBool lock); */
NS_IMETHODIMP nsFactory::LockFactory(PRBool lock)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_nsIFactory_h__ */
