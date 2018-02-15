C_OBJS = $(C_OBJS) \
  $O\Aes.obj

!IF "$(CPU)" != "IA64" && "$(CPU)" != "MIPS" && "$(CPU)" != "ARM" && "$(CPU)" != "ARM64"
ASM_OBJS = $(ASM_OBJS) \
  $O\AesOpt.obj
!ENDIF
