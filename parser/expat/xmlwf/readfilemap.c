/*
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
License for the specific language governing rights and limitations
under the License.

The Original Code is expat.

The Initial Developer of the Original Code is James Clark.
Portions created by James Clark are Copyright (C) 1998
James Clark. All Rights Reserved.

Contributor(s):
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef S_ISREG
#ifndef S_IFREG
#define S_IFREG _S_IFREG
#endif
#ifndef S_IFMT
#define S_IFMT _S_IFMT
#endif
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif /* not S_ISREG */

#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0
#endif
#endif

int filemap(const char *name,
	    void (*processor)(const void *, size_t, const char *, void *arg),
	    void *arg)
{
  size_t nbytes;
  int fd;
  int n;
  struct stat sb;
  void *p;

  fd = open(name, O_RDONLY|O_BINARY);
  if (fd < 0) {
    perror(name);
    return 0;
  }
  if (fstat(fd, &sb) < 0) {
    perror(name);
    return 0;
  }
  if (!S_ISREG(sb.st_mode)) {
    fprintf(stderr, "%s: not a regular file\n", name);
    return 0;
  }
  nbytes = sb.st_size;
  p = malloc(nbytes);
  if (!p) {
    fprintf(stderr, "%s: out of memory\n", name);
    return 0;
  }
  n = read(fd, p, nbytes);
  if (n < 0) {
    perror(name);
    close(fd);
    return 0;
  }
  if (n != nbytes) {
    fprintf(stderr, "%s: read unexpected number of bytes\n", name);
    close(fd);
    return 0;
  }
  processor(p, nbytes, name, arg);
  free(p);
  close(fd);
  return 1;
}
