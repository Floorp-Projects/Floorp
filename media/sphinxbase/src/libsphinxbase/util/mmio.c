/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2005 Carnegie Mellon University.  All rights 
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */
/*********************************************************************
 *
 * File: mmio.c
 * 
 * Description: mmap() wrappers for Unix/Windows
 * 
 * Author: David Huggins-Daines <dhuggins@cs.cmu.edu>
 * 
 *********************************************************************/

#include <string.h>
#include <stdlib.h>


#ifdef GNUWINCE
# include <sys/wcebase.h>
# include <sys/wcetypes.h>
# include <sys/wcememory.h>
# include <sys/wcefile.h>
#elif defined(__SYMBIAN32__) /* SYMBIAN32 must be before WIN32 since Symbian SDK defines WIN32 as well */
# include <unistd.h>
# include <fcntl.h>
# include <sys/stat.h>
# include <sys/mman.h>
#elif defined(_WIN32)
# include <windows.h>
#else
# include <unistd.h>
# include <fcntl.h>
# include <sys/stat.h>
# include <sys/file.h>
# include <sys/mman.h>
#endif

#include "sphinxbase/prim_type.h"
#include "sphinxbase/err.h"
#include "sphinxbase/mmio.h"
#include "sphinxbase/ckd_alloc.h"

#if defined(_WIN32_WCE) || defined(GNUWINCE)
struct mmio_file_s {
	int dummy;
};

mmio_file_t *
mmio_file_read(const char *filename)
{
    HANDLE ffm, fd;
    WCHAR *wfilename;
    void *rv;
    int len;

    len = mbstowcs(NULL, filename, 0) + 1;
    wfilename = malloc(len * sizeof(WCHAR));
    mbstowcs(wfilename, filename, len);

    if ((ffm =
         CreateFileForMappingW(wfilename, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                               NULL)) == INVALID_HANDLE_VALUE) {
        E_ERROR("Failed to create mapping for the file '%s': %08x\n", filename,
                GetLastError());
        return NULL;
    }
    if ((fd =
         CreateFileMappingW(ffm, NULL, PAGE_READONLY, 0, 0, NULL)) == NULL) {
        E_ERROR("Failed to CreateFileMapping: %08x\n", GetLastError());
        CloseHandle(ffm);
        return NULL;
    }
    rv = MapViewOfFile(fd, FILE_MAP_READ, 0, 0, 0);
    free(wfilename);
    CloseHandle(ffm);
    CloseHandle(fd);

    return (mmio_file_t *) rv;
}

void
mmio_file_unmap(mmio_file_t *mf)
{
    if (!UnmapViewOfFile((void *)mf)) {
        E_ERROR("Failed to UnmapViewOfFile: %08x\n", GetLastError());
    }
}

void *
mmio_file_ptr(mmio_file_t *mf)
{
    return (void *)mf;
}

#elif defined(_WIN32) && !defined(_WIN32_WP) /* !WINCE */
struct mmio_file_s {
	int dummy;
};

mmio_file_t *
mmio_file_read(const char *filename)
{
    HANDLE ffm, fd;
    void *rv;

    if ((ffm = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
                         NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                         NULL)) == INVALID_HANDLE_VALUE) {
        E_ERROR("Failed to create file '%s': %08x\n",
                filename, GetLastError());
        return NULL;
    }
    if ((fd = CreateFileMapping(ffm, NULL,
                                PAGE_READONLY, 0, 0, NULL)) == NULL) {
        E_ERROR("Failed to CreateFileMapping: %08x\n", GetLastError());
        CloseHandle(ffm);
    }
    rv = MapViewOfFile(fd, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(ffm);
    CloseHandle(fd);

    return (mmio_file_t *)rv;
}

void
mmio_file_unmap(mmio_file_t *mf)
{
    if (!UnmapViewOfFile((void *)mf)) {
        E_ERROR("Failed to UnmapViewOfFile: %08x\n", GetLastError());
    }
}

void *
mmio_file_ptr(mmio_file_t *mf)
{
    return (void *)mf;
}

#else /* !WIN32, !WINCE */
#if defined(__ADSPBLACKFIN__) || defined(_WIN32_WP) 
				/* This is true for both uClinux and VisualDSP++,
                                 but actually we need a better way to detect it. */
struct mmio_file_s {
    int dummy;
};

mmio_file_t *
mmio_file_read(const char *filename)
{
    E_ERROR("mmio is not implemented on this platform!");
    return NULL;
}

void
mmio_file_unmap(mmio_file_t *mf)
{
    E_ERROR("mmio is not implemented on this platform!");
}

void *
mmio_file_ptr(mmio_file_t *mf)
{
    E_ERROR("mmio is not implemented on this platform!");
    return NULL;
}
#else /* !__ADSPBLACKFIN__ */
struct mmio_file_s {
    void *ptr;
    size_t mapsize;
};

mmio_file_t *
mmio_file_read(const char *filename)
{
    mmio_file_t *mf;
    struct stat buf;
    void *ptr;
    int fd;
    size_t pagesize;

    if ((fd = open(filename, O_RDONLY)) == -1) {
        E_ERROR_SYSTEM("Failed to open %s", filename);
        return NULL;
    }
    if (fstat(fd, &buf) == -1) {
        E_ERROR_SYSTEM("Failed to stat %s", filename);
        close(fd);
        return NULL;
    }
    ptr = mmap(NULL, buf.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == (void *)-1) {
        E_ERROR_SYSTEM("Failed to mmap %lld bytes", (unsigned long long)buf.st_size);
        close(fd);
        return NULL;
    }
    close(fd);
    mf = ckd_calloc(1, sizeof(*mf));
    mf->ptr = ptr;
    /* Align map size to next page. */
    pagesize = sysconf(_SC_PAGESIZE);
    mf->mapsize = (buf.st_size + pagesize - 1) / pagesize * pagesize;

    return mf;
}

void
mmio_file_unmap(mmio_file_t *mf)
{
    if (mf == NULL)
        return;
    if (munmap(mf->ptr, mf->mapsize) < 0) {
        E_ERROR_SYSTEM("Failed to unmap %ld bytes at %p", mf->mapsize, mf->ptr);
    }
    ckd_free(mf);
}

void *
mmio_file_ptr(mmio_file_t *mf)
{
    return mf->ptr;
}
#endif /* !__ADSPBLACKFIN__ */ 
#endif /* !(WINCE || WIN32) */
