/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstring>
#include <sys/mman.h>
#include <vector>
#include <dlfcn.h>
#include <signal.h>
#include <string.h>
#include "CustomElf.h"
#include "BaseElf.h"
#include "Mappable.h"
#include "Logging.h"

using namespace Elf;
using namespace mozilla;

/* TODO: Fill ElfLoader::Singleton.lastError on errors. */

/* Function used to report library mappings from the custom linker to Gecko
 * crash reporter */
#ifdef ANDROID
extern "C" {
  void report_mapping(char *name, void *base, uint32_t len, uint32_t offset);
}
#else
#define report_mapping(...)
#endif

const Ehdr *Ehdr::validate(const void *buf)
{
  if (!buf || buf == MAP_FAILED)
    return nullptr;

  const Ehdr *ehdr = reinterpret_cast<const Ehdr *>(buf);

  /* Only support ELF executables or libraries for the host system */
  if (memcmp(ELFMAG, &ehdr->e_ident, SELFMAG) ||
      ehdr->e_ident[EI_CLASS] != ELFCLASS ||
      ehdr->e_ident[EI_DATA] != ELFDATA ||
      ehdr->e_ident[EI_VERSION] != 1 ||
      (ehdr->e_ident[EI_OSABI] != ELFOSABI && ehdr->e_ident[EI_OSABI] != ELFOSABI_NONE) ||
#ifdef EI_ABIVERSION
      ehdr->e_ident[EI_ABIVERSION] != ELFABIVERSION ||
#endif
      (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) ||
      ehdr->e_machine != ELFMACHINE ||
      ehdr->e_version != 1 ||
      ehdr->e_phentsize != sizeof(Phdr))
    return nullptr;

  return ehdr;
}

namespace {

void debug_phdr(const char *type, const Phdr *phdr)
{
  DEBUG_LOG("%s @0x%08" PRIxAddr " ("
            "filesz: 0x%08" PRIxAddr ", "
            "memsz: 0x%08" PRIxAddr ", "
            "offset: 0x%08" PRIxAddr ", "
            "flags: %c%c%c)",
            type, phdr->p_vaddr, phdr->p_filesz, phdr->p_memsz,
            phdr->p_offset, phdr->p_flags & PF_R ? 'r' : '-',
            phdr->p_flags & PF_W ? 'w' : '-', phdr->p_flags & PF_X ? 'x' : '-');
}

static int p_flags_to_mprot(Word flags)
{
  return ((flags & PF_X) ? PROT_EXEC : 0) |
         ((flags & PF_W) ? PROT_WRITE : 0) |
         ((flags & PF_R) ? PROT_READ : 0);
}

void
__void_stub(void)
{
}

} /* anonymous namespace */

/**
 * RAII wrapper for a mapping of the first page off a Mappable object.
 * This calls Mappable::munmap instead of system munmap.
 */
class Mappable1stPagePtr: public GenericMappedPtr<Mappable1stPagePtr> {
public:
  Mappable1stPagePtr(Mappable *mappable)
  : GenericMappedPtr<Mappable1stPagePtr>(
      mappable->mmap(nullptr, PageSize(), PROT_READ, MAP_PRIVATE, 0))
  , mappable(mappable)
  {
    /* Ensure the content of this page */
    mappable->ensure(*this);
  }

private:
  friend class GenericMappedPtr<Mappable1stPagePtr>;
  void munmap(void *buf, size_t length) {
    mappable->munmap(buf, length);
  }

  mozilla::RefPtr<Mappable> mappable;
};


TemporaryRef<LibHandle>
CustomElf::Load(Mappable *mappable, const char *path, int flags)
{
  DEBUG_LOG("CustomElf::Load(\"%s\", 0x%x) = ...", path, flags);
  if (!mappable)
    return nullptr;
  /* Keeping a RefPtr of the CustomElf is going to free the appropriate
   * resources when returning nullptr */
  RefPtr<CustomElf> elf = new CustomElf(mappable, path);
  /* Map the first page of the Elf object to access Elf and program headers */
  Mappable1stPagePtr ehdr_raw(mappable);
  if (ehdr_raw == MAP_FAILED)
    return nullptr;

  const Ehdr *ehdr = Ehdr::validate(ehdr_raw);
  if (!ehdr)
    return nullptr;

  /* Scan Elf Program Headers and gather some information about them */
  std::vector<const Phdr *> pt_loads;
  Addr min_vaddr = (Addr) -1; // We want to find the lowest and biggest
  Addr max_vaddr = 0;         // virtual address used by this Elf.
  const Phdr *dyn = nullptr;

  const Phdr *first_phdr = reinterpret_cast<const Phdr *>(
                           reinterpret_cast<const char *>(ehdr) + ehdr->e_phoff);
  const Phdr *end_phdr = &first_phdr[ehdr->e_phnum];
#ifdef __ARM_EABI__
  const Phdr *arm_exidx_phdr = nullptr;
#endif

  for (const Phdr *phdr = first_phdr; phdr < end_phdr; phdr++) {
    switch (phdr->p_type) {
      case PT_LOAD:
        debug_phdr("PT_LOAD", phdr);
        pt_loads.push_back(phdr);
        if (phdr->p_vaddr < min_vaddr)
          min_vaddr = phdr->p_vaddr;
        if (max_vaddr < phdr->p_vaddr + phdr->p_memsz)
          max_vaddr = phdr->p_vaddr + phdr->p_memsz;
        break;
      case PT_DYNAMIC:
        debug_phdr("PT_DYNAMIC", phdr);
        if (!dyn) {
          dyn = phdr;
        } else {
          ERROR("%s: Multiple PT_DYNAMIC segments detected", elf->GetPath());
          return nullptr;
        }
        break;
      case PT_TLS:
        debug_phdr("PT_TLS", phdr);
        if (phdr->p_memsz) {
          ERROR("%s: TLS is not supported", elf->GetPath());
          return nullptr;
        }
        break;
      case PT_GNU_STACK:
        debug_phdr("PT_GNU_STACK", phdr);
// Skip on Android until bug 706116 is fixed
#ifndef ANDROID
        if (phdr->p_flags & PF_X) {
          ERROR("%s: Executable stack is not supported", elf->GetPath());
          return nullptr;
        }
#endif
        break;
#ifdef __ARM_EABI__
      case PT_ARM_EXIDX:
        /* We cannot initialize arm_exidx here
           because we don't have a base yet */
        arm_exidx_phdr = phdr;
        break;
#endif
      default:
        DEBUG_LOG("%s: Program header type #%d not handled",
                  elf->GetPath(), phdr->p_type);
    }
  }

  if (min_vaddr != 0) {
    ERROR("%s: Unsupported minimal virtual address: 0x%08" PRIxAddr,
        elf->GetPath(), min_vaddr);
    return nullptr;
  }
  if (!dyn) {
    ERROR("%s: No PT_DYNAMIC segment found", elf->GetPath());
    return nullptr;
  }

  /* Reserve enough memory to map the complete virtual address space for this
   * library.
   * As we are using the base address from here to mmap something else with
   * MAP_FIXED | MAP_SHARED, we need to make sure these mmaps will work. For
   * instance, on armv6, MAP_SHARED mappings require a 16k alignment, but mmap
   * MAP_PRIVATE only returns a 4k aligned address. So we first get a base
   * address with MAP_SHARED, which guarantees the kernel returns an address
   * that we'll be able to use with MAP_FIXED, and then remap MAP_PRIVATE at
   * the same address, because of some bad side effects of keeping it as
   * MAP_SHARED. */
  elf->base.Assign(MemoryRange::mmap(nullptr, max_vaddr, PROT_NONE,
                                     MAP_SHARED | MAP_ANONYMOUS, -1, 0));
  if ((elf->base == MAP_FAILED) ||
      (mmap(elf->base, max_vaddr, PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != elf->base)) {
    ERROR("%s: Failed to mmap", elf->GetPath());
    return nullptr;
  }

  /* Load and initialize library */
  for (std::vector<const Phdr *>::iterator it = pt_loads.begin();
       it < pt_loads.end(); ++it)
    if (!elf->LoadSegment(*it))
      return nullptr;

  /* We're not going to mmap anymore */
  mappable->finalize();

  report_mapping(const_cast<char *>(elf->GetName()), elf->base,
                 (max_vaddr + PAGE_SIZE - 1) & PAGE_MASK, 0);

  elf->l_addr = elf->base;
  elf->l_name = elf->GetPath();
  elf->l_ld = elf->GetPtr<Dyn>(dyn->p_vaddr);
  ElfLoader::Singleton.Register(elf);

  if (!elf->InitDyn(dyn))
    return nullptr;

  if (elf->has_text_relocs) {
    for (std::vector<const Phdr *>::iterator it = pt_loads.begin();
         it < pt_loads.end(); ++it)
      mprotect(PageAlignedPtr(elf->GetPtr((*it)->p_vaddr)),
               PageAlignedEndPtr((*it)->p_memsz),
               p_flags_to_mprot((*it)->p_flags) | PROT_WRITE);
  }

  if (!elf->Relocate() || !elf->RelocateJumps())
    return nullptr;

  if (elf->has_text_relocs) {
    for (std::vector<const Phdr *>::iterator it = pt_loads.begin();
         it < pt_loads.end(); ++it)
      mprotect(PageAlignedPtr(elf->GetPtr((*it)->p_vaddr)),
               PageAlignedEndPtr((*it)->p_memsz),
               p_flags_to_mprot((*it)->p_flags));
  }

  if (!elf->CallInit())
    return nullptr;

#ifdef __ARM_EABI__
  if (arm_exidx_phdr)
    elf->arm_exidx.InitSize(elf->GetPtr(arm_exidx_phdr->p_vaddr),
                            arm_exidx_phdr->p_memsz);
#endif

  elf->stats("oneLibLoaded");
  DEBUG_LOG("CustomElf::Load(\"%s\", 0x%x) = %p", path, flags,
            static_cast<void *>(elf));
  return elf;
}

CustomElf::~CustomElf()
{
  DEBUG_LOG("CustomElf::~CustomElf(%p [\"%s\"])",
            reinterpret_cast<void *>(this), GetPath());
  CallFini();
  /* Normally, __cxa_finalize is called by the .fini function. However,
   * Android NDK before r6b doesn't do that. Our wrapped cxa_finalize only
   * calls destructors once, so call it in all cases. */
  ElfLoader::__wrap_cxa_finalize(this);
  ElfLoader::Singleton.Forget(this);
}

void *
CustomElf::GetSymbolPtr(const char *symbol) const
{
  return BaseElf::GetSymbolPtr(symbol, Hash(symbol));
}

void *
CustomElf::GetSymbolPtrInDeps(const char *symbol) const
{
  /* Resolve dlopen and related functions to point to ours */
  if (symbol[0] == 'd' && symbol[1] == 'l') {
    if (strcmp(symbol + 2, "open") == 0)
      return FunctionPtr(__wrap_dlopen);
    if (strcmp(symbol + 2, "error") == 0)
      return FunctionPtr(__wrap_dlerror);
    if (strcmp(symbol + 2, "close") == 0)
      return FunctionPtr(__wrap_dlclose);
    if (strcmp(symbol + 2, "sym") == 0)
      return FunctionPtr(__wrap_dlsym);
    if (strcmp(symbol + 2, "addr") == 0)
      return FunctionPtr(__wrap_dladdr);
    if (strcmp(symbol + 2, "_iterate_phdr") == 0)
      return FunctionPtr(__wrap_dl_iterate_phdr);
  } else if (symbol[0] == '_' && symbol[1] == '_') {
  /* Resolve a few C++ ABI specific functions to point to ours */
#ifdef __ARM_EABI__
    if (strcmp(symbol + 2, "aeabi_atexit") == 0)
      return FunctionPtr(&ElfLoader::__wrap_aeabi_atexit);
#else
    if (strcmp(symbol + 2, "cxa_atexit") == 0)
      return FunctionPtr(&ElfLoader::__wrap_cxa_atexit);
#endif
    if (strcmp(symbol + 2, "cxa_finalize") == 0)
      return FunctionPtr(&ElfLoader::__wrap_cxa_finalize);
    if (strcmp(symbol + 2, "dso_handle") == 0)
      return const_cast<CustomElf *>(this);
    if (strcmp(symbol + 2, "moz_linker_stats") == 0)
      return FunctionPtr(&ElfLoader::stats);
#ifdef __ARM_EABI__
    if (strcmp(symbol + 2, "gnu_Unwind_Find_exidx") == 0)
      return FunctionPtr(__wrap___gnu_Unwind_Find_exidx);
#endif
  } else if (symbol[0] == 's' && symbol[1] == 'i') {
    if (strcmp(symbol + 2, "gnal") == 0)
      return FunctionPtr(signal);
    if (strcmp(symbol + 2, "gaction") == 0)
      return FunctionPtr(sigaction);
  }

#define MISSING_FLASH_SYMNAME_START "_ZN7android10VectorImpl19reservedVectorImpl"

  // Android changed some symbols that Flash depended on in 4.4,
  // so stub those out here
  if (strncmp(symbol,
              MISSING_FLASH_SYMNAME_START,
              sizeof(MISSING_FLASH_SYMNAME_START) - 1) == 0) {
    return FunctionPtr(__void_stub);
  }

  void *sym;

  unsigned long hash = Hash(symbol);

  /* self_elf should never be NULL, but better safe than sorry. */
  if (ElfLoader::Singleton.self_elf) {
    /* We consider the library containing this code a permanent LD_PRELOAD,
     * so, check if the symbol exists here first. */
    sym = ElfLoader::Singleton.self_elf->GetSymbolPtr(symbol, hash);
    if (sym)
      return sym;
  }

  /* Then search the symbol in our dependencies. Since we already searched in
   * libraries the system linker loaded, skip those (on glibc systems). We
   * also assume the symbol is to be found in one of the dependent libraries
   * directly, not in their own dependent libraries. Building libraries with
   * --no-allow-shlib-undefined ensures such indirect symbol dependency don't
   * happen. */
  for (std::vector<RefPtr<LibHandle> >::const_iterator it = dependencies.begin();
       it < dependencies.end(); ++it) {
    if (!(*it)->IsSystemElf()) {
      sym = static_cast<BaseElf *>(
        static_cast<CustomElf *>((*it).get()))->GetSymbolPtr(symbol, hash);
    } else {
      sym = (*it)->GetSymbolPtr(symbol);
    }
    if (sym)
      return sym;
  }
  return nullptr;
}

bool
CustomElf::Contains(void *addr) const
{
  return base.Contains(addr);
}

#ifdef __ARM_EABI__
const void *
CustomElf::FindExidx(int *pcount) const
{
  if (arm_exidx) {
    *pcount = arm_exidx.numElements();
    return arm_exidx;
  }
  *pcount = 0;
  return nullptr;
}
#endif

void
CustomElf::stats(const char *when) const
{
  mappable->stats(when, GetPath());
}

bool
CustomElf::LoadSegment(const Phdr *pt_load) const
{
  if (pt_load->p_type != PT_LOAD) {
    DEBUG_LOG("%s: Elf::LoadSegment only takes PT_LOAD program headers", GetPath());
    return false;;
  }

  int prot = p_flags_to_mprot(pt_load->p_flags);

  /* Mmap at page boundary */
  Addr align = PageSize();
  Addr align_offset;
  void *mapped, *where;
  do {
    align_offset = pt_load->p_vaddr - AlignedPtr(pt_load->p_vaddr, align);
    where = GetPtr(pt_load->p_vaddr - align_offset);
    DEBUG_LOG("%s: Loading segment @%p %c%c%c", GetPath(), where,
                                                prot & PROT_READ ? 'r' : '-',
                                                prot & PROT_WRITE ? 'w' : '-',
                                                prot & PROT_EXEC ? 'x' : '-');
    mapped = mappable->mmap(where, pt_load->p_filesz + align_offset,
                            prot, MAP_PRIVATE | MAP_FIXED,
                            pt_load->p_offset - align_offset);
    if ((mapped != MAP_FAILED) || (pt_load->p_vaddr == 0) ||
        (pt_load->p_align == align))
      break;
    /* The virtual address space for the library is properly aligned at
     * 16k on ARMv6 (see CustomElf::Load), and so is the first segment
     * (p_vaddr == 0). But subsequent segments may not be 16k aligned
     * and fail to mmap. In such case, try to mmap again at the p_align
     * boundary instead of page boundary. */
    DEBUG_LOG("%s: Failed to mmap, retrying", GetPath());
    align = pt_load->p_align;
  } while (1);

  if (mapped != where) {
    if (mapped == MAP_FAILED) {
      ERROR("%s: Failed to mmap", GetPath());
    } else {
      ERROR("%s: Didn't map at the expected location (wanted: %p, got: %p)",
          GetPath(), where, mapped);
    }
    return false;
  }

  /* Ensure the availability of all pages within the mapping if on-demand
   * decompression is disabled (MOZ_LINKER_ONDEMAND=0 or signal handler not
   * registered). */
  const char *ondemand = getenv("MOZ_LINKER_ONDEMAND");
  if (!ElfLoader::Singleton.hasRegisteredHandler() ||
      (ondemand && !strncmp(ondemand, "0", 2 /* Including '\0' */))) {
    for (Addr off = 0; off < pt_load->p_filesz + align_offset;
         off += PageSize()) {
      mappable->ensure(reinterpret_cast<char *>(mapped) + off);
    }
  }
  /* When p_memsz is greater than p_filesz, we need to have nulled out memory
   * after p_filesz and before p_memsz.
   * Above the end of the last page, and up to p_memsz, we already have nulled
   * out memory because we mapped anonymous memory on the whole library virtual
   * address space. We just need to adjust this anonymous memory protection
   * flags. */
  if (pt_load->p_memsz > pt_load->p_filesz) {
    Addr file_end = pt_load->p_vaddr + pt_load->p_filesz;
    Addr mem_end = pt_load->p_vaddr + pt_load->p_memsz;
    Addr next_page = PageAlignedEndPtr(file_end);
    if (next_page > file_end) {
      /* The library is not registered at this point, so we can't rely on
       * on-demand decompression to handle missing pages here. */
      void *ptr = GetPtr(file_end);
      mappable->ensure(ptr);
      memset(ptr, 0, next_page - file_end);
    }
    if (mem_end > next_page) {
      if (mprotect(GetPtr(next_page), mem_end - next_page, prot) < 0) {
        ERROR("%s: Failed to mprotect", GetPath());
        return false;
      }
    }
  }
  return true;
}

namespace {

void debug_dyn(const char *type, const Dyn *dyn)
{
  DEBUG_LOG("%s 0x%08" PRIxAddr, type, dyn->d_un.d_val);
}

} /* anonymous namespace */

bool
CustomElf::InitDyn(const Phdr *pt_dyn)
{
  /* Scan PT_DYNAMIC segment and gather some information */
  const Dyn *first_dyn = GetPtr<Dyn>(pt_dyn->p_vaddr);
  const Dyn *end_dyn = GetPtr<Dyn>(pt_dyn->p_vaddr + pt_dyn->p_filesz);
  std::vector<Word> dt_needed;
  size_t symnum = 0;
  for (const Dyn *dyn = first_dyn; dyn < end_dyn && dyn->d_tag; dyn++) {
    switch (dyn->d_tag) {
      case DT_NEEDED:
        debug_dyn("DT_NEEDED", dyn);
        dt_needed.push_back(dyn->d_un.d_val);
        break;
      case DT_HASH:
        {
          debug_dyn("DT_HASH", dyn);
          const Word *hash_table_header = GetPtr<Word>(dyn->d_un.d_ptr);
          symnum = hash_table_header[1];
          buckets.Init(&hash_table_header[2], hash_table_header[0]);
          chains.Init(&*buckets.end());
        }
        break;
      case DT_STRTAB:
        debug_dyn("DT_STRTAB", dyn);
        strtab.Init(GetPtr(dyn->d_un.d_ptr));
        break;
      case DT_SYMTAB:
        debug_dyn("DT_SYMTAB", dyn);
        symtab.Init(GetPtr(dyn->d_un.d_ptr));
        break;
      case DT_SYMENT:
        debug_dyn("DT_SYMENT", dyn);
        if (dyn->d_un.d_val != sizeof(Sym)) {
          ERROR("%s: Unsupported DT_SYMENT", GetPath());
          return false;
        }
        break;
      case DT_TEXTREL:
        if (strcmp("libflashplayer.so", GetName()) == 0) {
          has_text_relocs = true;
        } else {
          ERROR("%s: Text relocations are not supported", GetPath());
          return false;
        }
        break;
      case DT_STRSZ: /* Ignored */
        debug_dyn("DT_STRSZ", dyn);
        break;
      case UNSUPPORTED_RELOC():
      case UNSUPPORTED_RELOC(SZ):
      case UNSUPPORTED_RELOC(ENT):
        ERROR("%s: Unsupported relocations", GetPath());
        return false;
      case RELOC():
        debug_dyn(STR_RELOC(), dyn);
        relocations.Init(GetPtr(dyn->d_un.d_ptr));
        break;
      case RELOC(SZ):
        debug_dyn(STR_RELOC(SZ), dyn);
        relocations.InitSize(dyn->d_un.d_val);
        break;
      case RELOC(ENT):
        debug_dyn(STR_RELOC(ENT), dyn);
        if (dyn->d_un.d_val != sizeof(Reloc)) {
          ERROR("%s: Unsupported DT_RELENT", GetPath());
          return false;
        }
        break;
      case DT_JMPREL:
        debug_dyn("DT_JMPREL", dyn);
        jumprels.Init(GetPtr(dyn->d_un.d_ptr));
        break;
      case DT_PLTRELSZ:
        debug_dyn("DT_PLTRELSZ", dyn);
        jumprels.InitSize(dyn->d_un.d_val);
        break;
      case DT_PLTGOT:
        debug_dyn("DT_PLTGOT", dyn);
        break;
      case DT_INIT:
        debug_dyn("DT_INIT", dyn);
        init = dyn->d_un.d_ptr;
        break;
      case DT_INIT_ARRAY:
        debug_dyn("DT_INIT_ARRAY", dyn);
        init_array.Init(GetPtr(dyn->d_un.d_ptr));
        break;
      case DT_INIT_ARRAYSZ:
        debug_dyn("DT_INIT_ARRAYSZ", dyn);
        init_array.InitSize(dyn->d_un.d_val);
        break;
      case DT_FINI:
        debug_dyn("DT_FINI", dyn);
        fini = dyn->d_un.d_ptr;
        break;
      case DT_FINI_ARRAY:
        debug_dyn("DT_FINI_ARRAY", dyn);
        fini_array.Init(GetPtr(dyn->d_un.d_ptr));
        break;
      case DT_FINI_ARRAYSZ:
        debug_dyn("DT_FINI_ARRAYSZ", dyn);
        fini_array.InitSize(dyn->d_un.d_val);
        break;
      case DT_PLTREL:
        if (dyn->d_un.d_val != RELOC()) {
          ERROR("%s: Error: DT_PLTREL is not " STR_RELOC(), GetPath());
          return false;
        }
        break;
      case DT_FLAGS:
        {
           Addr flags = dyn->d_un.d_val;
           /* Treat as a DT_TEXTREL tag */
           if (flags & DF_TEXTREL) {
             if (strcmp("libflashplayer.so", GetName()) == 0) {
               has_text_relocs = true;
             } else {
               ERROR("%s: Text relocations are not supported", GetPath());
               return false;
             }
           }
           /* we can treat this like having a DT_SYMBOLIC tag */
           flags &= ~DF_SYMBOLIC;
           if (flags)
             WARN("%s: unhandled flags #%" PRIxAddr" not handled",
                 GetPath(), flags);
        }
        break;
      case DT_SONAME: /* Should match GetName(), but doesn't matter */
      case DT_SYMBOLIC: /* Indicates internal symbols should be looked up in
                         * the library itself first instead of the executable,
                         * which is actually what this linker does by default */
      case RELOC(COUNT): /* Indicates how many relocations are relative, which
                          * is usually used to skip relocations on prelinked
                          * libraries. They are not supported anyways. */
      case UNSUPPORTED_RELOC(COUNT): /* This should error out, but it doesn't
                                      * really matter. */
      case DT_FLAGS_1: /* Additional linker-internal flags that we don't care about. See
                        * DF_1_* values in src/include/elf/common.h in binutils. */
      case DT_VERSYM: /* DT_VER* entries are used for symbol versioning, which */
      case DT_VERDEF: /* this linker doesn't support yet. */
      case DT_VERDEFNUM:
      case DT_VERNEED:
      case DT_VERNEEDNUM:
        /* Ignored */
        break;
      default:
        WARN("%s: dynamic header type #%" PRIxAddr" not handled",
            GetPath(), dyn->d_tag);
    }
  }

  if (!buckets || !symnum) {
    ERROR("%s: Missing or broken DT_HASH", GetPath());
    return false;
  }
  if (!strtab) {
    ERROR("%s: Missing DT_STRTAB", GetPath());
    return false;
  }
  if (!symtab) {
    ERROR("%s: Missing DT_SYMTAB", GetPath());
    return false;
  }

  /* Load dependent libraries */
  for (size_t i = 0; i < dt_needed.size(); i++) {
    const char *name = strtab.GetStringAt(dt_needed[i]);
    RefPtr<LibHandle> handle =
      ElfLoader::Singleton.Load(name, RTLD_GLOBAL | RTLD_LAZY, this);
    if (!handle)
      return false;
    dependencies.push_back(handle);
  }

  return true;
}

bool
CustomElf::Relocate()
{
  DEBUG_LOG("Relocate %s @%p", GetPath(), static_cast<void *>(base));
  uint32_t symtab_index = (uint32_t) -1;
  void *symptr = nullptr;
  for (Array<Reloc>::iterator rel = relocations.begin();
       rel < relocations.end(); ++rel) {
    /* Location of the relocation */
    void *ptr = GetPtr(rel->r_offset);

    /* R_*_RELATIVE relocations apply directly at the given location */
    if (ELF_R_TYPE(rel->r_info) == R_RELATIVE) {
      *(void **) ptr = GetPtr(rel->GetAddend(base));
      continue;
    }
    /* Other relocation types need a symbol resolution */
    /* Avoid symbol resolution when it's the same symbol as last iteration */
    if (symtab_index != ELF_R_SYM(rel->r_info)) {
      symtab_index = ELF_R_SYM(rel->r_info);
      const Sym sym = symtab[symtab_index];
      if (sym.st_shndx != SHN_UNDEF) {
        symptr = GetPtr(sym.st_value);
      } else {
        /* TODO: handle symbol resolving to nullptr vs. being undefined. */
        symptr = GetSymbolPtrInDeps(strtab.GetStringAt(sym.st_name));
      }
    }

    if (symptr == nullptr)
      WARN("%s: Relocation to NULL @0x%08" PRIxAddr,
          GetPath(), rel->r_offset);

    /* Apply relocation */
    switch (ELF_R_TYPE(rel->r_info)) {
    case R_GLOB_DAT:
      /* R_*_GLOB_DAT relocations simply use the symbol value */
      *(void **) ptr = symptr;
      break;
    case R_ABS:
      /* R_*_ABS* relocations add the relocation added to the symbol value */
      *(const char **) ptr = (const char *)symptr + rel->GetAddend(base);
      break;
    default:
      ERROR("%s: Unsupported relocation type: 0x%" PRIxAddr,
          GetPath(), ELF_R_TYPE(rel->r_info));
      return false;
    }
  }
  return true;
}

bool
CustomElf::RelocateJumps()
{
  /* TODO: Dynamic symbol resolution */
  for (Array<Reloc>::iterator rel = jumprels.begin();
       rel < jumprels.end(); ++rel) {
    /* Location of the relocation */
    void *ptr = GetPtr(rel->r_offset);

    /* Only R_*_JMP_SLOT relocations are expected */
    if (ELF_R_TYPE(rel->r_info) != R_JMP_SLOT) {
      ERROR("%s: Jump relocation type mismatch", GetPath());
      return false;
    }

    /* TODO: Avoid code duplication with the relocations above */
    const Sym sym = symtab[ELF_R_SYM(rel->r_info)];
    void *symptr;
    if (sym.st_shndx != SHN_UNDEF)
      symptr = GetPtr(sym.st_value);
    else
      symptr = GetSymbolPtrInDeps(strtab.GetStringAt(sym.st_name));

    if (symptr == nullptr) {
      if (ELF_ST_BIND(sym.st_info) == STB_WEAK) {
        WARN("%s: Relocation to NULL @0x%08" PRIxAddr " for symbol \"%s\"",
            GetPath(),
            rel->r_offset, strtab.GetStringAt(sym.st_name));
      } else {
        ERROR("%s: Relocation to NULL @0x%08" PRIxAddr " for symbol \"%s\"",
            GetPath(),
            rel->r_offset, strtab.GetStringAt(sym.st_name));
        return false;
      }
    }
    /* Apply relocation */
    *(void **) ptr = symptr;
  }
  return true;
}

bool
CustomElf::CallInit()
{
  if (init)
    CallFunction(init);

  for (Array<void *>::iterator it = init_array.begin();
       it < init_array.end(); ++it) {
    /* Android x86 NDK wrongly puts 0xffffffff in INIT_ARRAY */
    if (*it && *it != reinterpret_cast<void *>(-1))
      CallFunction(*it);
  }
  initialized = true;
  return true;
}

void
CustomElf::CallFini()
{
  if (!initialized)
    return;
  for (Array<void *>::reverse_iterator it = fini_array.rbegin();
       it < fini_array.rend(); ++it) {
    /* Android x86 NDK wrongly puts 0xffffffff in FINI_ARRAY */
    if (*it && *it != reinterpret_cast<void *>(-1))
      CallFunction(*it);
  }
  if (fini)
    CallFunction(fini);
}

Mappable *
CustomElf::GetMappable() const
{
  if (!mappable)
    return nullptr;
  if (mappable->GetKind() == Mappable::MAPPABLE_EXTRACT_FILE)
    return mappable;
  return ElfLoader::GetMappableFromPath(GetPath());
}
