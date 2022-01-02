; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this
; file, You can obtain one at http://mozilla.org/MPL/2.0/.

; Comments indicate stack memory layout during execution.
; For example at the top of a function, where RIP just points to the return
; address, the stack looks like
; rip = [ra]
; And after pushing rax to the stack,
; rip = [rax][ra]
; And then, after allocating 20h bytes on the stack,
; rip = [..20..][rax][ra]
; And then, after pushing a function pointer,
; rip = [pfn][..20..][rax][ra]

include ksamd64.inc

.code

; It helps to add padding between functions so they're not right up against
; each other. Adds clarity to debugging, and gives a bit of leeway when
; searching for symbols (e.g. a function whose last instruction is CALL
; would push a return address that's in the next function.)
PaddingBetweenFunctions macro
  repeat 10h
     int 3
  endm
endm

DoCrash macro
  mov rax, 7
  mov byte ptr [rax], 9
endm

PaddingBetweenFunctions

; There is no rip addressing mode in x64. The only way to get the value
; of rip is to call a function, and pop it from the stack.
WhoCalledMe proc
  pop rax ; rax is now ra
  push rax ; Restore ra so this function can return.
  sub rax, 5 ; Correct for the size of the call instruction
  ret
WhoCalledMe endp

PaddingBetweenFunctions

; Any function that we expect to test against on the stack, we'll need its
; real address. If we use function pointers in C, we'll get the address to jump
; table entries. This bit of code at the beginning of each function will
; return the real address we'd expect to see in stack traces.
;
; rcx (1st arg) = mode
; rax (return)  = address of either NO_MANS_LAND or this function.
;
; When mode is 0, we place the address of NO_MANS_LAND in RAX, for the function
; to use as it wants. This is just for convenience because almost all functions
; here need this address at some point.
;
; When mode is 1, the address of this function is returned.
TestHeader macro
  call WhoCalledMe
  test rcx, rcx
  je continue_test
  ret
continue_test:
  inc rcx
  call x64CrashCFITest_NO_MANS_LAND
  xor rcx, rcx
endm

; The point of this is to add a stack frame to test against.
; void* x64CrashCFITest_Launcher(int getAddress, void* pTestFn)
x64CrashCFITest_Launcher proc frame
  TestHeader

  .endprolog
  call rdx
  ret
x64CrashCFITest_Launcher endp

PaddingBetweenFunctions

; void* x64CrashCFITest_NO_MANS_LAND(uint64_t mode);
; Not meant to be called. Only when mode = 1 in order to return its address.
; Place this function's address on the stack so the stack scanning algorithm
; thinks this is a return address, and places it on the stack trace.
x64CrashCFITest_NO_MANS_LAND proc frame
  TestHeader
  .endprolog
  ret
x64CrashCFITest_NO_MANS_LAND endp

PaddingBetweenFunctions

; Test that we:
; - handle unknown opcodes gracefully
; - fall back to other stack unwind strategies if CFI doesn't work
;
; In order to properly unwind this frame, we'd need to fully support
; SET_FPREG with offsets, plus restoring registers via PUSH_NONVOL.
; To do this, sprinkle the stack with bad return addresses
; and stack pointers.
x64CrashCFITest_UnknownOpcode proc frame
  TestHeader

  push rax
  .allocstack 8

  push rbp
  .pushreg rbp

  push rax
  push rsp
  push rax
  push rsp
  .allocstack 20h
  ; rsp = [rsp][pfn][rsp][pfn][rbp][pfn][ra]

  lea rbp, [rsp+10h]
  .setframe rbp, 10h
  ; rsp = [rsp][pfn] [rsp][pfn][rbp][pfn][ra]
  ; rbp =           ^

  .endprolog

  ; Now modify RSP so measuring stack size from unwind ops will not help
  ; finding the return address.
  push rax
  push rsp
  ; rsp = [rsp][pfn][rsp][pfn] [rsp][pfn][rbp][pfn][ra]

  DoCrash

x64CrashCFITest_UnknownOpcode endp

PaddingBetweenFunctions

; void* x64CrashCFITest_PUSH_NONVOL(uint64_t mode);
;
; Test correct handling of PUSH_NONVOL unwind code.
;
x64CrashCFITest_PUSH_NONVOL proc frame
  TestHeader

  push r10
  .pushreg r10
  push r15
  .pushreg r15
  push rbx
  .pushreg rbx
  push rsi
  .pushreg rsi
  push rbp
  .pushreg rbp
  ; rsp = [rbp][rsi][rbx][r15][r10][ra]

  push rax
  .allocstack 8
  ; rsp = [pfn][rbp][rsi][rbx][r15][r10][ra]

  .endprolog

  DoCrash

x64CrashCFITest_PUSH_NONVOL endp

PaddingBetweenFunctions

; void* x64CrashCFITest_ALLOC_SMALL(uint64_t mode);
;
; Small allocations are between 8bytes and 512kb-8bytes
;
x64CrashCFITest_ALLOC_SMALL proc frame
  TestHeader

  push rax
  push rax
  push rax
  push rax
  .allocstack 20h
  ; rsp = [pfn][pfn][pfn][pfn][ra]

  .endprolog

  DoCrash

x64CrashCFITest_ALLOC_SMALL endp

PaddingBetweenFunctions

; void* x64CrashCFITest_ALLOC_LARGE(uint64_t mode);
;
; Allocations between 512kb and 4gb
; Note: ReserveStackSpace() in nsTestCrasher.cpp pre-allocates stack
; space for this.
x64CrashCFITest_ALLOC_LARGE proc frame
  TestHeader

  sub rsp, 0a000h
  .allocstack 0a000h
  ; rsp = [..640kb..][ra]

  mov qword ptr [rsp], rax
  ; rsp = [pfn][..640kb-8..][ra]

  .endprolog

  DoCrash

x64CrashCFITest_ALLOC_LARGE endp

PaddingBetweenFunctions

; void* x64CrashCFITest_SAVE_NONVOL(uint64_t mode);
;
; Test correct handling of SAVE_NONVOL unwind code.
;
x64CrashCFITest_SAVE_NONVOL proc frame
  TestHeader

  sub rsp, 30h
  .allocstack 30h
  ; rsp = [..30..][ra]

  mov qword ptr [rsp+28h], r10
  .savereg r10, 28h
  mov qword ptr [rsp+20h], rbp
  .savereg rbp, 20h
  mov qword ptr [rsp+18h], rsi
  .savereg rsi, 18h
  mov qword ptr [rsp+10h], rbx
  .savereg rbx, 10h
  mov qword ptr [rsp+8], r15
  .savereg r15, 8
  ; rsp = [r15][rbx][rsi][rbp][r10][ra]

  mov qword ptr [rsp], rax

  ; rsp = [pfn][r15][rbx][rsi][rbp][r10][ra]

  .endprolog

  DoCrash

x64CrashCFITest_SAVE_NONVOL endp

PaddingBetweenFunctions

; void* x64CrashCFITest_SAVE_NONVOL_FAR(uint64_t mode);
;
; Similar to the test above but adding 640kb to most offsets.
; Note: ReserveStackSpace() in nsTestCrasher.cpp pre-allocates stack
; space for this.
x64CrashCFITest_SAVE_NONVOL_FAR proc frame
  TestHeader

  sub rsp, 0a0030h
  .allocstack 0a0030h
  ; rsp = [..640k..][..30..][ra]

  mov qword ptr [rsp+28h+0a0000h], r10
  .savereg r10, 28h+0a0000h
  mov qword ptr [rsp+20h+0a0000h], rbp
  .savereg rbp, 20h+0a0000h
  mov qword ptr [rsp+18h+0a0000h], rsi
  .savereg rsi, 18h+0a0000h
  mov qword ptr [rsp+10h+0a0000h], rbx
  .savereg rbx, 10h+0a0000h
  mov qword ptr [rsp+8+0a0000h], r15
  .savereg r15, 8+0a0000h
  ; rsp = [..640k..][..8..][r15][rbx][rsi][rbp][r10][ra]

  mov qword ptr [rsp], rax

  ; rsp = [pfn][..640k..][r15][rbx][rsi][rbp][r10][ra]

  .endprolog

  DoCrash

x64CrashCFITest_SAVE_NONVOL_FAR endp

PaddingBetweenFunctions

; void* x64CrashCFITest_SAVE_XMM128(uint64_t mode);
;
; Test correct handling of SAVE_XMM128 unwind code.
x64CrashCFITest_SAVE_XMM128 proc frame
  TestHeader

  sub rsp, 30h
  .allocstack 30h
  ; rsp = [..30..][ra]

  movdqu [rsp+20h], xmm6
  .savexmm128 xmm6, 20h
  ; rsp = [..20..][xmm6][ra]

  movdqu [rsp+10h], xmm15
  .savexmm128 xmm15, 10h
  ; rsp = [..10..][xmm15][xmm6][ra]

  mov qword ptr [rsp], rax
  ; rsp = [pfn][..8..][xmm15][xmm6][ra]

  .endprolog

  DoCrash

x64CrashCFITest_SAVE_XMM128 endp

PaddingBetweenFunctions

; void* x64CrashCFITest_SAVE_XMM128(uint64_t mode);
;
; Similar to the test above but adding 640kb to most offsets.
; Note: ReserveStackSpace() in nsTestCrasher.cpp pre-allocates stack
; space for this.
x64CrashCFITest_SAVE_XMM128_FAR proc frame
  TestHeader

  sub rsp, 0a0030h
  .allocstack 0a0030h
  ; rsp = [..640kb..][..30..][ra]

  movdqu [rsp+20h+0a0000h], xmm6
  .savexmm128 xmm6, 20h+0a0000h
  ; rsp = [..640kb..][..20..][xmm6][ra]

  movdqu [rsp+10h+0a0000h], xmm6
  .savexmm128 xmm15, 10h+0a0000h
  ; rsp = [..640kb..][..10..][xmm15][xmm6][ra]

  mov qword ptr [rsp], rax
  ; rsp = [pfn][..640kb..][..8..][xmm15][xmm6][ra]

  .endprolog

  DoCrash

x64CrashCFITest_SAVE_XMM128_FAR endp

PaddingBetweenFunctions

; void* x64CrashCFITest_EPILOG(uint64_t mode);
;
; The epilog unwind op will also set the unwind version to 2.
; Test that we don't choke on UWOP_EPILOG or version 2 unwind info.
x64CrashCFITest_EPILOG proc frame
  TestHeader

  push rax
  .allocstack 8
  ; rsp = [pfn][ra]

  .endprolog

  DoCrash

  .beginepilog

  ret

x64CrashCFITest_EPILOG endp

PaddingBetweenFunctions

; Having an EOF symbol at the end of this file contains symbolication to this
; file. So addresses beyond this file don't get mistakenly symbolicated as a
; meaningful function name.
x64CrashCFITest_EOF proc frame
  TestHeader
  .endprolog
  ret
x64CrashCFITest_EOF endp

end
