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
#include <xp_core.h>
#include <xp_str.h>
#include <xpassert.h>

#include <prmem.h>
#include <prprf.h>
#include <plstr.h>

#include "nsCacheObject.h"
#include "nsCacheTrace.h"

/* 
 * nsCacheObject
 *
 * Gagan Saksena 02/02/98
 * 
 */

static const PRIntervalTime DEFAULT_EXPIRES = PR_SecondsToInterval(86400);

// m_Etag, m_Expires, m_Flags, m_Hits, m_LastAccessed, m_LastModified, m_Module, m_Size, m_Url
#define READ_WRITE_FORMAT "%s,%d,%d,%d,%d,%d,%d,%d,%s" 
#define READ_WRITE_ELEMENTS m_Etag,m_Expires,m_Flags,m_Hits,m_LastAccessed,m_LastModified,m_Module,m_Size,m_Url

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
        string = (char*)PR_Malloc(len); \
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

nsCacheObject::nsCacheObject():
    m_Flags(INIT), 
    m_Url(new char[1]), 
    m_Etag(new char[1]),
    m_Module(-1),
    m_pInfo(0)
{
    Init();
    *m_Url = '\0';
    *m_Etag = '\0';
}

nsCacheObject::~nsCacheObject() 
{
    delete[] m_Url;
    delete[] m_Etag;
}

nsCacheObject::nsCacheObject(const nsCacheObject& another):
    m_Flags(another.m_Flags),
    m_Url(new char[PL_strlen(another.m_Url)+1]), 
    m_Etag(new char[PL_strlen(another.m_Etag)+1]),
    m_pInfo(0)
{
    strcpy(m_Url, another.m_Url);
    strcpy(m_Etag, another.m_Etag);

    m_Hits = another.m_Hits;
    m_LastAccessed = another.m_LastAccessed;
    m_LastModified = another.m_LastModified;
    m_Size = another.m_Size;
    m_Module = another.m_Module;
}

nsCacheObject::nsCacheObject(const char* i_url):
    m_Flags(INIT), 
    m_Url(new char[strlen(i_url)+1]), 
    m_Etag(new char[1]),
    m_Module(-1),
    m_pInfo(0)
{
    Init();
    PR_ASSERT(i_url);
    strcpy(m_Url, i_url);
    *m_Etag = '\0';
}

void nsCacheObject::Address(const char* i_url) 
{
    PR_ASSERT(i_url && *i_url);
    if (m_Url)
        delete[] m_Url;
    m_Url = new char[strlen(i_url) + 1];
    strcpy(m_Url, i_url);
}

void nsCacheObject::Etag(const char* i_etag) 
{
    PR_ASSERT(i_etag && *i_etag);
    if (m_Etag)
        delete[] m_Etag;
    m_Etag = new char[strlen(i_etag) + 1];
    strcpy(m_Etag, i_etag);
}

void* nsCacheObject::Info(void) const
{
    if (m_pInfo)
        return m_pInfo;
    
    nsCacheObject* pThis = (nsCacheObject*) this;
    if (m_Url){
/*
        char* tmpBuff = PR_smprintf(READ_WRITE_FORMAT, READ_WRITE_ELEMENTS);
        int tmpLen = PL_strlen(tmpBuff);
        pThis->m_pInfo = PR_Malloc(tmpLen);
        memcpy(pThis->m_pInfo, tmpBuff, tmpLen);
        PR_Free(tmpBuff);
*/
        pThis->m_info_size = sizeof(nsCacheObject);
        pThis->m_info_size -= sizeof(void*); //m_info is not being serialized

        //Add the strings sizes
        pThis->m_info_size += PL_strlen(m_Etag)+1;
        pThis->m_info_size += PL_strlen(m_Url)+1;
        
        void* new_obj = PR_Malloc(m_info_size * sizeof(char));
     
        memset(new_obj, 0, m_info_size *sizeof(char));

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

        STUFF_STRING(m_Etag);
        STUFF_TIME(m_Expires);
        STUFF_NUMBER(m_Flags);
        STUFF_NUMBER(m_Hits);
        STUFF_TIME(m_LastAccessed);
        STUFF_TIME(m_LastModified);
        STUFF_NUMBER(m_Module);
        STUFF_NUMBER(m_Size);
        STUFF_STRING(m_Url);

        // Important Assertion. Dont remove!
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
    PR_FREEIF(m_pInfo);

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
    //m_Etag,m_Expires,m_Flags,m_Hits,m_LastAccessed,m_LastModified,m_Module,m_Size,m_Url
    RETRIEVE_STRING(m_Etag);
    RETRIEVE_TIME(m_Expires);
    RETRIEVE_NUMBER(m_Flags);
    RETRIEVE_NUMBER(m_Hits);
    RETRIEVE_TIME(m_LastAccessed);
    RETRIEVE_TIME(m_LastModified);
    RETRIEVE_NUMBER(m_Module);
    RETRIEVE_NUMBER(m_Size);
    RETRIEVE_STRING(m_Url);
 
    // Most important assertion! Don't ever remove!
    PR_ASSERT(cur_ptr == max_ptr);
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
    m_Expires = PR_IntervalNow() + DEFAULT_EXPIRES;
    m_Hits = 0;
}

/* Caller must free returned string */
// TODO  change to use PR_stuff...
const char* nsCacheObject::Trace() const
{
    char linebuffer[256];
    char* total;

    sprintf(linebuffer, "nsCacheObject:URL=%s,SIZE=%d,ET=%s,\n\tLM=%d,LA=%d,EXP=%d,HITS=%d\n", 
        m_Url, 
        m_Size,
        m_Etag,
        m_LastModified,
        m_LastAccessed,
        m_Expires,
        m_Hits);

    total = new char[PL_strlen(linebuffer) +1];
    strcpy(total, linebuffer);

    return total;
}
