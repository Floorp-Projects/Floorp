/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * nsDnsAsyncLookup.cpp --- standalone lightweight process to handle 
 *                          asyncrhonous dns lookup on Unix.
 */


/* Compile-time options:

 * -DGETHOSTBYNAME_DELAY=N
 *  to insert an artificial delay of N seconds before each
 *  call to gethostbyname (in order to simulate DNS lossage.)
 *
 * -DNO_SOCKS_NS_KLUDGE
 *  Set this to *disable* the $SOCKS_NS kludge.  Otherwise,
 *  that environment variable will be consulted for use as an
 *  alternate DNS root.  It's historical; don't ask me...
 */

#if defined(XP_UNIX) && defined(UNIX_ASYNC_DNS)

#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <assert.h>
#include <sys/un.h>
#include "nspr.h"
#include "nsCRT.h"
#include "unix_dns.h"

#if defined(AIX) || defined(__linux)
#include <sys/select.h>         // for fd_set
#endif

#if !defined(NO_SOCKS_NS_KLUDGE)
#include <arpa/inet.h>          // for in_addr (from nameser.h)
#include <arpa/nameser.h>       // for MAXDNAME (from resolv.h)
#include <resolv.h>             // for res_init() and _res
#endif

#if defined(__linux)
// Didn't find gettdtablehi() or gettdtablesize() on linux. Using FD_SETSIZE
#define getdtablehi() FD_SETSIZE
#elif !defined(__irix)
// looks like Irix is the only one that has getdtablehi()?
#define getdtablehi() getdtablesize()

//  If you find a system doesn't have getdtablesize try #define getdtablesize
//  to FD_SETSIZE.  And if you encounter a system that doesn't even have 
//  FD_SETSIZE, just grab your ankles and use 255.
#endif


#define BACKLOG 20          // maximum # of pending connections
#define ASSERT(x)   assert(x)

static int listen_fd = -1;



///////////////////////////////////////////////////////////////////////////
//  Name:  dnsSocksKludge
//
//  Description:  Gross historical kludge.
//                If the environment variable $SOCKS_NS is defined, stomp 
//                on the host that the DNS code uses for host lookup to be 
//                a specific ip address.
//
static void
dnsSocksKludge(void)
{
#ifndef NO_SOCKS_NS_KLUDGE

    char *ns = getenv("SOCKS_NS");
    if (ns && *ns) 
    {
        // Gross hack added to Gross historical kludge - need to
        // initialize resolv.h structure first with low-cost call
        // to gethostbyname() before hacking _res. Subsequent call
        // to gethostbyname() will then pick up $SOCKS_NS address.
        //
        gethostbyname("localhost");

        res_init();
	//  _res is defined in resolv.h
        _res.nsaddr_list[0].sin_addr.s_addr = inet_addr(ns);
        _res.nscount = 1;
    }
#endif /* !NO_SOCKS_NS_KLUDGE */
}


///////////////////////////////////////////////////////////////////////////
//  Name:  mySignalHandler
//
//  Description:  Signal handler. Close down the socket and exit.
//
static void 
mySignalHandler (int sig)
{
    // close down the socket.
    close (listen_fd);
    unlink (DNS_SOCK_NAME);     //just in case close doesn't remove this
    exit (0);
}

//////////////////////////////////////////////////////////////////////////
//  Name:  displaySysErr
//
//  Description:  Display system error.
//
static void 
displaySysErr(char *name) {
    perror ((const char *)name);
    exit (1);
}


/////////////////////////////////////////////////////////////////////////
//
//  The following data structure is used to hold information about a 
//  pending lookup request.
typedef struct dns_lookup {
    long id;                    //  id to identify this entry 
    pid_t pid;                  //  process id of the helper process
    int fd;                     //  file descriptor used to get result from
    int accept_fd;              //  file descriptor used to send hostent back
    char *name;                 //  name to lookup
    struct dns_lookup *next;    //  pointer to the next entry
} dns_lookup;

static dns_lookup *dns_lookup_queue = 0;


////////////////////////////////////////////////////////////////////////
//  Name:  getNextDnsEntry
//
//  Description:  Return the idx entry to the caller. Bump the index up
//                by 1 before the return.  Similiar to an iterator.
//
static dns_lookup*
getNextDnsEntry (int *idx)
{
    dns_lookup *obj = dns_lookup_queue;
    if (*idx < 0)
        return 0;

    for (int i = 1; i < *idx; i++)
    {
        obj = obj->next;
        if (!obj)
            return 0;
    }

    *idx += 1;
    return obj;
}


////////////////////////////////////////////////////////////////////////
//  Name:  addToDnsQueue
//
//  Description:  Add a lookup entry to the queue.
//
static void 
addToDnsQueue (dns_lookup* obj)
{
    dns_lookup* entry;
    obj->next = 0;
    if (!dns_lookup_queue)
    {
        dns_lookup_queue = obj;
        return;
    }

    entry = dns_lookup_queue;
    while (entry->next) 
        entry = entry->next;
    entry->next = obj;
}


//////////////////////////////////////////////////////////////////////////
//  Name:  removeFromDnsQueue 
//
//  Description:  Remove the specified entry from the queue.
//
static void
removeFromDnsQueue (dns_lookup *obj)
{
    dns_lookup* entry;

    if (!obj || !dns_lookup_queue) 
        return;

    if (obj == dns_lookup_queue)
    {
        dns_lookup_queue = obj->next;
    } 
    else
    {
        entry = dns_lookup_queue;
        while (obj != entry->next) 
            entry = entry->next;
        if (!entry || !entry->next)
            return;
        entry->next = obj->next;
    }

    if (obj->name)
    {
        free(obj->name);
        obj->name = 0;
    }
    free (obj);
}


///////////////////////////////////////////////////////////////////////////
//  Name:  newLookupObject
//  
//  Description:  Create a new lookup object and return a pointer to the
//                object to the caller.  0 is returned on error.
//
static dns_lookup*  
newLookupObject (const char *name)
{
    static int dnsId = 0;
    ASSERT (name);

    char *str = strdup (name);

    if (!str) return 0;           // MK_OUT_OF_MEMORY

    dns_lookup* obj = (dns_lookup *) malloc (sizeof (struct dns_lookup));
    if (!obj)
    {
        free (str);
        return 0;
    }

    memset( obj, 0, sizeof( struct dns_lookup ) );
    obj->id = ++dnsId;
    obj->name = str;
    obj->fd = -1;
    obj->accept_fd = -1;

    addToDnsQueue (obj);
    return (obj);
}


///////////////////////////////////////////////////////////////////////////
//  Name:  hostentToBytes
//
//  Description:  Pack the structure `hostent' into a character buffer.
//
//
static void 
hostentToBytes (hostent *h, char *buf, int *size)
{
    *size = 0;
    int len = strlen (h->h_name);
    char *p = buf;
    *(int *)p = len;        // Encode the length of the name
    p += sizeof (int);

    if (len > 0)
    {
        strcpy (p, h->h_name);
        p += strlen (h->h_name);
    }

    int n;
    // find out the number of aliases are present.  The last entry in
    // list is 0.
    for (n = 0; h->h_aliases[n]; n++);
    *(int *)p = n;          // Encode the size of aliases into buf
    p += sizeof (int);

    int i;
    // copy aliases to the buffer.
    for (i = 0; i < n; i++)
    {
        len = strlen (h->h_aliases[i]);
        *(int *)p = (int) len;
        p += sizeof(int);
        memcpy (p, h->h_aliases[i], (size_t) len);
        p += len;
    }

    *(int *)p = h->h_addrtype;      // Encode the member h_addrtype
    p += sizeof (int);
    *(int *)p = h->h_length;        // Encode the member h_length
    p += sizeof (int);
    
    // find out the number of addresses in h_addr_list list.
    for (n = 0; h->h_addr_list[n]; n++);
    *(int *)p = n;          // Encode the size of aliases into buf
    p += sizeof (int);

    //  copy the address list into the buffer.
    for (i = 0; i < n; i++)
    {
        len = strlen (h->h_addr_list[i]);
        *(int *)p = (int) len;
        p += sizeof(int);
        memcpy (p, h->h_addr_list[i], (size_t) len);
        p += len;
    }

    *size = p - buf;
}


////////////////////////////////////////////////////////////////////////
//  Name:  cancelLookup
//
//  Description:  Cancel an existing lookup request. Locate the right
//                child helper process and kill it.
//
static void 
cancelLookup (int id)
{
    dns_lookup *obj = dns_lookup_queue;

    while (obj)
    {
        if (id == obj->id)
            break;
        obj = obj->next;
    }

    if (obj && obj->pid)
    {
        kill (obj->pid, SIGQUIT);
        pid_t pid2 = waitpid (obj->pid, 0, 0);
        ASSERT ((obj->pid == pid2));
        removeFromDnsQueue (obj);
    }

}

//////////////////////////////////////////////////////////////////////////
//  Name:  blockingGethostbyname
//
//  Description:  Calls the blocking gethostbyname to perform the lookup.
//                When done, pack the hostent struct into a character
//                buffer and sent it back to the parent process via pipe.
//
static void
blockingGethostbyname (const char *name, int out_fd)
{
  int i;
    static int firstTime = true;
    if (firstTime)
    {
        firstTime = 0;
        dnsSocksKludge();
    }

#ifdef GETHOSTBYNAME_DELAY
    i = GETHOSTBYNAME_DELAY;
    sleep(i);
#endif // GETHOSTBYNAME_DELAY

    int size = 0;
    char buf[BUFSIZ];

    struct hostent *h = gethostbyname(name);
    if (h)
    {

        *(int *)&buf[0] = (int) DNS_STATUS_GETHOSTBYNAME_OK;
        char *p = buf + sizeof (int);

#if defined(DNS_DEBUG)
        printf("gethostbyname complete\n");
        for (i=0; h->h_addr_list[i]; i++);
        printf("%d addresses for %s\n",i,h->h_name);
        printf("address: ");
        for (i = 0; i <= h->h_length; i++){
          printf("%2.2x", (unsigned char)h->h_addr_list[0][i]);
        }
        printf("\n");
#endif

        hostentToBytes (h, p, &size);
        size = size + sizeof (int);
    }
    else
    {
        
        *(int *)&buf[0] = (int) DNS_STATUS_GETHOSTBYNAME_FAILED;
        size = sizeof (int);
    }
    // Send response back to parent.
    write(out_fd, buf, size);
}


//////////////////////////////////////////////////////////////////////////
//  Name:  spawnHelperProcess
//
//  Description:  Spawns a child helper process to do the standard Unix
//                blocking dns lookup.
//
dns_lookup* 
spawnHelperProcess (const char *name)
{
    pid_t forked;
    int fds[2];
    
    dns_lookup* obj = newLookupObject (name);

    if (!obj) return 0;

    if (pipe(fds))
    {
#ifdef DNS_DEBUG
        fprintf (stderr, "Can't make pipe\n");
#endif
        return 0;
    }

    obj->fd = fds[0];

    switch (forked = fork())
    {
    case -1:
#ifdef DNS_DEBUG
        fprintf (stderr, "Can't fork\n");
#endif
        removeFromDnsQueue (obj);
        break;

    case 0:  /* This is the forked process. */
        close (fds[0]);
        blockingGethostbyname (name, fds[1]);
        /* Close the file and exit the process. */
        close (fds[1]);
        exit (0);
        break;

    default:
        close (fds[1]);
        obj->pid = forked;
        return obj;
        break;
    }

    // shouldn't get here
    ASSERT (0);
    return 0;
}


////////////////////////////////////////////////////////////////////////////
  
int main (int argc, char **argv)
{
/*
  PRFileDesc* sock = PR_GetInheritedFD(DNS_SOCK_NAME);
  if (sock == nsnull)
    return -1;
*/
    signal (SIGINT, mySignalHandler);  // trap SIGINT
    //  TODO:  more signals need to be trapped.  Will do...

    // Create a socket of type PF_UNIX to listen for dns lookup requests
    listen_fd = socket (PF_UNIX, SOCK_STREAM, 0);

    if (listen_fd == -1)
        displaySysErr (argv[0]);

    struct saddr_un {
        sa_family_t sun_family;
        char sun_path[108];
    } unix_addr;

    unix_addr.sun_family = AF_UNIX;
    strcpy (unix_addr.sun_path, DNS_SOCK_NAME);

    if (bind (listen_fd, (struct sockaddr*)&unix_addr, sizeof(unix_addr)) == -1)
        displaySysErr (argv[0]);

    if (listen (listen_fd, BACKLOG) == -1)
        displaySysErr (argv[0]);

    int accept_fd = -1;

    /////////////////////////////////////////////////////////////////////////
    //  Loop to process incoming DNS lookup/cancel reqeusts.  When lookup is
    //  is completed by the helper child process, a "hostent" structure will
    //  be sent back to the client.
    /////////////////////////////////////////////////////////////////////////
    while (1) {

        fd_set readfds;

        FD_ZERO (&readfds);
        FD_SET (listen_fd, &readfds);

        if (accept_fd > 0)
            FD_SET (accept_fd, &readfds);

        int idx = 0;
        dns_lookup* obj;
        while ((obj = getNextDnsEntry (&idx)))
            FD_SET(obj->fd, &readfds);
        
        // Select will return if any one of the fd is ready for read.
        // Meaning either a request has arrived from Mozilla client or
        // a helper process has sent the lookup result back to us. 
        int n = select (getdtablehi(), &readfds, 0, 0, 0);
        if (n == -1)
            continue;

        if (FD_ISSET (listen_fd, &readfds))
        {
            struct sockaddr_in from;
            int fromlen = sizeof (from);
            accept_fd = accept (listen_fd, (struct sockaddr *)&from, 
                                    (socklen_t *)&fromlen);
            if (accept_fd == -1)
                displaySysErr (argv[0]);
        } 

        int r;
        char buffer[BUFSIZ];
        if (accept_fd > 0 && FD_ISSET (accept_fd, &readfds))
        {
            // Read the request from client. 
            r = recv (accept_fd, buffer,  sizeof(buffer), 0);

            if (r > 0 && r < (int) sizeof(buffer)) {
                buffer[r] = 0;

                if (!(strncmp (buffer, "shutdown:", 9)))
                    break;

                char *name = strchr ((const char *)buffer, ' ');
                if (name && *name)
                    name++;

                if (!name)
                    continue;

                if (!strncmp (buffer, "kill:", 5))
                {
                    //  On kill, the name is really an id number
                    int id = (int) (*(int *)name);
                    cancelLookup (id);
                    close (accept_fd);
                    accept_fd = -1;
                }
                else if (!strncmp (buffer, "lookup:", 7))
                {
#if defined(DNS_DEBUG)
                  printf("received lookup request for: %s\n",name);
#endif
                    obj = spawnHelperProcess (name);
                    obj->accept_fd = accept_fd;
                    char hId[5];
                    *(int *)&hId[0] = (int) obj->id;
                    if (!obj) {
#ifdef DNS_DEBUG
                        fprintf (stderr, "spawn Error\n");
#endif
                        ;
                    }
                    else
                    {
                        send (obj->accept_fd, hId, sizeof (int), 0);
                    }
                }
            }
            accept_fd = -1;
        } 
        idx = 0;
        while ((obj = getNextDnsEntry (&idx)))
        {
            // Check to see if any of the helper process is done with
            // the lookup operation. 
            if (FD_ISSET(obj->fd, &readfds))
            {
                r = read (obj->fd, buffer, BUFSIZ);
                int status;

                // Send the reponse "hostent" back to client.
                send (obj->accept_fd, buffer, r, 0);
                close (obj->accept_fd);
                removeFromDnsQueue (obj);

                wait(&status);
                close (obj->fd);
            }
        }
    }
    close (listen_fd);
    unlink (DNS_SOCK_NAME);
    exit (0);
}

#endif      // XP_UNIX && UNIX_ASYNC_DNS
