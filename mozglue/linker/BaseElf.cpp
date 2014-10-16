/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseElf.h"
#include "Elfxx.h"
#include "Logging.h"
#include "mozilla/RefPtr.h"

using namespace Elf;
using namespace mozilla;

unsigned long
BaseElf::Hash(const char *symbol)
{
  const unsigned char *sym = reinterpret_cast<const unsigned char *>(symbol);
  unsigned long h = 0, g;
  while (*sym) {
    h = (h << 4) + *sym++;
    g = h & 0xf0000000;
    h ^= g;
    h ^= g >> 24;
  }
  return h;
}

void *
BaseElf::GetSymbolPtr(const char *symbol) const
{
  return GetSymbolPtr(symbol, Hash(symbol));
}

void *
BaseElf::GetSymbolPtr(const char *symbol, unsigned long hash) const
{
  const Sym *sym = GetSymbol(symbol, hash);
  void *ptr = nullptr;
  if (sym && sym->st_shndx != SHN_UNDEF)
    ptr = GetPtr(sym->st_value);
  DEBUG_LOG("BaseElf::GetSymbolPtr(%p [\"%s\"], \"%s\") = %p",
            reinterpret_cast<const void *>(this), GetPath(), symbol, ptr);
  return ptr;
}

const Sym *
BaseElf::GetSymbol(const char *symbol, unsigned long hash) const
{
  /* Search symbol with the buckets and chains tables.
   * The hash computed from the symbol name gives an index in the buckets
   * table. The corresponding value in the bucket table is an index in the
   * symbols table and in the chains table.
   * If the corresponding symbol in the symbols table matches, we're done.
   * Otherwise, the corresponding value in the chains table is a new index
   * in both tables, which corresponding symbol is tested and so on and so
   * forth */
  size_t bucket = hash % buckets.numElements();
  for (size_t y = buckets[bucket]; y != STN_UNDEF; y = chains[y]) {
    if (strcmp(symbol, strtab.GetStringAt(symtab[y].st_name)))
      continue;
    return &symtab[y];
  }
  return nullptr;
}

bool
BaseElf::Contains(void *addr) const
{
  return base.Contains(addr);
}

#ifdef __ARM_EABI__
const void *
BaseElf::FindExidx(int *pcount) const
{
  if (arm_exidx) {
    *pcount = arm_exidx.numElements();
    return arm_exidx;
  }
  *pcount = 0;
  return nullptr;
}
#endif

mozilla::TemporaryRef<LibHandle>
LoadedElf::Create(const char *path, void *base_addr)
{
  DEBUG_LOG("LoadedElf::Create(\"%s\", %p) = ...", path, base_addr);

  uint8_t mapped;
  /* If the page is not mapped, mincore returns an error. If base_addr is
   * nullptr, as would happen if the corresponding binary is prelinked with
   * the prelink look (but not with the android apriori tool), no page being
   * mapped there (right?), mincore returns an error, too, which makes
   * prelinked libraries on glibc unsupported. This is not an interesting
   * use case for now, so don't try supporting that case.
   */
  if (mincore(const_cast<void*>(base_addr), PageSize(), &mapped))
    return nullptr;

  RefPtr<LoadedElf> elf = new LoadedElf(path);

  const Ehdr *ehdr = Ehdr::validate(base_addr);
  if (!ehdr)
    return nullptr;

  Addr min_vaddr = (Addr) -1; // We want to find the lowest and biggest
  Addr max_vaddr = 0;         // virtual address used by this Elf.
  const Phdr *dyn = nullptr;
#ifdef __ARM_EABI__
  const Phdr *arm_exidx_phdr = nullptr;
#endif

  Array<Phdr> phdrs(reinterpret_cast<const char *>(ehdr) + ehdr->e_phoff,
                    ehdr->e_phnum);
  for (auto phdr = phdrs.begin(); phdr < phdrs.end(); ++phdr) {
    switch (phdr->p_type) {
      case PT_LOAD:
        if (phdr->p_vaddr < min_vaddr)
          min_vaddr = phdr->p_vaddr;
        if (max_vaddr < phdr->p_vaddr + phdr->p_memsz)
          max_vaddr = phdr->p_vaddr + phdr->p_memsz;
        break;
      case PT_DYNAMIC:
        dyn = &*phdr;
        break;
#ifdef __ARM_EABI__
      case PT_ARM_EXIDX:
        /* We cannot initialize arm_exidx here
           because we don't have a base yet */
        arm_exidx_phdr = &*phdr;
        break;
#endif
    }
  }

  /* If the lowest PT_LOAD virtual address in headers is not 0, then the ELF
   * is either prelinked or a non-PIE executable. The former case is not
   * possible, because base_addr would be nullptr and the mincore test above
   * would already have made us return.
   * For a non-PIE executable, PT_LOADs contain absolute addresses, so in
   * practice, this means min_vaddr should be equal to base_addr. max_vaddr
   * can thus be adjusted accordingly.
   */
  if (min_vaddr != 0) {
    void *min_vaddr_ptr = reinterpret_cast<void *>(
      static_cast<uintptr_t>(min_vaddr));
    if (min_vaddr_ptr != base_addr) {
      LOG("%s: %p != %p", elf->GetPath(), min_vaddr_ptr, base_addr);
      return nullptr;
    }
    max_vaddr -= min_vaddr;
  }
  if (!dyn) {
    LOG("%s: No PT_DYNAMIC segment found", elf->GetPath());
    return nullptr;
  }

  elf->base.Assign(base_addr, max_vaddr);

  if (!elf->InitDyn(dyn))
    return nullptr;

#ifdef __ARM_EABI__
  if (arm_exidx_phdr)
    elf->arm_exidx.InitSize(elf->GetPtr(arm_exidx_phdr->p_vaddr),
                            arm_exidx_phdr->p_memsz);
#endif

  DEBUG_LOG("LoadedElf::Create(\"%s\", %p) = %p", path, base_addr,
    static_cast<void *>(elf));

  ElfLoader::Singleton.Register(elf);
  return elf;
}

bool
LoadedElf::InitDyn(const Phdr *pt_dyn)
{
  Array<Dyn> dyns;
  dyns.InitSize(GetPtr<Dyn>(pt_dyn->p_vaddr), pt_dyn->p_filesz);

  size_t symnum = 0;
  for (auto dyn = dyns.begin(); dyn < dyns.end() && dyn->d_tag; ++dyn) {
    switch (dyn->d_tag) {
      case DT_HASH:
        {
          DEBUG_LOG("%s 0x%08" PRIxAddr, "DT_HASH", dyn->d_un.d_val);
          const Elf::Word *hash_table_header = \
            GetPtr<Elf::Word>(dyn->d_un.d_ptr);
          symnum = hash_table_header[1];
          buckets.Init(&hash_table_header[2], hash_table_header[0]);
          chains.Init(&*buckets.end());
        }
        break;
      case DT_STRTAB:
        DEBUG_LOG("%s 0x%08" PRIxAddr, "DT_STRTAB", dyn->d_un.d_val);
        strtab.Init(GetPtr(dyn->d_un.d_ptr));
        break;
      case DT_SYMTAB:
        DEBUG_LOG("%s 0x%08" PRIxAddr, "DT_SYMTAB", dyn->d_un.d_val);
        symtab.Init(GetPtr(dyn->d_un.d_ptr));
        break;
    }
  }
  if (!buckets || !symnum) {
    ERROR("%s: Missing or broken DT_HASH", GetPath());
  } else if (!strtab) {
    ERROR("%s: Missing DT_STRTAB", GetPath());
  } else if (!symtab) {
    ERROR("%s: Missing DT_SYMTAB", GetPath());
  } else {
    return true;
  }
  return false;
}
