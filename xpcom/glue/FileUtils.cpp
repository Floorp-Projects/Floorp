/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FileUtils.h"

#include <errno.h>
#include <stdio.h>
#include <inttypes.h>

#include "nscore.h"
#include "private/pprio.h"
#include "prmem.h"
#include "mozilla/Assertions.h"
#include "mozilla/MemUtils.h"
#include "mozilla/BaseProfilerMarkers.h"

#if defined(XP_MACOSX)
#  include <fcntl.h>
#  include <unistd.h>
#  include <mach/machine.h>
#  include <mach-o/fat.h>
#  include <mach-o/loader.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <limits.h>
#elif defined(XP_UNIX)
#  include <fcntl.h>
#  include <unistd.h>
#  if defined(LINUX)
#    include <elf.h>
#  endif
#  include <sys/types.h>
#  include <sys/stat.h>
#elif defined(XP_WIN)
#  include <nsWindowsHelpers.h>
#  include <mozilla/NativeNt.h>
#  include <mozilla/ScopeExit.h>
#endif

// Functions that are not to be used in standalone glue must be implemented
// within this #if block
#if defined(MOZILLA_INTERNAL_API)

#  include "nsString.h"

bool mozilla::fallocate(PRFileDesc* aFD, int64_t aLength) {
#  if defined(HAVE_POSIX_FALLOCATE)
  return posix_fallocate(PR_FileDesc2NativeHandle(aFD), 0, aLength) == 0;
#  elif defined(XP_WIN)
  int64_t oldpos = PR_Seek64(aFD, 0, PR_SEEK_CUR);
  if (oldpos == -1) {
    return false;
  }

  if (PR_Seek64(aFD, aLength, PR_SEEK_SET) != aLength) {
    return false;
  }

  bool retval = (0 != SetEndOfFile((HANDLE)PR_FileDesc2NativeHandle(aFD)));

  PR_Seek64(aFD, oldpos, PR_SEEK_SET);
  return retval;
#  elif defined(XP_MACOSX)
  int fd = PR_FileDesc2NativeHandle(aFD);
  fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, aLength};
  // Try to get a continous chunk of disk space
  int ret = fcntl(fd, F_PREALLOCATE, &store);
  if (ret == -1) {
    // OK, perhaps we are too fragmented, allocate non-continuous
    store.fst_flags = F_ALLOCATEALL;
    ret = fcntl(fd, F_PREALLOCATE, &store);
    if (ret == -1) {
      return false;
    }
  }
  return ftruncate(fd, aLength) == 0;
#  elif defined(XP_UNIX)
  // The following is copied from fcntlSizeHint in sqlite
  /* If the OS does not have posix_fallocate(), fake it. First use
  ** ftruncate() to set the file size, then write a single byte to
  ** the last byte in each block within the extended region. This
  ** is the same technique used by glibc to implement posix_fallocate()
  ** on systems that do not have a real fallocate() system call.
  */
  int64_t oldpos = PR_Seek64(aFD, 0, PR_SEEK_CUR);
  if (oldpos == -1) {
    return false;
  }

  struct stat buf;
  int fd = PR_FileDesc2NativeHandle(aFD);
  if (fstat(fd, &buf)) {
    return false;
  }

  if (buf.st_size >= aLength) {
    return false;
  }

  const int nBlk = buf.st_blksize;

  if (!nBlk) {
    return false;
  }

  if (ftruncate(fd, aLength)) {
    return false;
  }

  int nWrite;  // Return value from write()
  int64_t iWrite = ((buf.st_size + 2 * nBlk - 1) / nBlk) * nBlk -
                   1;  // Next offset to write to
  while (iWrite < aLength) {
    nWrite = 0;
    if (PR_Seek64(aFD, iWrite, PR_SEEK_SET) == iWrite) {
      nWrite = PR_Write(aFD, "", 1);
    }
    if (nWrite != 1) {
      break;
    }
    iWrite += nBlk;
  }

  PR_Seek64(aFD, oldpos, PR_SEEK_SET);
  return nWrite == 1;
#  else
  return false;
#  endif
}

void mozilla::ReadAheadLib(nsIFile* aFile) {
#  if defined(XP_WIN)
  nsAutoString path;
  if (!aFile || NS_FAILED(aFile->GetPath(path))) {
    return;
  }
  ReadAheadLib(path.get());
#  elif defined(LINUX) && !defined(ANDROID) || defined(XP_MACOSX)
  nsAutoCString nativePath;
  if (!aFile || NS_FAILED(aFile->GetNativePath(nativePath))) {
    return;
  }
  ReadAheadLib(nativePath.get());
#  endif
}

void mozilla::ReadAheadFile(nsIFile* aFile, const size_t aOffset,
                            const size_t aCount, mozilla::filedesc_t* aOutFd) {
#  if defined(XP_WIN)
  nsAutoString path;
  if (!aFile || NS_FAILED(aFile->GetPath(path))) {
    return;
  }
  ReadAheadFile(path.get(), aOffset, aCount, aOutFd);
#  elif defined(LINUX) && !defined(ANDROID) || defined(XP_MACOSX)
  nsAutoCString nativePath;
  if (!aFile || NS_FAILED(aFile->GetNativePath(nativePath))) {
    return;
  }
  ReadAheadFile(nativePath.get(), aOffset, aCount, aOutFd);
#  endif
}

mozilla::PathString mozilla::GetLibraryName(mozilla::pathstr_t aDirectory,
                                            const char* aLib) {
#  ifdef XP_WIN
  nsAutoString fullName;
  if (aDirectory) {
    fullName.Assign(aDirectory);
    fullName.Append('\\');
  }
  AppendUTF8toUTF16(MakeStringSpan(aLib), fullName);
  if (!strstr(aLib, ".dll")) {
    fullName.AppendLiteral(".dll");
  }
  return std::move(fullName);
#  else
  char* temp = PR_GetLibraryName(aDirectory, aLib);
  if (!temp) {
    return ""_ns;
  }
  nsAutoCString libname(temp);
  PR_FreeLibraryName(temp);
  return std::move(libname);
#  endif
}

mozilla::PathString mozilla::GetLibraryFilePathname(mozilla::pathstr_t aName,
                                                    PRFuncPtr aAddr) {
#  ifdef XP_WIN
  HMODULE handle = GetModuleHandleW(char16ptr_t(aName));
  if (!handle) {
    return u""_ns;
  }

  nsAutoString path;
  path.SetLength(MAX_PATH);
  DWORD len = GetModuleFileNameW(handle, char16ptr_t(path.BeginWriting()),
                                 path.Length());
  if (!len) {
    return u""_ns;
  }

  path.SetLength(len);
  return std::move(path);
#  else
  char* temp = PR_GetLibraryFilePathname(aName, aAddr);
  if (!temp) {
    return ""_ns;
  }
  nsAutoCString path(temp);
  PR_Free(temp);  // PR_GetLibraryFilePathname() uses PR_Malloc().
  return std::move(path);
#  endif
}

#endif  // defined(MOZILLA_INTERNAL_API)

#if defined(LINUX) && !defined(ANDROID)

static const unsigned int bufsize = 4096;

#  ifdef __LP64__
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Phdr Elf_Phdr;
static const unsigned char ELFCLASS = ELFCLASS64;
typedef Elf64_Off Elf_Off;
#  else
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Phdr Elf_Phdr;
static const unsigned char ELFCLASS = ELFCLASS32;
typedef Elf32_Off Elf_Off;
#  endif

#elif defined(XP_MACOSX)

#  if defined(__i386__)
static const uint32_t CPU_TYPE = CPU_TYPE_X86;
#  elif defined(__x86_64__)
static const uint32_t CPU_TYPE = CPU_TYPE_X86_64;
#  elif defined(__ppc__)
static const uint32_t CPU_TYPE = CPU_TYPE_POWERPC;
#  elif defined(__ppc64__)
static const uint32_t CPU_TYPE = CPU_TYPE_POWERPC64;
#  elif defined(__aarch64__)
static const uint32_t CPU_TYPE = CPU_TYPE_ARM64;
#  else
#    error Unsupported CPU type
#  endif

#  ifdef __LP64__
#    undef LC_SEGMENT
#    define LC_SEGMENT LC_SEGMENT_64
#    undef MH_MAGIC
#    define MH_MAGIC MH_MAGIC_64
#    define cpu_mach_header mach_header_64
#    define segment_command segment_command_64
#  else
#    define cpu_mach_header mach_header
#  endif

class ScopedMMap {
 public:
  explicit ScopedMMap(const char* aFilePath) : buf(nullptr) {
    fd = open(aFilePath, O_RDONLY);
    if (fd < 0) {
      return;
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
      return;
    }
    size = st.st_size;
    buf = (char*)mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buf == MAP_FAILED) {
      buf = nullptr;
    }
  }
  ~ScopedMMap() {
    if (buf) {
      munmap(buf, size);
    }
    if (fd >= 0) {
      close(fd);
    }
  }
  operator char*() { return buf; }
  int getFd() { return fd; }

 private:
  int fd;
  char* buf;
  size_t size;
};
#endif

void mozilla::ReadAhead(mozilla::filedesc_t aFd, const size_t aOffset,
                        const size_t aCount) {
#if defined(XP_WIN)

  LARGE_INTEGER fpOriginal;
  LARGE_INTEGER fpOffset;
#  if defined(HAVE_LONG_LONG)
  fpOffset.QuadPart = 0;
#  else
  fpOffset.u.LowPart = 0;
  fpOffset.u.HighPart = 0;
#  endif

  // Get the current file pointer so that we can restore it. This isn't
  // really necessary other than to provide the same semantics regarding the
  // file pointer that other platforms do
  if (!SetFilePointerEx(aFd, fpOffset, &fpOriginal, FILE_CURRENT)) {
    return;
  }

  if (aOffset) {
#  if defined(HAVE_LONG_LONG)
    fpOffset.QuadPart = static_cast<LONGLONG>(aOffset);
#  else
    fpOffset.u.LowPart = aOffset;
    fpOffset.u.HighPart = 0;
#  endif

    if (!SetFilePointerEx(aFd, fpOffset, nullptr, FILE_BEGIN)) {
      return;
    }
  }

  char buf[64 * 1024];
  size_t totalBytesRead = 0;
  DWORD dwBytesRead;
  // Do dummy reads to trigger kernel-side readhead via
  // FILE_FLAG_SEQUENTIAL_SCAN. Abort when underfilling because during testing
  // the buffers are read fully A buffer that's not keeping up would imply that
  // readahead isn't working right
  while (totalBytesRead < aCount &&
         ReadFile(aFd, buf, sizeof(buf), &dwBytesRead, nullptr) &&
         dwBytesRead == sizeof(buf)) {
    totalBytesRead += dwBytesRead;
  }

  // Restore the file pointer
  SetFilePointerEx(aFd, fpOriginal, nullptr, FILE_BEGIN);

#elif defined(LINUX) && !defined(ANDROID)

  readahead(aFd, aOffset, aCount);

#elif defined(XP_MACOSX)

  struct radvisory ra;
  ra.ra_offset = aOffset;
  ra.ra_count = aCount;
  // The F_RDADVISE fcntl is equivalent to Linux' readahead() system call.
  fcntl(aFd, F_RDADVISE, &ra);

#endif
}

void mozilla::ReadAheadLib(mozilla::pathstr_t aFilePath) {
  if (!aFilePath) {
    return;
  }

#ifdef XP_WIN
  auto WideToUTF8 = [](const wchar_t* aStr) -> std::string {
    std::string s;
    // Determine the number of output bytes (including null terminator).
    const int numConv = ::WideCharToMultiByte(CP_UTF8, 0, aStr, -1, nullptr, 0,
                                              nullptr, nullptr);
    if (numConv == 0) {
      return s;
    }
    s.resize(numConv);
    const int numConvd = ::WideCharToMultiByte(CP_UTF8, 0, aStr, -1, s.data(),
                                               numConv, nullptr, nullptr);
    if (numConvd != numConv) {
      // Error during conversion, remove any temporary data.
      s.clear();
    }
    return s;
  };
#endif

  AUTO_BASE_PROFILER_MARKER_TEXT("ReadAheadLib", OTHER, {},
#ifdef XP_WIN
                                 WideToUTF8(aFilePath)
#else
                                 aFilePath
#endif
  );

#if defined(XP_WIN)
  if (!CanPrefetchMemory()) {
    ReadAheadFile(aFilePath);
    return;
  }
  nsAutoHandle fd(CreateFileW(aFilePath, GENERIC_READ | GENERIC_EXECUTE,
                              FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
  if (!fd) {
    return;
  }

  nsAutoHandle mapping(CreateFileMapping(
      fd, nullptr, SEC_IMAGE | PAGE_EXECUTE_READ, 0, 0, nullptr));
  if (!mapping) {
    return;
  }

  PVOID data = MapViewOfFile(
      mapping, FILE_MAP_READ | FILE_MAP_EXECUTE | SEC_IMAGE, 0, 0, 0);
  if (!data) {
    return;
  }
  auto guard = MakeScopeExit([=]() { UnmapViewOfFile(data); });
  mozilla::nt::PEHeaders headers(data);
  Maybe<Span<const uint8_t>> bounds = headers.GetBounds();
  if (!bounds) {
    return;
  }

  PrefetchMemory((uint8_t*)data, bounds->Length());

#elif defined(LINUX) && !defined(ANDROID)
  int fd = open(aFilePath, O_RDONLY);
  if (fd < 0) {
    return;
  }

  union {
    char buf[bufsize];
    Elf_Ehdr ehdr;
  } elf;
  // Read ELF header (ehdr) and program header table (phdr).
  // We check that the ELF magic is found, that the ELF class matches
  // our own, and that the program header table as defined in the ELF
  // headers fits in the buffer we read.
  if ((read(fd, elf.buf, bufsize) <= 0) || (memcmp(elf.buf, ELFMAG, 4)) ||
      (elf.ehdr.e_ident[EI_CLASS] != ELFCLASS) ||
      // Upcast e_phentsize so the multiplication is done in the same precision
      // as the subsequent addition, to satisfy static analyzers and avoid
      // issues with abnormally large program header tables.
      (elf.ehdr.e_phoff +
           (static_cast<Elf_Off>(elf.ehdr.e_phentsize) * elf.ehdr.e_phnum) >=
       bufsize)) {
    close(fd);
    return;
  }
  // The program header table contains segment definitions. One such
  // segment type is PT_LOAD, which describes how the dynamic loader
  // is going to map the file in memory. We use that information to
  // find the biggest offset from the library that will be mapped in
  // memory.
  Elf_Phdr* phdr = (Elf_Phdr*)&elf.buf[elf.ehdr.e_phoff];
  Elf_Off end = 0;
  for (int phnum = elf.ehdr.e_phnum; phnum; phdr++, phnum--) {
    if ((phdr->p_type == PT_LOAD) && (end < phdr->p_offset + phdr->p_filesz)) {
      end = phdr->p_offset + phdr->p_filesz;
    }
  }
  // Let the kernel read ahead what the dynamic loader is going to
  // map in memory soon after.
  if (end > 0) {
    ReadAhead(fd, 0, end);
  }
  close(fd);
#elif defined(XP_MACOSX)
  ScopedMMap buf(aFilePath);
  char* base = buf;
  if (!base) {
    return;
  }

  // An OSX binary might either be a fat (universal) binary or a
  // Mach-O binary. A fat binary actually embeds several Mach-O
  // binaries. If we have a fat binary, find the offset where the
  // Mach-O binary for our CPU type can be found.
  struct fat_header* fh = (struct fat_header*)base;

  if (OSSwapBigToHostInt32(fh->magic) == FAT_MAGIC) {
    uint32_t nfat_arch = OSSwapBigToHostInt32(fh->nfat_arch);
    struct fat_arch* arch = (struct fat_arch*)&buf[sizeof(struct fat_header)];
    for (; nfat_arch; arch++, nfat_arch--) {
      if (OSSwapBigToHostInt32(arch->cputype) == CPU_TYPE) {
        base += OSSwapBigToHostInt32(arch->offset);
        break;
      }
    }
    if (base == buf) {
      return;
    }
  }

  // Check Mach-O magic in the Mach header
  struct cpu_mach_header* mh = (struct cpu_mach_header*)base;
  if (mh->magic != MH_MAGIC) {
    return;
  }

  // The Mach header is followed by a sequence of load commands.
  // Each command has a header containing the command type and the
  // command size. LD_SEGMENT commands describes how the dynamic
  // loader is going to map the file in memory. We use that
  // information to find the biggest offset from the library that
  // will be mapped in memory.
  char* cmd = &base[sizeof(struct cpu_mach_header)];
  uint32_t end = 0;
  for (uint32_t ncmds = mh->ncmds; ncmds; ncmds--) {
    struct segment_command* sh = (struct segment_command*)cmd;
    if (sh->cmd != LC_SEGMENT) {
      continue;
    }
    if (end < sh->fileoff + sh->filesize) {
      end = sh->fileoff + sh->filesize;
    }
    cmd += sh->cmdsize;
  }
  // Let the kernel read ahead what the dynamic loader is going to
  // map in memory soon after.
  if (end > 0) {
    ReadAhead(buf.getFd(), base - buf, end);
  }
#endif
}

void mozilla::ReadAheadFile(mozilla::pathstr_t aFilePath, const size_t aOffset,
                            const size_t aCount, mozilla::filedesc_t* aOutFd) {
#if defined(XP_WIN)
  if (!aFilePath) {
    if (aOutFd) {
      *aOutFd = INVALID_HANDLE_VALUE;
    }
    return;
  }
  HANDLE fd = CreateFileW(aFilePath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                          OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (aOutFd) {
    *aOutFd = fd;
  }
  if (fd == INVALID_HANDLE_VALUE) {
    return;
  }
  ReadAhead(fd, aOffset, aCount);
  if (!aOutFd) {
    CloseHandle(fd);
  }
#elif defined(LINUX) && !defined(ANDROID) || defined(XP_MACOSX)
  if (!aFilePath) {
    if (aOutFd) {
      *aOutFd = -1;
    }
    return;
  }
  int fd = open(aFilePath, O_RDONLY);
  if (aOutFd) {
    *aOutFd = fd;
  }
  if (fd < 0) {
    return;
  }
  size_t count;
  if (aCount == SIZE_MAX) {
    struct stat st;
    if (fstat(fd, &st) < 0) {
      if (!aOutFd) {
        close(fd);
      }
      return;
    }
    count = st.st_size;
  } else {
    count = aCount;
  }
  ReadAhead(fd, aOffset, count);
  if (!aOutFd) {
    close(fd);
  }
#endif
}
