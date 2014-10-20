;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

%include "third_party/x86inc/x86inc.asm"

SECTION .text

%macro convolve_fn 1
INIT_XMM sse2
cglobal convolve_%1, 4, 7, 4, src, src_stride, dst, dst_stride, \
                              fx, fxs, fy, fys, w, h
  mov r4d, dword wm
  cmp r4d, 4
  je .w4
  cmp r4d, 8
  je .w8
  cmp r4d, 16
  je .w16
  cmp r4d, 32
  je .w32

  mov                    r4d, dword hm
.loop64:
  movu                    m0, [srcq]
  movu                    m1, [srcq+16]
  movu                    m2, [srcq+32]
  movu                    m3, [srcq+48]
  add                   srcq, src_strideq
%ifidn %1, avg
  pavgb                   m0, [dstq]
  pavgb                   m1, [dstq+16]
  pavgb                   m2, [dstq+32]
  pavgb                   m3, [dstq+48]
%endif
  mova             [dstq   ], m0
  mova             [dstq+16], m1
  mova             [dstq+32], m2
  mova             [dstq+48], m3
  add                   dstq, dst_strideq
  dec                    r4d
  jnz .loop64
  RET

.w32:
  mov                    r4d, dword hm
.loop32:
  movu                    m0, [srcq]
  movu                    m1, [srcq+16]
  movu                    m2, [srcq+src_strideq]
  movu                    m3, [srcq+src_strideq+16]
  lea                   srcq, [srcq+src_strideq*2]
%ifidn %1, avg
  pavgb                   m0, [dstq]
  pavgb                   m1, [dstq            +16]
  pavgb                   m2, [dstq+dst_strideq]
  pavgb                   m3, [dstq+dst_strideq+16]
%endif
  mova [dstq               ], m0
  mova [dstq            +16], m1
  mova [dstq+dst_strideq   ], m2
  mova [dstq+dst_strideq+16], m3
  lea                   dstq, [dstq+dst_strideq*2]
  sub                    r4d, 2
  jnz .loop32
  RET

.w16:
  mov                    r4d, dword hm
  lea                    r5q, [src_strideq*3]
  lea                    r6q, [dst_strideq*3]
.loop16:
  movu                    m0, [srcq]
  movu                    m1, [srcq+src_strideq]
  movu                    m2, [srcq+src_strideq*2]
  movu                    m3, [srcq+r5q]
  lea                   srcq, [srcq+src_strideq*4]
%ifidn %1, avg
  pavgb                   m0, [dstq]
  pavgb                   m1, [dstq+dst_strideq]
  pavgb                   m2, [dstq+dst_strideq*2]
  pavgb                   m3, [dstq+r6q]
%endif
  mova  [dstq              ], m0
  mova  [dstq+dst_strideq  ], m1
  mova  [dstq+dst_strideq*2], m2
  mova  [dstq+r6q          ], m3
  lea                   dstq, [dstq+dst_strideq*4]
  sub                    r4d, 4
  jnz .loop16
  RET

INIT_MMX sse
.w8:
  mov                    r4d, dword hm
  lea                    r5q, [src_strideq*3]
  lea                    r6q, [dst_strideq*3]
.loop8:
  movu                    m0, [srcq]
  movu                    m1, [srcq+src_strideq]
  movu                    m2, [srcq+src_strideq*2]
  movu                    m3, [srcq+r5q]
  lea                   srcq, [srcq+src_strideq*4]
%ifidn %1, avg
  pavgb                   m0, [dstq]
  pavgb                   m1, [dstq+dst_strideq]
  pavgb                   m2, [dstq+dst_strideq*2]
  pavgb                   m3, [dstq+r6q]
%endif
  mova  [dstq              ], m0
  mova  [dstq+dst_strideq  ], m1
  mova  [dstq+dst_strideq*2], m2
  mova  [dstq+r6q          ], m3
  lea                   dstq, [dstq+dst_strideq*4]
  sub                    r4d, 4
  jnz .loop8
  RET

.w4:
  mov                    r4d, dword hm
  lea                    r5q, [src_strideq*3]
  lea                    r6q, [dst_strideq*3]
.loop4:
  movh                    m0, [srcq]
  movh                    m1, [srcq+src_strideq]
  movh                    m2, [srcq+src_strideq*2]
  movh                    m3, [srcq+r5q]
  lea                   srcq, [srcq+src_strideq*4]
%ifidn %1, avg
  movh                    m4, [dstq]
  movh                    m5, [dstq+dst_strideq]
  movh                    m6, [dstq+dst_strideq*2]
  movh                    m7, [dstq+r6q]
  pavgb                   m0, m4
  pavgb                   m1, m5
  pavgb                   m2, m6
  pavgb                   m3, m7
%endif
  movh  [dstq              ], m0
  movh  [dstq+dst_strideq  ], m1
  movh  [dstq+dst_strideq*2], m2
  movh  [dstq+r6q          ], m3
  lea                   dstq, [dstq+dst_strideq*4]
  sub                    r4d, 4
  jnz .loop4
  RET
%endmacro

convolve_fn copy
convolve_fn avg
