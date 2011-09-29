/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2000
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

#if defined(AIX) || defined(__linux)
#include <sys/select.h>         // for fd_set
#endif

#if defined(__linux)
// Didn't find gettdtablehi() or gettdtablesize() on linux. Using FD_SETSIZE
#define getdtablehi() FD_SETSIZE
#else
#define getdtablehi() getdtablesize()

//  If you find a system doesn't have getdtablesize try #define getdtablesize
//  to FD_SETSIZE.  And if you encounter a system that doesn't even have 
//  FD_SETSIZE, just grab your ankles and use 255.
#endif


#include "nspr.h"
#include "nsCRT.h"
#include "unix_dns.h"

struct sockaddr_un  unix_addr;

int async_dns_lookup(char* hostName)
{
  fprintf(stderr, "start async_dns_lookup\n");
  int socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    fprintf(stderr, "socket returned error.\n");
    return -1;
  }

  unix_addr.sun_family = AF_UNIX;
  strcpy(unix_addr.sun_path, DNS_SOCK_NAME);

  int err = connect(socket_fd,(struct sockaddr*)&unix_addr, sizeof(unix_addr));
  if (err == -1) {
    fprintf(stderr, "connect failed (errno = %d).\n",errno);
    close(socket_fd);
    return -1;
  }

  char buf[256];
  strcpy(buf, "lookup: ");
  strcpy(&buf[8], hostName);

  err = send(socket_fd, buf, strlen(buf)+1, 0);
  if (err < 0)
    fprintf(stderr, "send(%s) returned error (errno=%d).\n",buf, errno);

  // receive 4 byte ID
  err = recv(socket_fd, buf, 256, 0);
  if (err < 0)
    fprintf(stderr, "recv() returned error (errno=%d).\n", errno);
  else
    {
      //  printf("recv() returned %d bytes.");
      int id = *(int *)buf;
      fprintf(stderr, "id: %d\n", id);
    }

  return socket_fd;
}

static char *
string_trim(char *s)
{
  char *s2;
  if (!s) return 0;
  s2 = s + strlen(s) - 1;
  while (s2 > s && (*s2 == '\n' || *s2 == '\r' || *s2 == ' ' || *s2 == '\t'))
    *s2-- = 0;
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
    s++;
  return s;
}

hostent *
bytesToHostent(char *buf)
{
  int i;
  // int size = 0;
  int len, aliasCount, addressCount;
  int addrtype, addrlength;
  char* p = buf;
  char s[1024];

  len = *(int *)p;            // length of name
  p += sizeof(int);           // advance past name length

  memcpy(s, p, len); s[len] = 0;
  fprintf(stderr, "hostname: %s\n", s);

  p += len;                   // advance past name
  aliasCount  = *(int *)p;    // number of aliases
  p += sizeof(int);           // advance past alias count

  for (i=0; i<aliasCount; i++) {
    len = *(int *)p;          // length of alias name
    p += sizeof(int);         // advance past alias name length

    memcpy(s, p, len); s[len] = 0;
    fprintf(stderr, "alias: %s\n", s);

    p += len;                 // advance past alias name
  }

  addrtype = *(int *)p;

  fprintf(stderr, "addrtype: %d\n", addrtype);

  p += sizeof(int);
  addrlength = *(int *)p;

  fprintf(stderr, "addrlength: %d\n", addrlength);

  p += sizeof(int);
  addressCount = *(int *)p;
  p += sizeof(int);

  for (i=0; i<addressCount; i++) {
    len = *(int *)p;    
    p += sizeof(int);

    fprintf(stderr, "addr len: %d\n", len);
    fprintf(stderr, "addr    : %x\n", *(int *)p);

    p += len;
  }

  // size = p - buf;
  // size += 1 + aliasCount;
  return 0;
}

int
main(int argc, char* argv[])
{
  PRStatus status;

  // launch daemon
  printf("### launch daemon...\n");

  PRProcessAttr *attributes = PR_NewProcessAttr();
  if (attributes == nsnull) {
    printf("PR_NewProcessAttr() failed.\n");
    return -1;
  }

  PRProcess *daemon = PR_CreateProcess("nsDnsAsyncLookup", nsnull, nsnull, attributes);
  if (daemon == nsnull) {
    printf("PR_CreateProcess failed.\n");
  } else {
    //    status = PR_DetachProcess(daemon);
    //if (status != 0)
    //  printf("PR_DetachProcess returned %d\n", status);
    //daemon = nsnull;
  }

  PR_DestroyProcessAttr(attributes);

  // create socket and connect to daemon
  int socket_fd = 0;


  bool notDone = true;
  char buf[1024];

  while(notDone) {
    int status = 0;
    fd_set fdset;

    FD_ZERO(&fdset);
    FD_SET(fileno(stdin), &fdset);
    if (socket_fd > 0)
      FD_SET(socket_fd, &fdset);
	   
    status = select(getdtablehi(), &fdset, 0, 0, 0);
    if (status <= 0)
      {
	fprintf(stderr, "%s: select() returned %d\n", argv[0], status);
	exit(-1);
      }

    // which fd is set?

    if (FD_ISSET(fileno(stdin), &fdset))
      {
	char *line = fgets(buf, sizeof(buf)-1, stdin);
	line = string_trim(line);
	
	if(!strcmp(line, "quit") || !strcmp(line, "exit"))
	  {
	    fprintf(stderr, "bye now.\n");
	    notDone = PR_FALSE;
	  }
	else if (!strncmp(line, "abort ", 6))
	  {
	    // abort id
	  }
	else if (strchr(line, ' ') || strchr(line, '\t'))
	  {
	    fprintf(stderr, "%s: unrecognized command %s.\n", argv[0], line);
	  }
	else
	  {
	    fprintf(stderr, "%s: looking up %s...\n", argv[0], line);
	    // initiate dns lookup
	    socket_fd = async_dns_lookup(line);
	  }
      }

    if (socket_fd && FD_ISSET(socket_fd, &fdset))
      {
	// read from socket, parse results
	int size = read(socket_fd, buf, 1024);
	if (size > 0)
	  {
	    // parse buffer into hostent
	    char *p = buf;
	    fprintf(stderr, "bytes read: %d\n", size);
	    fprintf(stderr, "response code: %d\n", *(int *)p);
	    p += sizeof(int);

	    for (int i=0; i < size; i++) {
	      if (!(i%8))
		fprintf(stderr, "\n");
	      fprintf(stderr, "%2.2x ",(unsigned char)buf[i]);
	    }
	    fprintf(stderr, "\n");
	    hostent *h;
	    h = bytesToHostent(p);
	  }
	close(socket_fd);
	socket_fd = 0;
      }
  }

  return 0;
}

/*
buffer

int nameLen;

if (nameLen > 0)
  char [nameLen+1] name

int aliasCount
for each alias
  int aliasNameLen
  char [aliasNameLen+1] aliasName

int h_addrtype
int h_length
int addrCount
for each addr
  char[h_length] addr



*/
