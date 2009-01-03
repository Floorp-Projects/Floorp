/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1989, 1993
 * The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/* nealsid:
 * This file was copied from libc/gen/nlist.c from Darwin's source code       
 * The version of nlist used as a base is from 10.5.2, libc-498               
 * http://www.opensource.apple.com/darwinsource/10.5.2/Libc-498/gen/nlist.c   
 *                                                                            
 * The full tarball is at:                                                    
 * http://www.opensource.apple.com/darwinsource/tarballs/apsl/Libc-498.tar.gz 
 *                                                                            
 * I've modified it to be compatible with 64-bit images. However,
 * 32-bit compatibility has not been retained. 
*/

#ifdef __LP64__

#include <mach-o/nlist.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "breakpad_nlist_64.h"
#include <TargetConditionals.h>
#include <stdio.h>
#include <mach/mach.h>

/* Stuff lifted from <a.out.h> and <sys/exec.h> since they are gone */
/*
 * Header prepended to each a.out file.
 */
struct exec {
  unsigned short  a_machtype;     /* machine type */
  unsigned short  a_magic;        /* magic number */
  unsigned long a_text;         /* size of text segment */
  unsigned long a_data;         /* size of initialized data */
  unsigned long a_bss;          /* size of uninitialized data */
  unsigned long a_syms;         /* size of symbol table */
  unsigned long a_entry;        /* entry point */
  unsigned long a_trsize;       /* size of text relocation */
  unsigned long a_drsize;       /* size of data relocation */
};

#define OMAGIC  0407            /* old impure format */
#define NMAGIC  0410            /* read-only text */
#define ZMAGIC  0413            /* demand load format */

#define N_BADMAG(x)                                                     \
  (((x).a_magic)!=OMAGIC && ((x).a_magic)!=NMAGIC && ((x).a_magic)!=ZMAGIC)
#define N_TXTOFF(x)                                     \
  ((x).a_magic==ZMAGIC ? 0 : sizeof (struct exec))
#define N_SYMOFF(x)                                                     \
  (N_TXTOFF(x) + (x).a_text+(x).a_data + (x).a_trsize+(x).a_drsize)

int
__breakpad_fdnlist_64(int fd, breakpad_nlist *list, const char **symbolNames);

/*
 * nlist - retreive attributes from name list (string table version)
 */

int
breakpad_nlist_64(const char *name,
                  breakpad_nlist *list,
                  const char **symbolNames) {
  int fd, n;

  fd = open(name, O_RDONLY, 0);
  if (fd < 0)
    return (-1);
  n = __breakpad_fdnlist_64(fd, list, symbolNames);
  (void)close(fd);
  return (n);
}

/* Note: __fdnlist() is called from kvm_nlist in libkvm's kvm.c */

int
__breakpad_fdnlist_64(int fd, breakpad_nlist *list, const char **symbolNames) {
  register breakpad_nlist *p, *q;
  breakpad_nlist space[BUFSIZ/sizeof (breakpad_nlist)];

  const register char *s1, *s2;
  register int n, m;
  int maxlen, nreq;
  off_t sa;             /* symbol address */
  off_t ss;             /* start of strings */
  struct exec buf;
  unsigned  arch_offset = 0;

  maxlen = 500;
  for (q = list, nreq = 0;
       symbolNames[q-list] && symbolNames[q-list][0];
       q++, nreq++) {

    q->n_type = 0;
    q->n_value = 0;
    q->n_desc = 0;
    q->n_sect = 0;
    q->n_un.n_strx = 0;
  }

  if (read(fd, (char *)&buf, sizeof(buf)) != sizeof(buf) ||
      (N_BADMAG(buf) && *((long *)&buf) != MH_MAGIC &&
       NXSwapBigLongToHost(*((long *)&buf)) != FAT_MAGIC) &&
      /* nealsid: The following is the big-endian ppc64 check */
      (*((uint32_t*)&buf)) != FAT_MAGIC) {
    return (-1);
  }

  /* Deal with fat file if necessary */
  if (NXSwapBigLongToHost(*((long *)&buf)) == FAT_MAGIC ||
      /* nealsid: The following is the big-endian ppc64 check */
      *((int*)&buf) == FAT_MAGIC) {
    struct host_basic_info hbi;
    struct fat_header fh;
    struct fat_arch *fat_archs, *fap;
    unsigned i;
    host_t host;

    /* Get our host info */
    host = mach_host_self();
    i = HOST_BASIC_INFO_COUNT;
    kern_return_t kr;
    if ((kr=host_info(host, HOST_BASIC_INFO,
                      (host_info_t)(&hbi), &i)) != KERN_SUCCESS) {
      return (-1);
    }
    mach_port_deallocate(mach_task_self(), host);

    /* Read in the fat header */
    lseek(fd, 0, SEEK_SET);
    if (read(fd, (char *)&fh, sizeof(fh)) != sizeof(fh)) {
      return (-1);
    }

    /* Convert fat_narchs to host byte order */
    fh.nfat_arch = NXSwapBigLongToHost(fh.nfat_arch);

    /* Read in the fat archs */
    fat_archs = (struct fat_arch *)malloc(fh.nfat_arch *
                                          sizeof(struct fat_arch));
    if (fat_archs == NULL) {
      return (-1);
    }
    if (read(fd, (char *)fat_archs,
             sizeof(struct fat_arch) * fh.nfat_arch) !=
        sizeof(struct fat_arch) * fh.nfat_arch) {
      free(fat_archs);
      return (-1);
    }

    /*
     * Convert archs to host byte ordering (a constraint of
     * cpusubtype_getbestarch()
     */
    for (i = 0; i < fh.nfat_arch; i++) {
      fat_archs[i].cputype =
        NXSwapBigLongToHost(fat_archs[i].cputype);
      fat_archs[i].cpusubtype =
        NXSwapBigLongToHost(fat_archs[i].cpusubtype);
      fat_archs[i].offset =
        NXSwapBigLongToHost(fat_archs[i].offset);
      fat_archs[i].size =
        NXSwapBigLongToHost(fat_archs[i].size);
      fat_archs[i].align =
        NXSwapBigLongToHost(fat_archs[i].align);
    }

    fap = NULL;
    for (i = 0; i < fh.nfat_arch; i++) {
      /* nealsid: Although the original Apple code uses host_info */
      /* to retrieve the CPU type, the host_info will still return */
      /* CPU_TYPE_X86 even if running as an x86_64 binary. Given that */
      /* this code isn't necessary on i386, I've decided to hardcode */
      /* looking for a 64-bit binary */
#if TARGET_CPU_X86_64
      if (fat_archs[i].cputype == CPU_TYPE_X86_64) {
#elif TARGET_CPU_PPC64
      if (fat_archs[i].cputype == CPU_TYPE_POWERPC64) {
#else
#error undefined cpu!
        {
#endif
          fap = &fat_archs[i];
          break;
        }
      }

      if (!fap) {
        free(fat_archs);
        return (-1);
      }
      arch_offset = fap->offset;
      free(fat_archs);

      /* Read in the beginning of the architecture-specific file */
      lseek(fd, arch_offset, SEEK_SET);
      if (read(fd, (char *)&buf, sizeof(buf)) != sizeof(buf)) {
        return (-1);
      }
    }

    if (*((int *)&buf) == MH_MAGIC_64) {
      struct mach_header_64 mh;
      struct load_command *load_commands, *lcp;
      struct symtab_command *stp;
      long i;

      lseek(fd, arch_offset, SEEK_SET);
      if (read(fd, (char *)&mh, sizeof(mh)) != sizeof(mh)) {
        return (-1);
      }
      load_commands = (struct load_command *)malloc(mh.sizeofcmds);
      if (load_commands == NULL) {
        return (-1);
      }
      if (read(fd, (char *)load_commands, mh.sizeofcmds) !=
          mh.sizeofcmds) {
        free(load_commands);
        return (-1);
      }
      stp = NULL;
      lcp = load_commands;
      // nealsid:iterate through all load commands, looking for
      // LC_SYMTAB load command
      for (i = 0; i < mh.ncmds; i++) {
        if (lcp->cmdsize % sizeof(long) != 0 ||
            lcp->cmdsize <= 0 ||
            (char *)lcp + lcp->cmdsize >
            (char *)load_commands + mh.sizeofcmds) {
          free(load_commands);
          return (-1);
        }
        if (lcp->cmd == LC_SYMTAB) {
          if (lcp->cmdsize !=
              sizeof(struct symtab_command)) {
            free(load_commands);
            return (-1);
          }
          stp = (struct symtab_command *)lcp;
          break;
        }
        lcp = (struct load_command *)
          ((char *)lcp + lcp->cmdsize);
      }
      if (stp == NULL) {
        free(load_commands);
        return (-1);
      }
      // sa points to the beginning of the symbol table
      sa = stp->symoff + arch_offset;
      // ss points to the beginning of the string table
      ss = stp->stroff + arch_offset;
      // n is the number of bytes in the symbol table
      // each symbol table entry is an nlist structure
      n = stp->nsyms * sizeof(breakpad_nlist);
      free(load_commands);
    }
    else {
      sa = N_SYMOFF(buf) + arch_offset;
      ss = sa + buf.a_syms + arch_offset;
      n = buf.a_syms;
    }

    lseek(fd, sa, SEEK_SET);

    // the algorithm here is to read the nlist entries in m-sized
    // chunks into q.  q is then iterated over. for each entry in q,
    // use the string table index(q->n_un.n_strx) to read the symbol 
    // name, then scan the nlist entries passed in by the user(via p),
    // and look for a match
    while (n) {
      long savpos;

      m = sizeof (space);
      if (n < m)
        m = n;
      if (read(fd, (char *)space, m) != m)
        break;
      n -= m;
      savpos = lseek(fd, 0, SEEK_CUR);
      for (q = space; (m -= sizeof(breakpad_nlist)) >= 0; q++) {
        char nambuf[BUFSIZ];

        if (q->n_un.n_strx == 0 || q->n_type & N_STAB)
          continue;

        // seek to the location in the binary where the symbol
        // name is stored & read it into memory
        lseek(fd, ss+q->n_un.n_strx, SEEK_SET);
        read(fd, nambuf, maxlen+1);
        s2 = nambuf;
        for (p = list; 
             symbolNames[p-list] && 
               symbolNames[p-list][0]; 
             p++) {
          // get the symbol name the user has passed in that 
          // corresponds to the nlist entry that we're looking at
          s1 = symbolNames[p - list];
          while (*s1) {
            if (*s1++ != *s2++)
              goto cont;
          }
          if (*s2)
            goto cont;

          p->n_value = q->n_value;
          p->n_type = q->n_type;
          p->n_desc = q->n_desc;
          p->n_sect = q->n_sect;
          p->n_un.n_strx = q->n_un.n_strx;
          if (--nreq == 0)
            return (nreq);

          break;
        cont:           ;
        }
      }
      lseek(fd, savpos, SEEK_SET);
    }
    return (nreq);
  }

#endif  /* __LP64__ */
