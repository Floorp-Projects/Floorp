// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_CHROMECLASSTESTER_H_
#define TOOLS_CLANG_PLUGINS_CHROMECLASSTESTER_H_

#include <set>
#include <vector>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Frontend/CompilerInstance.h"

// A class on top of ASTConsumer that forwards classes defined in Chromium
// headers to subclasses which implement CheckChromeClass().
class ChromeClassTester : public clang::ASTConsumer {
 public:
  explicit ChromeClassTester(clang::CompilerInstance& instance);
  virtual ~ChromeClassTester();

  // clang::ASTConsumer:
  virtual void HandleTagDeclDefinition(clang::TagDecl* tag);
  virtual bool HandleTopLevelDecl(clang::DeclGroupRef group_ref);

 protected:
  clang::CompilerInstance& instance() { return instance_; }
  clang::DiagnosticsEngine& diagnostic() { return diagnostic_; }

  // Emits a simple warning; this shouldn't be used if you require printf-style
  // printing.
  void emitWarning(clang::SourceLocation loc, const char* error);

  // Utility method for subclasses to check if this class is in a banned
  // namespace.
  bool InBannedNamespace(const clang::Decl* record);

  // Utility method for subclasses to determine the namespace of the
  // specified record, if any. Unnamed namespaces will be identified as
  // "<anonymous namespace>".
  std::string GetNamespace(const clang::Decl* record);

  // Utility method for subclasses to check if this class is within an
  // implementation (.cc, .cpp, .mm) file.
  bool InImplementationFile(clang::SourceLocation location);

 private:
  void BuildBannedLists();

  void CheckTag(clang::TagDecl*);

  // Filtered versions of tags that are only called with things defined in
  // chrome header files.
  virtual void CheckChromeClass(clang::SourceLocation record_location,
                                clang::CXXRecordDecl* record) = 0;

  // Utility methods used for filtering out non-chrome classes (and ones we
  // deliberately ignore) in HandleTagDeclDefinition().
  std::string GetNamespaceImpl(const clang::DeclContext* context,
                               const std::string& candidate);
  bool InBannedDirectory(clang::SourceLocation loc);
  bool IsIgnoredType(const std::string& base_name);

  // Attempts to determine the filename for the given SourceLocation.
  // Returns false if the filename could not be determined.
  bool GetFilename(clang::SourceLocation loc, std::string* filename);

  clang::CompilerInstance& instance_;
  clang::DiagnosticsEngine& diagnostic_;

  // List of banned namespaces.
  std::vector<std::string> banned_namespaces_;

  // List of banned directories.
  std::vector<std::string> banned_directories_;

  // List of types that we don't check.
  std::set<std::string> ignored_record_names_;

  // List of decls to check once the current top-level decl is parsed.
  std::vector<clang::TagDecl*> pending_class_decls_;
};

#endif  // TOOLS_CLANG_PLUGINS_CHROMECLASSTESTER_H_
