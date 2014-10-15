/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CustomElf_h
#define CustomElf_h

#include "ElfLoader.h"
#include "BaseElf.h"
#include "Logging.h"
#include "Elfxx.h"

/**
 * Library Handle class for ELF libraries we don't let the system linker
 * handle.
 */
class CustomElf: public BaseElf, private ElfLoader::link_map
{
  friend class ElfLoader;
  friend class SEGVHandler;
public:
  /**
   * Returns a new CustomElf using the given file descriptor to map ELF
   * content. The file descriptor ownership is stolen, and it will be closed
   * in CustomElf's destructor if an instance is created, or by the Load
   * method otherwise. The path corresponds to the file descriptor, and flags
   * are the same kind of flags that would be given to dlopen(), though
   * currently, none are supported and the behaviour is more or less that of
   * RTLD_GLOBAL | RTLD_BIND_NOW.
   */
  static mozilla::TemporaryRef<LibHandle> Load(Mappable *mappable,
                                               const char *path, int flags);

  /**
   * Inherited from LibHandle/BaseElf
   */
  virtual ~CustomElf();
  virtual void *GetSymbolPtr(const char *symbol) const;
  virtual bool Contains(void *addr) const;
  virtual void *GetBase() const { return GetPtr(0); }

#ifdef __ARM_EABI__
  virtual const void *FindExidx(int *pcount) const;
#endif

protected:
  virtual Mappable *GetMappable() const;

public:
  /**
   * Shows some stats about the Mappable instance. The when argument is to be
   * used by the caller to give an identifier of the when the stats call is
   * made.
   */
  void stats(const char *when) const;

private:
  /**
   * Scan dependent libraries to find the address corresponding to the
   * given symbol name. This is used to find symbols that are undefined
   * in the Elf object.
   */
  void *GetSymbolPtrInDeps(const char *symbol) const;

  /**
   * Private constructor
   */
  CustomElf(Mappable *mappable, const char *path)
  : BaseElf(path)
  , link_map()
  , mappable(mappable)
  , init(0)
  , fini(0)
  , initialized(false)
  , has_text_relocs(false)
  { }

  /**
   * Loads an Elf segment defined by the given PT_LOAD header.
   * Returns whether this succeeded or failed.
   */
  bool LoadSegment(const Elf::Phdr *pt_load) const;

  /**
   * Initializes the library according to information found in the given
   * PT_DYNAMIC header.
   * Returns whether this succeeded or failed.
   */
  bool InitDyn(const Elf::Phdr *pt_dyn);

  /**
   * Apply .rel.dyn/.rela.dyn relocations.
   * Returns whether this succeeded or failed.
   */
  bool Relocate();

  /**
   * Apply .rel.plt/.rela.plt relocations.
   * Returns whether this succeeded or failed.
   */
  bool RelocateJumps();

  /**
   * Call initialization functions (.init/.init_array)
   * Returns true;
   */
  bool CallInit();

  /**
   * Call destructor functions (.fini_array/.fini)
   * Returns whether this succeeded or failed.
   */
  void CallFini();

  /**
   * Call a function given a pointer to its location.
   */
  void CallFunction(void *ptr) const
  {
    /* C++ doesn't allow direct conversion between pointer-to-object
     * and pointer-to-function. */
    union {
      void *ptr;
      void (*func)(void);
    } f;
    f.ptr = ptr;
    DEBUG_LOG("%s: Calling function @%p", GetPath(), ptr);
    f.func();
  }

  /**
   * Call a function given a an address relative to the library base
   */
  void CallFunction(Elf::Addr addr) const
  {
    return CallFunction(GetPtr(addr));
  }

  /* Appropriated Mappable */
  mozilla::RefPtr<Mappable> mappable;

  /* List of dependent libraries */
  std::vector<mozilla::RefPtr<LibHandle> > dependencies;

  /* List of .rel.dyn/.rela.dyn relocations */
  Array<Elf::Reloc> relocations;

  /* List of .rel.plt/.rela.plt relocation */
  Array<Elf::Reloc> jumprels;

  /* Relative address of the initialization and destruction functions
   * (.init/.fini) */
  Elf::Addr init, fini;

  /* List of initialization and destruction functions
   * (.init_array/.fini_array) */
  Array<void *> init_array, fini_array;

  bool initialized;

  bool has_text_relocs;

#ifdef __ARM_EABI__
  /* ARM.exidx information used by FindExidx */
  Array<uint32_t[2]> arm_exidx;
#endif
};

#endif /* CustomElf_h */
