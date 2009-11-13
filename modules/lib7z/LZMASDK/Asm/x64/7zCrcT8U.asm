.code




CRC1b macro
    movzx EDX, BYTE PTR [RSI]
    inc RSI
    movzx EBX, AL
    xor EDX, EBX
    shr EAX, 8
    xor EAX, [RDI + RDX * 4]
    dec R8
endm




align 16
CrcUpdateT8 PROC

    push RBX
    push RSI
    push RDI
    push RBP

    mov EAX, ECX
    mov RSI, RDX
    mov RDI, R9


    test R8, R8
    jz sl_end
  sl:
    test RSI, 7
    jz sl_end
    CRC1b
    jnz sl
  sl_end:

    cmp R8, 16
    jb crc_end
    mov R9, R8
    and R8, 7
    add R8, 8
    sub R9, R8
            
    add R9, RSI
    xor EAX, [RSI]
    mov EBX, [RSI + 4]
    movzx ECX, BL
    align 16
  main_loop:
    mov EDX, [RDI + RCX*4 + 0C00h]
    movzx EBP, BH
    xor EDX, [RDI + RBP*4 + 0800h]
    shr EBX, 16
    movzx ECX, BL
    xor EDX, [RSI + 8]
    xor EDX, [RDI + RCX*4 + 0400h]
    movzx ECX, AL
    movzx EBP, BH
    xor EDX, [RDI + RBP*4 + 0000h]

    mov EBX, [RSI + 12]

    xor EDX, [RDI + RCX*4 + 01C00h]
    movzx EBP, AH
    shr EAX, 16
    movzx ECX, AL
    xor EDX, [RDI + RBP*4 + 01800h]
    movzx EBP, AH
    mov EAX, [RDI + RCX*4 + 01400h]
    add RSI, 8
    xor EAX, [RDI + RBP*4 + 01000h]
    movzx ECX, BL
    xor EAX,EDX

    cmp RSI, R9
    jne main_loop
    xor EAX, [RSI]


  
  crc_end:

    test R8, R8
    jz fl_end
  fl:
    CRC1b
    jnz fl
  fl_end:

    pop RBP
    pop RDI
    pop RSI
    pop RBX
    ret
CrcUpdateT8 ENDP

end
