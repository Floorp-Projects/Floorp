/* -*- Mode: C++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlueLinking.h"
#include "nsXPCOMGlue.h"

#include <mach-o/dyld.h>
#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <fcntl.h>
#include <unistd.h>
#include <mach/machine.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <limits.h>

#if defined(__i386__)
static const uint32_t CPU_TYPE = CPU_TYPE_X86;
#elif defined(__x86_64__)
static const uint32_t CPU_TYPE = CPU_TYPE_X86_64;
#elif defined(__ppc__)
static const uint32_t CPU_TYPE = CPU_TYPE_POWERPC;
#elif defined(__ppc64__)
static const uint32_t CPU_TYPE = CPU_TYPE_POWERPC64;
#else
#error Unsupported CPU type
#endif

#ifdef HAVE_64BIT_OS
#undef LC_SEGMENT
#define LC_SEGMENT LC_SEGMENT_64
#undef MH_MAGIC
#define MH_MAGIC MH_MAGIC_64
#define cpu_mach_header mach_header_64
#define segment_command segment_command_64
#else
#define cpu_mach_header mach_header
#endif

class ScopedMMap
{
public:
    ScopedMMap(const char *file): buf(NULL) {
        fd = open(file, O_RDONLY);
        if (fd < 0)
            return;
        struct stat st;
        if (fstat(fd, &st) < 0)
            return;
        size = st.st_size;
        buf = (char *)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    }
    ~ScopedMMap() {
        if (buf)
            munmap(buf, size);
        if (fd >= 0)
            close(fd);
    }
    operator char *() { return buf; }
    int getFd() { return fd; }
private:
    int fd;
    char *buf;
    size_t size;
};

static void
preload(const char *file)
{
    ScopedMMap buf(file);
    char *base = buf;
    if (!base)
        return;

    // An OSX binary might either be a fat (universal) binary or a
    // Mach-O binary. A fat binary actually embeds several Mach-O
    // binaries. If we have a fat binary, find the offset where the
    // Mach-O binary for our CPU type can be found.
    struct fat_header *fh = (struct fat_header *)base;

    if (OSSwapBigToHostInt32(fh->magic) == FAT_MAGIC) {
        uint32_t nfat_arch = OSSwapBigToHostInt32(fh->nfat_arch);
        struct fat_arch *arch = (struct fat_arch *)&buf[sizeof(struct fat_header)];
        for (; nfat_arch; arch++, nfat_arch--) {
            if (OSSwapBigToHostInt32(arch->cputype) == CPU_TYPE) {
                base += OSSwapBigToHostInt32(arch->offset);
                break;
            }
        }
        if (base == buf)
            return;
    }

    // Check Mach-O magic in the Mach header
    struct cpu_mach_header *mh = (struct cpu_mach_header *)base;
    if (mh->magic != MH_MAGIC)
        return;

    // The Mach header is followed by a sequence of load commands.
    // Each command has a header containing the command type and the
    // command size. LD_SEGMENT commands describes how the dynamic
    // loader is going to map the file in memory. We use that
    // information to find the biggest offset from the library that
    // will be mapped in memory.
    char *cmd = &base[sizeof(struct cpu_mach_header)];
    off_t end = 0;
    for (uint32_t ncmds = mh->ncmds; ncmds; ncmds--) {
        struct segment_command *sh = (struct segment_command *)cmd;
        if (sh->cmd != LC_SEGMENT)
            continue;
        if (end < sh->fileoff + sh->filesize)
            end = sh->fileoff + sh->filesize;
        cmd += sh->cmdsize;
    }
    // Let the kernel read ahead what the dynamic loader is going to
    // map in memory soon after. The F_RDADVISE fcntl is equivalent
    // to Linux' readahead() system call.
    if (end > 0) {
        struct radvisory ra;
        ra.ra_offset = (base - buf);
        ra.ra_count = end;
        fcntl(buf.getFd(), F_RDADVISE, &ra);
    }
}

static const mach_header* sXULLibImage;

static void
ReadDependentCB(const char *aDependentLib, bool do_preload)
{
    if (do_preload)
        preload(aDependentLib);
    (void) NSAddImage(aDependentLib,
                      NSADDIMAGE_OPTION_RETURN_ON_ERROR |
                      NSADDIMAGE_OPTION_MATCH_FILENAME_BY_INSTALLNAME);
}

static void*
LookupSymbol(const mach_header* aLib, const char* aSymbolName)
{
    // Try to use |NSLookupSymbolInImage| since it is faster than searching
    // the global symbol table.  If we couldn't get a mach_header pointer
    // for the XPCOM dylib, then use |NSLookupAndBindSymbol| to search the
    // global symbol table (this shouldn't normally happen, unless the user
    // has called XPCOMGlueStartup(".") and relies on libxpcom.dylib
    // already being loaded).
    NSSymbol sym = nullptr;
    if (aLib) {
        sym = NSLookupSymbolInImage(aLib, aSymbolName,
                                 NSLOOKUPSYMBOLINIMAGE_OPTION_BIND |
                                 NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
    } else {
        if (NSIsSymbolNameDefined(aSymbolName))
            sym = NSLookupAndBindSymbol(aSymbolName);
    }
    if (!sym)
        return nullptr;

    return NSAddressOfSymbol(sym);
}

nsresult
XPCOMGlueLoad(const char *xpcomFile, GetFrozenFunctionsFunc *func)
{
    const mach_header* lib = nullptr;

    if (xpcomFile[0] != '.' || xpcomFile[1] != '\0') {
        char xpcomDir[PATH_MAX];
        if (realpath(xpcomFile, xpcomDir)) {
            char *lastSlash = strrchr(xpcomDir, '/');
            if (lastSlash) {
                *lastSlash = '\0';

                XPCOMGlueLoadDependentLibs(xpcomDir, ReadDependentCB);

                snprintf(lastSlash, PATH_MAX - strlen(xpcomDir), "/" XUL_DLL);

                sXULLibImage = NSAddImage(xpcomDir,
                              NSADDIMAGE_OPTION_RETURN_ON_ERROR |
                              NSADDIMAGE_OPTION_WITH_SEARCHING |
                              NSADDIMAGE_OPTION_MATCH_FILENAME_BY_INSTALLNAME);
            }
        }

        lib = NSAddImage(xpcomFile,
                         NSADDIMAGE_OPTION_RETURN_ON_ERROR |
                         NSADDIMAGE_OPTION_WITH_SEARCHING |
                         NSADDIMAGE_OPTION_MATCH_FILENAME_BY_INSTALLNAME);

        if (!lib) {
            NSLinkEditErrors linkEditError;
            int errorNum;
            const char *errorString;
            const char *fileName;
            NSLinkEditError(&linkEditError, &errorNum, &fileName, &errorString);
            fprintf(stderr, "XPCOMGlueLoad error %d:%d for file %s:\n%s\n",
                    linkEditError, errorNum, fileName, errorString);
        }
    }

    *func = (GetFrozenFunctionsFunc) LookupSymbol(lib, "_NS_GetFrozenFunctions");

    return *func ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

void
XPCOMGlueUnload()
{
  // nothing to do, since we cannot unload dylibs on OS X
}

nsresult
XPCOMGlueLoadXULFunctions(const nsDynamicFunctionLoad *symbols)
{
    nsresult rv = NS_OK;
    while (symbols->functionName) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "_%s", symbols->functionName);

        *symbols->function = (NSFuncPtr) LookupSymbol(sXULLibImage, buffer);
        if (!*symbols->function)
            rv = NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;

        ++symbols;
    }

    return rv;
}
