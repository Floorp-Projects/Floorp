/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef XP_MAC
#include "memory.h"
#else
#include "cstring"
#endif

#include "prmem.h"
#include "prprf.h"
#include "plstr.h"

#include "nsCacheObject.h"
#include "nsCacheTrace.h"
#include "nsCacheModule.h"
#include "nsCacheManager.h"
#include "nsStream.h"

/* 
 * nsCacheObject
 *
 * Gagan Saksena 02/02/98
 * 
 */

static const PRIntervalTime DEFAULT_EXPIRES = PR_SecondsToInterval(86400);

#if !defined(IS_LITTLE_ENDIAN) && !defined(IS_BIG_ENDIAN)
ERROR! Must have a byte order
#endif

#ifdef IS_LITTLE_ENDIAN
#define COPY_INT32(_a,_b)  memcpy(_a, _b, sizeof(int32))
#else
#define COPY_INT32(_a,_b)  /* swap */                   \
    do {                                                \
    ((char *)(_a))[0] = ((char *)(_b))[3];              \
    ((char *)(_a))[1] = ((char *)(_b))[2];              \
    ((char *)(_a))[2] = ((char *)(_b))[1];              \
    ((char *)(_a))[3] = ((char *)(_b))[0];              \
    } while(0)
#endif

/* Convenient macros used in stuffing and reading back cache object to a void * */
/* TODO: Change these to file scope functions */
#define STUFF_STRING(string)                 \
{                                            \
  len = (string ? PL_strlen(string)+1 : 0);  \
  COPY_INT32((void *)cur_ptr, &len);         \
  cur_ptr = cur_ptr + sizeof(int32);         \
  if(len)                                    \
      memcpy((void *)cur_ptr, string, len);  \
  cur_ptr += len;                            \
}

#define STUFF_NUMBER(number)                \
{                                           \
  COPY_INT32((void *)cur_ptr, &number);     \
  cur_ptr = cur_ptr + sizeof(PRUint32);        \
}

#define STUFF_TIME(number)                      \
{                                               \
  COPY_INT32((void *)cur_ptr, &number);         \
  cur_ptr = cur_ptr + sizeof(PRIntervalTime);   \
}

#define STUFF_BOOL(bool_val)                \
{                                           \
  if(bool_val)                              \
    ((char *)(cur_ptr))[0] = 1;             \
  else                                      \
    ((char *)(cur_ptr))[0] = 0;             \
  cur_ptr = cur_ptr + sizeof(char);         \
}

/* if any strings are larger than this then
 * there was a serious database error
 */
#define MAX_HUGE_STRING_SIZE 10000

#define RETRIEVE_STRING(string)     \
{                                   \
    if(cur_ptr > max_ptr)           \
    {                               \
        return PR_TRUE;             \
    }                               \
    COPY_INT32(&len, cur_ptr);      \
    cur_ptr += sizeof(int32);       \
    if(len)                         \
    {                               \
        if(len > MAX_HUGE_STRING_SIZE)  \
        {                               \
            return PR_FALSE;            \
        }                               \
        string = new char[len]; \
        if(!string)                     \
        {                               \
            return PR_FALSE;            \
        }                               \
        memcpy(string, cur_ptr, len);   \
        cur_ptr += len;                 \
   }                                    \
}

#define RETRIEVE_NUMBER(number)     \
{                                   \
    if(cur_ptr > max_ptr)           \
        return PR_TRUE;             \
    COPY_INT32(&number, cur_ptr);   \
    cur_ptr += sizeof(int32);       \
}

#define RETRIEVE_TIME(number)           \
{                                       \
    if(cur_ptr > max_ptr)               \
        return PR_TRUE;                 \
    COPY_INT32(&number, cur_ptr);       \
    cur_ptr += sizeof(PRIntervalTime);  \
}

#define RETRIEVE_BOOL(bool)         \
{                                   \
  if(cur_ptr > max_ptr)             \
    return PR_TRUE;                 \
  if(((char *)(cur_ptr))[0])        \
    bool = TRUE;                    \
  else                              \
    bool = FALSE;                   \
  cur_ptr += sizeof(char);          \
}

/*  TODO- Further optimization, initialize these to null, 
    but that will add more work on copy constructor which is ok */
nsCacheObject::nsCacheObject():
    m_Charset(new char[1]),
    m_ContentEncoding(new char[1]),
    m_ContentType(new char[1]),
    m_Etag(new char[1]),
    m_Filename(new char[1]),
    m_State(INIT), 
    m_bIsCompleted(PR_FALSE),
    m_Module(-1),
    m_pInfo(0),
    m_PageServicesURL(new char[1]),
    m_PostData(new char[1]),
    m_PostDataLen(0),
    m_pStream(0),
    m_URL(new char[1]) 
{
    Init();
    *m_Charset = '\0';
    *m_ContentEncoding = '\0';
    *m_ContentType = '\0';
    *m_Etag = '\0';
    *m_Filename = '\0';
    *m_PageServicesURL = '\0';
    *m_PostData = '\0';
    *m_URL = '\0';
}

nsCacheObject::~nsCacheObject() 
{
    if (m_Charset)
        delete[] m_Charset;
    if (m_ContentEncoding)
        delete[] m_ContentEncoding;
    if (m_ContentType)
        delete[] m_ContentType;
    if (m_Etag)
        delete[] m_Etag;
    if (m_Filename)
        delete[] m_Filename;
    if (m_PageServicesURL)
        delete[] m_PageServicesURL;
    if (m_PostData)
        delete[] m_PostData;
    if (m_URL)
        delete[] m_URL;
    if (m_pStream)
        delete m_pStream;
}

nsCacheObject::nsCacheObject(const nsCacheObject& another):
    m_Charset(new char[PL_strlen(another.m_Charset)+1]), 
    m_ContentEncoding(new char[PL_strlen(another.m_ContentEncoding)+1]),
    m_ContentType(new char[PL_strlen(another.m_ContentType)+1]),
    m_Etag(new char[PL_strlen(another.m_Etag)+1]),
    m_Filename(new char[PL_strlen(another.m_Filename)+1]),
    m_State(another.m_State),
    m_bIsCompleted(another.m_bIsCompleted),
    m_PageServicesURL(new char[PL_strlen(another.m_PageServicesURL)+1]),
    m_PostDataLen(another.m_PostDataLen),
    m_PostData(new char[another.m_PostDataLen+1]),
    m_URL(new char[PL_strlen(another.m_URL)+1]), 
    m_pStream(0),
    m_pInfo(0) /* Should this be copied as well? */
{
    PL_strncpyz(m_Charset, another.m_Charset, PL_strlen(another.m_Charset)+1);
    PL_strncpyz(m_ContentEncoding, another.m_ContentEncoding, PL_strlen(another.m_ContentEncoding)+1);
    PL_strncpyz(m_ContentType, another.m_ContentType, PL_strlen(another.m_ContentType)+1);
    PL_strncpyz(m_Etag, another.m_Etag, PL_strlen(another.m_Etag)+1);
    PL_strncpyz(m_Filename, another.m_Filename, PL_strlen(another.m_Filename)+1);
    PL_strncpyz(m_PageServicesURL, another.m_PageServicesURL, PL_strlen(another.m_PageServicesURL)+1);
    PL_strncpyz(m_PostData, another.m_PostData, another.m_PostDataLen+1);
    PL_strncpyz(m_URL, another.m_URL, PL_strlen(another.m_URL)+1);

    m_Hits = another.m_Hits;
    m_LastAccessed = another.m_LastAccessed;
    m_LastModified = another.m_LastModified;
    m_Size = another.m_Size;
    m_Module = another.m_Module;
}

nsCacheObject::nsCacheObject(const char* i_url):
    m_Charset(new char[1]),
    m_ContentEncoding(new char[1]),
    m_ContentType(new char[1]),
    m_Etag(new char[1]),
    m_Filename(new char[1]),
    m_State(INIT), 
    m_bIsCompleted(PR_FALSE),
    m_PageServicesURL(new char[1]),
    m_PostData(new char[1]),
    m_PostDataLen(0),
    m_URL(new char[PL_strlen(i_url)+1]), 
    m_Module(-1),
    m_pStream(0),
    m_pInfo(0)
{
    Init();
    PR_ASSERT(i_url);
    PL_strncpyz(m_URL, i_url, PL_strlen(i_url)+1);

    *m_Charset = '\0';
    *m_ContentEncoding = '\0';
    *m_ContentType = '\0';
    *m_Etag = '\0';
    *m_Filename = '\0';
    *m_PageServicesURL = '\0';
    *m_PostData = '\0';
}

void nsCacheObject::Address(const char* i_url) 
{
    PR_ASSERT(i_url && *i_url);
    if (!i_url)
        return;
    if (m_URL)
        delete[] m_URL;
    int len = PL_strlen(i_url);
    m_URL = new char[len + 1];
    PL_strncpyz(m_URL, i_url, len+1);
}

void nsCacheObject::Charset(const char* i_Charset) 
{
//    PR_ASSERT(i_Charset && *i_Charset);
    if (!i_Charset) //TODO reset m_charset here 
        return;
    if (m_Charset)
        delete[] m_Charset;
    int len = PL_strlen(i_Charset);
    m_Charset = new char[len + 1];
    PL_strncpyz(m_Charset, i_Charset, len+1);
}

void nsCacheObject::ContentEncoding(const char* i_Encoding) 
{
//    PR_ASSERT(i_Encoding && *i_Encoding);
    if (!i_Encoding)
        return;
    if (m_ContentEncoding)
        delete[] m_ContentEncoding;
    int len = PL_strlen(i_Encoding);
    m_ContentEncoding = new char[len + 1];
    PL_strncpyz(m_ContentEncoding, i_Encoding, len+1);
}

void nsCacheObject::ContentType(const char* i_Type) 
{
//    PR_ASSERT(i_Type && *i_Type);
    if (!i_Type)
    {
        /* Reset to empty */ // TODO ??
        return;
    }
    if (m_ContentType)
        delete[] m_ContentType;
    int len = PL_strlen(i_Type);
    m_ContentType = new char[len + 1];
    PL_strncpyz(m_ContentType, i_Type, len+1);
}

void nsCacheObject::Etag(const char* i_etag) 
{
//    PR_ASSERT(i_etag && *i_etag);
    if (!i_etag)
        return;
    if (m_Etag)
        delete[] m_Etag;
    m_Etag = new char[PL_strlen(i_etag) + 1];
    PL_strncpyz(m_Etag, i_etag, PL_strlen(i_etag)+1);
}

void nsCacheObject::Filename(const char* i_Filename)
{
//    PR_ASSERT(i_Filename && *i_Filename);
    if (!i_Filename)
        return;
    if (m_Filename)
        delete[] m_Filename;
    int len = PL_strlen(i_Filename);
    m_Filename = new char[len +1];
    PL_strncpyz(m_Filename, i_Filename, len+1);
}

void* nsCacheObject::Info(void) const
{
    if (m_pInfo)
        return m_pInfo;
    
    nsCacheObject* pThis = (nsCacheObject*) this;
    if (m_URL){

        pThis->m_info_size = sizeof(nsCacheObject);
        
        pThis->m_info_size -= sizeof(void*); // m_info itself is not being serialized
        pThis->m_info_size -= sizeof(char*); // neither is m_PostData
        pThis->m_info_size -= sizeof(nsStream*); // nor the stream
        pThis->m_info_size -= sizeof(PRBool); // bIsCompleted. 
        //todo -optimize till here

        //Add the strings sizes
        pThis->m_info_size += PL_strlen(m_Charset)+1;
        pThis->m_info_size += PL_strlen(m_ContentEncoding)+1;
        pThis->m_info_size += PL_strlen(m_ContentType)+1;
        pThis->m_info_size += PL_strlen(m_Etag)+1;
        pThis->m_info_size += PL_strlen(m_Filename)+1;
        pThis->m_info_size += PL_strlen(m_PageServicesURL)+1;
        pThis->m_info_size += PL_strlen(m_URL)+1;
        
        //Add the Postdata len
        pThis->m_info_size += m_PostDataLen+1;

        void* new_obj = PR_Calloc(1, m_info_size * sizeof(char));
     
        if (!new_obj)
        {
            PR_Free(new_obj);
            return 0;
        }

        PRUint32 len;

        char* cur_ptr = (char*) new_obj;
        /* put the total size of the struct into
         * the first field so that we have
         * a cross check against corruption
         */
        COPY_INT32((void *)cur_ptr, &m_info_size);
        cur_ptr += sizeof(PRUint32);

        /* put the version number of the structure
         * format that we are using. By using a version 
         * string when writting we can support 
         * backwards compatibility in our reading code
         */
        COPY_INT32((void *)cur_ptr, &kCACHE_VERSION);
        cur_ptr += sizeof(PRUint32);

        STUFF_STRING(m_Charset);
        STUFF_STRING(m_ContentEncoding);
        STUFF_NUMBER(m_ContentLength);
        STUFF_STRING(m_ContentType);
        STUFF_STRING(m_Etag);
        STUFF_TIME(m_Expires);
        STUFF_STRING(m_Filename);
        STUFF_NUMBER(m_Hits);
        STUFF_TIME(m_LastAccessed);
        STUFF_TIME(m_LastModified);
        STUFF_NUMBER(m_Module);
        STUFF_STRING(m_PageServicesURL);
        STUFF_NUMBER(m_PostDataLen);
        /* There is a possibility of it not being a string! */
        if (m_PostData)
        {
            memcpy(cur_ptr, m_PostData, m_PostDataLen+1);
            cur_ptr += m_PostDataLen+1;
        }
        STUFF_NUMBER(m_Size);
        STUFF_NUMBER(m_State);
        STUFF_STRING(m_URL);

        // Important Assertion. Dont remove!
        // If this fails then you or somebody has added a variable to the 
        // nsCacheObject class and a decision on its "cacheability" has
        // not yet been made. 
        PR_ASSERT(cur_ptr == (char*) new_obj + m_info_size);
        pThis->m_pInfo = new_obj;
    }
    return m_pInfo;
}

PRBool nsCacheObject::Info(void* i_data) 
{
    if (!i_data)
        return PR_FALSE;
    
    char* cur_ptr = (char*) i_data;

    //Reset the m_pInfo;
    if (m_pInfo)
    {
        PR_Free(m_pInfo);
        m_pInfo = 0;
    }

    //Reset all strings
    if (m_Charset)
        delete[] m_Charset;
    if (m_ContentEncoding)
        delete[] m_ContentEncoding;
    if (m_ContentType)
        delete[] m_ContentType;
    if (m_Etag)
        delete[] m_Etag;
    if (m_Filename)
        delete[] m_Filename;
    if (m_PageServicesURL)
        delete[] m_PageServicesURL;
    if (m_URL)
        delete[] m_URL;
    if (m_PostData)
        delete[] m_PostData;

    m_PostDataLen = 0;

    COPY_INT32(&m_info_size, cur_ptr);
    char* max_ptr = cur_ptr + m_info_size;
    cur_ptr += sizeof(PRUint32);

    PRUint32 version;
    COPY_INT32(&version, cur_ptr);
    cur_ptr += sizeof(PRUint32);

    PR_ASSERT(version == kCACHE_VERSION);
    if (version != kCACHE_VERSION)
    {
        //TODO Bad cache version
        return PR_FALSE;
    }

    PRUint32 len;
    
    RETRIEVE_STRING(m_Charset);
    RETRIEVE_STRING(m_ContentEncoding);
    RETRIEVE_NUMBER(m_ContentLength);
    RETRIEVE_STRING(m_ContentType);
    RETRIEVE_STRING(m_Etag);
    RETRIEVE_TIME(m_Expires);
    RETRIEVE_STRING(m_Filename);
    RETRIEVE_NUMBER(m_Hits);
    RETRIEVE_TIME(m_LastAccessed);
    RETRIEVE_TIME(m_LastModified);
    RETRIEVE_NUMBER(m_Module);
    RETRIEVE_STRING(m_PageServicesURL);
    RETRIEVE_NUMBER(m_PostDataLen);
    // Special case-
    m_PostData = new char[m_PostDataLen + 1];
    if (m_PostData)
    {
        memcpy(m_PostData, cur_ptr, m_PostDataLen+1);
    }
    cur_ptr += m_PostDataLen +1;

    RETRIEVE_NUMBER(m_Size);
    RETRIEVE_NUMBER(m_State);
    RETRIEVE_STRING(m_URL);
 
    // Most important assertion! Don't ever remove!
    PR_ASSERT(cur_ptr == max_ptr);

    // Since we are reading off its info from an indexed entry
    m_bIsCompleted = PR_TRUE;

    return PR_TRUE;
}

PRUint32 nsCacheObject::InfoSize(void) const
{
    if (!m_pInfo)
    {
        ((nsCacheObject*) this)->Info();
    }
    return m_info_size;
}

void nsCacheObject::Init() 
{
    m_Size = 0;
    m_Expires = PR_IntervalNow() + DEFAULT_EXPIRES;
    m_Hits = 0;
}

void nsCacheObject::PageServicesURL(const char* i_Url) 
{
//    PR_ASSERT(i_Url && *i_Url);
    if (!i_Url)
        return;
    if (m_PageServicesURL)
        delete[] m_PageServicesURL;
    m_PageServicesURL = new char[PL_strlen(i_Url) + 1];
    PL_strncpyz(m_PageServicesURL, i_Url, PL_strlen(i_Url)+1);
}

void nsCacheObject::PostData(const char* i_data, const PRUint32 i_Len)
{
    if (!i_data || (0 ==i_Len))
        return;
    if (m_PostData)
        delete[] m_PostData;
    m_PostData = new char[i_Len+1];
    PL_strncpyz(m_PostData, i_data, i_Len+1);
    m_PostDataLen = i_Len;
}

PRUint32 nsCacheObject::Read(char* o_Buffer, PRUint32 len)
{
    if (!m_pStream)
    {
        PR_ASSERT(m_Module >=0);
        if (0 <= m_Module)
        {
            nsCacheModule* pModule = nsCacheManager::GetInstance()->GetModule(m_Module);
            if (pModule)
            {
                m_pStream = pModule->GetStreamFor(this);
                if (m_pStream)
                {
                    return m_pStream->Read(o_Buffer, len);
                }
            }
        }
        return 0;
    }
    return m_pStream->Read(o_Buffer, len);
}

void nsCacheObject::Reset()
{
    if (m_pStream)
        m_pStream->Reset();
    // TODO change states too.
}

#if 0
/* Caller must free returned string */
// TODO  change to use PR_stuff...
const char* nsCacheObject::Trace() const
{
    char linebuffer[256];
    char* total;

    PR_Sprintf(linebuffer, "nsCacheObject:URL=%s,SIZE=%d,ET=%s,\n\tLM=%d,LA=%d,EXP=%d,HITS=%d\n", 
        m_URL, 
        m_Size,
        m_Etag,
        m_LastModified,
        m_LastAccessed,
        m_Expires,
        m_Hits);

    total = new char[PL_strlen(linebuffer) +1];
    PL_strcpy(total, linebuffer);

    return total;
}
#endif

PRUint32 nsCacheObject::Write(const char* i_Buffer, const PRUint32 len)
{
    PRUint32 amountWritten = 0;
    if (!m_pStream)
    {
        PR_ASSERT(m_Module >=0);
        if (0 <= m_Module)
        {
            nsCacheModule* pModule = nsCacheManager::GetInstance()->GetModule(m_Module);
            if (pModule)
            {   
                m_pStream = pModule->GetStreamFor(this);
                PR_ASSERT(m_pStream);
                if (!m_pStream)
                    return 0;
            }
        }
        else
            return 0;
    }
    amountWritten = m_pStream->Write(i_Buffer, len);
    m_Size += amountWritten;
    return amountWritten;
}
