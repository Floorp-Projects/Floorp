// Copyright (c) 2010 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// Implement the DwarfCUToModule class; see dwarf_cu_to_module.h.

// For <inttypes.h> PRI* macros, before anything else might #include it.
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif  /* __STDC_FORMAT_MACROS */

#include "common/dwarf_cu_to_module.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <algorithm>
#include <numeric>
#include <utility>

#include "common/dwarf_line_to_module.h"
#include "common/unordered.h"

namespace google_breakpad {

using std::accumulate;
using std::map;
using std::pair;
using std::sort;
using std::vector;

// Data provided by a DWARF specification DIE.
//
// In DWARF, the DIE for a definition may contain a DW_AT_specification
// attribute giving the offset of the corresponding declaration DIE, and
// the definition DIE may omit information given in the declaration. For
// example, it's common for a function's address range to appear only in
// its definition DIE, but its name to appear only in its declaration
// DIE.
//
// The dumper needs to be able to follow DW_AT_specification links to
// bring all this information together in a FUNC record. Conveniently,
// DIEs that are the target of such links have a DW_AT_declaration flag
// set, so we can identify them when we first see them, and record their
// contents for later reference.
//
// A Specification holds information gathered from a declaration DIE that
// we may need if we find a DW_AT_specification link pointing to it.
struct DwarfCUToModule::Specification {
  // The qualified name that can be found by demangling DW_AT_MIPS_linkage_name.
  string qualified_name;

  // The name of the enclosing scope, or the empty string if there is none.
  string enclosing_name;

  // The name for the specification DIE itself, without any enclosing
  // name components.
  string unqualified_name;
};

// An abstract origin -- base definition of an inline function.
struct AbstractOrigin {
  AbstractOrigin() : name() {}
  explicit AbstractOrigin(const string& name) : name(name) {}

  string name;
};

typedef map<uint64, AbstractOrigin> AbstractOriginByOffset;

// Data global to the DWARF-bearing file that is private to the
// DWARF-to-Module process.
struct DwarfCUToModule::FilePrivate {
  // A set of strings used in this CU. Before storing a string in one of
  // our data structures, insert it into this set, and then use the string
  // from the set.
  //
  // In some STL implementations, strings are reference-counted internally,
  // meaning that simply using strings from this set, even if passed by
  // value, assigned, or held directly in structures and containers
  // (map<string, ...>, for example), causes those strings to share a
  // single instance of each distinct piece of text. GNU's libstdc++ uses
  // reference counts, and I believe MSVC did as well, at some point.
  // However, C++ '11 implementations are moving away from reference
  // counting.
  //
  // In other implementations, string assignments copy the string's text,
  // so this set will actually hold yet another copy of the string (although
  // everything will still work). To improve memory consumption portably,
  // we will probably need to use pointers to strings held in this set.
  unordered_set<string> common_strings;

  // A map from offsets of DIEs within the .debug_info section to
  // Specifications describing those DIEs. Specification references can
  // cross compilation unit boundaries.
  SpecificationByOffset specifications;

  AbstractOriginByOffset origins;

  struct InlinedSubroutineRange {
    InlinedSubroutineRange(Module::Range range, uint64 call_file,
                           uint64 call_line)
      : range_(range), call_file_(call_file), call_line_(call_line) {}

    Module::Range range_;
    uint64 call_file_, call_line_;
  };

  // A collection of address ranges with the file and line that they
  // correspond to. We'll use this information to replace the precise line
  // information gathered from .debug_line.
  vector<InlinedSubroutineRange> inlined_ranges;
};

DwarfCUToModule::FileContext::FileContext(const string &filename,
                                          Module *module,
                                          bool handle_inter_cu_refs)
    : filename_(filename),
      module_(module),
      handle_inter_cu_refs_(handle_inter_cu_refs),
      file_private_(new FilePrivate()) {
}

DwarfCUToModule::FileContext::~FileContext() {
}

void DwarfCUToModule::FileContext::AddSectionToSectionMap(
    const string& name, const uint8_t *contents, uint64 length) {
  section_map_[name] = std::make_pair(contents, length);
}

void DwarfCUToModule::FileContext::ClearSectionMapForTest() {
  section_map_.clear();
}

const dwarf2reader::SectionMap&
DwarfCUToModule::FileContext::section_map() const {
  return section_map_;
}

void DwarfCUToModule::FileContext::ClearSpecifications() {
  if (!handle_inter_cu_refs_)
    file_private_->specifications.clear();
}

bool DwarfCUToModule::FileContext::IsUnhandledInterCUReference(
    uint64 offset, uint64 compilation_unit_start) const {
  if (handle_inter_cu_refs_)
    return false;
  return offset < compilation_unit_start;
}

// Information global to the particular compilation unit we're
// parsing. This is for data shared across the CU's entire DIE tree,
// and parameters from the code invoking the CU parser.
struct DwarfCUToModule::CUContext {
  CUContext(FileContext *file_context_arg, WarningReporter *reporter_arg,
            RangesHandler *ranges_handler_arg)
      : file_context(file_context_arg),
        reporter(reporter_arg),
        ranges_handler(ranges_handler_arg),
        language(Language::CPlusPlus),
        low_pc(0),
        high_pc(0),
        ranges(0) {}

  ~CUContext() {
    for (vector<Module::Function *>::iterator it = functions.begin();
         it != functions.end(); ++it) {
      delete *it;
    }
  };

  // The DWARF-bearing file into which this CU was incorporated.
  FileContext *file_context;

  // For printing error messages.
  WarningReporter *reporter;

  // For reading ranges from the .debug_ranges section
  RangesHandler *ranges_handler;

  // The source language of this compilation unit.
  const Language *language;

  // Addresses covered by this CU. If high_pc_ is non-zero then the CU covers
  // low_pc to high_pc, otherwise ranges is non-zero and low_pc represents
  // the base address of the ranges covered by the CU.
  uint64 low_pc;
  uint64 high_pc;
  uint64 ranges;

  // The functions defined in this compilation unit. We accumulate
  // them here during parsing. Then, in DwarfCUToModule::Finish, we
  // assign them lines and add them to file_context->module.
  //
  // Destroying this destroys all the functions this vector points to.
  vector<Module::Function *> functions;

  // Keep a list of forward references from DW_AT_abstract_origin and
  // DW_AT_specification attributes so names can be fixed up.
  std::map<uint64_t, Module::Function *> forward_ref_die_to_func;
};

// Information about the context of a particular DIE. This is for
// information that changes as we descend the tree towards the leaves:
// the containing classes/namespaces, etc.
struct DwarfCUToModule::DIEContext {
  // The fully-qualified name of the context. For example, for a
  // tree like:
  //
  // DW_TAG_namespace Foo
  //   DW_TAG_class Bar
  //     DW_TAG_subprogram Baz
  //
  // in a C++ compilation unit, the DIEContext's name for the
  // DW_TAG_subprogram DIE would be "Foo::Bar". The DIEContext's
  // name for the DW_TAG_namespace DIE would be "".
  string name;
};

// An abstract base class for all the dumper's DIE handlers.
class DwarfCUToModule::GenericDIEHandler: public dwarf2reader::DIEHandler {
 public:
  // Create a handler for the DIE at OFFSET whose compilation unit is
  // described by CU_CONTEXT, and whose immediate context is described
  // by PARENT_CONTEXT.
  GenericDIEHandler(CUContext *cu_context, DIEContext *parent_context,
                    uint64 offset)
      : cu_context_(cu_context),
        parent_context_(parent_context),
        offset_(offset),
        declaration_(false),
        specification_(NULL),
        forward_ref_die_offset_(0) { }

  // Derived classes' ProcessAttributeUnsigned can defer to this to
  // handle DW_AT_declaration, or simply not override it.
  void ProcessAttributeUnsigned(enum DwarfAttribute attr,
                                enum DwarfForm form,
                                uint64 data);

  // Derived classes' ProcessAttributeReference can defer to this to
  // handle DW_AT_specification, or simply not override it.
  void ProcessAttributeReference(enum DwarfAttribute attr,
                                 enum DwarfForm form,
                                 uint64 data);

  // Derived classes' ProcessAttributeReference can defer to this to
  // handle DW_AT_specification, or simply not override it.
  void ProcessAttributeString(enum DwarfAttribute attr,
                              enum DwarfForm form,
                              const string &data);

 protected:
  // Compute and return the fully-qualified name of the DIE. If this
  // DIE is a declaration DIE, to be cited by other DIEs'
  // DW_AT_specification attributes, record its enclosing name and
  // unqualified name in the specification table.
  //
  // Use this from EndAttributes member functions, not ProcessAttribute*
  // functions; only the former can be sure that all the DIE's attributes
  // have been seen.
  string ComputeQualifiedName();

  CUContext *cu_context_;
  DIEContext *parent_context_;
  uint64 offset_;

  // Place the name in the global set of strings. Even though this looks
  // like a copy, all the major string implementations use reference
  // counting internally, so the effect is to have all the data structures
  // share copies of strings whenever possible.
  // FIXME: Should this return something like a string_ref to avoid the
  // assumption about how strings are implemented?
  string AddStringToPool(const string &str);

  // If this DIE has a DW_AT_declaration attribute, this is its value.
  // It is false on DIEs with no DW_AT_declaration attribute.
  bool declaration_;

  // If this DIE has a DW_AT_specification attribute, this is the
  // Specification structure for the DIE the attribute refers to.
  // Otherwise, this is NULL.
  Specification *specification_;

  // If this DIE has a DW_AT_specification or DW_AT_abstract_origin and it is a
  // forward reference, no Specification will be available. Track the reference
  // to be fixed up when the DIE is parsed.
  uint64_t forward_ref_die_offset_;

  // The value of the DW_AT_name attribute, or the empty string if the
  // DIE has no such attribute.
  string name_attribute_;

  // The demangled value of the DW_AT_MIPS_linkage_name attribute, or the empty
  // string if the DIE has no such attribute or its content could not be
  // demangled.
  string demangled_name_;

  // The non-demangled value of the DW_AT_MIPS_linkage_name attribute,
  // it its content count not be demangled.
  string raw_name_;
};

void DwarfCUToModule::GenericDIEHandler::ProcessAttributeUnsigned(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    uint64 data) {
  switch (attr) {
    case dwarf2reader::DW_AT_declaration: declaration_ = (data != 0); break;
    default: break;
  }
}

void DwarfCUToModule::GenericDIEHandler::ProcessAttributeReference(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    uint64 data) {
  switch (attr) {
    case dwarf2reader::DW_AT_specification: {
      FileContext *file_context = cu_context_->file_context;
      if (file_context->IsUnhandledInterCUReference(
              data, cu_context_->reporter->cu_offset())) {
        cu_context_->reporter->UnhandledInterCUReference(offset_, data);
        break;
      }
      // Find the Specification to which this attribute refers, and
      // set specification_ appropriately. We could do more processing
      // here, but it's better to leave the real work to our
      // EndAttribute member function, at which point we know we have
      // seen all the DIE's attributes.
      SpecificationByOffset *specifications =
          &file_context->file_private_->specifications;
      SpecificationByOffset::iterator spec = specifications->find(data);
      if (spec != specifications->end()) {
        specification_ = &spec->second;
      } else if (data > offset_) {
        forward_ref_die_offset_ = data;
      } else {
        cu_context_->reporter->UnknownSpecification(offset_, data);
      }
      break;
    }
    default: break;
  }
}

string DwarfCUToModule::GenericDIEHandler::AddStringToPool(const string &str) {
  pair<unordered_set<string>::iterator, bool> result =
    cu_context_->file_context->file_private_->common_strings.insert(str);
  return *result.first;
}

void DwarfCUToModule::GenericDIEHandler::ProcessAttributeString(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    const string &data) {
  switch (attr) {
    case dwarf2reader::DW_AT_name:
      name_attribute_ = AddStringToPool(data);
      break;
    case dwarf2reader::DW_AT_MIPS_linkage_name:
    case dwarf2reader::DW_AT_linkage_name: {
      string demangled;
      Language::DemangleResult result =
          cu_context_->language->DemangleName(data, &demangled);
      switch (result) {
        case Language::kDemangleSuccess:
          demangled_name_ = AddStringToPool(demangled);
          break;

        case Language::kDemangleFailure:
          cu_context_->reporter->DemangleError(data);
          // fallthrough
        case Language::kDontDemangle:
          demangled_name_.clear();
          raw_name_ = AddStringToPool(data);
          break;
      }
      break;
    }
    default: break;
  }
}

string DwarfCUToModule::GenericDIEHandler::ComputeQualifiedName() {
  // Use the demangled name, if one is available. Demangled names are
  // preferable to those inferred from the DWARF structure because they
  // include argument types.
  const string *qualified_name = NULL;
  if (!demangled_name_.empty()) {
    // Found it is this DIE.
    qualified_name = &demangled_name_;
  } else if (specification_ && !specification_->qualified_name.empty()) {
    // Found it on the specification.
    qualified_name = &specification_->qualified_name;
  }

  const string *unqualified_name = NULL;
  const string *enclosing_name;
  if (!qualified_name) {
    // Find the unqualified name. If the DIE has its own DW_AT_name
    // attribute, then use that; otherwise, check the specification.
    if (!name_attribute_.empty())
      unqualified_name = &name_attribute_;
    else if (specification_)
      unqualified_name = &specification_->unqualified_name;
    else if (!raw_name_.empty())
      unqualified_name = &raw_name_;

    // Find the name of the enclosing context. If this DIE has a
    // specification, it's the specification's enclosing context that
    // counts; otherwise, use this DIE's context.
    if (specification_)
      enclosing_name = &specification_->enclosing_name;
    else
      enclosing_name = &parent_context_->name;
  }

  // Prepare the return value before upcoming mutations possibly invalidate the
  // existing pointers.
  string return_value;
  if (qualified_name) {
    return_value = *qualified_name;
  } else if (unqualified_name && enclosing_name) {
    // Combine the enclosing name and unqualified name to produce our
    // own fully-qualified name.
    return_value = cu_context_->language->MakeQualifiedName(*enclosing_name,
                                                            *unqualified_name);
  }

  // If this DIE was marked as a declaration, record its names in the
  // specification table.
  if ((declaration_ && qualified_name) ||
      (unqualified_name && enclosing_name)) {
    Specification spec;
    if (qualified_name) {
      spec.qualified_name = *qualified_name;
    } else {
      spec.enclosing_name = *enclosing_name;
      spec.unqualified_name = *unqualified_name;
    }
    cu_context_->file_context->file_private_->specifications[offset_] = spec;
  }

  return return_value;
}

// A handler class for DW_TAG_inlined_subroutine DIEs.
class DwarfCUToModule::InlinedSubroutineHandler: public GenericDIEHandler {
 public:
  InlinedSubroutineHandler(CUContext *cu_context, DIEContext *parent_context,
                           uint64 offset)
    : GenericDIEHandler(cu_context, parent_context, offset),
      low_pc_(0), high_pc_(0), high_pc_form_(dwarf2reader::DW_FORM_addr),
      ranges_(0), call_file_(0), call_file_set_(false), call_line_(0),
      call_line_set_(false) {}

  void ProcessAttributeUnsigned(enum DwarfAttribute attr,
                                enum DwarfForm form,
                                uint64 data);

  bool EndAttributes();

 private:
  uint64 low_pc_, high_pc_; // DW_AT_low_pc, DW_AT_high_pc
  DwarfForm high_pc_form_; // DW_AT_high_pc can be length or address.
  uint64 ranges_; // DW_AT_ranges
  uint64 call_file_; // DW_AT_call_file
  bool call_file_set_;
  uint64 call_line_; // DW_AT_call_line
  bool call_line_set_;
};

void DwarfCUToModule::InlinedSubroutineHandler::ProcessAttributeUnsigned(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    uint64 data) {
  switch (attr) {
    case dwarf2reader::DW_AT_low_pc:      low_pc_  = data; break;
    case dwarf2reader::DW_AT_high_pc:
      high_pc_form_ = form;
      high_pc_ = data;
      break;
    case dwarf2reader::DW_AT_ranges:
      ranges_ = data;
      break;
    case dwarf2reader::DW_AT_call_file:
      call_file_ = data;
      call_file_set_ = true;
      break;
    case dwarf2reader::DW_AT_call_line:
      call_line_ = data;
      call_line_set_ = true;
      break;

    default:
      GenericDIEHandler::ProcessAttributeUnsigned(attr, form, data);
      break;
  }
}

bool DwarfCUToModule::InlinedSubroutineHandler::EndAttributes() {
  // DW_TAG_inlined_subroutine child DIEs are only information about formal
  // parameters and any subroutines that were further inlined, which we're
  // not particularly concerned about.
  const bool ignore_children = false;

  // If we didn't find complete information about what file and line we were
  // inlined from, then there's no point in computing anything.
  if (!call_file_set_ || !call_line_set_) {
    return ignore_children;
  }

  vector<Module::Range> ranges;

  if (!ranges_) {
    // Make high_pc_ an address, if it isn't already.
    if (high_pc_form_ != dwarf2reader::DW_FORM_addr &&
        high_pc_form_ != dwarf2reader::DW_FORM_GNU_addr_index) {
      high_pc_ += low_pc_;
    }

    Module::Range range(low_pc_, high_pc_ - low_pc_);
    ranges.push_back(range);
  } else {
    RangesHandler *ranges_handler = cu_context_->ranges_handler;

    if (ranges_handler) {
      if (!ranges_handler->ReadRanges(ranges_, cu_context_->low_pc, &ranges)) {
        ranges.clear();
        cu_context_->reporter->MalformedRangeList(ranges_);
      }
    } else {
      cu_context_->reporter->MissingRanges();
    }
  }

  for (const auto& range : ranges) {
    if (range.size > 0) {
      FilePrivate::InlinedSubroutineRange inline_range(range, call_file_, call_line_);
      cu_context_->file_context->file_private_->inlined_ranges.push_back(inline_range);
    }
  }

  return ignore_children;
}

// A handler class for DW_TAG_lexical_block DIEs.
class DwarfCUToModule::LexicalBlockHandler: public GenericDIEHandler {
 public:
  LexicalBlockHandler(CUContext *cu_context, DIEContext *parent_context,
                      uint64 offset)
      : GenericDIEHandler(cu_context, parent_context, offset) {}

  bool EndAttributes();

  DIEHandler* FindChildHandler(uint64 offset, enum DwarfTag tag);
};


bool DwarfCUToModule::LexicalBlockHandler::EndAttributes() {
  // Parse child DIEs if possible.
  return true;
}

dwarf2reader::DIEHandler* DwarfCUToModule::LexicalBlockHandler::FindChildHandler(
    uint64 offset,
    enum DwarfTag tag) {
  switch (tag) {
    case dwarf2reader::DW_TAG_inlined_subroutine:
      return new InlinedSubroutineHandler(cu_context_, parent_context_, offset);

    default:
      return NULL;
  }
}

// A handler class for DW_TAG_subprogram DIEs.
class DwarfCUToModule::FuncHandler: public GenericDIEHandler {
 public:
  FuncHandler(CUContext *cu_context, DIEContext *parent_context,
              uint64 offset)
      : GenericDIEHandler(cu_context, parent_context, offset),
        low_pc_(0), high_pc_(0), high_pc_form_(dwarf2reader::DW_FORM_addr),
        ranges_(0), abstract_origin_(NULL), inline_(false) { }
  void ProcessAttributeUnsigned(enum DwarfAttribute attr,
                                enum DwarfForm form,
                                uint64 data);
  void ProcessAttributeSigned(enum DwarfAttribute attr,
                              enum DwarfForm form,
                              int64 data);
  void ProcessAttributeReference(enum DwarfAttribute attr,
                                 enum DwarfForm form,
                                 uint64 data);

  bool EndAttributes();
  void Finish();

  DIEHandler *FindChildHandler(uint64 offset, enum DwarfTag tag);

 private:
  // The fully-qualified name, as derived from name_attribute_,
  // specification_, parent_context_.  Computed in EndAttributes.
  string name_;
  uint64 low_pc_, high_pc_; // DW_AT_low_pc, DW_AT_high_pc
  DwarfForm high_pc_form_; // DW_AT_high_pc can be length or address.
  uint64 ranges_; // DW_AT_ranges
  const AbstractOrigin* abstract_origin_;
  bool inline_;
};

void DwarfCUToModule::FuncHandler::ProcessAttributeUnsigned(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    uint64 data) {
  switch (attr) {
    // If this attribute is present at all --- even if its value is
    // DW_INL_not_inlined --- then GCC may cite it as someone else's
    // DW_AT_abstract_origin attribute.
    case dwarf2reader::DW_AT_inline:      inline_  = true; break;

    case dwarf2reader::DW_AT_low_pc:      low_pc_  = data; break;
    case dwarf2reader::DW_AT_high_pc:
      high_pc_form_ = form;
      high_pc_ = data;
      break;
    case dwarf2reader::DW_AT_ranges:
      ranges_ = data;
      break;

    default:
      GenericDIEHandler::ProcessAttributeUnsigned(attr, form, data);
      break;
  }
}

void DwarfCUToModule::FuncHandler::ProcessAttributeSigned(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    int64 data) {
  switch (attr) {
    // If this attribute is present at all --- even if its value is
    // DW_INL_not_inlined --- then GCC may cite it as someone else's
    // DW_AT_abstract_origin attribute.
    case dwarf2reader::DW_AT_inline:      inline_  = true; break;

    default:
      break;
  }
}

void DwarfCUToModule::FuncHandler::ProcessAttributeReference(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    uint64 data) {
  switch (attr) {
    case dwarf2reader::DW_AT_abstract_origin: {
      const AbstractOriginByOffset& origins =
          cu_context_->file_context->file_private_->origins;
      AbstractOriginByOffset::const_iterator origin = origins.find(data);
      if (origin != origins.end()) {
        abstract_origin_ = &(origin->second);
      } else if (data > offset_) {
        forward_ref_die_offset_ = data;
      } else {
        cu_context_->reporter->UnknownAbstractOrigin(offset_, data);
      }
      break;
    }
    default:
      GenericDIEHandler::ProcessAttributeReference(attr, form, data);
      break;
  }
}

bool DwarfCUToModule::FuncHandler::EndAttributes() {
  // Compute our name, and record a specification, if appropriate.
  name_ = ComputeQualifiedName();
  if (name_.empty() && abstract_origin_) {
    name_ = abstract_origin_->name;
  }
  return true;
}

static bool IsEmptyRange(const vector<Module::Range>& ranges) {
  uint64 size = accumulate(ranges.cbegin(), ranges.cend(), 0,
    [](uint64 total, Module::Range entry) {
      return total + entry.size;
    }
  );

  return size == 0;
}

void DwarfCUToModule::FuncHandler::Finish() {
  vector<Module::Range> ranges;

  // Check if this DIE was one of the forward references that was not able
  // to be processed, and fix up the name of the appropriate Module::Function.
  // "name_" will have already been fixed up in EndAttributes().
  if (!name_.empty()) {
    auto iter = cu_context_->forward_ref_die_to_func.find(offset_);
    if (iter != cu_context_->forward_ref_die_to_func.end())
      iter->second->name = name_;
  }

  if (!ranges_) {
    // Make high_pc_ an address, if it isn't already.
    if (high_pc_form_ != dwarf2reader::DW_FORM_addr &&
        high_pc_form_ != dwarf2reader::DW_FORM_GNU_addr_index) {
      high_pc_ += low_pc_;
    }

    Module::Range range(low_pc_, high_pc_ - low_pc_);
    ranges.push_back(range);
  } else {
    RangesHandler *ranges_handler = cu_context_->ranges_handler;

    if (ranges_handler) {
      if (!ranges_handler->ReadRanges(ranges_, cu_context_->low_pc, &ranges)) {
        ranges.clear();
        cu_context_->reporter->MalformedRangeList(ranges_);
      }
    } else {
      cu_context_->reporter->MissingRanges();
    }
  }

  // Did we collect the information we need?  Not all DWARF function
  // entries are non-empty (for example, inlined functions that were never
  // used), but all the ones we're interested in cover a non-empty range of
  // bytes.
  if (!IsEmptyRange(ranges)) {
    low_pc_ = ranges.front().address;

    // Malformed DWARF may omit the name, but all Module::Functions must
    // have names.
    string name;
    if (!name_.empty()) {
      name = name_;
    } else {
      // If we have a forward reference to a DW_AT_specification or
      // DW_AT_abstract_origin, then don't warn, the name will be fixed up
      // later
      if (forward_ref_die_offset_ == 0)
        cu_context_->reporter->UnnamedFunction(offset_);
      name = "<name omitted>";
    }

    // Create a Module::Function based on the data we've gathered, and
    // add it to the functions_ list.
    scoped_ptr<Module::Function> func(new Module::Function(name, low_pc_));
    func->ranges = ranges;
    func->parameter_size = 0;
    if (func->address) {
      // If the function address is zero this is a sign that this function
      // description is just empty debug data and should just be discarded.
      cu_context_->functions.push_back(func.release());
      if (forward_ref_die_offset_ != 0) {
        auto iter =
            cu_context_->forward_ref_die_to_func.find(forward_ref_die_offset_);
        if (iter == cu_context_->forward_ref_die_to_func.end()) {
          cu_context_->reporter->UnknownSpecification(offset_,
                                                      forward_ref_die_offset_);
        } else {
          iter->second = cu_context_->functions.back();
        }
      }
    }
  } else if (inline_) {
    AbstractOrigin origin(name_);
    cu_context_->file_context->file_private_->origins[offset_] = origin;
  }
}

dwarf2reader::DIEHandler *DwarfCUToModule::FuncHandler::FindChildHandler(
    uint64 offset,
    enum DwarfTag tag) {
  switch (tag) {
    case dwarf2reader::DW_TAG_inlined_subroutine:
      return new InlinedSubroutineHandler(cu_context_, parent_context_, offset);

      // Compilers will sometimes give DW_TAG_subprogram DIEs
      // DW_TAG_lexical_block children DIEs, which then in turn contain
      // DW_TAG_inlined_subroutine DIEs.  We want to parse those
      // grandchildren as though they belonged to the original
      // DW_TAG_subprogram DIE.
    case dwarf2reader::DW_TAG_lexical_block:
      return new LexicalBlockHandler(cu_context_, parent_context_, offset);

    default:
      return NULL;
  }
}

// A handler for DIEs that contain functions and contribute a
// component to their names: namespaces, classes, etc.
class DwarfCUToModule::NamedScopeHandler: public GenericDIEHandler {
 public:
  NamedScopeHandler(CUContext *cu_context, DIEContext *parent_context,
                    uint64 offset)
      : GenericDIEHandler(cu_context, parent_context, offset) { }
  bool EndAttributes();
  DIEHandler *FindChildHandler(uint64 offset, enum DwarfTag tag);

 private:
  DIEContext child_context_; // A context for our children.
};

bool DwarfCUToModule::NamedScopeHandler::EndAttributes() {
  child_context_.name = ComputeQualifiedName();
  return true;
}

dwarf2reader::DIEHandler *DwarfCUToModule::NamedScopeHandler::FindChildHandler(
    uint64 offset,
    enum DwarfTag tag) {
  switch (tag) {
    case dwarf2reader::DW_TAG_subprogram:
      return new FuncHandler(cu_context_, &child_context_, offset);
    case dwarf2reader::DW_TAG_namespace:
    case dwarf2reader::DW_TAG_class_type:
    case dwarf2reader::DW_TAG_structure_type:
    case dwarf2reader::DW_TAG_union_type:
      return new NamedScopeHandler(cu_context_, &child_context_, offset);
    default:
      return NULL;
  }
}

void DwarfCUToModule::WarningReporter::CUHeading() {
  if (printed_cu_header_)
    return;
  fprintf(stderr, "%s: in compilation unit '%s' (offset 0x%llx):\n",
          filename_.c_str(), cu_name_.c_str(), cu_offset_);
  printed_cu_header_ = true;
}

void DwarfCUToModule::WarningReporter::UnknownSpecification(uint64 offset,
                                                            uint64 target) {
  CUHeading();
  fprintf(stderr, "%s: the DIE at offset 0x%llx has a DW_AT_specification"
          " attribute referring to the DIE at offset 0x%llx, which was not"
          " marked as a declaration\n",
          filename_.c_str(), offset, target);
}

void DwarfCUToModule::WarningReporter::UnknownAbstractOrigin(uint64 offset,
                                                             uint64 target) {
  CUHeading();
  fprintf(stderr, "%s: the DIE at offset 0x%llx has a DW_AT_abstract_origin"
          " attribute referring to the DIE at offset 0x%llx, which was not"
          " marked as an inline\n",
          filename_.c_str(), offset, target);
}

void DwarfCUToModule::WarningReporter::MissingSection(const string &name) {
  CUHeading();
  fprintf(stderr, "%s: warning: couldn't find DWARF '%s' section\n",
          filename_.c_str(), name.c_str());
}

void DwarfCUToModule::WarningReporter::BadLineInfoOffset(uint64 offset) {
  CUHeading();
  fprintf(stderr, "%s: warning: line number data offset beyond end"
          " of '.debug_line' section\n",
          filename_.c_str());
}

void DwarfCUToModule::WarningReporter::UncoveredHeading() {
  if (printed_unpaired_header_)
    return;
  CUHeading();
  fprintf(stderr, "%s: warning: skipping unpaired lines/functions:\n",
          filename_.c_str());
  printed_unpaired_header_ = true;
}

void DwarfCUToModule::WarningReporter::UncoveredFunction(
    const Module::Function &function) {
  if (!uncovered_warnings_enabled_)
    return;
  UncoveredHeading();
  fprintf(stderr, "    function%s: %s\n",
          IsEmptyRange(function.ranges) ? " (zero-length)" : "",
          function.name.c_str());
}

void DwarfCUToModule::WarningReporter::UncoveredLine(const Module::Line &line) {
  if (!uncovered_warnings_enabled_)
    return;
  UncoveredHeading();
  fprintf(stderr, "    line%s: %s:%d at 0x%" PRIx64 "\n",
          (line.size == 0 ? " (zero-length)" : ""),
          line.file->name.c_str(), line.number, line.address);
}

void DwarfCUToModule::WarningReporter::UnnamedFunction(uint64 offset) {
  CUHeading();
  fprintf(stderr, "%s: warning: function at offset 0x%llx has no name\n",
          filename_.c_str(), offset);
}

void DwarfCUToModule::WarningReporter::DemangleError(const string &input) {
  CUHeading();
  fprintf(stderr, "%s: warning: failed to demangle %s\n",
          filename_.c_str(), input.c_str());
}

void DwarfCUToModule::WarningReporter::UnhandledInterCUReference(
    uint64 offset, uint64 target) {
  CUHeading();
  fprintf(stderr, "%s: warning: the DIE at offset 0x%llx has a "
                  "DW_FORM_ref_addr attribute with an inter-CU reference to "
                  "0x%llx, but inter-CU reference handling is turned off.\n",
                  filename_.c_str(), offset, target);
}

void DwarfCUToModule::WarningReporter::MalformedRangeList(uint64 offset) {
  CUHeading();
  fprintf(stderr, "%s: warning: the range list at offset 0x%llx falls out of "
                  "the .debug_ranges section.\n",
                  filename_.c_str(), offset);
}

void DwarfCUToModule::WarningReporter::MissingRanges() {
  CUHeading();
  fprintf(stderr, "%s: warning: A DW_AT_ranges attribute was encountered but "
                  "the .debug_ranges section is missing.\n", filename_.c_str());
}

DwarfCUToModule::DwarfCUToModule(FileContext *file_context,
                                 LineToModuleHandler *line_reader,
                                 RangesHandler *ranges_handler,
                                 WarningReporter *reporter)
    : line_reader_(line_reader),
      cu_context_(new CUContext(file_context, reporter, ranges_handler)),
      child_context_(new DIEContext()),
      has_source_line_info_(false) {
}

DwarfCUToModule::~DwarfCUToModule() {
}

void DwarfCUToModule::ProcessAttributeSigned(enum DwarfAttribute attr,
                                             enum DwarfForm form,
                                             int64 data) {
  switch (attr) {
    case dwarf2reader::DW_AT_language: // source language of this CU
      SetLanguage(static_cast<DwarfLanguage>(data));
      break;
    default:
      break;
  }
}

void DwarfCUToModule::ProcessAttributeUnsigned(enum DwarfAttribute attr,
                                               enum DwarfForm form,
                                               uint64 data) {
  switch (attr) {
    case dwarf2reader::DW_AT_stmt_list: // Line number information.
      has_source_line_info_ = true;
      source_line_offset_ = data;
      break;
    case dwarf2reader::DW_AT_language: // source language of this CU
      SetLanguage(static_cast<DwarfLanguage>(data));
      break;
    case dwarf2reader::DW_AT_low_pc:
      cu_context_->low_pc  = data;
      break;
    case dwarf2reader::DW_AT_high_pc:
      cu_context_->high_pc  = data;
      break;
    case dwarf2reader::DW_AT_ranges:
      cu_context_->ranges = data;
      break;

    default:
      break;
  }
}

void DwarfCUToModule::ProcessAttributeString(enum DwarfAttribute attr,
                                             enum DwarfForm form,
                                             const string &data) {
  switch (attr) {
    case dwarf2reader::DW_AT_name:
      cu_context_->reporter->SetCUName(data);
      break;
    case dwarf2reader::DW_AT_comp_dir:
      line_reader_->StartCompilationUnit(data);
      break;
    default:
      break;
  }
}

bool DwarfCUToModule::EndAttributes() {
  return true;
}

dwarf2reader::DIEHandler *DwarfCUToModule::FindChildHandler(
    uint64 offset,
    enum DwarfTag tag) {
  switch (tag) {
    case dwarf2reader::DW_TAG_subprogram:
      return new FuncHandler(cu_context_.get(), child_context_.get(), offset);
    case dwarf2reader::DW_TAG_namespace:
    case dwarf2reader::DW_TAG_class_type:
    case dwarf2reader::DW_TAG_structure_type:
    case dwarf2reader::DW_TAG_union_type:
    case dwarf2reader::DW_TAG_module:
      return new NamedScopeHandler(cu_context_.get(), child_context_.get(),
                                   offset);
    default:
      return NULL;
  }
}

void DwarfCUToModule::SetLanguage(DwarfLanguage language) {
  switch (language) {
    case dwarf2reader::DW_LANG_Java:
      cu_context_->language = Language::Java;
      break;

    case dwarf2reader::DW_LANG_Swift:
      cu_context_->language = Language::Swift;
      break;

    case dwarf2reader::DW_LANG_Rust:
      cu_context_->language = Language::Rust;
      break;

    // DWARF has no generic language code for assembly language; this is
    // what the GNU toolchain uses.
    case dwarf2reader::DW_LANG_Mips_Assembler:
      cu_context_->language = Language::Assembler;
      break;

    // C++ covers so many cases that it probably has some way to cope
    // with whatever the other languages throw at us. So make it the
    // default.
    //
    // Objective C and Objective C++ seem to create entries for
    // methods whose DW_AT_name values are already fully-qualified:
    // "-[Classname method:]".  These appear at the top level.
    //
    // DWARF data for C should never include namespaces or functions
    // nested in struct types, but if it ever does, then C++'s
    // notation is probably not a bad choice for that.
    default:
    case dwarf2reader::DW_LANG_ObjC:
    case dwarf2reader::DW_LANG_ObjC_plus_plus:
    case dwarf2reader::DW_LANG_C:
    case dwarf2reader::DW_LANG_C89:
    case dwarf2reader::DW_LANG_C99:
    case dwarf2reader::DW_LANG_C_plus_plus:
      cu_context_->language = Language::CPlusPlus;
      break;
  }
}

void DwarfCUToModule::ReadSourceLines(uint64 offset,
                                      LineToModuleHandler::FileMap *files) {
  const dwarf2reader::SectionMap &section_map
      = cu_context_->file_context->section_map();
  dwarf2reader::SectionMap::const_iterator map_entry
      = section_map.find(".debug_line");
  // Mac OS X puts DWARF data in sections whose names begin with "__"
  // instead of ".".
  if (map_entry == section_map.end())
    map_entry = section_map.find("__debug_line");
  if (map_entry == section_map.end()) {
    cu_context_->reporter->MissingSection(".debug_line");
    return;
  }
  const uint8_t *section_start = map_entry->second.first;
  uint64 section_length = map_entry->second.second;
  if (offset >= section_length) {
    cu_context_->reporter->BadLineInfoOffset(offset);
    return;
  }
  line_reader_->ReadProgram(section_start + offset, section_length - offset,
                            cu_context_->file_context->module_, &lines_, files);
}

namespace {
class FunctionRange {
 public:
  FunctionRange(const Module::Range &range, Module::Function *function) :
      address(range.address), size(range.size), function(function) { }

  void AddLine(Module::Line &line) {
    function->lines.push_back(line);
  }

  Module::Address address;
  Module::Address size;
  Module::Function *function;
};

// Fills an array of ranges with pointers to the functions which owns
// them. The array is sorted in ascending order and the ranges are non
// empty and non-overlapping.

static void FillSortedFunctionRanges(vector<FunctionRange> &dest_ranges,
                                     vector<Module::Function *> *functions) {
  for (vector<Module::Function *>::const_iterator func_it = functions->cbegin();
       func_it != functions->cend();
       func_it++)
  {
    Module::Function *func = *func_it;
    vector<Module::Range> &ranges = func->ranges;
    for (vector<Module::Range>::const_iterator ranges_it = ranges.cbegin();
         ranges_it != ranges.cend();
         ++ranges_it) {
      FunctionRange range(*ranges_it, func);
      if (range.size != 0) {
          dest_ranges.push_back(range);
      }
    }
  }

  sort(dest_ranges.begin(), dest_ranges.end(),
    [](const FunctionRange &fr1, const FunctionRange &fr2) {
      return fr1.address < fr2.address;
    }
  );
}

// Return true if ADDRESS falls within the range of ITEM.
template <class T>
inline bool within(const T &item, Module::Address address) {
  // Because Module::Address is unsigned, and unsigned arithmetic
  // wraps around, this will be false if ADDRESS falls before the
  // start of ITEM, or if it falls after ITEM's end.
  return address - item.address < item.size;
}

// LINES contains all the information that we have read from .debug_line.
// INLINES contains synthesized line information gathered from
// DW_TAG_inlined_subroutine DIEs.  We're going to merge the two such that
// we have:
//
// 1. Lines from INLINES; and
// 2. Lines from LINES that don't overlap lines from INLINES.
//
// since the coarser-grained information from INLINES is generally what you
// want when getting stack traces.
vector<Module::Line> MergeLines(const vector<Module::Line>& inlines,
                                const vector<Module::Line>& lines) {
  vector<Module::Line> merged_lines;
  vector<Module::Line>::const_iterator orig_lines = lines.begin();
  vector<Module::Line>::const_iterator inline_lines = inlines.begin();
  vector<Module::Line>::const_iterator orig_end = lines.end();
  vector<Module::Line>::const_iterator inline_end = inlines.end();

  while (true) {
    if (orig_lines == orig_end) {
      break;
    }

    if (inline_lines == inline_end) {
      merged_lines.push_back(*orig_lines);
      ++orig_lines;
      continue;
    }

    // We are going to make the simplifying assumption that an inline line
    // will *always* start at the exact position of some original line.
    // This assumption significantly reduces the number of cases we have
    // to consider.

    // If we haven't caught up to where the inline lines are, keep going.
    if (orig_lines->address < inline_lines->address) {
      merged_lines.push_back(*orig_lines);
      ++orig_lines;
      continue;
    }

    // We found some overlap!  See how far we can go, and merge the inline
    // line into our list.
    if (orig_lines->address == inline_lines->address) {
      auto start = orig_lines + 1;
      while ((start->address - inline_lines->address) < inline_lines->size
             && start != orig_end) {
        ++start;
      }

      // start now points just beyond the range covered by *inline_lines.
      // But we might have encountered a case like:
      //
      // | OL1 | OL2 | OL3 | ... | OLN      | ...
      // | IL1                      | <gap> | IL2 ...
      //
      // where the end of the inline line splits the last original line that
      // we've seen in two.  This case seems like a bug in the debug
      // information, but we have to deal with it intelligently.  There are
      // several options available:
      //
      // 1. Split OLN into two parts: the part covered by IL1 and the part
      //    not covered.  Merge IL1 and then merge the latter part.
      // 2. Extend IL1 to cover the entirety of OLN, and merge IL1.
      //
      // Note that due to our simplifying assumption that any inline lines
      // start exactly on some original line, we do not have to consider
      // the case:
      //
      // | OL1 | OL2 | OL3 | ... | OLN      | ...
      // | IL1                      | IL2 ...
      //
      // where two inline lines overlap the range of one original line.
      //
      // The conservative solution is option 1, which preserves as much of
      // the original information as possible.  Let's go with that.
      merged_lines.push_back(*inline_lines);
      auto overlapped = start - 1;
      if (within(*overlapped, inline_lines->address + inline_lines->size)) {
        // Create a line that covers the rest of the space and merge that.
        Module::Line rest;
        rest.address = inline_lines->address + inline_lines->size;
        rest.size = overlapped->address + overlapped->size - rest.address;
        rest.file = overlapped->file;
        rest.number = overlapped->number;
        merged_lines.push_back(rest);
      }

      ++inline_lines;
      orig_lines = start;
      continue;
    }

    // This case is weird: we have inlined lines that exist prior to any
    // lines recorded in our debug information.  Just skip them.
    if (orig_lines->address > inline_lines->address) {
      ++inline_lines;
      continue;
    }
  }

  return merged_lines;
}

// After merging the line information, we may have adjacent lines that belong
// to the same file and line number.  (The compiler shouldn't be producing
// such line records on its own.)  Let's merge adjacent lines where possible
// to make symbol files smaller.
void CollapseAdjacentLines(vector<Module::Line>& lines) {
  if (lines.empty()) {
    return;
  }

  auto merging_into = lines.begin();
  auto next = merging_into + 1;
  const auto end = lines.end();

  while (next != end) {
    // The next record might be able to be merged.
    if ((merging_into->address + merging_into->size) == next->address &&
        merging_into->file == next->file &&
        merging_into->number == next->number) {
      merging_into->size = next->address + next->size - merging_into->address;
      ++next;
      continue;
    }

    // We've merged all we can into this record.  Move on.
    ++merging_into;

    // next now points at the most recent record that wasn't able to be
    // merged with a previous record.  We may still have more records to
    // consider, and if merging_into and next have become discontiguous,
    // we need to copy things around.
    if (next != end) {
      if (next != merging_into) {
        *merging_into = std::move(*next);
      }
      ++next;
    }
  }

  lines.erase(merging_into + 1, end);
}
}

void DwarfCUToModule::AssignLinesToFunctions(const LineToModuleHandler::FileMap &files) {
  vector<Module::Function *> *functions = &cu_context_->functions;
  WarningReporter *reporter = cu_context_->reporter;

  // This would be simpler if we assumed that source line entries
  // don't cross function boundaries.  However, there's no real reason
  // to assume that (say) a series of function definitions on the same
  // line wouldn't get coalesced into one line number entry.  The
  // DWARF spec certainly makes no such promises.
  //
  // So treat the functions and lines as peers, and take the trouble
  // to compute their ranges' intersections precisely.  In any case,
  // the hair here is a constant factor for performance; the
  // complexity from here on out is linear.

  // Put both our functions and lines in order by address.
  std::sort(functions->begin(), functions->end(),
            Module::Function::CompareByAddress);
  std::sort(lines_.begin(), lines_.end(), Module::Line::CompareByAddress);

  // Prepare a sorted list of lines containing inlined subroutines.
  vector<Module::Line> inlines;

  for (const auto& range : cu_context_->file_context->file_private_->inlined_ranges) {
    auto f = files.find(range.call_file_);
    if (f == files.end()) {
      // Uh, that's weird.  Skip this?
      continue;
    }

    Module::Line line;
    line.address = range.range_.address;
    line.size = range.range_.size;
    line.number = range.call_line_;
    line.file = f->second;
    inlines.push_back(line);
  }
  std::sort(inlines.begin(), inlines.end(), Module::Line::CompareByAddress);

  if (!inlines.empty()) {
    vector<Module::Line> merged_lines = MergeLines(inlines, lines_);

    CollapseAdjacentLines(merged_lines);

    lines_ = std::move(merged_lines);
  }

  // The last line that we used any piece of.  We use this only for
  // generating warnings.
  const Module::Line *last_line_used = NULL;

  // The last function and line we warned about --- so we can avoid
  // doing so more than once.
  const Module::Function *last_function_cited = NULL;
  const Module::Line *last_line_cited = NULL;

  // Prepare a sorted list of ranges with range-to-function mapping
  vector<FunctionRange> sorted_ranges;
  FillSortedFunctionRanges(sorted_ranges, functions);

  // Make a single pass through both the range and line vectors from lower to
  // higher addresses, populating each range's function lines vector with lines
  // from our lines_ vector that fall within the range.
  vector<FunctionRange>::iterator range_it = sorted_ranges.begin();
  vector<Module::Line>::const_iterator line_it = lines_.begin();

  Module::Address current;

  // Pointers to the referents of func_it and line_it, or NULL if the
  // iterator is at the end of the sequence.
  FunctionRange *range;
  const Module::Line *line;

  // Start current at the beginning of the first line or function,
  // whichever is earlier.
  if (range_it != sorted_ranges.end() && line_it != lines_.end()) {
    range = &*range_it;
    line = &*line_it;
    current = std::min(range->address, line->address);
  } else if (line_it != lines_.end()) {
    range = NULL;
    line = &*line_it;
    current = line->address;
  } else if (range_it != sorted_ranges.end()) {
    range = &*range_it;
    line = NULL;
    current = range->address;
  } else {
    return;
  }

  while (range || line) {
    // This loop has two invariants that hold at the top.
    //
    // First, at least one of the iterators is not at the end of its
    // sequence, and those that are not refer to the earliest
    // range or line that contains or starts after CURRENT.
    //
    // Note that every byte is in one of four states: it is covered
    // or not covered by a range, and, independently, it is
    // covered or not covered by a line.
    //
    // The second invariant is that CURRENT refers to a byte whose
    // state is different from its predecessor, or it refers to the
    // first byte in the address space. In other words, CURRENT is
    // always the address of a transition.
    //
    // Note that, although each iteration advances CURRENT from one
    // transition address to the next in each iteration, it might
    // not advance the iterators. Suppose we have a range that
    // starts with a line, has a gap, and then a second line, and
    // suppose that we enter an iteration with CURRENT at the end of
    // the first line. The next transition address is the start of
    // the second line, after the gap, so the iteration should
    // advance CURRENT to that point. At the head of that iteration,
    // the invariants require that the line iterator be pointing at
    // the second line. But this is also true at the head of the
    // next. And clearly, the iteration must not change the range
    // iterator. So neither iterator moves.

    // Assert the first invariant (see above).
    assert(!range || current < range->address || within(*range, current));
    assert(!line || current < line->address || within(*line, current));

    // The next transition after CURRENT.
    Module::Address next_transition;

    // Figure out which state we're in, add lines or warn, and compute
    // the next transition address.
    if (range && current >= range->address) {
      if (line && current >= line->address) {
        // Covered by both a line and a range.
        Module::Address range_left = range->size - (current - range->address);
        Module::Address line_left = line->size - (current - line->address);
        // This may overflow, but things work out.
        next_transition = current + std::min(range_left, line_left);
        Module::Line l = *line;
        l.address = current;
        l.size = next_transition - current;
        range->AddLine(l);
        last_line_used = line;
      } else {
        // Covered by a range, but no line.
        if (range->function != last_function_cited) {
          reporter->UncoveredFunction(*(range->function));
          last_function_cited = range->function;
        }
        if (line && within(*range, line->address))
          next_transition = line->address;
        else
          // If this overflows, we'll catch it below.
          next_transition = range->address + range->size;
      }
    } else {
      if (line && current >= line->address) {
        // Covered by a line, but no range.
        //
        // If GCC emits padding after one function to align the start
        // of the next, then it will attribute the padding
        // instructions to the last source line of function (to reduce
        // the size of the line number info), but omit it from the
        // DW_AT_{low,high}_pc range given in .debug_info (since it
        // costs nothing to be precise there). If we did use at least
        // some of the line we're about to skip, and it ends at the
        // start of the next function, then assume this is what
        // happened, and don't warn.
        if (line != last_line_cited
            && !(range
                 && line == last_line_used
                 && range->address - line->address == line->size)) {
          reporter->UncoveredLine(*line);
          last_line_cited = line;
        }
        if (range && within(*line, range->address))
          next_transition = range->address;
        else
          // If this overflows, we'll catch it below.
          next_transition = line->address + line->size;
      } else {
        // Covered by neither a range nor a line. By the invariant,
        // both range and line begin after CURRENT. The next transition
        // is the start of the next range or next line, whichever
        // is earliest.
        assert(range || line);
        if (range && line)
          next_transition = std::min(range->address, line->address);
        else if (range)
          next_transition = range->address;
        else
          next_transition = line->address;
      }
    }

    // If a function or line abuts the end of the address space, then
    // next_transition may end up being zero, in which case we've completed
    // our pass. Handle that here, instead of trying to deal with it in
    // each place we compute next_transition.
    if (!next_transition)
      break;

    // Advance iterators as needed. If lines overlap or functions overlap,
    // then we could go around more than once. We don't worry too much
    // about what result we produce in that case, just as long as we don't
    // hang or crash.
    while (range_it != sorted_ranges.end()
           && next_transition >= range_it->address
           && !within(*range_it, next_transition))
      range_it++;
    range = (range_it != sorted_ranges.end()) ? &(*range_it) : NULL;
    while (line_it != lines_.end()
           && next_transition >= line_it->address
           && !within(*line_it, next_transition))
      line_it++;
    line = (line_it != lines_.end()) ? &*line_it : NULL;

    // We must make progress.
    assert(next_transition > current);
    current = next_transition;
  }
}

void DwarfCUToModule::Finish() {
  // Assembly language files have no function data, and that gives us
  // no place to store our line numbers (even though the GNU toolchain
  // will happily produce source line info for assembly language
  // files).  To avoid spurious warnings about lines we can't assign
  // to functions, skip CUs in languages that lack functions.
  if (!cu_context_->language->HasFunctions())
    return;

  // Read source line info, if we have any.
  LineToModuleHandler::FileMap files;
  if (has_source_line_info_)
    ReadSourceLines(source_line_offset_, &files);

  vector<Module::Function *> *functions = &cu_context_->functions;

  // Dole out lines to the appropriate functions.
  AssignLinesToFunctions(files);

  // Add our functions, which now have source lines assigned to them,
  // to module_.
  cu_context_->file_context->module_->AddFunctions(functions->begin(),
                                                   functions->end());

  // Ownership of the function objects has shifted from cu_context to
  // the Module.
  functions->clear();

  cu_context_->file_context->ClearSpecifications();
}

bool DwarfCUToModule::StartCompilationUnit(uint64 offset,
                                           uint8 address_size,
                                           uint8 offset_size,
                                           uint64 cu_length,
                                           uint8 dwarf_version) {
  return dwarf_version >= 2;
}

bool DwarfCUToModule::StartRootDIE(uint64 offset, enum DwarfTag tag) {
  // We don't deal with partial compilation units (the only other tag
  // likely to be used for root DIE).
  return tag == dwarf2reader::DW_TAG_compile_unit;
}

} // namespace google_breakpad
