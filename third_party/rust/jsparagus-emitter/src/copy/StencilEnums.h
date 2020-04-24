/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_StencilEnums_h
#define vm_StencilEnums_h

#include <stdint.h>  // uint8_t

//
// Enum definitions shared between frontend, stencil, and the VM.
//

namespace js {

// [SMDOC] Try Notes
//
// Trynotes are attached to regions that are involved with
// exception unwinding. They can be broken up into four categories:
//
// 1. Catch and Finally: Basic exception handling. A Catch trynote
//    covers the range of the associated try. A Finally trynote covers
//    the try and the catch.
//
// 2. ForIn and Destructuring: These operations create an iterator
//    which must be cleaned up (by calling IteratorClose) during
//    exception unwinding.
//
// 3. ForOf and ForOfIterclose: For-of loops handle unwinding using
//    catch blocks. These trynotes are used for for-of breaks/returns,
//    which create regions that are lexically within a for-of block,
//    but logically outside of it. See TryNoteIter::settle for more
//    details.
//
// 4. Loop: This represents normal for/while/do-while loops. It is
//    unnecessary for exception unwinding, but storing the boundaries
//    of loops here is helpful for heuristics that need to know
//    whether a given op is inside a loop.
enum class TryNoteKind : uint8_t {
  Catch,
  Finally,
  ForIn,
  Destructuring,
  ForOf,
  ForOfIterClose,
  Loop
};

// [SMDOC] Script Flags
//
// Interpreted scripts represented by the BaseScript type use two flag words to
// encode an assortment of conditions and attributes about the script.
//
// The "immutable" flags are a combination of input flags describing aspects of
// the execution context that affect parsing (such as if we are an ES module or
// normal script), and flags derived from source text. These flags are preserved
// during cloning and serializing. As well, they should never change after the
// BaseScript is created (although there are currently a few exceptions for
// de-/re-lazification that remain).
//
// The "mutable" flags are temporary flags that are used by subsystems in the
// engine such as the debugger or JITs. These flags are not preserved through
// serialization or cloning since the attributes are generally associated with
// one specific instance of a BaseScript.

enum class ImmutableScriptFlagsEnum : uint32_t {
  // Input Flags
  //
  // Flags that come from CompileOptions.
  // ----

  // No need for result value of last expression statement.
  NoScriptRval = 1 << 0,

  // See Parser::selfHostingMode.
  SelfHosted = 1 << 1,

  // TreatAsRunOnce roughly indicates that a script is expected to be run no
  // more than once. This affects optimizations and heuristics.
  //
  // On top-level global/eval/module scripts, this is set when the embedding
  // ensures this script will not be re-used. In this case, parser literals may
  // be exposed directly instead of being cloned.
  //
  // For non-lazy functions, this is set when the function is almost-certain to
  // be run once (and its parents transitively the same). In this case, the
  // function may be marked as a singleton to improve typeset precision. Note
  // that under edge cases with fun.caller the function may still run multiple
  // times.
  //
  // For lazy functions, the situation is more complex. If enclosing script is
  // not yet compiled, this flag is undefined and should not be used. As the
  // enclosing script is compiled, this flag is updated to the same definition
  // the eventual non-lazy function will use.
  TreatAsRunOnce = 1 << 2,

  // Code was forced into strict mode using CompileOptions.
  ForceStrict = 1 << 3,

  // Script came from eval().
  IsForEval = 1 << 4,

  // Script is parsed with a top-level goal of Module. This may be a top-level
  // or an inner-function script.
  IsModule = 1 << 5,

  // Script is for function.
  IsFunction = 1 << 6,
  // ----

  // Parser Flags
  //
  // Flags that come from the parser.
  // ----

  // Code is in strict mode.
  Strict = 1 << 7,

  // The (static) bindings of this script need to support dynamic name
  // read/write access. Here, 'dynamic' means dynamic dictionary lookup on
  // the scope chain for a dynamic set of keys. The primary examples are:
  //  - direct eval
  //  - function:
  //  - with
  // since both effectively allow any name to be accessed. Non-examples are:
  //  - upvars of nested functions
  //  - function statement
  // since the set of assigned name is known dynamically.
  //
  // Note: access through the arguments object is not considered dynamic
  // binding access since it does not go through the normal name lookup
  // mechanism. This is debatable and could be changed (although care must be
  // taken not to turn off the whole 'arguments' optimization). To answer the
  // more general "is this argument aliased" question, script->needsArgsObj
  // should be tested (see JSScript::argIsAliased).
  BindingsAccessedDynamically = 1 << 8,

  // This function does something that can extend the set of bindings in its
  // call objects --- it does a direct eval in non-strict code, or includes a
  // function statement (as opposed to a function definition).
  //
  // This flag is *not* inherited by enclosed or enclosing functions; it
  // applies only to the function in whose flags it appears.
  //
  FunHasExtensibleScope = 1 << 9,

  // True if a tagged template exists in the body => Bytecode contains
  // JSOp::CallSiteObj
  // (We don't relazify functions with template strings, due to observability)
  HasCallSiteObj = 1 << 10,

  // Script is parsed with a top-level goal of Module. This may be a top-level
  // or an inner-function script.
  HasModuleGoal = 1 << 11,

  // Whether this function has a .this binding. If true, we need to emit
  // JSOp::FunctionThis in the prologue to initialize it.
  FunctionHasThisBinding = 1 << 12,

  // Whether the arguments object for this script, if it needs one, should be
  // mapped (alias formal parameters).
  HasMappedArgsObj = 1 << 13,

  // Script contains inner functions. Used to check if we can relazify the
  // script.
  HasInnerFunctions = 1 << 14,

  NeedsHomeObject = 1 << 15,
  IsDerivedClassConstructor = 1 << 16,

  // 'this', 'arguments' and f.apply() are used. This is likely to be a
  // wrapper.
  IsLikelyConstructorWrapper = 1 << 17,

  // Set if this function is a generator function or async generator.
  IsGenerator = 1 << 18,

  // Set if this function is an async function or async generator.
  IsAsync = 1 << 19,

  // Set if this function has a rest parameter.
  HasRest = 1 << 20,

  // Whether 'arguments' has a local binding.
  //
  // Technically, every function has a binding named 'arguments'. Internally,
  // this binding is only added when 'arguments' is mentioned by the function
  // body. This flag indicates whether 'arguments' has been bound either
  // through implicit use:
  //   function f() { return arguments }
  // or explicit redeclaration:
  //   function f() { var arguments; return arguments }
  //
  // Note 1: overwritten arguments (function() { arguments = 3 }) will cause
  // this flag to be set but otherwise require no special handling:
  // 'arguments' is just a local variable and uses of 'arguments' will just
  // read the local's current slot which may have been assigned. The only
  // special semantics is that the initial value of 'arguments' is the
  // arguments object (not undefined, like normal locals).
  //
  // Note 2: if 'arguments' is bound as a formal parameter, there will be an
  // 'arguments' in Bindings, but, as the "LOCAL" in the name indicates, this
  // flag will not be set. This is because, as a formal, 'arguments' will
  // have no special semantics: the initial value is unconditionally the
  // actual argument (or undefined if nactual < nformal).
  ArgumentsHasVarBinding = 1 << 21,

  // Whether 'arguments' always must be the arguments object. If this is unset,
  // but ArgumentsHasVarBinding is set then an analysis pass is performed at
  // runtime to decide if we can optimize it away.
  AlwaysNeedsArgsObj = 1 << 22,

  // Whether the Parser declared 'arguments'.
  ShouldDeclareArguments = 1 << 23,

  // Whether this script contains a direct eval statement.
  HasDirectEval = 1 << 24,

  // An extra VarScope is used as the body scope instead of the normal
  // FunctionScope. This is needed when parameter expressions are used AND the
  // function has var bindings or a sloppy-direct-eval. For example,
  //    `function(x = eval("")) { var y; }`
  FunctionHasExtraBodyVarScope = 1 << 25,

  // Whether this function needs a call object or named lambda environment.
  NeedsFunctionEnvironmentObjects = 1 << 26,
  // ----

  // Bytecode Emitter Flags
  //
  // Flags that are initialized by the BCE.
  // ----

  // True if the script has a non-syntactic scope on its dynamic scope chain.
  // That is, there are objects about which we know nothing between the
  // outermost syntactic scope and the global.
  HasNonSyntacticScope = 1 << 27,
};

enum class MutableScriptFlagsEnum : uint32_t {
  // Number of times the |warmUpCount| was forcibly discarded. The counter is
  // reset when a script is successfully jit-compiled.
  WarmupResets_MASK = 0xFF,

  // (1 << 8) is unused

  // If treatAsRunOnce, whether script has executed.
  HasRunOnce = 1 << 9,

  // Script has been reused for a clone.
  HasBeenCloned = 1 << 10,

  // Script has an entry in Realm::scriptCountsMap.
  HasScriptCounts = 1 << 12,

  // Script has an entry in Realm::debugScriptMap.
  HasDebugScript = 1 << 13,

  // Script supports relazification where it releases bytecode and gcthings to
  // save memory. This process is opt-in since various complexities may disallow
  // this for some scripts.
  // NOTE: Must check for isRelazifiable() before setting this flag.
  AllowRelazify = 1 << 14,

  // IonMonkey compilation hints.

  // Script has had hoisted bounds checks fail.
  FailedBoundsCheck = 1 << 15,

  // Script has had hoisted shape guard fail.
  FailedShapeGuard = 1 << 16,

  HadFrequentBailouts = 1 << 17,
  HadOverflowBailout = 1 << 18,

  // Whether Baseline or Ion compilation has been disabled for this script.
  // IonDisabled is equivalent to |jitScript->canIonCompile() == false| but
  // JitScript can be discarded on GC and we don't want this to affect
  // observable behavior (see ArgumentsGetterImpl comment).
  BaselineDisabled = 1 << 19,
  IonDisabled = 1 << 20,

  // Explicitly marked as uninlineable.
  Uninlineable = 1 << 21,

  // Idempotent cache has triggered invalidation.
  InvalidatedIdempotentCache = 1 << 22,

  // Lexical check did fail and bail out.
  FailedLexicalCheck = 1 << 23,

  // See comments below.
  NeedsArgsAnalysis = 1 << 24,
  NeedsArgsObj = 1 << 25,

  // Set if the script has opted into spew
  SpewEnabled = 1 << 26,
};

}  // namespace js

#endif /* vm_StencilEnums_h */
