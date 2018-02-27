/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/*
 * Structures matching the in-memory representation of typelib structures.
 * http://www.mozilla.org/scriptable/typelib_file.html
 */

#ifndef xpt_struct_h
#define xpt_struct_h

#include "nsID.h"
#include <stdint.h>

/*
 * Originally, I was going to have structures that exactly matched the on-disk
 * representation, but that proved difficult: different compilers can pack
 * their structs differently, and that makes overlaying them atop a
 * read-from-disk byte buffer troublesome.  So now I just have some structures
 * that are used in memory, and we're going to write a nice XDR library to
 * write them to disk and stuff.  It is pure joy. -- shaver
 */

/* Structures for the typelib components */

struct XPTHeader;
struct XPTInterfaceDirectoryEntry;
struct XPTInterfaceDescriptor;
struct XPTConstDescriptor;
struct XPTMethodDescriptor;
struct XPTParamDescriptor;
struct XPTTypeDescriptor;
struct XPTTypeDescriptorPrefix;

/*
 * Every XPCOM typelib file begins with a header.
 */
struct XPTHeader {
  // Some of these fields exists in the on-disk format but don't need to be
  // stored in memory (other than very briefly, which can be done with local
  // variables).

  //uint8_t magic[16];
  uint8_t major_version;
  //uint8_t minor_version;
  uint16_t num_interfaces;
  //uint32_t file_length;
  const XPTInterfaceDirectoryEntry* interface_directory;
  //uint32_t data_pool;
};

/*
 * Any file with a major version number of XPT_MAJOR_INCOMPATIBLE_VERSION
 * or higher is to be considered incompatible by this version of xpt and
 * we will refuse to read it. We will return a header with magic, major and
 * minor versions set from the file. num_interfaces will be set to zero to
 * confirm our inability to read the file; i.e. even if some client of this
 * library gets out of sync with us regarding the agreed upon value for
 * XPT_MAJOR_INCOMPATIBLE_VERSION, anytime num_interfaces is zero we *know*
 * that this library refused to read the file due to version incompatibility.
 */
#define XPT_MAJOR_INCOMPATIBLE_VERSION 0x02

/*
 * A contiguous array of fixed-size InterfaceDirectoryEntry records begins at
 * the byte offset identified by the interface_directory field in the file
 * header.  The array is used to quickly locate an interface description
 * using its IID.  No interface should appear more than once in the array.
 */
struct XPTInterfaceDirectoryEntry {
  nsID iid;
  const char* name;

  // This field exists in the on-disk format. But it isn't used so we don't
  // allocate space for it in memory.
  //const char* name_space;

  const XPTInterfaceDescriptor* interface_descriptor;
};

/*
 * An InterfaceDescriptor describes a single XPCOM interface, including all of
 * its methods.
 */
struct XPTInterfaceDescriptor {
  static const uint8_t kScriptableMask =                0x80;
  static const uint8_t kFunctionMask =                  0x40;
  static const uint8_t kBuiltinClassMask =              0x20;
  static const uint8_t kMainProcessScriptableOnlyMask = 0x10;

  bool IsScriptable() const { return !!(flags & kScriptableMask); }
  bool IsFunction() const { return !!(flags & kFunctionMask); }
  bool IsBuiltinClass() const { return !!(flags & kBuiltinClassMask); }
  bool IsMainProcessScriptableOnly() const { return !!(flags & kMainProcessScriptableOnlyMask); }

  /*
   * This field ordering minimizes the size of this struct.
   * The fields are serialized on disk in a different order.
   * See DoInterfaceDescriptor().
   */
  const XPTMethodDescriptor* method_descriptors;
  XPTConstDescriptor* const_descriptors;
  XPTTypeDescriptor* additional_types;
  uint16_t parent_interface;
  uint16_t num_methods;
  uint16_t num_constants;
  uint8_t flags;

  /*
   * additional_types are used for arrays where we may need multiple
   * XPTTypeDescriptors for a single XPTMethodDescriptor. Since we still
   * want to have a simple array of XPTMethodDescriptor (each with a single
   * embedded XPTTypeDescriptor), a XPTTypeDescriptor can have a reference
   * to an 'additional_type'. That reference is an index in this
   * "additional_types" array. So a given XPTMethodDescriptor might have
   * a whole chain of these XPTTypeDescriptors to represent, say, a multi
   * dimensional array.
   *
   * Note that in the typelib file these additional types are stored 'inline'
   * in the MethodDescriptor. But, in the typelib MethodDescriptors can be
   * of varying sizes, where in XPT's in memory mapping of the data we want
   * them to be of fixed size. This additional_types scheme is here to allow
   * for that.
   */
  uint8_t num_additional_types;
};

/*
 * A TypeDescriptor is a variable-size record used to identify the type of a
 * method argument or return value.
 *
 * There are three types of TypeDescriptors:
 *
 * SimpleTypeDescriptor
 * InterfaceTypeDescriptor
 * InterfaceIsTypeDescriptor
 *
 * The tag field in the prefix indicates which of the variant TypeDescriptor
 * records is being used, and hence the way any remaining fields should be
 * parsed. Values from 0 to 17 refer to SimpleTypeDescriptors. The value 18
 * designates an InterfaceTypeDescriptor, while 19 represents an
 * InterfaceIsTypeDescriptor.
 */

/* why bother with a struct?  - other code relies on this being a struct */
struct XPTTypeDescriptorPrefix {
  uint8_t TagPart() const {
    static const uint8_t kFlagMask = 0xe0;
    return (uint8_t) (flags & ~kFlagMask);
  }

  uint8_t flags;
};

/*
 * The following enum maps mnemonic names to the different numeric values
 * of XPTTypeDescriptor->tag.
 */
enum XPTTypeDescriptorTags {
  TD_INT8              = 0,
  TD_INT16             = 1,
  TD_INT32             = 2,
  TD_INT64             = 3,
  TD_UINT8             = 4,
  TD_UINT16            = 5,
  TD_UINT32            = 6,
  TD_UINT64            = 7,
  TD_FLOAT             = 8,
  TD_DOUBLE            = 9,
  TD_BOOL              = 10,
  TD_CHAR              = 11,
  TD_WCHAR             = 12,
  TD_VOID              = 13,
  TD_PNSIID            = 14,
  TD_DOMSTRING         = 15,
  TD_PSTRING           = 16,
  TD_PWSTRING          = 17,
  TD_INTERFACE_TYPE    = 18,
  TD_INTERFACE_IS_TYPE = 19,
  TD_ARRAY             = 20,
  TD_PSTRING_SIZE_IS   = 21,
  TD_PWSTRING_SIZE_IS  = 22,
  TD_UTF8STRING        = 23,
  TD_CSTRING           = 24,
  TD_ASTRING           = 25,
  TD_JSVAL             = 26
};

struct XPTTypeDescriptor {
  uint8_t Tag() const {
    return prefix.TagPart();
  }

  XPTTypeDescriptorPrefix prefix;

  // The memory layout here doesn't exactly match (for the appropriate types)
  // the on-disk format. This is to save memory.
  union {
    // Used for TD_INTERFACE_IS_TYPE.
    struct {
      uint8_t argnum;
    } interface_is;

    // Used for TD_PSTRING_SIZE_IS, TD_PWSTRING_SIZE_IS.
    struct {
      uint8_t argnum;
      //uint8_t argnum2;          // Present on disk, omitted here.
    } pstring_is;

    // Used for TD_ARRAY.
    struct {
      uint8_t argnum;
      //uint8_t argnum2;          // Present on disk, omitted here.
      uint8_t additional_type;    // uint16_t on disk, uint8_t here;
                                  // in practice it never exceeds 20.
    } array;

    // Used for TD_INTERFACE_TYPE.
    struct {
      // We store the 16-bit iface value as two 8-bit values in order to
      // avoid 16-bit alignment requirements for XPTTypeDescriptor, which
      // reduces its size and also the size of XPTParamDescriptor.
      uint8_t iface_hi8;
      uint8_t iface_lo8;
    } iface;
  } u;
};

/*
 * A ConstDescriptor is a variable-size record that records the name and
 * value of a scoped interface constant. This is allowed only for a subset
 * of types.
 *
 * The type (and thus the size) of the value record is determined by the
 * contents of the associated TypeDescriptor record. For instance, if type
 * corresponds to int16_t, then value is a two-byte record consisting of a
 * 16-bit signed integer.
 */
union XPTConstValue {
  int16_t i16;
  uint16_t ui16;
  int32_t i32;
  uint32_t ui32;
}; /* varies according to type */

struct XPTConstDescriptor {
  const char* name;
  XPTTypeDescriptor type;
  union XPTConstValue value;
};

/*
 * A ParamDescriptor is a variable-size record used to describe either a
 * single argument to a method or a method's result.
 */
struct XPTParamDescriptor {
  uint8_t flags;
  XPTTypeDescriptor type;
};

/*
 * A MethodDescriptor is a variable-size record used to describe a single
 * interface method.
 */
struct XPTMethodDescriptor {
  const char* name;
  XPTParamDescriptor* params;
  //XPTParamDescriptor result; // Present on disk, omitted here.
  uint8_t flags;
  uint8_t num_args;
};

#endif /* xpt_struct_h */
