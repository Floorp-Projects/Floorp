/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIInputStream.idl
 */

#ifndef __gen_nsIInputStream_h__
#define __gen_nsIInputStream_h__

#include "nsIBaseStream.h"


/* starting interface:    nsIInputStream */
#define NS_IINPUTSTREAM_IID_STR "022396f0-93b5-11d1-895b-006008911b81"

#define NS_IINPUTSTREAM_IID \
  {0x022396f0, 0x93b5, 0x11d1, \
    { 0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81 }}

class nsIInputStream : public nsIBaseStream {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IINPUTSTREAM_IID)

  /** Return the number of bytes currently available in the stream
     *  @param aLength out parameter to hold the number of bytes
     *         if an error occurs, the parameter will be undefined
     *  @return error status
     */
  /* unsigned long Available (); */
  NS_IMETHOD Available(PRUint32 *_retval) = 0;

  /** Read data from the stream.
     *  @param aBuf the buffer into which the data is read
     *  @param aCount the maximum number of bytes to read
     *  @return aReadCount out parameter to hold the number of
     *         bytes read, eof if 0. if an error occurs, the
     *         read count will be undefined
     */
  /* [noscript] unsigned long Read (in charStar buf, in unsigned long count); */
  NS_IMETHOD Read(char * buf, PRUint32 count, PRUint32 *_retval) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIINPUTSTREAM \
  NS_IMETHOD Available(PRUint32 *_retval); \
  NS_IMETHOD Read(char * buf, PRUint32 count, PRUint32 *_retval); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIINPUTSTREAM(_to) \
  NS_IMETHOD Available(PRUint32 *_retval) { return _to ## Available(_retval); } \
  NS_IMETHOD Read(char * buf, PRUint32 count, PRUint32 *_retval) { return _to ## Read(buf, count, _retval); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsInputStream : public nsIInputStream
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM

  nsInputStream();
  virtual ~nsInputStream();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsInputStream, nsIInputStream)

nsInputStream::nsInputStream()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsInputStream::~nsInputStream()
{
  /* destructor code */
}

/* unsigned long Available (); */
NS_IMETHODIMP nsInputStream::Available(PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] unsigned long Read (in charStar buf, in unsigned long count); */
NS_IMETHODIMP nsInputStream::Read(char * buf, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_nsIInputStream_h__ */
