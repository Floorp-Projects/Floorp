/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIOutputStream.idl
 */

#ifndef __gen_nsIOutputStream_h__
#define __gen_nsIOutputStream_h__

#include "nsIBaseStream.h"
#include "nsIInputStream.h"


/* starting interface:    nsIOutputStream */
#define NS_IOUTPUTSTREAM_IID_STR "7f13b870-e95f-11d1-beae-00805f8a66dc"

#define NS_IOUTPUTSTREAM_IID \
  {0x7f13b870, 0xe95f, 0x11d1, \
    { 0xbe, 0xae, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc }}

class nsIOutputStream : public nsIBaseStream {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IOUTPUTSTREAM_IID)

  /** Write data into the stream.
     *  @param aBuf the buffer from which the data is read
     *  @param aCount the maximum number of bytes to write
     *  @return aWriteCount out parameter to hold the number of
     *         bytes written. if an error occurs, the writecount
     *         is undefined
     */
  /* unsigned long Write (in string buf, in unsigned long count); */
  NS_IMETHOD Write(const char *buf, PRUint32 count, PRUint32 *_retval) = 0;

  /**
     * Flushes the stream.
     */
  /* void Flush (); */
  NS_IMETHOD Flush(void) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIOUTPUTSTREAM \
  NS_IMETHOD Write(const char *buf, PRUint32 count, PRUint32 *_retval); \
  NS_IMETHOD Flush(void); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSIOUTPUTSTREAM(_to) \
  NS_IMETHOD Write(const char *buf, PRUint32 count, PRUint32 *_retval) { return _to ## Write(buf, count, _retval); } \
  NS_IMETHOD Flush(void) { return _to ## Flush(); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsOutputStream : public nsIOutputStream
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM

  nsOutputStream();
  virtual ~nsOutputStream();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsOutputStream, nsIOutputStream)

nsOutputStream::nsOutputStream()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsOutputStream::~nsOutputStream()
{
  /* destructor code */
}

/* unsigned long Write (in string buf, in unsigned long count); */
NS_IMETHODIMP nsOutputStream::Write(const char *buf, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Flush (); */
NS_IMETHODIMP nsOutputStream::Flush()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif


#endif /* __gen_nsIOutputStream_h__ */
