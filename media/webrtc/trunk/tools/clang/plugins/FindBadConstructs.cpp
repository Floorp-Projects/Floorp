// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a bunch of recurring problems in the Chromium C++ code.
//
// Checks that are implemented:
// - Constructors/Destructors should not be inlined if they are of a complex
//   class type.
// - Missing "virtual" keywords on methods that should be virtual.
// - Non-annotated overriding virtual methods.
// - Virtual methods with nonempty implementations in their headers.
// - Classes that derive from base::RefCounted / base::RefCountedThreadSafe
//   should have protected or private destructors.

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/AST.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"

#include "ChromeClassTester.h"

using namespace clang;

namespace {

bool TypeHasNonTrivialDtor(const Type* type) {
  if (const CXXRecordDecl* cxx_r = type->getCXXRecordDeclForPointerType())
    return cxx_r->hasTrivialDestructor();

  return false;
}

// Returns the underlying Type for |type| by expanding typedefs and removing
// any namespace qualifiers.
const Type* UnwrapType(const Type* type) {
  if (const ElaboratedType* elaborated = dyn_cast<ElaboratedType>(type))
    return UnwrapType(elaborated->getNamedType().getTypePtr());
  if (const TypedefType* typedefed = dyn_cast<TypedefType>(type))
    return UnwrapType(typedefed->desugar().getTypePtr());
  return type;
}

// Searches for constructs that we know we don't want in the Chromium code base.
class FindBadConstructsConsumer : public ChromeClassTester {
 public:
  FindBadConstructsConsumer(CompilerInstance& instance,
                            bool check_refcounted_dtors,
                            bool check_virtuals_in_implementations)
      : ChromeClassTester(instance),
        check_refcounted_dtors_(check_refcounted_dtors),
        check_virtuals_in_implementations_(check_virtuals_in_implementations) {
  }

  virtual void CheckChromeClass(SourceLocation record_location,
                                CXXRecordDecl* record) {
    bool implementation_file = InImplementationFile(record_location);

    if (!implementation_file) {
      // Only check for "heavy" constructors/destructors in header files;
      // within implementation files, there is no performance cost.
      CheckCtorDtorWeight(record_location, record);
    }

    if (!implementation_file || check_virtuals_in_implementations_) {
      bool warn_on_inline_bodies = !implementation_file;

      // Check that all virtual methods are marked accordingly with both
      // virtual and OVERRIDE.
      CheckVirtualMethods(record_location, record, warn_on_inline_bodies);
    }

    if (check_refcounted_dtors_)
      CheckRefCountedDtors(record_location, record);
  }

 private:
  bool check_refcounted_dtors_;
  bool check_virtuals_in_implementations_;

  // Returns true if |base| specifies one of the Chromium reference counted
  // classes (base::RefCounted / base::RefCountedThreadSafe). |user_data| is
  // ignored.
  static bool IsRefCountedCallback(const CXXBaseSpecifier* base,
                                   CXXBasePath& path,
                                   void* user_data) {
    FindBadConstructsConsumer* self =
        static_cast<FindBadConstructsConsumer*>(user_data);

    const TemplateSpecializationType* base_type =
        dyn_cast<TemplateSpecializationType>(
            UnwrapType(base->getType().getTypePtr()));
    if (!base_type) {
      // Base-most definition is not a template, so this cannot derive from
      // base::RefCounted. However, it may still be possible to use with a
      // scoped_refptr<> and support ref-counting, so this is not a perfect
      // guarantee of safety.
      return false;
    }

    TemplateName name = base_type->getTemplateName();
    if (TemplateDecl* decl = name.getAsTemplateDecl()) {
      std::string base_name = decl->getNameAsString();

      // Check for both base::RefCounted and base::RefCountedThreadSafe.
      if (base_name.compare(0, 10, "RefCounted") == 0 &&
          self->GetNamespace(decl) == "base") {
        return true;
      }
    }
    return false;
  }

  // Prints errors if the destructor of a RefCounted class is public.
  void CheckRefCountedDtors(SourceLocation record_location,
                            CXXRecordDecl* record) {
    // Skip anonymous structs.
    if (record->getIdentifier() == NULL)
      return;

    CXXBasePaths paths;
    if (!record->lookupInBases(
            &FindBadConstructsConsumer::IsRefCountedCallback, this, paths)) {
      return;  // Class does not derive from a ref-counted base class.
    }

    if (!record->hasUserDeclaredDestructor()) {
      emitWarning(
          record_location,
          "Classes that are ref-counted should have explicit "
          "destructors that are protected or private.");
    } else if (CXXDestructorDecl* dtor = record->getDestructor()) {
      if (dtor->getAccess() == AS_public) {
        emitWarning(
            dtor->getInnerLocStart(),
            "Classes that are ref-counted should not have "
            "public destructors.");
      }
    }
  }

  // Prints errors if the constructor/destructor weight is too heavy.
  void CheckCtorDtorWeight(SourceLocation record_location,
                           CXXRecordDecl* record) {
    // We don't handle anonymous structs. If this record doesn't have a
    // name, it's of the form:
    //
    // struct {
    //   ...
    // } name_;
    if (record->getIdentifier() == NULL)
      return;

    // Count the number of templated base classes as a feature of whether the
    // destructor can be inlined.
    int templated_base_classes = 0;
    for (CXXRecordDecl::base_class_const_iterator it = record->bases_begin();
         it != record->bases_end(); ++it) {
      if (it->getTypeSourceInfo()->getTypeLoc().getTypeLocClass() ==
          TypeLoc::TemplateSpecialization) {
        ++templated_base_classes;
      }
    }

    // Count the number of trivial and non-trivial member variables.
    int trivial_member = 0;
    int non_trivial_member = 0;
    int templated_non_trivial_member = 0;
    for (RecordDecl::field_iterator it = record->field_begin();
         it != record->field_end(); ++it) {
      CountType(it->getType().getTypePtr(),
                &trivial_member,
                &non_trivial_member,
                &templated_non_trivial_member);
    }

    // Check to see if we need to ban inlined/synthesized constructors. Note
    // that the cutoffs here are kind of arbitrary. Scores over 10 break.
    int dtor_score = 0;
    // Deriving from a templated base class shouldn't be enough to trigger
    // the ctor warning, but if you do *anything* else, it should.
    //
    // TODO(erg): This is motivated by templated base classes that don't have
    // any data members. Somehow detect when templated base classes have data
    // members and treat them differently.
    dtor_score += templated_base_classes * 9;
    // Instantiating a template is an insta-hit.
    dtor_score += templated_non_trivial_member * 10;
    // The fourth normal class member should trigger the warning.
    dtor_score += non_trivial_member * 3;

    int ctor_score = dtor_score;
    // You should be able to have 9 ints before we warn you.
    ctor_score += trivial_member;

    if (ctor_score >= 10) {
      if (!record->hasUserDeclaredConstructor()) {
        emitWarning(record_location,
                    "Complex class/struct needs an explicit out-of-line "
                    "constructor.");
      } else {
        // Iterate across all the constructors in this file and yell if we
        // find one that tries to be inline.
        for (CXXRecordDecl::ctor_iterator it = record->ctor_begin();
             it != record->ctor_end(); ++it) {
          if (it->hasInlineBody()) {
            if (it->isCopyConstructor() &&
                !record->hasUserDeclaredCopyConstructor()) {
              emitWarning(record_location,
                          "Complex class/struct needs an explicit out-of-line "
                          "copy constructor.");
            } else {
              emitWarning(it->getInnerLocStart(),
                          "Complex constructor has an inlined body.");
            }
          }
        }
      }
    }

    // The destructor side is equivalent except that we don't check for
    // trivial members; 20 ints don't need a destructor.
    if (dtor_score >= 10 && !record->hasTrivialDestructor()) {
      if (!record->hasUserDeclaredDestructor()) {
        emitWarning(
            record_location,
            "Complex class/struct needs an explicit out-of-line "
            "destructor.");
      } else if (CXXDestructorDecl* dtor = record->getDestructor()) {
        if (dtor->hasInlineBody()) {
          emitWarning(dtor->getInnerLocStart(),
                      "Complex destructor has an inline body.");
        }
      }
    }
  }

  void CheckVirtualMethod(const CXXMethodDecl* method,
                          bool warn_on_inline_bodies) {
    if (!method->isVirtual())
      return;

    if (!method->isVirtualAsWritten()) {
      SourceLocation loc = method->getTypeSpecStartLoc();
      if (isa<CXXDestructorDecl>(method))
        loc = method->getInnerLocStart();
      emitWarning(loc, "Overriding method must have \"virtual\" keyword.");
    }

    // Virtual methods should not have inline definitions beyond "{}". This
    // only matters for header files.
    if (warn_on_inline_bodies && method->hasBody() &&
        method->hasInlineBody()) {
      if (CompoundStmt* cs = dyn_cast<CompoundStmt>(method->getBody())) {
        if (cs->size()) {
          emitWarning(
              cs->getLBracLoc(),
              "virtual methods with non-empty bodies shouldn't be "
              "declared inline.");
        }
      }
    }
  }

  bool InTestingNamespace(const Decl* record) {
    return GetNamespace(record).find("testing") != std::string::npos;
  }

  bool IsMethodInBannedNamespace(const CXXMethodDecl* method) {
    if (InBannedNamespace(method))
      return true;
    for (CXXMethodDecl::method_iterator i = method->begin_overridden_methods();
         i != method->end_overridden_methods();
         ++i) {
      const CXXMethodDecl* overridden = *i;
      if (IsMethodInBannedNamespace(overridden))
        return true;
    }

    return false;
  }

  void CheckOverriddenMethod(const CXXMethodDecl* method) {
    if (!method->size_overridden_methods() || method->getAttr<OverrideAttr>())
      return;

    if (isa<CXXDestructorDecl>(method) || method->isPure())
      return;

    if (IsMethodInBannedNamespace(method))
      return;

    SourceLocation loc = method->getTypeSpecStartLoc();
    emitWarning(loc, "Overriding method must be marked with OVERRIDE.");
  }

  // Makes sure there is a "virtual" keyword on virtual methods.
  //
  // Gmock objects trigger these for each MOCK_BLAH() macro used. So we have a
  // trick to get around that. If a class has member variables whose types are
  // in the "testing" namespace (which is how gmock works behind the scenes),
  // there's a really high chance we won't care about these errors
  void CheckVirtualMethods(SourceLocation record_location,
                           CXXRecordDecl* record,
                           bool warn_on_inline_bodies) {
    for (CXXRecordDecl::field_iterator it = record->field_begin();
         it != record->field_end(); ++it) {
      CXXRecordDecl* record_type =
          it->getTypeSourceInfo()->getTypeLoc().getTypePtr()->
          getAsCXXRecordDecl();
      if (record_type) {
        if (InTestingNamespace(record_type)) {
          return;
        }
      }
    }

    for (CXXRecordDecl::method_iterator it = record->method_begin();
         it != record->method_end(); ++it) {
      if (it->isCopyAssignmentOperator() || isa<CXXConstructorDecl>(*it)) {
        // Ignore constructors and assignment operators.
      } else if (isa<CXXDestructorDecl>(*it) &&
          !record->hasUserDeclaredDestructor()) {
        // Ignore non-user-declared destructors.
      } else {
        CheckVirtualMethod(*it, warn_on_inline_bodies);
        CheckOverriddenMethod(*it);
      }
    }
  }

  void CountType(const Type* type,
                 int* trivial_member,
                 int* non_trivial_member,
                 int* templated_non_trivial_member) {
    switch (type->getTypeClass()) {
      case Type::Record: {
        // Simplifying; the whole class isn't trivial if the dtor is, but
        // we use this as a signal about complexity.
        if (TypeHasNonTrivialDtor(type))
          (*trivial_member)++;
        else
          (*non_trivial_member)++;
        break;
      }
      case Type::TemplateSpecialization: {
        TemplateName name =
            dyn_cast<TemplateSpecializationType>(type)->getTemplateName();
        bool whitelisted_template = false;

        // HACK: I'm at a loss about how to get the syntax checker to get
        // whether a template is exterened or not. For the first pass here,
        // just do retarded string comparisons.
        if (TemplateDecl* decl = name.getAsTemplateDecl()) {
          std::string base_name = decl->getNameAsString();
          if (base_name == "basic_string")
            whitelisted_template = true;
        }

        if (whitelisted_template)
          (*non_trivial_member)++;
        else
          (*templated_non_trivial_member)++;
        break;
      }
      case Type::Elaborated: {
        CountType(
            dyn_cast<ElaboratedType>(type)->getNamedType().getTypePtr(),
            trivial_member, non_trivial_member, templated_non_trivial_member);
        break;
      }
      case Type::Typedef: {
        while (const TypedefType* TT = dyn_cast<TypedefType>(type)) {
          type = TT->getDecl()->getUnderlyingType().getTypePtr();
        }
        CountType(type, trivial_member, non_trivial_member,
                  templated_non_trivial_member);
        break;
      }
      default: {
        // Stupid assumption: anything we see that isn't the above is one of
        // the 20 integer types.
        (*trivial_member)++;
        break;
      }
    }
  }
};

class FindBadConstructsAction : public PluginASTAction {
 public:
  FindBadConstructsAction()
      : check_refcounted_dtors_(true),
        check_virtuals_in_implementations_(true) {
  }

 protected:
  // Overridden from PluginASTAction:
  virtual ASTConsumer* CreateASTConsumer(CompilerInstance& instance,
                                         llvm::StringRef ref) {
    return new FindBadConstructsConsumer(
        instance, check_refcounted_dtors_, check_virtuals_in_implementations_);
  }

  virtual bool ParseArgs(const CompilerInstance& instance,
                         const std::vector<std::string>& args) {
    bool parsed = true;

    for (size_t i = 0; i < args.size() && parsed; ++i) {
      if (args[i] == "skip-refcounted-dtors") {
        check_refcounted_dtors_ = false;
      } else if (args[i] == "skip-virtuals-in-implementations") {
        check_virtuals_in_implementations_ = false;
      } else {
        parsed = false;
        llvm::errs() << "Unknown argument: " << args[i] << "\n";
      }
    }

    return parsed;
  }

 private:
  bool check_refcounted_dtors_;
  bool check_virtuals_in_implementations_;
};

}  // namespace

static FrontendPluginRegistry::Add<FindBadConstructsAction>
X("find-bad-constructs", "Finds bad C++ constructs");
