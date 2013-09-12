/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This is an implementation of stack unwinding according to a subset
 * of the ARM Exception Handling ABI, as described in:
 *   http://infocenter.arm.com/help/topic/com.arm.doc.ihi0038a/IHI0038A_ehabi.pdf
 *
 * This handles only the ARM-defined "personality routines" (chapter
 * 9), and don't track the value of FP registers, because profiling
 * needs only chain of PC/SP values.
 *
 * Because the exception handling info may not be accurate for all
 * possible places where an async signal could occur (e.g., in a
 * prologue or epilogue), this bounds-checks all stack accesses.
 *
 * This file uses "struct" for structures in the exception tables and
 * "class" otherwise.  We should avoid violating the C++11
 * standard-layout rules in the former.
 */

#include "EHABIStackWalk.h"

#include "shared-libraries.h"
#include "platform.h"

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Endian.h"

#include <algorithm>
#include <elf.h>
#include <stdint.h>
#include <vector>
#include <string>

#ifndef PT_ARM_EXIDX
#define PT_ARM_EXIDX 0x70000001
#endif


namespace mozilla {

struct EHEntry;

class EHState {
  // Note that any core register can be used as a "frame pointer" to
  // influence the unwinding process, so this must track all of them.
  uint32_t mRegs[16];
public:
  bool unwind(const EHEntry *aEntry, const void *stackBase);
  uint32_t &operator[](int i) { return mRegs[i]; }
  const uint32_t &operator[](int i) const { return mRegs[i]; }
  EHState(const mcontext_t &);
};

enum {
  R_SP = 13,
  R_LR = 14,
  R_PC = 15
};

class EHEntryHandle {
  const EHEntry *mValue;
public:
  EHEntryHandle(const EHEntry *aEntry) : mValue(aEntry) { }
  const EHEntry *value() const { return mValue; }
};

class EHTable {
  uint32_t mStartPC;
  uint32_t mEndPC;
  uint32_t mLoadOffset;
  // In principle we should be able to binary-search the index section in
  // place, but the ICS toolchain's linker is noncompliant and produces
  // indices that aren't entirely sorted (e.g., libc).  So we have this:
  std::vector<EHEntryHandle> mEntries;
  std::string mName;
public:
  EHTable(const void *aELF, size_t aSize, const std::string &aName);
  const EHEntry *lookup(uint32_t aPC) const;
  bool isValid() const { return mEntries.size() > 0; }
  const std::string &name() const { return mName; }
  uint32_t startPC() const { return mStartPC; }
  uint32_t endPC() const { return mEndPC; }
  uint32_t loadOffset() const { return mLoadOffset; }
};

class EHAddrSpace {
  std::vector<uint32_t> mStarts;
  std::vector<EHTable> mTables;
  static mozilla::Atomic<const EHAddrSpace*> sCurrent;
public:
  explicit EHAddrSpace(const std::vector<EHTable>& aTables);
  const EHTable *lookup(uint32_t aPC) const;
  static void Update();
  static const EHAddrSpace *Get();
};


void EHABIStackWalkInit()
{
  EHAddrSpace::Update();
}

size_t EHABIStackWalk(const mcontext_t &aContext, void *stackBase,
                      void **aSPs, void **aPCs, const size_t aNumFrames)
{
  const EHAddrSpace *space = EHAddrSpace::Get();
  EHState state(aContext);
  size_t count = 0;

  while (count < aNumFrames) {
    uint32_t pc = state[R_PC], sp = state[R_SP];
    aPCs[count] = reinterpret_cast<void *>(pc);
    aSPs[count] = reinterpret_cast<void *>(sp);
    count++;

    if (!space)
      break;
    // TODO: cache these lookups.  Binary-searching libxul is
    // expensive (possibly more expensive than doing the actual
    // unwind), and even a small cache should help.
    const EHTable *table = space->lookup(pc);
    if (!table)
      break;
    const EHEntry *entry = table->lookup(pc);
    if (!entry)
      break;
    if (!state.unwind(entry, stackBase))
      break;
  }
  
  return count;
}


struct PRel31 {
  uint32_t mBits;
  bool topBit() const { return mBits & 0x80000000; }
  uint32_t value() const { return mBits & 0x7fffffff; }
  int32_t offset() const { return (static_cast<int32_t>(mBits) << 1) >> 1; }
  const void *compute() const {
    return reinterpret_cast<const char *>(this) + offset();
  }
private:
  PRel31(const PRel31 &copied) MOZ_DELETE;
  PRel31() MOZ_DELETE;
};

struct EHEntry {
  PRel31 startPC;
  PRel31 exidx;
private:
  EHEntry(const EHEntry &copied) MOZ_DELETE;
  EHEntry() MOZ_DELETE;
};


class EHInterp {
public:
  EHInterp(EHState &aState, const EHEntry *aEntry,
           uint32_t aStackLimit, uint32_t aStackBase)
    : mState(aState),
      mStackLimit(aStackLimit),
      mStackBase(aStackBase),
      mNextWord(0),
      mWordsLeft(0),
      mFailed(false)
  {
    const PRel31 &exidx = aEntry->exidx;
    uint32_t firstWord;

    if (exidx.mBits == 1) {  // EXIDX_CANTUNWIND
      mFailed = true;
      return;
    }
    if (exidx.topBit()) {
      firstWord = exidx.mBits;
    } else {
      mNextWord = reinterpret_cast<const uint32_t *>(exidx.compute());
      firstWord = *mNextWord++;
    }

    switch (firstWord >> 24) {
    case 0x80: // short
      mWord = firstWord << 8;
      mBytesLeft = 3;
      break;
    case 0x81: case 0x82: // long; catch descriptor size ignored
      mWord = firstWord << 16;
      mBytesLeft = 2;
      mWordsLeft = (firstWord >> 16) & 0xff;
      break;
    default:
      // unknown personality
      mFailed = true;
    }
  }

  bool unwind();

private:
  // TODO: GCC has been observed not CSEing repeated reads of
  // mState[R_SP] with writes to mFailed between them, suggesting that
  // it hasn't determined that they can't alias and is thus missing
  // optimization opportunities.  So, we may want to flatten EHState
  // into this class; this may also make the code simpler.
  EHState &mState;
  uint32_t mStackLimit;
  uint32_t mStackBase;
  const uint32_t *mNextWord;
  uint32_t mWord;
  uint8_t mWordsLeft;
  uint8_t mBytesLeft;
  bool mFailed;

  enum {
    I_ADDSP    = 0x00, // 0sxxxxxx (subtract if s)
    M_ADDSP    = 0x80,
    I_POPMASK  = 0x80, // 1000iiii iiiiiiii (if any i set)
    M_POPMASK  = 0xf0,
    I_MOVSP    = 0x90, // 1001nnnn
    M_MOVSP    = 0xf0,
    I_POPN     = 0xa0, // 1010lnnn
    M_POPN     = 0xf0,
    I_FINISH   = 0xb0, // 10110000
    I_POPLO    = 0xb1, // 10110001 0000iiii (if any i set)
    I_ADDSPBIG = 0xb2, // 10110010 uleb128
    I_POPFDX   = 0xb3, // 10110011 sssscccc
    I_POPFDX8  = 0xb8, // 10111nnn
    M_POPFDX8  = 0xf8,
    // "Intel Wireless MMX" extensions omitted.
    I_POPFDD   = 0xc8, // 1100100h sssscccc
    M_POPFDD   = 0xfe,
    I_POPFDD8  = 0xd0, // 11010nnn
    M_POPFDD8  = 0xf8
  };

  uint8_t next() {
    if (mBytesLeft == 0) {
      if (mWordsLeft == 0) {
        return I_FINISH;
      }
      mWordsLeft--;
      mWord = *mNextWord++;
      mBytesLeft = 4;
    }
    mBytesLeft--;
    mWord = (mWord << 8) | (mWord >> 24); // rotate
    return mWord;
  }

  uint32_t &vSP() { return mState[R_SP]; }
  uint32_t *ptrSP() { return reinterpret_cast<uint32_t *>(vSP()); }

  void checkStackBase() { if (vSP() > mStackBase) mFailed = true; }
  void checkStackLimit() { if (vSP() <= mStackLimit) mFailed = true; }
  void checkStackAlign() { if ((vSP() & 3) != 0) mFailed = true; }
  void checkStack() {
    checkStackBase();
    checkStackLimit();
    checkStackAlign();
  }

  void popRange(uint8_t first, uint8_t last, uint16_t mask) {
    bool hasSP = false;
    uint32_t tmpSP;
    if (mask == 0)
      mFailed = true;
    for (uint8_t r = first; r <= last; ++r) {
      if (mask & 1) {
        if (r == R_SP) {
          hasSP = true;
          tmpSP = *ptrSP();
        } else
          mState[r] = *ptrSP();
        vSP() += 4;
        checkStackBase();
        if (mFailed)
          return;
      }
      mask >>= 1;
    }
    if (hasSP) {
      vSP() = tmpSP;
      checkStack();
    }
  }
};


bool EHState::unwind(const EHEntry *aEntry, const void *stackLimit) {
  EHInterp interp(*this, aEntry, mRegs[R_SP] - 4,
                  reinterpret_cast<uint32_t>(stackLimit));

  return interp.unwind();
}

bool EHInterp::unwind() {
  mState[R_PC] = 0;
  checkStack();
  while (!mFailed) {
    uint8_t insn = next();
#if DEBUG_EHABI_UNWIND
    LOGF("unwind insn = %02x", (unsigned)insn);
#endif
    // Try to put the common cases first.

    // 00xxxxxx: vsp = vsp + (xxxxxx << 2) + 4
    // 01xxxxxx: vsp = vsp - (xxxxxx << 2) - 4
    if ((insn & M_ADDSP) == I_ADDSP) {
      uint32_t offset = ((insn & 0x3f) << 2) + 4;
      if (insn & 0x40) {
        vSP() -= offset;
        checkStackLimit();
      } else {
        vSP() += offset;
        checkStackBase();
      }
      continue;
    }

    // 10100nnn: Pop r4-r[4+nnn]
    // 10101nnn: Pop r4-r[4+nnn], r14
    if ((insn & M_POPN) == I_POPN) {
      uint8_t n = (insn & 0x07) + 1;
      bool lr = insn & 0x08;
      uint32_t *ptr = ptrSP();
      vSP() += (n + (lr ? 1 : 0)) * 4;
      checkStackBase();
      for (uint8_t r = 4; r < 4 + n; ++r)
        mState[r] = *ptr++;
      if (lr)
        mState[R_LR] = *ptr++;
      continue;
    }

    // 1011000: Finish
    if (insn == I_FINISH) {
      if (mState[R_PC] == 0)
        mState[R_PC] = mState[R_LR];
      return true;
    }

    // 1001nnnn: Set vsp = r[nnnn]
    if ((insn & M_MOVSP) == I_MOVSP) {
      vSP() = mState[insn & 0x0f];
      checkStack();
      continue;
    }

    // 11001000 sssscccc: Pop VFP regs D[16+ssss]-D[16+ssss+cccc] (as FLDMFDD)
    // 11001001 sssscccc: Pop VFP regs D[ssss]-D[ssss+cccc] (as FLDMFDD)
    if ((insn & M_POPFDD) == I_POPFDD) {
      uint8_t n = (next() & 0x0f) + 1;
      // Note: if the 16+ssss+cccc > 31, the encoding is reserved.
      // As the space is currently unused, we don't try to check.
      vSP() += 8 * n;
      checkStackBase();
      continue;
    }

    // 11010nnn: Pop VFP regs D[8]-D[8+nnn] (as FLDMFDD)
    if ((insn & M_POPFDD8) == I_POPFDD8) {
      uint8_t n = (insn & 0x07) + 1;
      vSP() += 8 * n;
      checkStackBase();
      continue;
    }

    // 10110010 uleb128: vsp = vsp + 0x204 + (uleb128 << 2)
    if (insn == I_ADDSPBIG) {
      uint32_t acc = 0;
      uint8_t shift = 0;
      uint8_t byte;
      do {
        if (shift >= 32)
          return false;
        byte = next();
        acc |= (byte & 0x7f) << shift;
        shift += 7;
      } while (byte & 0x80);
      uint32_t offset = 0x204 + (acc << 2);
      // The calculations above could have overflowed.
      // But the one we care about is this:
      if (vSP() + offset < vSP())
        mFailed = true;
      vSP() += offset;
      // ...so that this is the only other check needed:
      checkStackBase();
      continue;
    }

    // 1000iiii iiiiiiii (i not all 0): Pop under masks {r15-r12}, {r11-r4}
    if ((insn & M_POPMASK) == I_POPMASK) {
      popRange(4, 15, ((insn & 0x0f) << 8) | next());
      continue;
    }

    // 1011001 0000iiii (i not all 0): Pop under mask {r3-r0}
    if (insn == I_POPLO) {
      popRange(0, 3, next() & 0x0f);
      continue;
    }

    // 10110011 sssscccc: Pop VFP regs D[ssss]-D[ssss+cccc] (as FLDMFDX)
    if (insn == I_POPFDX) {
      uint8_t n = (next() & 0x0f) + 1;
      vSP() += 8 * n + 4;
      checkStackBase();
      continue;
    }

    // 10111nnn: Pop VFP regs D[8]-D[8+nnn] (as FLDMFDX)
    if ((insn & M_POPFDX8) == I_POPFDX8) {
      uint8_t n = (insn & 0x07) + 1;
      vSP() += 8 * n + 4;
      checkStackBase();
      continue;
    }

    // unhandled instruction
#ifdef DEBUG_EHABI_UNWIND
    LOGF("Unhandled EHABI instruction 0x%02x", insn);
#endif
    mFailed = true;
  }
  return false;
}


bool operator<(const EHTable &lhs, const EHTable &rhs) {
  return lhs.startPC() < rhs.endPC();
}

// Async signal unsafe.
EHAddrSpace::EHAddrSpace(const std::vector<EHTable>& aTables)
  : mTables(aTables)
{
  std::sort(mTables.begin(), mTables.end());
  DebugOnly<uint32_t> lastEnd = 0;
  for (std::vector<EHTable>::iterator i = mTables.begin();
       i != mTables.end(); ++i) {
    MOZ_ASSERT(i->startPC() >= lastEnd);
    mStarts.push_back(i->startPC());
    lastEnd = i->endPC();
  }
}

const EHTable *EHAddrSpace::lookup(uint32_t aPC) const {
  ptrdiff_t i = (std::upper_bound(mStarts.begin(), mStarts.end(), aPC)
                 - mStarts.begin()) - 1;

  if (i < 0 || aPC >= mTables[i].endPC())
    return 0;
  return &mTables[i];
}


bool operator<(const EHEntryHandle &lhs, const EHEntryHandle &rhs) {
  return lhs.value()->startPC.compute() < rhs.value()->startPC.compute();
}

const EHEntry *EHTable::lookup(uint32_t aPC) const {
  MOZ_ASSERT(aPC >= mStartPC);
  if (aPC >= mEndPC)
    return NULL;

  std::vector<EHEntryHandle>::const_iterator begin = mEntries.begin();
  std::vector<EHEntryHandle>::const_iterator end = mEntries.end();
  MOZ_ASSERT(begin < end);
  if (aPC < reinterpret_cast<uint32_t>(begin->value()->startPC.compute()))
    return NULL;

  while (end - begin > 1) {
    std::vector<EHEntryHandle>::const_iterator mid = begin + (end - begin) / 2;
    if (aPC < reinterpret_cast<uint32_t>(mid->value()->startPC.compute()))
      end = mid;
    else
      begin = mid;
  }
  return begin->value();
}


#if MOZ_LITTLE_ENDIAN
static const unsigned char hostEndian = ELFDATA2LSB;
#elif MOZ_BIG_ENDIAN
static const unsigned char hostEndian = ELFDATA2MSB;
#else
#error "No endian?"
#endif

// Async signal unsafe.  (Note use of std::vector::reserve.)
EHTable::EHTable(const void *aELF, size_t aSize, const std::string &aName)
  : mStartPC(~0), // largest uint32_t
    mEndPC(0),
    mName(aName)
{
  const uint32_t base = reinterpret_cast<uint32_t>(aELF);

  if (aSize < sizeof(Elf32_Ehdr))
    return;

  const Elf32_Ehdr &file = *(reinterpret_cast<Elf32_Ehdr *>(base));
  if (memcmp(&file.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
      file.e_ident[EI_CLASS] != ELFCLASS32 ||
      file.e_ident[EI_DATA] != hostEndian ||
      file.e_ident[EI_VERSION] != EV_CURRENT ||
      file.e_ident[EI_OSABI] != ELFOSABI_SYSV ||
      file.e_ident[EI_ABIVERSION] != 0 ||
      file.e_machine != EM_ARM ||
      file.e_version != EV_CURRENT)
    // e_flags?
    return;

  MOZ_ASSERT(file.e_phoff + file.e_phnum * file.e_phentsize <= aSize);
  const Elf32_Phdr *exidxHdr = 0, *zeroHdr = 0;
  for (unsigned i = 0; i < file.e_phnum; ++i) {
    const Elf32_Phdr &phdr =
      *(reinterpret_cast<Elf32_Phdr *>(base + file.e_phoff
                                       + i * file.e_phentsize));
    if (phdr.p_type == PT_ARM_EXIDX) {
      exidxHdr = &phdr;
    } else if (phdr.p_type == PT_LOAD) {
      if (phdr.p_offset == 0) {
        zeroHdr = &phdr;
      }
      if (phdr.p_flags & PF_X) {
        mStartPC = std::min(mStartPC, phdr.p_vaddr);
        mEndPC = std::max(mEndPC, phdr.p_vaddr + phdr.p_memsz);
      }
    }
  }
  if (!exidxHdr)
    return;
  if (!zeroHdr)
    return;
  mLoadOffset = base - zeroHdr->p_vaddr;
  mStartPC += mLoadOffset;
  mEndPC += mLoadOffset;

  // Create a sorted index of the index to work around linker bugs.
  const EHEntry *startTable =
    reinterpret_cast<const EHEntry *>(mLoadOffset + exidxHdr->p_vaddr);
  const EHEntry *endTable =
    reinterpret_cast<const EHEntry *>(mLoadOffset + exidxHdr->p_vaddr
                                    + exidxHdr->p_memsz);
  mEntries.reserve(endTable - startTable);
  for (const EHEntry *i = startTable; i < endTable; ++i)
    mEntries.push_back(i);
  std::sort(mEntries.begin(), mEntries.end());
}


mozilla::Atomic<const EHAddrSpace*> EHAddrSpace::sCurrent(nullptr);

// Async signal safe; can fail if Update() hasn't returned yet.
const EHAddrSpace *EHAddrSpace::Get() {
  return sCurrent;
}

// Collect unwinding information from loaded objects.  Calls after the
// first have no effect.  Async signal unsafe.
void EHAddrSpace::Update() {
  const EHAddrSpace *space = sCurrent;
  if (space)
    return;

  SharedLibraryInfo info = SharedLibraryInfo::GetInfoForSelf();
  std::vector<EHTable> tables;

  for (size_t i = 0; i < info.GetSize(); ++i) {
    const SharedLibrary &lib = info.GetEntry(i);
    if (lib.GetOffset() != 0)
      // TODO: if it has a name, and we haven't seen a mapping of
      // offset 0 for that file, try opening it and reading the
      // headers instead.  The only thing I've seen so far that's
      // linked so as to need that treatment is the dynamic linker
      // itself.
      continue;
    EHTable tab(reinterpret_cast<const void *>(lib.GetStart()),
              lib.GetEnd() - lib.GetStart(), lib.GetName());
    if (tab.isValid())
      tables.push_back(tab);
  }
  space = new EHAddrSpace(tables);

  if (!sCurrent.compareExchange(nullptr, space)) {
    delete space;
    space = sCurrent;
  }
}


EHState::EHState(const mcontext_t &context) {
#ifdef linux
  mRegs[0] = context.arm_r0;
  mRegs[1] = context.arm_r1;
  mRegs[2] = context.arm_r2;
  mRegs[3] = context.arm_r3;
  mRegs[4] = context.arm_r4;
  mRegs[5] = context.arm_r5;
  mRegs[6] = context.arm_r6;
  mRegs[7] = context.arm_r7;
  mRegs[8] = context.arm_r8;
  mRegs[9] = context.arm_r9;
  mRegs[10] = context.arm_r10;
  mRegs[11] = context.arm_fp;
  mRegs[12] = context.arm_ip;
  mRegs[13] = context.arm_sp;
  mRegs[14] = context.arm_lr;
  mRegs[15] = context.arm_pc;
#else
# error "Unhandled OS for ARM EHABI unwinding"
#endif
}

} // namespace mozilla

