/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIBaseStream.idl
 */

#ifndef __gen_nsIBaseStream_h__
#define __gen_nsIBaseStream_h__

#include "nsISupports.h"


/* starting interface:    nsIBaseStream */
#define NS_IBASESTREAM_IID_STR "6ccb17a0-e95e-11d1-beae-00805f8a66dc"

#define NS_IBASESTREAM_IID \
  {0x6ccb17a0, 0xe95e, 0x11d1, \
    { 0xbe, 0xae, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc }}

class nsIBaseStream : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IBASESTREAM_IID)

  /** Close the stream. */
  /* void Close (); */
  NS_IMETHOD Close(void) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIBASESTREAM \
  NS_IMETHOD Close(void); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIBASESTREAM(_to) \
  NS_IMETHOD Close(void) { return _to ## Close(); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsBaseStream : public nsIBaseStream
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBASESTREAM

  nsBaseStream();
  virtual ~nsBaseStream();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsBaseStream, nsIBaseStream)

nsBaseStream::nsBaseStream()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsBaseStream::~nsBaseStream()
{
  /* destructor code */
}

/* void Close (); */
NS_IMETHODIMP nsBaseStream::Close()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_nsIBaseStream_h__ */
