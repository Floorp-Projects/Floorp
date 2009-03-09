;################################################################
; ExtDLL header for MASM32
;
; Author: Ramon
;
; Obs: This header must be included after windows.inc and kernel32.inc
;      because it need the prototypes for lstrcpy, lstrcpyn, 
;      GlobalAlloc and GlobalFree
;
;################################################################
stack_t struct
  next dd ?
  text dd ? ; 1 DUP(?) ; this should be the length of string_size
stack_t ends

.const
; For page showing plug-ins
WM_NOTIFY_OUTER_NEXT   equ (WM_USER+0x8)
WM_NOTIFY_CUSTOM_READY equ (WM_USER+0xd)
NOTIFY_BYE_BYE         equ 'x'

INST_0 EQU 0         ; $0
INST_1 EQU 1         ; $1
INST_2 EQU 2         ; $2
INST_3 EQU 3         ; $3
INST_4 EQU 4         ; $4
INST_5 EQU 5         ; $5
INST_6 EQU 6         ; $6
INST_7 EQU 7         ; $7
INST_8 EQU 8         ; $8
INST_9 EQU 9         ; $9
INST_R0 EQU 10        ; $R0
INST_R1 EQU 11        ; $R1
INST_R2 EQU 12        ; $R2
INST_R3 EQU 13        ; $R3
INST_R4 EQU 14        ; $R4
INST_R5 EQU 15        ; $R5
INST_R6 EQU 16        ; $R6
INST_R7 EQU 17        ; $R7
INST_R8 EQU 18        ; $R8
INST_R9 EQU 19        ; $R9
INST_CMDLINE EQU 20   ; $CMDLINE
INST_INSTDIR EQU 21   ; $INSTDIR
INST_OUTDIR EQU 22    ; $OUTDIR
INST_EXEDIR EQU 23    ; $EXEDIR
INST_LANG EQU 24      ; $LANGUAGE
__INST_LAST EQU 25

.data?
g_stringsize dd ?
g_stacktop dd ?
g_variables dd ?

m2m MACRO M1, M2
      push M2
      pop  M1
ENDM

EXDLL_INIT MACRO
      m2m g_stringsize, string_size
      m2m g_stacktop, stacktop
      m2m g_variables, variables
ENDM

.code

; utility functions (not required but often useful)
popstring proc uses edi pStr:DWORD

  LOCAL th:DWORD

  mov edi, g_stacktop
  cmp edi, 0
  jz  STACK_ERR
  mov edi, [edi]
  cmp edi, 0
  jz  STACK_ERR

  ASSUME edi:PTR stack_t
  invoke lstrcpy, pStr, ADDR [edi].text
  mov th , edi
  mov edi, [edi].next
  mov eax, g_stacktop
  mov [eax], edi
  invoke GlobalFree, th
  ASSUME edi:PTR NOTHING
  mov eax, 0
  ret
  
STACK_ERR:
  mov eax, 1
  ret

popstring endp

pushstring proc uses edi pStr:DWORD

  cmp g_stacktop, 0
  jz  STACK_ERR

  mov eax, sizeof stack_t
  add eax, g_stringsize
  invoke GlobalAlloc, GPTR, eax

  mov edi, eax
  assume edi:PTR stack_t

  invoke lstrcpyn, ADDR [edi].text, pStr, g_stringsize
  mov eax, g_stacktop
  push DWORD PTR[eax]
  mov [eax], edi
  pop eax
  ;lea edi, [edi].next ; Not needed [edi].next == edi
  mov DWORD PTR[edi], eax
  ASSUME edi:PTR NOTHING

STACK_ERR:
  ret

pushstring endp

getuservariable proc varnum:DWORD

  .if varnum < 0 || varnum >= __INST_LAST
    xor eax, eax
  .else
    mov eax, varnum
    imul eax, g_stringsize
    add eax, g_variables
  .endif
  ret

getuservariable endp

setuservariable proc varnum:DWORD, var:DWORD

  .if (var != NULL && varnum >= 0 && varnum < __INST_LAST)
    mov eax, varnum
    imul eax, g_stringsize
    add eax, g_variables
    invoke lstrcpy, eax, var
  .endif
  ret

setuservariable endp
