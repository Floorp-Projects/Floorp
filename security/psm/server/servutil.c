/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "serv.h"
#include "secport.h"
#include "collectn.h"
#include "resource.h"
#include "connect.h"
#include "plstr.h"
#include "prprf.h"
#include "base64.h"
#include "textgen.h"
#include "protocolf.h"
#include "p12res.h"
#include "ctrlconn.h"
#include "newproto.h"
#include "messages.h"

#ifdef XP_UNIX
#include "private/pprio.h"
#include <unistd.h>

#define GET_LOCK_FILE_PATH(b)  \
        sprintf(b, "/tmp/.nsmc-%d-lock", (int)geteuid())
#define GET_CONTROL_SOCK_PATH(b) \
        sprintf(b, "/tmp/.nsmc-%d", (int)geteuid())

PRFileDesc *lockfile = NULL;

#endif

#define DEBUG_QUEUES

extern SSMCollection * connections;

/* Dumps a (potentially binary) buffer using SSM_DEBUG. 
   (We could have used the version in ssltrace.c, but that's
   specifically tailored to SSLTRACE. Sigh. */
#define DUMPBUF_LINESIZE 8
void
SSM_DumpBuffer(char *buf, PRIntn len)
{
    char hexbuf[DUMPBUF_LINESIZE*3+1];
    char chrbuf[DUMPBUF_LINESIZE+1];
    static const char *hex = "0123456789abcdef";
    PRIntn i = 0, l = 0;
    char ch, *c, *h;

    hexbuf[DUMPBUF_LINESIZE*3] = '\0';
    chrbuf[DUMPBUF_LINESIZE] = '\0';
    (void) memset(hexbuf, 0x20, DUMPBUF_LINESIZE*3);
    (void) memset(chrbuf, 0x20, DUMPBUF_LINESIZE);
    h = hexbuf;
    c = chrbuf;

    while (i < len)
    {
        ch = buf[i];

        if (l == DUMPBUF_LINESIZE)
        {
            SSM_DEBUG("%s%s\n", hexbuf, chrbuf);
            (void) memset(hexbuf, 0x20, DUMPBUF_LINESIZE*3);
            (void) memset(chrbuf, 0x20, DUMPBUF_LINESIZE);
            h = hexbuf;
            c = chrbuf;
            l = 0;
        }

        /* Convert a character to hex. */
        *h++ = hex[(ch >> 4) & 0xf];
        *h++ = hex[ch & 0xf];
        h++;
        
        /* Put the character (if it's printable) into the character buffer. */
        if ((ch >= 0x20) && (ch <= 0x7e))
            *c++ = ch;
        else
            *c++ = '.';
        i++; l++;
    }
    SSM_DEBUG("%s%s\n", hexbuf, chrbuf);
}

PRIntn SSM_ReadThisMany(PRFileDesc * sockID, void * buffer, PRIntn thisMany)
{
    int got = 0;
    PRIntn total = 0;
  
    while (total < thisMany) {
        got = PR_Recv(sockID, (void *)((char *)buffer + total), thisMany-total, 
                      0, PR_INTERVAL_NO_TIMEOUT);
        if (got <= 0 ) break;
        total += got;
    } 

    /* Clear the PR error if we got EOF in the final read, 
       as opposed to -1 bytes. */
    if (got == 0)
        PR_SetError(0, 0);
    
    return total;
}
 
PRIntn SSM_WriteThisMany(PRFileDesc * sockID, void * buffer, PRIntn thisMany)
{
  PRIntn total = 0;
  
  while (total < thisMany) {
    PRIntn got;
    got = PR_Send(sockID, (void *)((char *)buffer+total), thisMany-total, 
		  0, PR_INTERVAL_NO_TIMEOUT);
    if (got < 0) break;
    total += got;
  }
  return total;
}


SECItem * SSM_ConstructMessage(PRUintn size) 
{
  SECItem * ptr = NULL;
  
#if 0
  SSM_DEBUG("allocating message of size %ld.\n", (long) size);
#endif
  
  ptr = (SECItem *)PORT_ZAlloc(sizeof(SECItem));
  if (!ptr) {
    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    goto loser;
  }
  ptr->len = size;
  if (size > 0) { 
    ptr->data = (unsigned char *)PORT_ZAlloc(size);
    if (!ptr->data) {
      PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
      goto loser;
      }
  }
  return ptr;
loser:
  SSM_FreeMessage(ptr);
  return NULL;
}

void SSM_FreeMessage(SECItem * msg)
{
  if (!msg) return;
  if (msg->data)
    PORT_Free(msg->data);
  PORT_Free(msg);
}

void
ssm_DrainAndDestroyQueue(SSMCollection **coll)
{
    SSMCollection *v;
    PRIntn type, len;
    char *data;

    if ((coll == NULL) || (*coll == NULL))
	return;

    v = *coll;

    /* Drain the collection. Don't block; presumably this is
       being done at destruction time. */
    while(SSM_RecvQMessage(v, SSM_PRIORITY_ANY, 
			   &type, &len, &data, 
			   PR_FALSE) == PR_SUCCESS)
    {
	if (data)
	    PR_Free(data);
    }

#if 0
    /* NotifyAll on the lock and yield time. */
    /* ### mwelch - Causing problems on Linux, and shouldn't even 
                    be here since we don't have the queue locked, 
                    so I'm removing it. */
    PR_NotifyAll(v->lock);
#endif
    /* Destroy the collection. */
    SSM_DestroyCollection(v);

    /* Zero out the original member. */
    *coll = NULL;
}

void
ssm_DrainAndDestroyChildCollection(SSMCollection **coll)
{
    SSMCollection *v;
    SSMConnection *conn;

    if ((coll == NULL) || (*coll == NULL))
	return;

    v = *coll;

    /* Drain the collection. Don't block; presumably this is
       being done at destruction time. */
    while(SSM_Dequeue(v, SSM_PRIORITY_ANY, (void **) &conn, PR_FALSE) == PR_SUCCESS) {;}

    /* Destroy the collection. */
    SSM_DestroyCollection(v);

    /* Zero out the original member. */
    *coll = NULL;
}

static SSMHashTable *ssmThreads = NULL;

void SSM_ConnectionThreadName(char *buf)
{
    PRThread * threadptr = PR_GetCurrentThread();
    char *name;

    buf[0] = '\0'; /* in case we fail */

    if (!ssmThreads) return;

    if (SSM_HashFind(ssmThreads, (SSMHashKey) threadptr, (void **) &name)
        != SSM_SUCCESS)
        return;
    
    if (name)
        PL_strcpy(buf, name);
}

static SSMCollection *logSockets = NULL;
static PRMonitor *logSocketLock = NULL;

/* Add the filedesc to a list of sockets to receive log output */
SSMStatus
SSM_AddLogSocket(PRFileDesc *fd)
{
    SSMStatus rv = SSM_SUCCESS;

    PR_ASSERT(logSockets);
    PR_ASSERT(logSocketLock);

    PR_EnterMonitor(logSocketLock);
    rv = SSM_Enqueue(logSockets, SSM_PRIORITY_NORMAL, fd);
    PR_ExitMonitor(logSocketLock);    

    if (rv != SSM_SUCCESS) 
        SSM_DEBUG("SSM_AddLogSocket returned error %d.\n", rv);

    return rv;
}


/* Called by a newly created thread, this associates a name with the thread. */
void
SSM_RegisterThread(char *threadName, SSMResource *ptr)
{
    SSMStatus rv = SSM_SUCCESS;
    PRThread *thr = PR_GetCurrentThread();
    char *value;

#ifdef XP_MAC
    /* 
    	Each thread, when registering itself using one of the two functions
    	below, assigns itself as private data at index (thdIndex). This is
    	necessary because when a thread exits, we want it to delete itself
    	from the list of threads we need to kill at exit time. 
    */
    { 
      PRUintn threadIndex = GetThreadIndex();
      if (threadIndex > 0) {
      	if (PR_GetThreadPrivate(threadIndex) != thr)
	      	PR_SetThreadPrivate(threadIndex, thr);
      }
    }
#endif

    if (ptr)
    {
        PR_ASSERT(SSM_IsAKindOf(ptr, SSM_RESTYPE_RESOURCE));
        /* Increment the thread count for this resource. */
        SSM_LockResource(ptr);
        ptr->m_threadCount++;
        SSM_UnlockResource(ptr);
    }
    if (threadName)
    {
        if (ptr)
            value = PR_smprintf("%s %p", threadName, ptr);
        else
            value = PL_strdup(threadName);
    }
    else
        value = PR_smprintf("unknown %p/%p", thr, ptr);

    if ((!ssmThreads) &&
        ((rv = SSM_HashCreate(&ssmThreads)) != SSM_SUCCESS))
        return;

    if (!value) return;

    SSM_HashInsert(ssmThreads, (SSMHashKey) thr, value);
}

/* Called by a newly created thread, this associates a name with the thread. */
void
SSM_RegisterNewThread(char *threadName, SSMResource *ptr)
{
    SSMStatus rv = SSM_SUCCESS;
    PRThread *thr = PR_GetCurrentThread();
    char *value;

    if (threadName)
    {
        if (ptr)
            value = PR_smprintf("%s %p", threadName, ptr);
        else
            value = PL_strdup(threadName);
    }
    else
        value = PR_smprintf("unknown %p/%p", thr, ptr);

    if ((!ssmThreads) &&
        ((rv = SSM_HashCreate(&ssmThreads)) != SSM_SUCCESS))
        return;

    if (!value) return;

    SSM_HashInsert(ssmThreads, (SSMHashKey) thr, value);
}

void SSM_Debug(SSMResource *conn, char *msg)
{
#ifdef DEBUG
  char name[256];
  
  SSM_ConnectionThreadName(name);
  printf("%s: %s", name, msg);
  fflush(stdout); 
  PR_Free(msg);
#endif
}

void SSM_DebugP(char *fmt, ...)
{
#if defined(DEBUG)
    char *tmp = NULL, *tmp2 = NULL;
	char *timeStamp = NULL;
    PRFileDesc *sock = NULL;
    PRIntn len;
    PRIntn numWritten;
    char name[256];
    va_list argp;
    PRIntn count, i;
    PRUint32 rv = 0;
	PRExplodedTime exploded;

    /* protect against another socket add */
    PR_EnterMonitor(logSocketLock);

    if (!logSockets)
        goto done;
    
    count = SSM_Count(logSockets);
    if (count == 0)
        goto done;

    va_start(argp, fmt);
    tmp = PR_vsmprintf(fmt, argp);
    va_end(argp);
    if (!tmp)
        goto done;
	
	PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &exploded);
	timeStamp = PR_smprintf("%02i:%02i:%02i.%03i", exploded.tm_hour, 
                            exploded.tm_min, exploded.tm_sec, 
                            exploded.tm_usec/1000);
    SSM_ConnectionThreadName(name);

    tmp2 = PR_smprintf("[%s] %s: %s", timeStamp, name, tmp);
    if (!tmp2)
        goto done;

    len = strlen(tmp2);
    count = SSM_Count(logSockets);

    /* count backwards in case we get an error */
    for(i=(count-1);i>=0;i--)
    {
        numWritten = 0;
        sock = (PRFileDesc *) SSM_At(logSockets, i);
        if (sock)
            numWritten = PR_Write(sock, tmp2, len);
        if (numWritten < len)
        {
            /*SSM_Remove(logSockets, sock);*/
            /*PR_Close(sock);*/
            /*PR_Shutdown(sock, PR_SHUTDOWN_BOTH);*/
            rv = 0;
        }
    }

 done:
    if (tmp)
        PR_smprintf_free(tmp);
    if (tmp2)
        PR_smprintf_free(tmp2);
	if (timeStamp) 
		PR_smprintf_free(timeStamp);
    PR_ExitMonitor(logSocketLock);
#endif
}

#ifdef DEBUG
void
SSM_InitLogging(void)
{
    char *suppressConsole = NULL;
    char *debugFile = NULL;

    /* Create the condition variable we use to control logging. */
    if (!(logSocketLock = PR_NewMonitor()))
        goto loser;
    if (!(logSockets = SSM_NewCollection()))
        goto loser; 

    suppressConsole = PR_GetEnv(SSM_ENV_SUPPRESS_CONSOLE);
    debugFile = PR_GetEnv(SSM_ENV_LOG_FILE);

    if (!suppressConsole)
    {
        PRFileDesc *stdfd = PR_GetSpecialFD(PR_StandardOutput);
        if (!stdfd)
            stdfd = PR_GetSpecialFD(PR_StandardError);
        if (stdfd)
            SSM_AddLogSocket(stdfd);
    }

    if (debugFile)
    {
        PRFileDesc *fd = PR_Open(debugFile, 
                                 PR_WRONLY | PR_APPEND | 
                                 PR_CREATE_FILE | PR_SYNC, 
                                 0644);
        if (fd)
            SSM_AddLogSocket(fd);
    }

    SSM_DEBUG("Started logging.\n");

    return;
 loser:
    fprintf(stderr, "Can't initialize logging! Exiting.\n");
    exit(1);
}
#endif

SSMStatus SSM_SendQMessage(SSMCollection *q,
			  PRIntn priority,
			  PRIntn type, PRIntn length, char *data,
			  PRBool doBlock)
{
  SECItem *n = NULL;
  unsigned char *c = NULL;
  SSMStatus rv = PR_FAILURE;

  PR_ASSERT(priority != SSM_PRIORITY_ANY);

#ifdef DEBUG_QUEUES
  SSM_DEBUG("SendQMessage on %lx: prio %d, type %lx, len %d\n", 
            (unsigned long) q, priority, (unsigned long) type, length);
#endif

  /* Create a new message. */
  n = (SECItem *) PORT_ZAlloc(sizeof(SECItem));
  if (!n)
      goto loser;
  
  if (data && length > 0) {
     c = (unsigned char *) PORT_ZAlloc(length);
     if (!c)
         goto loser;

     memcpy(c, data, length);
  }
  
  n->type = (SECItemType) type;
  n->len = length;
  n->data = c;

  /* ### mwelch We'll want to put a throttle here when we
                start really securing Cartman. It's currently
		possible for someone to make us run out of
		memory very quickly by flooding us with data. */
                
  /* Put the msg in the queue. */
  SSM_Enqueue(q, priority, n);
  return PR_SUCCESS;

loser:
  if (n) PORT_Free(n);
  if (c) PORT_Free(c);
  return rv;
}

SSMStatus SSM_RecvQMessage(SSMCollection *q,
			  PRIntn priority,
			  PRIntn *type, PRIntn *length, char **data,
			  PRBool doBlock)
{
  SECItem *p;
  void *v;

#ifdef DEBUG_QUEUES
  SSM_DEBUG("RecvQMessage on %lx: %s read at prio %d\n", 
            (unsigned long) q, 
            (doBlock ? "blocking" : "nonblocking"), 
            priority);
#endif

  /* Remove the item at the head of the queue. */
  SSM_Dequeue(q, priority, &v, doBlock);
  p = (SECItem *) v;

  if (!p)
    return PR_FAILURE;

  /* Strip out the items. */
  *data = (char *)p->data;
  *length = p->len;
  *type = p->type;
  
#ifdef DEBUG_QUEUES
  SSM_DEBUG("RecvQMessage on %lx: type %lx, len %d\n", 
            (unsigned long) q, (unsigned long) *type, 
            *length);
#endif

  /* Free the containing SECItem. */
  PORT_Free(p);

  return PR_SUCCESS;
}

SSMStatus
SSM_SECItemToSSMString(void ** ssmstring, SECItem * data)
{
 SSMString * result = NULL;

 if (!ssmstring || !data || data->len <= 0 ) 
	goto loser;
 
 *ssmstring = NULL; /* in case we fail */
 result = (SSMString *) SSMPORT_ZAlloc(sizeof(data->len) + 
			SSMSTRING_PADDED_LENGTH(data->len));
 if (!result) 
	goto loser;
 memcpy((char *)(&(result->m_data)), data->data, data->len);
 result->m_length = data->len;
 *ssmstring = result;
 return PR_SUCCESS;

loser:
 if (result) 

     PR_Free(result);
 return PR_FAILURE;
}

SSMStatus
SSM_CopyCMTItem(CMTItem * dest, CMTItem * source)
{
    SSMStatus rv = SSM_FAILURE;
    
    if (!dest || !source) 
        goto loser;
    
    dest->len = source->len;
    if (!dest->len) 
        goto done;
    
    dest->data = (unsigned char *) PR_Malloc(dest->len);
    if (!dest->data) 
        goto loser;
    memcpy(dest->data, source->data, dest->len);
 done: 
    rv = SSM_SUCCESS;
 loser:
    return rv;
}


PRFileDesc * SSM_OpenControlPort(void)
{
    PRFileDesc * datasocket          = NULL;
    PRNetAddr servaddr;
    SSMStatus status;
    PRSocketOptionData sockOpData;
#ifdef XP_UNIX
    char lock_name_buf[32];
#endif

    /* disable Nagle algorithm delay for control sockets */
    sockOpData.option = PR_SockOpt_NoDelay;
    sockOpData.value.no_delay = PR_TRUE;

#ifdef XP_UNIX
    GET_LOCK_FILE_PATH(lock_name_buf);
    lockfile = PR_Open(lock_name_buf, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE,
		       0600);
    if (!lockfile)
	goto loser;

    status = PR_TLockFile(lockfile);
    if (status != PR_SUCCESS)
	goto loser;

    datasocket = PR_Socket(AF_UNIX, SOCK_STREAM, 0);
    if (!datasocket)
        goto loser;
    servaddr.local.family = AF_UNIX;
    GET_CONTROL_SOCK_PATH(servaddr.local.path);
    PR_Delete(servaddr.local.path);
#else
    datasocket = PR_NewTCPSocket();
    if (!datasocket)
        goto loser;

    status = PR_SetSocketOption(datasocket, &sockOpData);
    if (status != PR_SUCCESS) {
        goto loser;
    }

    /* Bind to PSM port on loopback address: connections from non-localhosts
     * will be disallowed
     */
    status = PR_InitializeNetAddr(PR_IpAddrLoopback, (PRUint16) PSM_PORT,
				  &servaddr);
    if (status != PR_SUCCESS)
        goto loser;
#endif

    status = PR_Bind(datasocket, &servaddr);
    if (status != PR_SUCCESS)
        goto loser;
    status = PR_Listen(datasocket, MAX_CONNECTIONS);
    if (status != PR_SUCCESS)
        goto loser;

#ifdef XP_UNIX  
    SSM_DEBUG("Ready for client connection on port %s.\n", servaddr.local.path);
#else
    SSM_DEBUG("Ready for client connection on port %d.\n", PSM_PORT);
#endif

    return datasocket;
 loser:
    return NULL;
}

/* Make sure that the peer is on the same machine that we are */
PRBool
SSM_SocketPeerCheck(PRFileDesc *sock, PRBool isCtrl)
{
    PRBool rv = PR_FALSE;
    SSMStatus st = PR_SUCCESS;
    PRNetAddr theirAddr;
    char localaddr[] = {0x7F,0x00,0x00,0x01};/* 127.0.0.1 in network order */

#if defined(XP_UNIX) || defined(XP_MAC)
#ifdef XP_UNIX
    if (isCtrl)
    {
        /* unix domain socket == same machine */
        /* ### mwelch Well, the only time this isn't true is when
           one wants to join many unix machines in a cluster, but 
           even in that case the cluster should be treated as a 
           single machine anyway. */
#endif
		/*
			GetPeerName isn't implemented on the Mac, so say yes and hope/pray for now 
			### mwelch - Need to implement GetPeerName in Mac NSPR before Mac Cartman RTM 
		*/
        rv = PR_TRUE;
        goto done;
#ifdef XP_UNIX
    }
#endif
#endif

    st = PR_GetPeerName(sock, &theirAddr);
    if (st != PR_SUCCESS)
        goto done; /* default is to fail */

    /* Compare against localhost IP */
    if (!memcmp(&(theirAddr.inet.ip), localaddr, 4))
        rv = PR_TRUE;

    if (rv != PR_TRUE)
        SSM_DEBUG("Failed IP address check!\n");

 done:
    return rv;
}

PRFileDesc * SSM_OpenPort(void)
{
    PRFileDesc * datasocket          = NULL;
    PRNetAddr servaddr;
    SSMStatus status;
    PRSocketOptionData sockOpData;

    /* disable Nagle algorithm delay for data sockets */
    sockOpData.option = PR_SockOpt_NoDelay;
    sockOpData.value.no_delay = PR_TRUE;

    datasocket = PR_NewTCPSocket();
    if (!datasocket)
        goto loser;
    status = PR_SetSocketOption(datasocket, &sockOpData);
    if (status != PR_SUCCESS) {
        goto loser;
    }

    /* Bind to PSM port on loopback address: connections from non-localhosts
     * will be disallowed
     */
    status = PR_InitializeNetAddr(PR_IpAddrLoopback, 0, &servaddr);
    if (status != PR_SUCCESS)
        goto loser;

    status = PR_Bind(datasocket, &servaddr);
    if (status != PR_SUCCESS)
        goto loser;
#if defined(XP_MAC)
    status = PR_Listen(datasocket, 1);
#else
    status = PR_Listen(datasocket, MAX_CONNECTIONS);
#endif
    if (status != PR_SUCCESS)
        goto loser;
  
    return datasocket;
 loser:
    return NULL;
}

#if 0
/* Whee. Base 64 decoding. Have to do it for basic authentication. */
#define XX 127
/*
 * Table for decoding base64. We have this everywhere else, why not here.
 */
static char index64[256] = {
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, XX,XX,XX,63,
    52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,XX,XX,XX,
    XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
    XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};

#ifdef XP_OS2_HACK
/*DSR102196 - the OS/2 Visual Age compiler croaks when it tries*/
/*to optomize this macro (/O+ on CSD4)                         */ 
char CHAR64(int c)
{
   unsigned char index;
   char rc;

   index = (unsigned char) c;
   rc = index64[index];
   return rc;
} 
#else /*normal code...*/
#define CHAR64(c)  (index64[(unsigned char)(c)])
#endif

/* Decode a base 64 string in place. */
void decode_base64_string(char *str)
{
    char *s = str;
    char *d = str;
    char c[4];
    PRIntn i;
    PRBool done = PR_FALSE;
    
    while((!done) && (*s))
    {
        /* Process 4 bytes at a time. */
        for(i=0;i<4;i++)
            c[i] = 0;
        
        for(i=0;i<4;i++)
        {
            if ((IS_WHITESPACE(*s)) || (IS_EOL(*s)))
            {
                /* End here, because we're out of bytes. */
                done = PR_TRUE;
            }
            else
                c[i] = *s++;
        }
        
        /* Make sure we can at least decode one character. */
        if ((!done) && ((c[0] == '=') || (c[1] == '=')))
            done = PR_TRUE; /* don't even have enough for one character, leave */
        
        if (!done)
        {
            /* Decode the first character. */
            c[0] = CHAR64(c[0]);
            c[1] = CHAR64(c[1]);
            *d++ = ((c[0]<<2) | ((c[1] & 0x30)>>4));
            
            /* If we have data for a second character, decode that. */
            if (c[2] != '=')
            {
                c[2] = CHAR64(c[2]);
                *d++ = (((c[1] & 0x0F) << 4) | ((c[2] & 0x3C) >> 2));
                
                /* If we have data for a third character, decode that. */
                if (c[3] != '=')
                {
                    c[3] = CHAR64(c[3]);
                    *d++ = (((c[2] & 0x03) << 6) | c[3]);
                }
                else
                    done = PR_TRUE;
            }
            else
                done = PR_TRUE;
        }
    }

    /* Terminate the string. */
    *d = '\0';
}
#endif

#ifdef DEBUG
char * 
SSM_StandaloneGetPasswdCallback(PK11SlotInfo *slot, PRBool retry, 
                                void *arg)
{
    char *result = NULL;
    char *env = NULL;
    SECItem theItem = {siBuffer, NULL, 0};

    if (retry)
    {
        /* Don't spin forever, just bail */
        SSM_DEBUG("Static password you provided was not accepted. "
                  "Cancelling request.\n");
        return NULL;
    }

    env = PR_GetEnv(SSM_ENV_STATIC_PASSWORD);
    if (!env)
        goto done;
    
    ATOB_ConvertAsciiToItem(&theItem, env);
    if (theItem.data == NULL)
        goto done;

    result = (char *) PR_CALLOC(theItem.len + 1);
    if (!result)
        goto done;

    strncpy(result, (char *) theItem.data, theItem.len);

 done:
    if (theItem.data != NULL)
        SECITEM_FreeItem(&theItem, PR_FALSE);
    if (result)
        SSM_DEBUG("Responding to password request with a password.\n");
    else
        SSM_DEBUG("Responding to password callback with NULL password.\n");

    return result;
}

PRBool 
SSM_StandaloneVerifyPasswdCallback(PK11SlotInfo * slot, void * arg)
{
    /* yeah, sure, trust me, we're the ones who are sposta be logged in */
    return PR_TRUE; 
}
#endif

SSMStatus
SSM_CloseSocketWithLinger(PRFileDesc *sock)
{
    PRSocketOptionData opt;
    SSMStatus rv = PR_SUCCESS;
    
    /* 
       Set linger for 15 minutes.
       ### mwelch This time should really depend on what has been written
       to the socket. For example, if we've just written 5 megabytes of data
       down a 14.4 wire, we should wait considerably longer than 15 minutes.
       The point here is that there's no way to determine a constant value 
       ahead of time. So, 15 minutes. (Note that generally only client sockets
       are closed in this way, so we hopefully will avoid major trouble.)
     */
    opt.option = PR_SockOpt_Linger;
    opt.value.linger.polarity = PR_TRUE;
    opt.value.linger.linger = PR_SecondsToInterval(60*15);
    rv = PR_SetSocketOption(sock, &opt);

    /* Now close the socket. */
    if (rv == PR_SUCCESS)
        rv = PR_Close(sock);

    return rv;
}

char*
SSM_GetCharFromKey(const char *key, const char *locale)
{
    SSMTextGenContext *textGenCxt  = NULL;
    char              *asciiString = NULL;
    SSMStatus           rv;

    rv = SSMTextGen_NewContext(NULL, NULL, NULL, NULL, &textGenCxt);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    rv = SSM_FindUTF8StringInBundles(textGenCxt, key,
                                     &asciiString);
    if (rv != PR_SUCCESS) {
        goto loser;
    }
    SSMTextGen_DestroyContext(textGenCxt);
    return asciiString;
 loser:
    if (textGenCxt != NULL) {
        SSMTextGen_DestroyContext(textGenCxt);
    }
    if (asciiString != NULL) {
        PR_Free(asciiString);
    }
    return NULL;
}

SSMStatus SSM_RequestFilePathFromUser(SSMResource *res,
                                      const char  *promptKey,
                                      const char  *fileRegEx,
                                      PRBool       getExistingFile)
{
    SECItem           *filePathRequest = NULL;
    char              *asciiPrompt     = NULL;
    SSMStatus          rv;
    FilePathRequest    request;

    filePathRequest = SSM_ZNEW(SECItem);
    if (filePathRequest == NULL) {
        return SSM_FAILURE;
    }
    asciiPrompt = SSM_GetCharFromKey(promptKey, "ISO_8859-1");
    if (asciiPrompt == NULL) {
        goto loser;
    }

    request.resID = res->m_id;
    request.prompt = asciiPrompt;
    request.getExistingFile = (CMBool) getExistingFile;
    request.fileRegEx = fileRegEx;

    if (CMT_EncodeMessage(FilePathRequestTemplate, (CMTItem*)filePathRequest, (void*)&request) != CMTSuccess) {
        goto loser;
    }
    filePathRequest->type = (SECItemType) (SSM_EVENT_MESSAGE | SSM_FILE_PATH_EVENT);
    SSM_LockResource(res);
    rv = SSM_SendQMessage(res->m_connection->m_controlOutQ,
                          20,
                          filePathRequest->type,
                          filePathRequest->len,
                          (char*)filePathRequest->data,
                          PR_TRUE);
    SECITEM_FreeItem(filePathRequest, PR_TRUE);

    SSM_WaitResource(res, PR_INTERVAL_NO_TIMEOUT);
    SSM_UnlockResource(res);
    return (res->m_fileName == NULL) ? SSM_FAILURE : SSM_SUCCESS;
    
 loser:
    if (filePathRequest != NULL) {
        SECITEM_FreeItem(filePathRequest, PR_TRUE);
    }
    return SSM_FAILURE;
}

void SSM_HandleFilePathReply(SSMControlConnection *ctrl, 
                             SECItem              *message)
{
  SSMStatus rv;
  SSMResource  *res;
  FilePathReply reply;

  if (CMT_DecodeMessage(FilePathReplyTemplate, &reply, (CMTItem*)message) != CMTSuccess) {
      return;
  }

  rv = SSMControlConnection_GetResource(ctrl, reply.resID, &res);
  if (rv != PR_SUCCESS || res == NULL) {
      return;
  }
  if (reply.filePath != NULL) {
    if (reply.filePath[0] == '\0') {
      res->m_fileName = NULL;
    } else {
      res->m_fileName = PL_strdup(reply.filePath);
    }
    PR_Free(reply.filePath);
  } else {
    res->m_fileName = NULL;
    res->m_buttonType = SSM_BUTTON_CANCEL;
  }
  SSM_LockResource(res);
  SSM_NotifyResource(res);
  SSM_UnlockResource(res);
  SSM_FreeResource(res);
}

void SSM_HandleUserPromptReply(SSMControlConnection *ctrl,
                               SECItem              *msg)
{
    SSMStatus rv;
    SSMResource *res;
    PromptReply reply;

    if (CMT_DecodeMessage(PromptReplyTemplate, &reply, (CMTItem*)msg) != CMTSuccess) {
        return;
    }

    rv = SSMControlConnection_GetResource(ctrl, reply.resID, &res);
    if (rv != PR_SUCCESS || res == NULL)  {
        return;
    }

    /* XXX sjlee: if the password length was zero, it would have been 
     * translated as a null pointer through transport.  We need to restore it 
     * to an empty string.  We can do that by looking up the cancel value of 
     * the reply.
     */
    if ((reply.promptReply == NULL) && !reply.cancel) {
        reply.promptReply = "";
    }
    SSM_HandlePromptReply(res, reply.promptReply);
}

#ifdef TIMEBOMB

SECItem * SSMTimebomb_GetMessage(SSMControlConnection * control)
{
  SECItem * message = NULL;
  PRInt32 read, type, len;
  char * buffer = NULL;
  SSMStatus rv = PR_FAILURE;
 
  buffer = (char *)PORT_ZAlloc(sizeof(type)+ sizeof(len));
  if (!buffer)
    goto loser;

  SSM_DEBUG("waiting for new message from socket.\n");
  read = SSM_ReadThisMany(control->m_socket,(void *)buffer,sizeof(type)+sizeof(len));
  if (read != sizeof(type)+sizeof(len))
    {
     SSM_DEBUG("Bytes read (%ld) != bytes expected (%ld). (hdr)\n",
               (long) read,
               (long) (sizeof(type)+sizeof(len)));
     rv = PR_FAILURE;
     goto loser;
    }
  message = SSM_ConstructMessage(PR_ntohl(((unsigned long *)buffer)[1]));
  if (!message || !message->data)
    {
      SSM_DEBUG("Missing %s.\n",(!message) ? "message" : "message data");
      rv = PR_OUT_OF_MEMORY_ERROR;
      goto loser;
    }
  message->type = PR_ntohl(((unsigned long *)buffer)[0]);
  SSM_DEBUG("reading %ld more from socket.\n", message->len);
  read = SSM_ReadThisMany(control->m_socket, (void *)message->data, message->len);
  if ((unsigned int) read != message->len)
    {
     SSM_DEBUG("Bytes read (%ld) != bytes expected (%ld). (msg)\n",
                           (long) read,
                           (long) message->len);
     rv = PR_GetError();
     if (rv == PR_SUCCESS) rv = PR_FAILURE;
     goto loser;
   }
  if (buffer) 
    PR_Free(buffer);
  return message;
loser:
  if (buffer)
    PR_Free(buffer);
  return NULL;
}


SSMStatus SSMTimebomb_SendMessage(SSMControlConnection * control, 
                                 SECItem * message)
{
  PRInt32 tmp, sent;

  if (!message)
    goto loser;
  /* now send message.
   * I want to do it here to keep timebomb code close together -jane */
  tmp = PR_htonl(message->type);
  sent =  SSM_WriteThisMany(control->m_socket, &tmp, sizeof(tmp));
  if (sent != sizeof(tmp))
    goto loser;
  tmp = PR_htonl(message->len);
  sent =  SSM_WriteThisMany(control->m_socket, &tmp, sizeof(tmp));
  if (sent != sizeof(tmp))
    goto loser;
  sent = SSM_WriteThisMany(control->m_socket, message->data, message->len);
  if (sent != message->len)
    goto loser;
  return SSM_SUCCESS;
loser: 
  SSM_DEBUG("timebomb: can't send error message!\n");
  return SSM_FAILURE;
}
#endif

void SSM_DestroyAttrValue(SSMAttributeValue *value, PRBool freeit)
{
    if ((value->type == SSM_STRING_ATTRIBUTE) && (value->u.string.data))
        PR_Free(value->u.string.data);
    value->type = (SSMResourceAttrType) 0;
    if (freeit)
        PR_Free(value);
}

CMTStatus SSM_SSMStringToString(char ** string,
			       int *len, 
			       SSMString * ssmString) 
{
  char * str = NULL;
  int realLen;
  CMTStatus rv = CMTSuccess;

  if (!ssmString || !string ) { 
    rv = (CMTStatus) PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }

  /* in case we fail */
  *string = NULL;
  if (len) *len = 0;

  /* Convert from net byte order */
  realLen = SSMPR_ntohl(ssmString->m_length);

  str = (char *)PR_CALLOC(realLen+1); /* add 1 byte for end 0 */
  if (!str) {
    rv = (CMTStatus) PR_OUT_OF_MEMORY_ERROR;
    goto loser;
  }
  
  memcpy(str, (char *) &(ssmString->m_data), realLen);
  /* str[realLen]=0; */

  if (len) *len = realLen;
  *string = str;
  return rv;
  
loser:
  if (str) 
    PR_Free(str);
  if (string && *string) {
    PR_Free(*string);
    *string = NULL;
  }
  if (rv == CMTSuccess) 
    rv = CMTFailure;
  return rv;
}


CMTStatus SSM_StringToSSMString(SSMString ** ssmString, int length, 
				  char * string)
{
  SSMPRUint32 len;
  SSMString *result = NULL;
  CMTStatus rv = CMTSuccess;
  
  if (!string || !ssmString) {
    rv = (CMTStatus) PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }

  *ssmString = NULL; /* in case we fail */

  if (length) len = length; 
  else len = strlen(string);
  if (len <= 0) {
    rv = (CMTStatus) PR_INVALID_ARGUMENT_ERROR;
    goto loser;
  }
  result = (SSMString *) PR_CALLOC(sizeof(PRUint32) + 
				   SSMSTRING_PADDED_LENGTH(len));
  if (!result) {
    rv = (CMTStatus) PR_OUT_OF_MEMORY_ERROR;
    goto loser;
  }

  result->m_length = SSMPR_htonl(len);
  memcpy((char *) (&(result->m_data)), string, len);

  *ssmString = result;
  goto done;
  
loser:
  if (result)
    PR_Free(result);
  *ssmString = NULL;
  if (rv == CMTSuccess)
    rv = CMTFailure;
done:
  return rv;
}

#ifdef XP_UNIX
void SSM_ReleaseLockFile()
{
  if (lockfile) {
    char filePath[64];
    
    GET_LOCK_FILE_PATH(filePath);
    SSM_DEBUG("Releasing and deleting the file %s\n", filePath);
    PR_UnlockFile(lockfile);
    PR_Close(lockfile);
    lockfile = NULL;
    PR_Delete(filePath);
    GET_CONTROL_SOCK_PATH(filePath);
    SSM_DEBUG("Deleting %s. (The control connection socket)\n", filePath);
    PR_Delete(filePath);
  }
}
#endif

int SSM_strncasecmp(const char *s1, const char *s2, size_t count)
{
    char *myS1 = NULL, *myS2 = NULL;
    int rv, len, i;

    myS1 = strdup(s1);
    myS2 = strdup(s2);
    len = strlen(myS1);
    for (i=0;i<len;i++) {
        myS1[i] = tolower(myS1[i]);
    }
    len = strlen(myS2);
    for (i=0;i<len;i++) {
        myS2[i] = tolower(myS2[i]);
    }
    rv = strncmp(myS1,myS2,count);
    free(myS1);
    free(myS2);
    return rv;
}
