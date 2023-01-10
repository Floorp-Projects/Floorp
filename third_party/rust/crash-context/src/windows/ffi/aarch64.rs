// Copyright 2019 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#[derive(Copy, Clone)]
// Note this is intentionally using natural alignment
#[repr(C)]
pub union ARM64_NT_NEON128 {
    pub Anonymous: ARM64_NT_NEON128_0,
    pub D: [f64; 2],
    pub S: [f32; 4],
    pub H: [u16; 8],
    pub B: [u8; 16],
}

#[derive(Copy, Clone)]
// Note this is intentionally using natural alignment
#[repr(C)]
pub struct ARM64_NT_NEON128_0 {
    pub Low: u64,
    pub High: i64,
}

#[derive(Copy, Clone)]
#[repr(C, align(16))]
pub struct CONTEXT {
    pub ContextFlags: u32,
    pub Cpsr: u32,
    pub Anonymous: CONTEXT_0,
    pub Sp: u64,
    pub Pc: u64,
    pub V: [ARM64_NT_NEON128; 32],
    pub Fpcr: u32,
    pub Fpsr: u32,
    pub Bcr: [u32; 8],
    pub Bvr: [u64; 8],
    pub Wcr: [u32; 2],
    pub Wvr: [u64; 2],
}

#[derive(Copy, Clone)]
#[repr(C, align(16))]
pub union CONTEXT_0 {
    pub Anonymous: CONTEXT_0_0,
    pub X: [u64; 31],
}

#[derive(Copy, Clone)]
#[repr(C, align(16))]
pub struct CONTEXT_0_0 {
    pub X0: u64,
    pub X1: u64,
    pub X2: u64,
    pub X3: u64,
    pub X4: u64,
    pub X5: u64,
    pub X6: u64,
    pub X7: u64,
    pub X8: u64,
    pub X9: u64,
    pub X10: u64,
    pub X11: u64,
    pub X12: u64,
    pub X13: u64,
    pub X14: u64,
    pub X15: u64,
    pub X16: u64,
    pub X17: u64,
    pub X18: u64,
    pub X19: u64,
    pub X20: u64,
    pub X21: u64,
    pub X22: u64,
    pub X23: u64,
    pub X24: u64,
    pub X25: u64,
    pub X26: u64,
    pub X27: u64,
    pub X28: u64,
    pub Fp: u64,
    pub Lr: u64,
}

std::arch::global_asm! {
  ".text",
  ".global capture_context",
"capture_context:",
  // Save general purpose registers in context.regs[i].
  // The original x0 can't be recovered.
  "stp x0, x1, [x0, #0x008]",
  "stp x2, x3, [x0, #0x018]",
  "stp x4, x5, [x0, #0x028]",
  "stp x6, x7, [x0, #0x038]",
  "stp x8, x9, [x0, #0x048]",
  "stp x10, x11, [x0, #0x058]",
  "stp x12, x13, [x0, #0x068]",
  "stp x14, x15, [x0, #0x078]",
  "stp x16, x17, [x0, #0x088]",
  "stp x18, x19, [x0, #0x098]",
  "stp x20, x21, [x0, #0x0a8]",
  "stp x22, x23, [x0, #0x0b8]",
  "stp x24, x25, [x0, #0x0c8]",
  "stp x26, x27, [x0, #0x0d8]",
  "stp x28, x29, [x0, #0x0e8]",

  // The original LR can't be recovered.
  "str LR, [x0, #0x0f8]",

  // Use x1 as a scratch register.
  "mov x1, SP",
  // context.sp
  "str x1, [x0, #0x100]",

  // The link register holds the return address for this function.
  // context.pc
  "str LR, [x0, #0x108]",

  // pstate should hold SPSR but NZCV are the only bits we know about.
  "mrs x1, NZCV",

  // Enable Control flags, such as CONTEXT_ARM64, CONTEXT_CONTROL,
  // CONTEXT_INTEGER
  "ldr w1, =0x00400003",

  // Set ControlFlags /0x000/ and pstate /0x004/ at the same time.
  "str x1, [x0, #0x000]",

  // Restore x1 from the saved context.
  "ldr x1, [x0, #0x010]",

  // TODO(https://crashpad.chromium.org/bug/300): save floating-point registers
  "ret",
}
