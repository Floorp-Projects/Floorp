/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * secport.c - portability interfaces for security libraries
 *
 * This file abstracts out libc functionality that libsec depends on
 * 
 * NOTE - These are not public interfaces
 *
 * $Id: secport.c,v 1.4 2000/09/20 17:07:22 relyea%netscape.com Exp $
 */

#include "seccomon.h"
#include "prmem.h"
#include "prerror.h"
#include "plarena.h"
#include "secerr.h"
#include "prmon.h"
#include "nsslocks.h"
#include "secport.h"

#ifdef DEBUG
#define THREADMARK
#endif /* DEBUG */

#ifdef THREADMARK
#include "prthread.h"
#endif /* THREADMARK */

#if defined(XP_UNIX) || defined(XP_MAC) || defined(XP_OS2)
#include <stdlib.h>
#else
#include "wtypes.h"
#endif

#define SET_ERROR_CODE	/* place holder for code to set PR error code. */

#ifdef THREADMARK
typedef struct threadmark_mark_str {
  struct threadmark_mark_str *next;
  void *mark;
} threadmark_mark;

typedef struct threadmark_arena_str {
  PLArenaPool arena;
  PRUint32 magic;
  PRThread *marking_thread;
  threadmark_mark *first_mark;
} threadmark_arena;

#define THREADMARK_MAGIC 0xB8AC9BDD /* In honor of nelsonb */
#endif /* THREADMARK */

/* count of allocation failures. */
unsigned long port_allocFailures;

/* locations for registering Unicode conversion functions.  
 * XXX is this the appropriate location?  or should they be
 *     moved to client/server specific locations?
 */
PORTCharConversionFunc ucs4Utf8ConvertFunc;
PORTCharConversionFunc ucs2Utf8ConvertFunc;
PORTCharConversionWSwapFunc  ucs2AsciiConvertFunc;

void *
PORT_Alloc(size_t bytes)
{
    void *rv;

    /* Always allocate a non-zero amount of bytes */
    rv = (void *)PR_Malloc(bytes ? bytes : 1);
    if (!rv) {
	++port_allocFailures;
	PORT_SetError(SEC_ERROR_NO_MEMORY);
    }
    return rv;
}

void *
PORT_Realloc(void *oldptr, size_t bytes)
{
    void *rv;

    rv = (void *)PR_Realloc(oldptr, bytes);
    if (!rv) {
	++port_allocFailures;
	PORT_SetError(SEC_ERROR_NO_MEMORY);
    }
    return rv;
}

#ifdef XP_MAC
char *
PORT_Strdup(const char *cp)
{
	size_t len = PORT_Strlen(cp);
	char *buf;

	buf = (char *)PORT_Alloc(len+1);
	if (buf == NULL) return;

	PORT_Memcpy(buf,cp,len+1);
	return buf;
}
#endif


void *
PORT_ZAlloc(size_t bytes)
{
    void *rv;

    /* Always allocate a non-zero amount of bytes */
    rv = (void *)PR_Calloc(1, bytes ? bytes : 1);
    if (!rv) {
	++port_allocFailures;
	PORT_SetError(SEC_ERROR_NO_MEMORY);
    }
    return rv;
}

void
PORT_Free(void *ptr)
{
    if (ptr) {
	PR_Free(ptr);
    }
}

void
PORT_ZFree(void *ptr, size_t len)
{
    if (ptr) {
	memset(ptr, 0, len);
	PR_Free(ptr);
    }
}

void
PORT_SetError(int value)
{	
    PR_SetError(value, 0);
    return;
}

int
PORT_GetError(void)
{
    return(PR_GetError());
}

/********************* Arena code follows *****************************/

PRMonitor * arenaMonitor;

static void
getArenaLock(void)
{
    if (!arenaMonitor) {
	nss_InitMonitor(&arenaMonitor);
    }
    if (arenaMonitor) 
	PR_EnterMonitor(arenaMonitor);
}

static void
releaseArenaLock(void)
{
    if (arenaMonitor) 
	PR_ExitMonitor(arenaMonitor);
}

PLArenaPool *
PORT_NewArena(unsigned long chunksize)
{
    PLArenaPool *arena;
    
    getArenaLock();
#ifdef THREADMARK
    {
      threadmark_arena *tarena = (threadmark_arena *)
        PORT_ZAlloc(sizeof(threadmark_arena));
      if( (threadmark_arena *)NULL != tarena ) {
       arena = &tarena->arena;
       tarena->magic = THREADMARK_MAGIC;
      } else {
        arena = (PLArenaPool *)NULL;
      }
    }
#else /* THREADMARK */
    arena = (PLArenaPool*)PORT_ZAlloc(sizeof(PLArenaPool));
#endif /* THREADMARK */
    if ( arena != NULL ) {
	PL_InitArenaPool(arena, "security", chunksize, sizeof(double));
    }
    releaseArenaLock();
    return(arena);
}

void *
PORT_ArenaAlloc(PLArenaPool *arena, size_t size)
{
    void *p;

    getArenaLock();
#ifdef THREADMARK
    {
      /* Is it one of ours?  Assume so and check the magic */
      threadmark_arena *tarena = (threadmark_arena *)arena;
      if( THREADMARK_MAGIC == tarena->magic ) {
        /* Most likely one of ours.  Is there a thread id? */
        if( (PRThread *)NULL != tarena->marking_thread ) {
          /* Yes.  Has this arena been marked by this thread? */
          if( tarena->marking_thread == PR_GetCurrentThread() ) {
            /* Yup.  Everything's okay. */
            ;
          } else {
            /* Nope.  BZZT!  error */
            releaseArenaLock();
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            PORT_Assert(0);
            return (void *)NULL;
          }
        } /* tid != null */
      } /* tarena */
    } /* scope */
#endif /* THREADMARK */

    PL_ARENA_ALLOCATE(p, arena, size);
    releaseArenaLock();
    if (p == NULL) {
	++port_allocFailures;
	PORT_SetError(SEC_ERROR_NO_MEMORY);
    }

    return(p);
}

void *
PORT_ArenaZAlloc(PLArenaPool *arena, size_t size)
{
    void *p;

    getArenaLock();
#ifdef THREADMARK
    {
      /* Is it one of ours?  Assume so and check the magic */
      threadmark_arena *tarena = (threadmark_arena *)arena;
      if( THREADMARK_MAGIC == tarena->magic ) {
        /* Most likely one of ours.  Is there a thread id? */
        if( (PRThread *)NULL != tarena->marking_thread ) {
          /* Yes.  Has this arena been marked by this thread? */
          if( tarena->marking_thread == PR_GetCurrentThread() ) {
            /* Yup.  Everything's okay. */
            ;
          } else {
            /* No, it was a different thread  BZZT!  error */
            releaseArenaLock();
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            PORT_Assert(0);
            return (void *)NULL;
          }
        } /* tid != null */
      } /* tarena */
    } /* scope */
#endif /* THREADMARK */

    PL_ARENA_ALLOCATE(p, arena, size);
    releaseArenaLock();
    if (p == NULL) {
	++port_allocFailures;
	PORT_SetError(SEC_ERROR_NO_MEMORY);
    } else {
	PORT_Memset(p, 0, size);
    }

    return(p);
}

/* XXX - need to zeroize!! - jsw */
void
PORT_FreeArena(PLArenaPool *arena, PRBool zero)
{
    getArenaLock();
    PL_FinishArenaPool(arena);
    PORT_Free(arena);
    releaseArenaLock();
}

void *
PORT_ArenaGrow(PLArenaPool *arena, void *ptr, size_t oldsize, size_t newsize)
{
    PORT_Assert(newsize >= oldsize);
    
    getArenaLock();
    /* Do we do a THREADMARK check here? */
    PL_ARENA_GROW(ptr, arena, oldsize, ( newsize - oldsize ) );
    releaseArenaLock();
    
    return(ptr);
}

void *
PORT_ArenaMark(PLArenaPool *arena)
{
    void * result;

    getArenaLock();
#ifdef THREADMARK
    {
      threadmark_mark *tm, **pw;

      threadmark_arena *tarena = (threadmark_arena *)arena;
      if( THREADMARK_MAGIC == tarena->magic ) {
        /* one of ours */
        if( (PRThread *)NULL == tarena->marking_thread ) {
          /* First mark */
          tarena->marking_thread = PR_GetCurrentThread();
        } else {
          if( PR_GetCurrentThread() != tarena->marking_thread ) {
            releaseArenaLock();
            PORT_SetError(SEC_ERROR_NO_MEMORY);
            PORT_Assert(0);
            return (void *)NULL;
          }
        }

        result = PL_ARENA_MARK(arena);
        PL_ARENA_ALLOCATE(tm, arena, sizeof(threadmark_mark));
        if( (threadmark_mark *)NULL == tm ) {
          releaseArenaLock();
          PORT_SetError(SEC_ERROR_NO_MEMORY);
          return (void *)NULL;
        }

        tm->mark = result;
        tm->next = (threadmark_mark *)NULL;

        pw = &tarena->first_mark;
        while( (threadmark_mark *)NULL != *pw ) {
             pw = &(*pw)->next;
        }

        *pw = tm;
      } else {
        /* a "pure" NSPR arena */
        result = PL_ARENA_MARK(arena);
      }
    }
#else /* THREADMARK */
    result = PL_ARENA_MARK(arena);
#endif /* THREADMARK */
    releaseArenaLock();
    return result;
}

void
PORT_ArenaRelease(PLArenaPool *arena, void *mark)
{
    getArenaLock();
#ifdef THREADMARK
    {
      threadmark_arena *tarena = (threadmark_arena *)arena;
      if( THREADMARK_MAGIC == tarena->magic ) {
        threadmark_mark **pw, *tm;

        if( PR_GetCurrentThread() != tarena->marking_thread ) {
          releaseArenaLock();
          PORT_SetError(SEC_ERROR_NO_MEMORY);
          PORT_Assert(0);
          return /* no error indication available */ ;
        }

        pw = &tarena->first_mark;
        while( ((threadmark_mark *)NULL != *pw) && (mark != (*pw)->mark) ) {
          pw = &(*pw)->next;
        }

        if( (threadmark_mark *)NULL == *pw ) {
          /* bad mark */
          releaseArenaLock();
          PORT_SetError(SEC_ERROR_NO_MEMORY);
          PORT_Assert(0);
          return /* no error indication available */ ;
        }

        tm = *pw;
        *pw = (threadmark_mark *)NULL;

        PL_ARENA_RELEASE(arena, mark);

        if( (threadmark_mark *)NULL == tarena->first_mark ) {
          tarena->marking_thread = (PRThread *)NULL;
        }
      } else {
        PL_ARENA_RELEASE(arena, mark);
      }
    }
#else /* THREADMARK */
    PL_ARENA_RELEASE(arena, mark);
#endif /* THREADMARK */
    releaseArenaLock();
}

void
PORT_ArenaUnmark(PLArenaPool *arena, void *mark)
{
#ifdef THREADMARK
    getArenaLock();
    {
      threadmark_arena *tarena = (threadmark_arena *)arena;
      if( THREADMARK_MAGIC == tarena->magic ) {
        threadmark_mark **pw, *tm;

        if( PR_GetCurrentThread() != tarena->marking_thread ) {
          releaseArenaLock();
          PORT_SetError(SEC_ERROR_NO_MEMORY);
          PORT_Assert(0);
          return /* no error indication available */ ;
        }

        pw = &tarena->first_mark;
        while( ((threadmark_mark *)NULL != *pw) && (mark != (*pw)->mark) ) {
          pw = &(*pw)->next;
        }

        if( (threadmark_mark *)NULL == *pw ) {
          /* bad mark */
          releaseArenaLock();
          PORT_SetError(SEC_ERROR_NO_MEMORY);
          PORT_Assert(0);
          return /* no error indication available */ ;
        }

        tm = *pw;
        *pw = (threadmark_mark *)NULL;

        if( (threadmark_mark *)NULL == tarena->first_mark ) {
          tarena->marking_thread = (PRThread *)NULL;
        }
      } else {
        PL_ARENA_RELEASE(arena, mark);
      }
    }
    releaseArenaLock();
#endif /* THREADMARK */
}

char *
PORT_ArenaStrdup(PLArenaPool *arena, char *str) {
    int len = PORT_Strlen(str)+1;
    char *newstr;

    getArenaLock();
    newstr = (char*)PORT_ArenaAlloc(arena,len);
    releaseArenaLock();
    if (newstr) {
        PORT_Memcpy(newstr,str,len);
    }
    return newstr;
}

/********************** end of arena functions ***********************/

/****************** unicode conversion functions ***********************/
/*
 * NOTE: These conversion functions all assume that the multibyte
 * characters are going to be in NETWORK BYTE ORDER, not host byte
 * order.  This is because the only time we deal with UCS-2 and UCS-4
 * are when the data was received from or is going to be sent out
 * over the wire (in, e.g. certificates).
 */

void
PORT_SetUCS4_UTF8ConversionFunction(PORTCharConversionFunc convFunc)
{ 
    ucs4Utf8ConvertFunc = convFunc;
}

void
PORT_SetUCS2_ASCIIConversionFunction(PORTCharConversionWSwapFunc convFunc)
{ 
    ucs2AsciiConvertFunc = convFunc;
}

void
PORT_SetUCS2_UTF8ConversionFunction(PORTCharConversionFunc convFunc)
{ 
    ucs2Utf8ConvertFunc = convFunc;
}

PRBool 
PORT_UCS4_UTF8Conversion(PRBool toUnicode, unsigned char *inBuf,
			 unsigned int inBufLen, unsigned char *outBuf,
			 unsigned int maxOutBufLen, unsigned int *outBufLen)
{
    if(!ucs4Utf8ConvertFunc) {
      return sec_port_ucs4_utf8_conversion_function(toUnicode,
        inBuf, inBufLen, outBuf, maxOutBufLen, outBufLen);
    }

    return (*ucs4Utf8ConvertFunc)(toUnicode, inBuf, inBufLen, outBuf, 
				  maxOutBufLen, outBufLen);
}

PRBool 
PORT_UCS2_UTF8Conversion(PRBool toUnicode, unsigned char *inBuf,
			 unsigned int inBufLen, unsigned char *outBuf,
			 unsigned int maxOutBufLen, unsigned int *outBufLen)
{
    if(!ucs2Utf8ConvertFunc) {
      return sec_port_ucs2_utf8_conversion_function(toUnicode,
        inBuf, inBufLen, outBuf, maxOutBufLen, outBufLen);
    }

    return (*ucs2Utf8ConvertFunc)(toUnicode, inBuf, inBufLen, outBuf, 
				  maxOutBufLen, outBufLen);
}

PRBool 
PORT_UCS2_ASCIIConversion(PRBool toUnicode, unsigned char *inBuf,
			  unsigned int inBufLen, unsigned char *outBuf,
			  unsigned int maxOutBufLen, unsigned int *outBufLen,
			  PRBool swapBytes)
{
    if(!ucs2AsciiConvertFunc) {
	return PR_FALSE;
    }

    return (*ucs2AsciiConvertFunc)(toUnicode, inBuf, inBufLen, outBuf, 
				  maxOutBufLen, outBufLen, swapBytes);
}


/* Portable putenv.  Creates/replaces an environment variable of the form
 *  envVarName=envValue
 */
int
NSS_PutEnv(const char * envVarName, const char * envValue)
{
    SECStatus result = SECSuccess;
#ifdef _WIN32
    PRBool      setOK;

    setOK = SetEnvironmentVariable(envVarName, envValue);
    if (!setOK) {
        SET_ERROR_CODE
        result = SECFailure;
    }
#elif  defined(XP_MAC)
	result = SECFailure;
#else
    char *          encoded;
    int             putEnvFailed;

    encoded = (char *)PORT_ZAlloc(strlen(envVarName) + 2 + strlen(envValue));
    strcpy(encoded, envVarName);
    strcat(encoded, "=");
    strcat(encoded, envValue);

    putEnvFailed = putenv(encoded); /* adopt. */
    if (putEnvFailed) {
        SET_ERROR_CODE
        result = SECFailure;
        PORT_Free(encoded);
    }
#endif
    return result;
}

