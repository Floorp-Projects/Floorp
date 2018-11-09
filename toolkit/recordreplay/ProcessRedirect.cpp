/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessRedirect.h"

#include "InfallibleVector.h"
#include "MiddlemanCall.h"
#include "ipc/ChildInternal.h"
#include "ipc/ParentInternal.h"
#include "mozilla/Sprintf.h"

#include <dlfcn.h>
#include <string.h>

namespace {

#include "udis86/udis86.c"
#include "udis86/decode.c"
#include "udis86/itab.c"

} // anonymous namespace

namespace mozilla {
namespace recordreplay {

///////////////////////////////////////////////////////////////////////////////
// Redirection Skeleton
///////////////////////////////////////////////////////////////////////////////

static bool
CallPreambleHook(PreambleFn aPreamble, size_t aCallId, CallArguments* aArguments)
{
  PreambleResult result = aPreamble(aArguments);
  switch (result) {
  case PreambleResult::Veto:
    return true;
  case PreambleResult::PassThrough: {
    AutoEnsurePassThroughThreadEvents pt;
    RecordReplayInvokeCall(aCallId, aArguments);
    return true;
  }
  case PreambleResult::Redirect:
    return false;
  }
  Unreachable();
}

extern "C" {

__attribute__((used)) int
RecordReplayInterceptCall(int aCallId, CallArguments* aArguments)
{
  Redirection& redirection = gRedirections[aCallId];

  // Call the preamble to see if this call needs special handling, even when
  // events have been passed through.
  if (redirection.mPreamble) {
    if (CallPreambleHook(redirection.mPreamble, aCallId, aArguments)) {
      return 0;
    }
  }

  Thread* thread = Thread::Current();
  Maybe<RecordingEventSection> res;
  res.emplace(thread);

  if (!res.ref().CanAccessEvents()) {
    // When events are passed through, invoke the call with the original stack
    // and register state.
    if (!thread || thread->PassThroughEvents()) {
      // RecordReplayRedirectCall will load the function to call from the
      // return value slot.
      aArguments->Rval<uint8_t*>() = redirection.mOriginalFunction;
      return 1;
    }

    MOZ_RELEASE_ASSERT(thread->HasDivergedFromRecording());

    // After we have diverged from the recording, we can't access the thread's
    // recording anymore.

    // If the redirection has a middleman preamble hook, call it to see if it
    // can handle this call. The middleman preamble hook is separate from the
    // normal preamble hook because entering the RecordingEventSection can
    // cause the current thread to diverge from the recording; testing for
    // HasDivergedFromRecording() does not work reliably in the normal preamble.
    if (redirection.mMiddlemanPreamble) {
      if (CallPreambleHook(redirection.mMiddlemanPreamble, aCallId, aArguments)) {
        return 0;
      }
    }

    // If the redirection has a middleman call hook, try to perform the call in
    // the middleman instead.
    if (redirection.mMiddlemanCall) {
      if (SendCallToMiddleman(aCallId, aArguments, /* aPopulateOutput = */ true)) {
        return 0;
      }
    }

    if (child::CurrentRepaintCannotFail()) {
      // EnsureNotDivergedFromRecording is going to force us to crash, so fail
      // earlier with a more helpful error message.
      child::ReportFatalError(Nothing(), "Could not perform middleman call: %s\n",
                              redirection.mName);
    }

    // Calling any redirection which performs the standard steps will cause
    // debugger operations that have diverged from the recording to fail.
    EnsureNotDivergedFromRecording();
    Unreachable();
  }

  if (IsRecording()) {
    // Call the original function, passing through events while we do so.
    // Destroy the RecordingEventSection so that we don't prevent the file
    // from being flushed in case we end up blocking.
    res.reset();
    thread->SetPassThrough(true);
    RecordReplayInvokeCall(aCallId, aArguments);
    thread->SetPassThrough(false);
    res.emplace(thread);
  }

  // Save any system error in case we want to record/replay it.
  ErrorType error = SaveError();

  // Add an event for the thread.
  thread->Events().RecordOrReplayThreadEvent(CallIdToThreadEvent(aCallId));

  // Save any output produced by the call.
  if (redirection.mSaveOutput) {
    redirection.mSaveOutput(thread->Events(), aArguments, &error);
  }

  // Save information about any potential middleman calls encountered if we
  // haven't diverged from the recording, in case we diverge and later calls
  // access data produced by this one.
  if (IsReplaying() && redirection.mMiddlemanCall) {
    (void) SendCallToMiddleman(aCallId, aArguments, /* aDiverged = */ false);
  }

  RestoreError(error);
  return 0;
}

// Entry point for all redirections. When generated code jumps here, %rax holds
// the CallEvent being invoked, and all other registers and stack contents are
// the same as when the call was originally invoked. This fills in a
// CallArguments structure with information about the call, before invoking
// RecordReplayInterceptCall.
extern size_t
RecordReplayRedirectCall(...);

__asm(
"_RecordReplayRedirectCall:"

  // Make space for a CallArguments struct on the stack.
  "subq $616, %rsp;"

  // Fill in the structure's contents.
  "movq %rdi, 0(%rsp);"
  "movq %rsi, 8(%rsp);"
  "movq %rdx, 16(%rsp);"
  "movq %rcx, 24(%rsp);"
  "movq %r8, 32(%rsp);"
  "movq %r9, 40(%rsp);"
  "movsd %xmm0, 48(%rsp);"
  "movsd %xmm1, 56(%rsp);"
  "movsd %xmm2, 64(%rsp);"

  // Count how many stack arguments we need to save.
  "movq $64, %rsi;"

  // Enter the loop below. The compiler might not place this block of code
  // adjacent to the loop, so perform the jump explicitly.
  "jmp _RecordReplayRedirectCall_Loop;"

  // Save stack arguments into the structure.
"_RecordReplayRedirectCall_Loop:"
  "subq $1, %rsi;"
  "movq 624(%rsp, %rsi, 8), %rdx;" // Ignore the return ip on the stack.
  "movq %rdx, 104(%rsp, %rsi, 8);"
  "testq %rsi, %rsi;"
  "jne _RecordReplayRedirectCall_Loop;"

  // Place the CallEvent being made into the first argument register.
  "movq %rax, %rdi;"

  // Place the structure's address into the second argument register.
  "movq %rsp, %rsi;"

  // Determine how to handle this call.
  "call _RecordReplayInterceptCall;"

  "testq %rax, %rax;"
  "je RecordReplayRedirectCall_done;"

  // Events are passed through. The function to call has been stored in rval0.
  // Call the function with the original stack and register state.
  "movq 0(%rsp), %rdi;"
  "movq 8(%rsp), %rsi;"
  "movq 16(%rsp), %rdx;"
  "movq 24(%rsp), %rcx;"
  "movq 32(%rsp), %r8;"
  "movq 40(%rsp), %r9;"
  "movsd 48(%rsp), %xmm0;"
  "movsd 56(%rsp), %xmm1;"
  "movsd 64(%rsp), %xmm2;"
  "movq 72(%rsp), %rax;"
  "addq $616, %rsp;"
  "jmpq *%rax;"

  // The message has been recorded/replayed.
"RecordReplayRedirectCall_done:"
  // Restore scalar and floating point return values.
  "movq 72(%rsp), %rax;"
  "movq 80(%rsp), %rdx;"
  "movsd 88(%rsp), %xmm0;"
  "movsd 96(%rsp), %xmm1;"

  // Pop the structure from the stack.
  "addq $616, %rsp;"

  // Return to caller.
  "ret;"
);

// Call a function address with the specified arguments.
extern void
RecordReplayInvokeCallRaw(CallArguments* aArguments, void* aFnPtr);

__asm(
"_RecordReplayInvokeCallRaw:"

  // Save function pointer in rax.
  "movq %rsi, %rax;"

  // Save arguments on the stack. This also aligns the stack.
  "push %rdi;"

  // Count how many stack arguments we need to copy.
  "movq $64, %rsi;"

  // Enter the loop below. The compiler might not place this block of code
  // adjacent to the loop, so perform the jump explicitly.
  "jmp _RecordReplayInvokeCallRaw_Loop;"

  // Copy each stack argument to the stack.
"_RecordReplayInvokeCallRaw_Loop:"
  "subq $1, %rsi;"
  "movq 104(%rdi, %rsi, 8), %rdx;"
  "push %rdx;"
  "testq %rsi, %rsi;"
  "jne _RecordReplayInvokeCallRaw_Loop;"

  // Copy each register argument into the appropriate register.
  "movq 8(%rdi), %rsi;"
  "movq 16(%rdi), %rdx;"
  "movq 24(%rdi), %rcx;"
  "movq 32(%rdi), %r8;"
  "movq 40(%rdi), %r9;"
  "movsd 48(%rdi), %xmm0;"
  "movsd 56(%rdi), %xmm1;"
  "movsd 64(%rdi), %xmm2;"
  "movq 0(%rdi), %rdi;"

  // Call the saved function pointer.
  "callq *%rax;"

  // Pop the copied stack arguments.
  "addq $512, %rsp;"

  // Save any return values to the arguments.
  "pop %rdi;"
  "movq %rax, 72(%rdi);"
  "movq %rdx, 80(%rdi);"
  "movsd %xmm0, 88(%rdi);"
  "movsd %xmm1, 96(%rdi);"

  "ret;"
);

} // extern "C"

MOZ_NEVER_INLINE void
RecordReplayInvokeCall(size_t aCallId, CallArguments* aArguments)
{
  RecordReplayInvokeCallRaw(aArguments, OriginalFunction(aCallId));
}

///////////////////////////////////////////////////////////////////////////////
// Library API Redirections
///////////////////////////////////////////////////////////////////////////////

// Redirecting system library APIs requires delicacy. We have to patch the code
// so that whenever control reaches the beginning of the library API's symbol,
// we will end up jumping to an address of our choice instead. This has to be
// done without corrupting the instructions of any functions in the library,
// which principally means ensuring that there are no internal jumps into the
// code segments we have patched.
//
// The patching we do here might fail: it isn't possible to redirect an
// arbitrary symbol within an arbitrary block of code. We are doing a best
// effort sort of thing, and any failures will be noted for reporting and
// without touching the original code at all.

// Keep track of the jumps we know about which could affect the validity if a
// code patch.
static StaticInfallibleVector<std::pair<uint8_t*,uint8_t*>> gInternalJumps;

// Jump to patch in at the end of redirecting. To avoid issues with calling
// redirected functions before all redirections have been installed
// (particularly due to locks being taken while checking for internal jump
// targets), all modification of the original code is delayed until after no
// further system calls are needed.
struct JumpPatch
{
  uint8_t* mStart;
  uint8_t* mTarget;
  bool mShort;

  JumpPatch(uint8_t* aStart, uint8_t* aTarget, bool aShort)
    : mStart(aStart), mTarget(aTarget), mShort(aShort)
  {}
};
static StaticInfallibleVector<JumpPatch> gJumpPatches;

static void
AddJumpPatch(uint8_t* aStart, uint8_t* aTarget, bool aShort)
{
  gInternalJumps.emplaceBack(aStart, aTarget);
  gJumpPatches.emplaceBack(aStart, aTarget, aShort);
}

// A range of instructions to clobber at the end of redirecting.
struct ClobberPatch
{
  uint8_t* mStart;
  uint8_t* mEnd;

  ClobberPatch(uint8_t* aStart, uint8_t* aEnd)
    : mStart(aStart), mEnd(aEnd)
  {}
};
static StaticInfallibleVector<ClobberPatch> gClobberPatches;

static void
AddClobberPatch(uint8_t* aStart, uint8_t* aEnd)
{
  if (aStart < aEnd) {
    gClobberPatches.emplaceBack(aStart, aEnd);
  }
}

static uint8_t*
SymbolBase(uint8_t* aPtr)
{
  Dl_info info;
  if (!dladdr(aPtr, &info)) {
    MOZ_CRASH();
  }
  return static_cast<uint8_t*>(info.dli_saddr);
}

// Use Udis86 to decode a single instruction, returning the number of bytes
// consumed.
static size_t
DecodeInstruction(uint8_t* aIp, ud_t* aUd)
{
  ud_init(aUd);
  ud_set_input_buffer(aUd, aIp, MaximumInstructionLength);
  ud_set_mode(aUd, 64);

  size_t nbytes = ud_decode(aUd);
  MOZ_RELEASE_ASSERT(nbytes && nbytes <= MaximumInstructionLength);
  return nbytes;
}

// If it is unsafe to patch new instructions into [aIpStart, aIpEnd> then
// return an instruction at which a new search can be started from.
static uint8_t*
MaybeInternalJumpTarget(uint8_t* aIpStart, uint8_t* aIpEnd)
{
  // The start and end have to be associated with the same symbol, as otherwise
  // a jump could come into the start of the later symbol.
  const char* startName = SymbolNameRaw(aIpStart);
  const char* endName = SymbolNameRaw(aIpEnd - 1);
  if (strcmp(startName, endName)) {
    return SymbolBase(aIpEnd - 1);
  }

  // Look for any internal jumps from outside the patch range into the middle
  // of the patch range.
  for (auto jump : gInternalJumps) {
    if (!(jump.first >= aIpStart && jump.first < aIpEnd) &&
        jump.second > aIpStart && jump.second < aIpEnd) {
      return jump.second;
    }
  }

  // Treat patched regions of code as if they had internal jumps.
  for (auto patch : gJumpPatches) {
    uint8_t* end = patch.mStart + (patch.mShort ? ShortJumpBytes : JumpBytesClobberRax);
    if (MemoryIntersects(aIpStart, aIpEnd - aIpStart, patch.mStart, end - patch.mStart)) {
      return end;
    }
  }
  for (auto patch : gClobberPatches) {
    if (MemoryIntersects(aIpStart, aIpEnd - aIpStart, patch.mStart, patch.mEnd - patch.mStart)) {
      return patch.mEnd;
    }
  }

  if ((size_t)(aIpEnd - aIpStart) > ShortJumpBytes) {
    // Manually annotate functions which might have backedges that interfere
    // with redirecting the initial bytes of the function. Ideally we would
    // find these backedges with some binary analysis, but this is easier said
    // than done, especially since there doesn't seem to be a standard way to
    // determine the extent of a symbol's code on OSX. Use strstr to avoid
    // issues with goo in the symbol names.
    if ((strstr(startName, "CTRunGetGlyphs") &&
         !strstr(startName, "CTRunGetGlyphsPtr")) ||
        (strstr(startName, "CTRunGetPositions") &&
         !strstr(startName, "CTRunGetPositionsPtr")) ||
        (strstr(startName, "CTRunGetStringIndices") &&
         !strstr(startName, "CTRunGetStringIndicesPtr")) ||
        strstr(startName, "CGColorSpaceCreateDeviceGray") ||
        strstr(startName, "CGColorSpaceCreateDeviceRGB") ||
        // For these functions, there is a syscall near the beginning which
        // other system threads might be inside.
        strstr(startName, "__workq_kernreturn") ||
        strstr(startName, "kevent64")) {
      return aIpEnd - 1;
    }
  }

  return nullptr;
}

// Any reasons why redirection failed.
static StaticInfallibleVector<char*> gRedirectFailures;

static void
RedirectFailure(const char* aFormat, ...)
{
  va_list ap;
  va_start(ap, aFormat);
  char buf[4096];
  VsprintfLiteral(buf, aFormat, ap);
  va_end(ap);
  gRedirectFailures.emplaceBack(strdup(buf));
}

static void
UnknownInstruction(const char* aName, uint8_t* aIp, size_t aNbytes)
{
  char buf[4096];
  char* ptr = buf;
  for (size_t i = 0; i < aNbytes; i++) {
    int written = snprintf(ptr, sizeof(buf) - (ptr - buf), " %d", (int) aIp[i]);
    ptr += written;
  }
  RedirectFailure("Unknown instruction in %s:%s", aName, buf);
}

// Try to emit instructions to |aAssembler| with equivalent behavior to any
// special jump or ip-dependent instruction at |aIp|, returning true if the
// instruction was copied.
static bool
CopySpecialInstruction(uint8_t* aIp, ud_t* aUd, size_t aNbytes, Assembler& aAssembler)
{
  aAssembler.NoteOriginalInstruction(aIp);

  if (aUd->pfx_seg) {
    return false;
  }

  ud_mnemonic_code mnemonic = ud_insn_mnemonic(aUd);
  if (mnemonic == UD_Icall || mnemonic == UD_Ijmp || (mnemonic >= UD_Ijo && mnemonic <= UD_Ijg)) {
    MOZ_RELEASE_ASSERT(!ud_insn_opr(aUd, 1));
    const ud_operand* op = ud_insn_opr(aUd, 0);
    if (op->type == UD_OP_JIMM) {
      // Call or jump relative to rip.
      uint8_t* target = aIp + aNbytes;
      switch (op->size) {
      case 8:  target += op->lval.sbyte;  break;
      case 32: target += op->lval.sdword; break;
      default: return false;
      }
      gInternalJumps.emplaceBack(nullptr, target);
      if (mnemonic == UD_Icall) {
        aAssembler.MoveImmediateToRax(target);
        aAssembler.CallRax();
      } else if (mnemonic == UD_Ijmp) {
        aAssembler.Jump(target);
      } else {
        aAssembler.ConditionalJump(aUd->primary_opcode, target);
      }
      return true;
    }
    if (op->type == UD_OP_MEM && op->base == UD_R_RIP && !op->index && op->offset == 32) {
      // jmp *$offset32(%rip)
      uint8_t* addr = aIp + aNbytes + op->lval.sdword;
      aAssembler.MoveImmediateToRax(addr);
      aAssembler.LoadRax(8);
      aAssembler.JumpToRax();
      return true;
    }
  }

  if (mnemonic == UD_Imov || mnemonic == UD_Ilea) {
    MOZ_RELEASE_ASSERT(!ud_insn_opr(aUd, 2));
    const ud_operand* dst = ud_insn_opr(aUd, 0);
    const ud_operand* src = ud_insn_opr(aUd, 1);
    if (dst->type == UD_OP_REG &&
        src->type == UD_OP_MEM && src->base == UD_R_RIP && !src->index && src->offset == 32) {
      // mov/lea $offset32(%rip), reg
      int reg = Assembler::NormalizeRegister(dst->base);
      if (!reg) {
        return false;
      }
      uint8_t* addr = aIp + aNbytes + src->lval.sdword;
      if (reg != UD_R_RAX) {
        aAssembler.PushRax();
      }
      aAssembler.MoveImmediateToRax(addr);
      if (mnemonic == UD_Imov) {
        aAssembler.LoadRax(src->size / 8);
      }
      if (reg != UD_R_RAX) {
        aAssembler.MoveRaxToRegister(reg);
        aAssembler.PopRax();
      }
      return true;
    }
    if (dst->type == UD_OP_MEM && dst->base == UD_R_RIP && !dst->index && dst->offset == 32 &&
        src->type == UD_OP_REG && mnemonic == UD_Imov) {
      // movl reg, $offset32(%rip)
      int reg = Assembler::NormalizeRegister(src->base);
      if (!reg) {
        return false;
      }
      uint8_t* addr = aIp + aNbytes + dst->lval.sdword;
      aAssembler.PushRax();
      aAssembler.PushRbx();
      aAssembler.MoveRegisterToRax(reg);
      aAssembler.PushRax();
      aAssembler.PopRbx();
      aAssembler.MoveImmediateToRax(addr);
      aAssembler.StoreRbxToRax(src->size / 8);
      aAssembler.PopRbx();
      aAssembler.PopRax();
      return true;
    }
  }

  if (mnemonic == UD_Icmp) {
    MOZ_RELEASE_ASSERT(!ud_insn_opr(aUd, 2));
    const ud_operand* dst = ud_insn_opr(aUd, 0);
    const ud_operand* src = ud_insn_opr(aUd, 1);
    if (dst->type == UD_OP_MEM && dst->base == UD_R_RIP && !dst->index && dst->offset == 32 &&
        src->type == UD_OP_IMM && src->size == 8) {
      // cmp $literal8, $offset32(%rip)
      uint8_t value = src->lval.ubyte;
      uint8_t* addr = aIp + aNbytes + dst->lval.sdword;
      aAssembler.PushRax();
      aAssembler.MoveImmediateToRax(addr);
      aAssembler.LoadRax(dst->size / 8);
      aAssembler.CompareValueWithRax(value, dst->size / 8);
      aAssembler.PopRax();
      return true;
    }
    if (dst->type == UD_OP_REG &&
        src->type == UD_OP_MEM && src->base == UD_R_RIP && !src->index && src->offset == 32) {
      // cmpq $offset32(%rip), reg
      int reg = Assembler::NormalizeRegister(dst->base);
      if (!reg) {
        return false;
      }
      uint8_t* addr = aIp + aNbytes + src->lval.sdword;
      aAssembler.PushRax();
      aAssembler.MoveRegisterToRax(reg);
      aAssembler.PushRax();
      aAssembler.MoveImmediateToRax(addr);
      aAssembler.LoadRax(8);
      aAssembler.CompareRaxWithTopOfStack();
      aAssembler.PopRax();
      aAssembler.PopRax();
      return true;
    }
  }

  if (mnemonic == UD_Ixchg) {
    const ud_operand* dst = ud_insn_opr(aUd, 0);
    const ud_operand* src = ud_insn_opr(aUd, 1);
    if (src->type == UD_OP_REG && src->size == 8 &&
        dst->type == UD_OP_MEM && dst->base == UD_R_RIP && !dst->index && dst->offset == 32) {
      // xchgb reg, $offset32(%rip)
      int reg = Assembler::NormalizeRegister(src->base);
      if (!reg) {
        return false;
      }
      uint8_t* addr = aIp + aNbytes + dst->lval.sdword;
      if (reg == UD_R_RBX) {
        aAssembler.PushRax();
        aAssembler.MoveImmediateToRax(addr);
        aAssembler.ExchangeByteRbxWithAddressAtRax();
        aAssembler.PopRax();
      } else {
        aAssembler.PushRbx();
        aAssembler.PushRax();
        aAssembler.MoveImmediateToRax(addr);
        aAssembler.MoveRaxToRegister(UD_R_RBX);
        aAssembler.PopRax();
        aAssembler.ExchangeByteRegisterWithAddressAtRbx(reg);
        aAssembler.PopRbx();
      }
      return true;
    }
  }

  return false;
}

// Copy an instruction to aAssembler, returning the number of bytes used by the
// instruction.
static size_t
CopyInstruction(const char* aName, uint8_t* aIp, Assembler& aAssembler)
{
  // Use Udis86 to decode a single instruction.
  ud_t ud;
  size_t nbytes = DecodeInstruction(aIp, &ud);

  // Check for a special cased instruction.
  if (CopySpecialInstruction(aIp, &ud, nbytes, aAssembler)) {
    return nbytes;
  }

  // Don't copy call and jump instructions. We should have special cased these,
  // and these may not behave correctly after a naive copy if their behavior is
  // relative to the instruction pointer.
  ud_mnemonic_code_t mnemonic = ud_insn_mnemonic(&ud);
  if (mnemonic == UD_Icall || (mnemonic >= UD_Ijo && mnemonic <= UD_Ijmp)) {
    UnknownInstruction(aName, aIp, nbytes);
    return nbytes;
  }

  // Don't copy instructions which have the instruction pointer as an operand.
  // We should have special cased these, and as above these will not behave
  // correctly after being naively copied due to their dependence on the
  // instruction pointer.
  for (size_t i = 0;; i++) {
    const ud_operand_t* op = ud_insn_opr(&ud, i);
    if (!op) {
      break;
    }
    switch (op->type) {
    case UD_OP_MEM:
      if (op->index == UD_R_RIP) {
        UnknownInstruction(aName, aIp, nbytes);
        return nbytes;
      }
      MOZ_FALLTHROUGH;
    case UD_OP_REG:
      if (op->base == UD_R_RIP) {
        UnknownInstruction(aName, aIp, nbytes);
        return nbytes;
      }
      break;
    default:
      break;
    }
  }

  aAssembler.CopyInstruction(aIp, nbytes);
  return nbytes;
}

// Copy all instructions containing bytes in the range [aIpStart, aIpEnd) to
// the given assembler, returning the address of the first instruction not
// copied (i.e. the fallthrough instruction from the copied range).
static uint8_t*
CopyInstructions(const char* aName, uint8_t* aIpStart, uint8_t* aIpEnd,
                 Assembler& aAssembler)
{
  MOZ_RELEASE_ASSERT(!MaybeInternalJumpTarget(aIpStart, aIpEnd));

  uint8_t* ip = aIpStart;
  while (ip < aIpEnd) {
    ip += CopyInstruction(aName, ip, aAssembler);
  }
  return ip;
}

// Get the instruction pointer to use as the address of the base function for a
// redirection.
static uint8_t*
FunctionStartAddress(Redirection& aRedirection)
{
  uint8_t* addr = static_cast<uint8_t*>(dlsym(RTLD_DEFAULT, aRedirection.mName));
  if (!addr)
    return nullptr;

  if (addr[0] == 0xFF && addr[1] == 0x25) {
    return *(uint8_t**)(addr + 6 + *reinterpret_cast<int32_t*>(addr + 2));
  }

  return addr;
}

// Generate code to set %rax and enter RecordReplayRedirectCall.
static uint8_t*
GenerateRedirectStub(Assembler& aAssembler, size_t aCallId)
{
  uint8_t* newFunction = aAssembler.Current();
  aAssembler.MoveImmediateToRax((void*) aCallId);
  aAssembler.Jump(BitwiseCast<void*>(RecordReplayRedirectCall));
  return newFunction;
}

// Setup a redirection: overwrite the machine code for its base function, and
// fill in its original function, to satisfy the function pointer behaviors
// described in the Redirection structure. aCursor and aCursorEnd are used to
// allocate executable memory for use in the redirection.
static void
Redirect(size_t aCallId, Redirection& aRedirection, Assembler& aAssembler, bool aFirstPass)
{
  // The patching we do here might fail: it isn't possible to redirect an
  // arbitrary instruction pointer within an arbitrary block of code. This code
  // is doing a best effort sort of thing, and on failure it will crash safely.
  // The main thing we want to avoid is corrupting the code so that it has been
  // redirected but might crash or behave incorrectly when executed.
  uint8_t* functionStart = aRedirection.mBaseFunction;
  uint8_t* ro = functionStart;

  if (!functionStart) {
    if (aFirstPass) {
      PrintSpew("Could not find symbol %s for redirecting.\n", aRedirection.mName);
    }
    return;
  }

  if (aRedirection.mOriginalFunction != aRedirection.mBaseFunction) {
    // We already redirected this function.
    MOZ_RELEASE_ASSERT(!aFirstPass);
    return;
  }

  // First, see if we can overwrite JumpBytesClobberRax bytes of instructions
  // at the base function with a direct jump to the new function. Rax is never
  // live at the start of a function and we can emit a jump to an arbitrary
  // location with fewer instruction bytes on x64 if we clobber it.
  //
  // This will work if there are no extraneous jump targets within the region
  // of memory we are overwriting. If there are, we will corrupt the behavior
  // of those jumps if we patch the memory.
  uint8_t* extent = ro + JumpBytesClobberRax;
  if (!MaybeInternalJumpTarget(ro, extent)) {
    // Given code instructions for the base function as follows (AA are
    // instructions we will end up copying, -- are instructions that will never
    // be inspected or modified):
    //
    // base function: AA--
    //
    // Transform the code into:
    //
    // base function: J0--
    // generated code: AAJ1
    //
    // Where J0 jumps to the new function, the original function is at AA, and
    // J1 jumps to the point after J0.

    // Set the new function to the start of the generated code.
    aRedirection.mOriginalFunction = aAssembler.Current();

    // Copy AA into generated code.
    ro = CopyInstructions(aRedirection.mName, ro, extent, aAssembler);

    // Emit jump J1.
    aAssembler.Jump(ro);

    // Emit jump J0.
    uint8_t* newFunction = GenerateRedirectStub(aAssembler, aCallId);
    AddJumpPatch(functionStart, newFunction, /* aShort = */ false);
    AddClobberPatch(functionStart + JumpBytesClobberRax, ro);
    return;
  }

  // We don't have enough space to patch in a long jump to an arbitrary
  // instruction. Attempt to find another region of code that is long enough
  // for two long jumps, has no internal jump targets, and is within range of
  // the base function for a short jump.
  //
  // Given code instructions for the base function, with formatting as above:
  //
  // base function: AA--BBBB--
  //
  // Transform the code into:
  //
  // base function: J0--J1J2--
  // generated code: AAJ3 BBBBJ4
  //
  // With the original function at AA, the jump targets are as follows:
  //
  // J0: short jump to J2
  // J1: jump to BBBB
  // J2: jump to the new function
  // J3: jump to the point after J0
  // J4: jump to the point after J2

  // Skip this during the first pass, we don't want to patch a jump in over the
  // initial bytes of a function we haven't redirected yet.
  if (aFirstPass) {
    return;
  }

  // The original symbol must have enough bytes to insert a short jump.
  MOZ_RELEASE_ASSERT(!MaybeInternalJumpTarget(ro, ro + ShortJumpBytes));

  // Copy AA into generated code.
  aRedirection.mOriginalFunction = aAssembler.Current();
  uint8_t* nro = CopyInstructions(aRedirection.mName, ro, ro + ShortJumpBytes, aAssembler);

  // Emit jump J3.
  aAssembler.Jump(nro);

  // Keep advancing the instruction pointer until we get to a region that is
  // large enough for two long jump patches.
  ro = SymbolBase(extent);
  while (true) {
    extent = ro + JumpBytesClobberRax * 2;
    uint8_t* target = MaybeInternalJumpTarget(ro, extent);
    if (target) {
      ro = target;
      continue;
    }
    break;
  }

  // Copy BBBB into generated code.
  uint8_t* firstJumpTarget = aAssembler.Current();
  uint8_t* afterip = CopyInstructions(aRedirection.mName, ro, extent, aAssembler);

  // Emit jump J4.
  aAssembler.Jump(afterip);

  // Emit jump J1.
  AddJumpPatch(ro, firstJumpTarget, /* aShort = */ false);

  // Emit jump J2.
  uint8_t* newFunction = GenerateRedirectStub(aAssembler, aCallId);
  AddJumpPatch(ro + JumpBytesClobberRax, newFunction, /* aShort = */ false);
  AddClobberPatch(ro + 2 * JumpBytesClobberRax, afterip);

  // Emit jump J0.
  AddJumpPatch(functionStart, ro + JumpBytesClobberRax, /* aShort = */ true);
  AddClobberPatch(functionStart + ShortJumpBytes, nro);
}

void
EarlyInitializeRedirections()
{
  for (size_t i = 0;; i++) {
    Redirection& redirection = gRedirections[i];
    if (!redirection.mName) {
      break;
    }
    MOZ_ASSERT(!redirection.mBaseFunction);
    MOZ_ASSERT(!redirection.mOriginalFunction);

    redirection.mBaseFunction = FunctionStartAddress(redirection);
    redirection.mOriginalFunction = redirection.mBaseFunction;

    if (redirection.mBaseFunction && IsRecordingOrReplaying()) {
      // We will get confused if we try to redirect the same address in multiple places.
      for (size_t j = 0; j < i; j++) {
        if (gRedirections[j].mBaseFunction == redirection.mBaseFunction) {
          PrintSpew("Redirection %s shares the same address as %s, skipping.\n",
                    redirection.mName, gRedirections[j].mName);
          redirection.mBaseFunction = nullptr;
          break;
        }
      }
    }
  }
}

bool
InitializeRedirections()
{
  MOZ_ASSERT(IsRecordingOrReplaying());

  {
    Assembler assembler;

    for (size_t i = 0;; i++) {
      Redirection& redirection = gRedirections[i];
      if (!redirection.mName) {
        break;
      }
      Redirect(i, redirection, assembler, /* aFirstPass = */ true);
    }

    for (size_t i = 0;; i++) {
      Redirection& redirection = gRedirections[i];
      if (!redirection.mName) {
        break;
      }
      Redirect(i, redirection, assembler, /* aFirstPass = */ false);
    }
  }

  // Don't install redirections if we had any failures.
  if (!gRedirectFailures.empty()) {
    size_t len = 4096;
    gInitializationFailureMessage = new char[4096];
    gInitializationFailureMessage[--len] = 0;

    char* ptr = gInitializationFailureMessage;
    for (char* reason : gRedirectFailures) {
      size_t n = snprintf(ptr, len, "%s\n", reason);
      if (n >= len) {
        break;
      }
      ptr += n;
      len -= n;
    }

    return false;
  }

  // Remove write protection from all patched regions, so that we don't call
  // into the system while we are in the middle of redirecting.
  for (const JumpPatch& patch : gJumpPatches) {
    UnprotectExecutableMemory(patch.mStart, patch.mShort ? ShortJumpBytes : JumpBytesClobberRax);
  }
  for (const ClobberPatch& patch : gClobberPatches) {
    UnprotectExecutableMemory(patch.mStart, patch.mEnd - patch.mStart);
  }

  // Do the actual patching of executable code for the functions we are
  // redirecting.

  for (const JumpPatch& patch : gJumpPatches) {
    if (patch.mShort) {
      Assembler::PatchShortJump(patch.mStart, patch.mTarget);
    } else {
      Assembler::PatchJumpClobberRax(patch.mStart, patch.mTarget);
    }
  }

  for (const ClobberPatch& patch : gClobberPatches) {
    for (uint8_t* ip = patch.mStart; ip < patch.mEnd; ip++) {
      Assembler::PatchClobber(ip);
    }
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////////////////////////////

Atomic<size_t, SequentiallyConsistent, Behavior::DontPreserve> gMemoryLeakBytes;

void*
BindFunctionArgument(void* aFunction, void* aArgument, size_t aArgumentPosition,
                     Assembler& aAssembler)
{
  void* res = aAssembler.Current();

  // On x64 the argument will be in a register, so to add an extra argument for
  // the callee we just need to fill in the appropriate register for the
  // argument position with the bound argument value.
  aAssembler.MoveImmediateToRax(aArgument);

  switch (aArgumentPosition) {
  case 1: aAssembler.MoveRaxToRegister(UD_R_RSI); break;
  case 2: aAssembler.MoveRaxToRegister(UD_R_RDX); break;
  case 3: aAssembler.MoveRaxToRegister(UD_R_RCX); break;
  default: MOZ_CRASH();
  }

  // Jump to the function that was bound.
  aAssembler.Jump(aFunction);

  return res;
}

} // namespace recordreplay
} // namespace mozilla
