.386
.model flat

_TEXT$00   SEGMENT PARA PUBLIC 'CODE'

CRC1b macro
    movzx EDX, BYTE PTR [ESI]
    inc ESI
    movzx EBX, AL
    xor EDX, EBX
    shr EAX, 8
    xor EAX, [EBP + EDX * 4]
    dec EDI
endm

data_size equ (4 + 4*4)
crc_table equ (data_size + 4)

align 16
public @CrcUpdateT8@16
@CrcUpdateT8@16:
    push EBX
    push ESI
    push EDI
    push EBP

    mov EAX, ECX
    mov ESI, EDX
    mov EDI, [ESP + data_size]
    mov EBP, [ESP + crc_table]

    test EDI, EDI
    jz sl_end
  sl:
    test ESI, 7
    jz sl_end
    CRC1b
    jnz sl
  sl_end:

    cmp EDI, 16
    jb crc_end
    mov [ESP + data_size], EDI
    sub EDI, 8
    and EDI, NOT 7
    sub [ESP + data_size], EDI

    add EDI, ESI
    xor EAX, [ESI]
    mov EBX, [ESI + 4]
    movzx ECX, BL
    align 16
  main_loop:
    mov EDX, [EBP + ECX*4 + 0C00h]
    movzx ECX, BH
    xor EDX, [EBP + ECX*4 + 0800h]
    shr EBX, 16
    movzx ECX, BL
    xor EDX, [EBP + ECX*4 + 0400h]
    xor EDX, [ESI + 8]
    movzx ECX, AL
    movzx EBX, BH
    xor EDX, [EBP + EBX*4 + 0000h]

    mov EBX, [ESI + 12]

    xor EDX, [EBP + ECX*4 + 01C00h]
    movzx ECX, AH
    add ESI, 8
    shr EAX, 16
    xor EDX, [EBP + ECX*4 + 01800h]
    movzx ECX, AL
    xor EDX, [EBP + ECX*4 + 01400h]
    movzx ECX, AH
    mov EAX, [EBP + ECX*4 + 01000h]
    movzx ECX, BL
    xor EAX,EDX

    cmp ESI, EDI
    jne main_loop
    xor EAX, [ESI]

    mov EDI, [ESP + data_size]

  crc_end:

    test EDI, EDI
    jz fl_end
  fl:
    CRC1b
    jnz fl
  fl_end:

    pop EBP
    pop EDI
    pop ESI
    pop EBX
    ret 8


end
