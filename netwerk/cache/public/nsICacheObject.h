#ifndef _NS_CACHEOBJECT_H_
#define _NS_CACHEOBJECT_H_

#include "nsISupports.h"
#include "nsStream.h"

#include "prtypes.h"
#include "prinrval.h"

// nsICacheObject {A2D9A8A0-414B-11d3-87EF-000629D01344}
#define NS_ICACHEOBJECT_IID \
 {0xa2d9a8a0, 0x414b, 0x11d3, \
  {0x87, 0xef, 0x0, 0x6, 0x29, 0xd0, 0x13, 0x44 }}

// nsCacheObject {A2D9A8A1-414B-11d3-87EF-000629D01344}
#define NS_CACHEOBJECT_CID \
 {0xa2d9a8a1, 0x414b, 0x11d3, \
  {0x87, 0xef, 0x0, 0x6, 0x29, 0xd0, 0x13, 0x44 }}


class nsICacheObject :public nsISupports
{
  public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICACHEOBJECT_IID);

  /* Cache Object- check nsCacheObject.h for details on these functions */

  /* This is added because we need to initialize a new cacheobject 
  NS_IMETHOD InitUrl(const char * i_url) = 0 ;
  */

  NS_IMETHOD GetAddress(char ** Addr) const = 0 ;
  NS_IMETHOD SetAddress(const char* i_Address) = 0 ;

  NS_IMETHOD GetCharset(char ** CSet) const = 0 ;
  NS_IMETHOD SetCharset(const char* i_CharSet) = 0 ;

  NS_IMETHOD GetContentEncoding(char ** Encoding) const = 0 ;
  NS_IMETHOD SetContentEncoding(const char* i_Encoding) = 0 ;

  NS_IMETHOD GetContentLength(PRUint32 * CLeng) const = 0 ;
  NS_IMETHOD SetContentLength(PRUint32 i_Len) = 0 ;

  NS_IMETHOD GetContentType(char ** CType) const = 0 ;
  NS_IMETHOD SetContentType(const char* i_Type) = 0 ;

  NS_IMETHOD GetEtag(char ** Etag) const = 0 ;
  NS_IMETHOD SetEtag(const char* i_Etag) = 0 ;

  NS_IMETHOD GetExpires(PRIntervalTime * iTime) const = 0 ;
  NS_IMETHOD SetExpires(const PRIntervalTime i_Time) = 0 ;

  NS_IMETHOD GetFilename(char ** Filename) const = 0 ;
  NS_IMETHOD SetFilename(const char* i_Filename) = 0 ;

  NS_IMETHOD GetIsCompleted(PRBool * bComplete) const = 0 ;
  NS_IMETHOD SetIsCompleted(PRBool bComplete) = 0 ;

  NS_IMETHOD GetLastAccessed(PRIntervalTime * iTime) const = 0 ;
  NS_IMETHOD SetLastModified(const PRIntervalTime i_Time) = 0 ;

  NS_IMETHOD GetLastModified(PRIntervalTime * iTime) const = 0 ;

  NS_IMETHOD GetModuleIndex(PRInt16 * m_index) const = 0 ;
  NS_IMETHOD SetModuleIndex(const PRUint16 m_index) = 0 ;

  NS_IMETHOD GetPageServicesURL(char ** o_url) const = 0 ;
  NS_IMETHOD SetPageServicesURL(const char* i_Url) = 0 ;

  NS_IMETHOD GetPostData(char ** pData) const = 0 ;
  NS_IMETHOD SetPostData(const char* i_PostData, const PRUint32 i_Len) = 0 ;

  NS_IMETHOD GetPostDataLen(PRUint32 * dLeng) const = 0 ;

  NS_IMETHOD GetSize(PRUint32 * pSize) const = 0 ;
  NS_IMETHOD SetSize(const PRUint32 i_Size) = 0 ;

  NS_IMETHOD GetState(PRUint32 * pState) const = 0 ;
  NS_IMETHOD SetState(const PRUint32 i_State) = 0 ;

  NS_IMETHOD GetStream(nsStream ** pStream) const = 0 ;
//  NS_IMETHOD MakeStream(void) = 0 ;

  NS_IMETHOD Hits(PRUint32 * pHits) const = 0 ;

  NS_IMETHOD IsExpired(PRBool * bGet) const = 0 ;

  NS_IMETHOD IsPartial(PRBool * bGet) const = 0;

  NS_IMETHOD Read(char* o_Destination, PRUint32 i_Len, PRUint32 * pLeng) = 0 ;

  NS_IMETHOD Reset(void) = 0 ;

  NS_IMETHOD Write(const char* i_buffer, const PRUint32 i_length, 
                                                      PRUint32 * oLeng) = 0 ;

  /* Read and write info about this cache object */
  NS_IMETHOD GetInfo(void** o_info)=0 ;
  NS_IMETHOD SetInfo(void* i_info /*, PRUint32 len */)=0;

  NS_IMETHOD GetInfoSize(PRUint32* o_size)=0 ;

} ;

#endif // _NSICACHEOBJECT_H_
