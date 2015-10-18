/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BaseElf_h
#define BaseElf_h

#include "ElfLoader.h"
#include "Elfxx.h"


/**
 * Base class for ELF libraries. This class includes things that will be
 * common between SystemElfs and CustomElfs.
 */
class BaseElf: public LibHandle
{
public:
  /**
   * Hash function for symbol lookup, as defined in ELF standard for System V.
   */
  static unsigned long Hash(const char *symbol);

  /**
   * Returns the address corresponding to the given symbol name (with a
   * pre-computed hash).
   */
  void *GetSymbolPtr(const char *symbol, unsigned long hash) const;

  /**
   * Returns a pointer to the Elf Symbol in the Dynamic Symbol table
   * corresponding to the given symbol name (with a pre-computed hash).
   */
  const Elf::Sym *GetSymbol(const char *symbol, unsigned long hash) const;

  BaseElf(const char *path, Mappable *mappable = nullptr)
  : LibHandle(path)
  , mappable(mappable)
  {
  }

protected:
   /**
    * Inherited from LibHandle. Those are temporary and are not supposed to
    * be used.
    */
   virtual void *GetSymbolPtr(const char *symbol) const;
   virtual bool Contains(void *addr) const;
   virtual void *GetBase() const { return GetPtr(0); }

#ifdef __ARM_EABI__
  virtual const void *FindExidx(int *pcount) const;
#endif

  virtual Mappable *GetMappable() const { return NULL; };

public:
/* private: */
  /**
   * Returns a pointer relative to the base address where the library is
   * loaded.
   */
  void *GetPtr(const Elf::Addr offset) const
  {
    if (reinterpret_cast<void *>(offset) > base)
      return reinterpret_cast<void *>(offset);
    return base + offset;
  }

  /**
   * Like the above, but returns a typed (const) pointer
   */
  template <typename T>
  const T *GetPtr(const Elf::Addr offset) const
  {
    if (reinterpret_cast<void *>(offset) > base)
      return reinterpret_cast<const T *>(offset);
    return reinterpret_cast<const T *>(base + offset);
  }

  /* Appropriated Mappable */
  /* /!\ we rely on this being nullptr for BaseElf instances, but not
   * CustomElf instances. */
  RefPtr<Mappable> mappable;

  /* Base address where the library is loaded */
  MappedPtr base;

  /* Buckets and chains for the System V symbol hash table */
  Array<Elf::Word> buckets;
  UnsizedArray<Elf::Word> chains;

/* protected: */
  /* String table */
  Elf::Strtab strtab;

  /* Symbol table */
  UnsizedArray<Elf::Sym> symtab;

#ifdef __ARM_EABI__
  /* ARM.exidx information used by FindExidx */
  Array<uint32_t[2]> arm_exidx;
#endif
};


/**
 * Class for ELF libraries that already loaded in memory.
 */
class LoadedElf: public BaseElf
{
public:
  /**
   * Returns a LoadedElf corresponding to the already loaded ELF
   * at the given base address.
   */
  static already_AddRefed<LibHandle> Create(const char *path,
                                                 void *base_addr);

private:
  LoadedElf(const char *path)
  : BaseElf(path) { }

  ~LoadedElf()
  {
    /* Avoid base's destructor unmapping something that doesn't actually
     * belong to it. */
    base.release();
    ElfLoader::Singleton.Forget(this);
  }

  /**
   * Initializes the library according to information found in the given
   * PT_DYNAMIC header.
   * Returns whether this succeeded or failed.
   */
  bool InitDyn(const Elf::Phdr *pt_dyn);
};


#endif /* BaseElf_h */
