/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ElfLoader_h
#define ElfLoader_h

#include <vector>
/* Until RefPtr.h stops using JS_Assert */
#undef DEBUG
#include "mozilla/RefPtr.h"

/**
 * dlfcn.h replacement functions
 */
extern "C" {
  void *__wrap_dlopen(const char *path, int flags);
  const char *__wrap_dlerror(void);
  void *__wrap_dlsym(void *handle, const char *symbol);
  int __wrap_dlclose(void *handle);

#ifndef HAVE_DLADDR
  typedef struct {
    const char *dli_fname;
    void *dli_fbase;
    const char *dli_sname;
    void *dli_saddr;
  } Dl_info;
#endif
  int __wrap_dladdr(void *addr, Dl_info *info);
}

/**
 * Abstract class for loaded libraries. Libraries may be loaded through the
 * system linker or this linker, both cases will be derived from this class.
 */
class LibHandle: public mozilla::RefCounted<LibHandle>
{
public:
  /**
   * Constructor. Takes the path of the loaded library and will store a copy
   * of the leaf name.
   */
  LibHandle(const char *path)
  : directRefCnt(0), path(path ? strdup(path) : NULL) { }

  /**
   * Destructor.
   */
  virtual ~LibHandle();

  /**
   * Returns the pointer to the address to which the given symbol resolves
   * inside the library. It is not supposed to resolve the symbol in other
   * libraries, although in practice, it will for system libraries.
   */
  virtual void *GetSymbolPtr(const char *symbol) const = 0;

  /**
   * Returns whether the given address is part of the virtual address space
   * covered by the loaded library.
   */
  virtual bool Contains(void *addr) const = 0;

  /**
   * Returns the file name of the library without the containing directory.
   */
  const char *GetName() const;

  /**
   * Returns the full path of the library, when available. Otherwise, returns
   * the file name.
   */
  const char *GetPath() const
  {
    return path;
  }

  /**
   * Library handles can be referenced from other library handles or
   * externally (when dlopen()ing using this linker). We need to be
   * able to distinguish between the two kind of referencing for better
   * bookkeeping.
   */
  void AddDirectRef()
  {
    ++directRefCnt;
    mozilla::RefCounted<LibHandle>::AddRef();
  }

  /**
   * Releases a direct reference, and returns whether there are any direct
   * references left.
   */
  bool ReleaseDirectRef()
  {
    bool ret = false;
    if (directRefCnt) {
      // ASSERT(directRefCnt >= mozilla::RefCounted<LibHandle>::refCount())
      if (--directRefCnt)
        ret = true;
      mozilla::RefCounted<LibHandle>::Release();
    }
    return ret;
  }

  /**
   * Returns the number of direct references
   */
  int DirectRefCount()
  {
    return directRefCnt;
  }

protected:
  /**
   * Returns whether the handle is a SystemElf or not. (short of a better way
   * to do this without RTTI)
   */
  friend class ElfLoader;
  virtual bool IsSystemElf() const { return false; }

private:
  int directRefCnt;
  char *path;
};

/**
 * Class handling libraries loaded by the system linker
 */
class SystemElf: public LibHandle
{
public:
  /**
   * Returns a new SystemElf for the given path. The given flags are passed
   * to dlopen().
   */
  static mozilla::TemporaryRef<LibHandle> Load(const char *path, int flags);

  /**
   * Inherited from LibHandle
   */
  virtual ~SystemElf();
  virtual void *GetSymbolPtr(const char *symbol) const;
  virtual bool Contains(void *addr) const { return false; /* UNIMPLEMENTED */ }

protected:
  /**
   * Returns whether the handle is a SystemElf or not. (short of a better way
   * to do this without RTTI)
   */
  friend class ElfLoader;
  virtual bool IsSystemElf() const { return true; }

  /**
   * Remove the reference to the system linker handle. This avoids dlclose()
   * being called when the instance is destroyed.
   */
  void Forget()
  {
    dlhandle = NULL;
  }

private:
  /**
   * Private constructor
   */
  SystemElf(const char *path, void *handle)
  : LibHandle(path), dlhandle(handle) { }

  /* Handle as returned by system dlopen() */
  void *dlhandle;
};

/**
 * Elf Loader class in charge of loading and bookkeeping libraries.
 */
class ElfLoader
{
public:
  /**
   * The Elf Loader instance
   */
  static ElfLoader Singleton;

  /**
   * Loads the given library with the given flags. Equivalent to dlopen()
   * The extra "parent" argument optionally gives the handle of the library
   * requesting the given library to be loaded. The loader may look in the
   * directory containing that parent library for the library to load.
   */
  mozilla::TemporaryRef<LibHandle> Load(const char *path, int flags,
                                        LibHandle *parent = NULL);

  /**
   * Returns the handle of the library containing the given address in
   * its virtual address space, i.e. the library handle for which
   * LibHandle::Contains returns true. Its purpose is to allow to
   * implement dladdr().
   */
  mozilla::TemporaryRef<LibHandle> GetHandleByPtr(void *addr);

protected:
  /**
   * Forget about the given handle. This method is meant to be called by
   * the LibHandle destructor.
   */
  friend LibHandle::~LibHandle();
  void Forget(LibHandle *handle);

  /* Last error. Used for dlerror() */
  friend class SystemElf;
  friend const char *__wrap_dlerror(void);
  friend void *__wrap_dlsym(void *handle, const char *symbol);
  friend int __wrap_dlclose(void *handle);
  const char *lastError;

private:
  ElfLoader() { }
  ~ElfLoader();

  /* Bookkeeping */
  typedef std::vector<LibHandle *> LibHandleList;
  LibHandleList handles;
};

#endif /* ElfLoader_h */
