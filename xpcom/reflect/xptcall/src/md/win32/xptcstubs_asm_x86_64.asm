; This Source Code Form is subject to the terms of the Mozilla Public
; License, v. 2.0. If a copy of the MPL was not distributed with this
; file, You can obtain one at http://mozilla.org/MPL/2.0/.

extrn PrepareAndDispatch:PROC

.code

SharedStub PROC FRAME
    sub     rsp, 104
    .ALLOCSTACK 104
    .ENDPROLOG

   ; rcx is this pointer.  Need backup for optimized build

   mov      qword ptr [rsp+88], rcx

   ;
   ; fist 4 parameters (1st is "this" pointer) are passed in registers.
   ;

   ; for floating value

    movsd   qword ptr [rsp+64], xmm1
    movsd   qword ptr [rsp+72], xmm2
    movsd   qword ptr [rsp+80], xmm3

   ; for integer value

    mov     qword ptr [rsp+40], rdx
    mov     qword ptr [rsp+48], r8
    mov     qword ptr [rsp+56], r9

    ;
    ; Call PrepareAndDispatch function
    ;

    ; 5th parameter (floating parameters) of PrepareAndDispatch

    lea     r9, qword ptr [rsp+64]
    mov     qword ptr [rsp+32], r9

    ; 4th parameter (normal parameters) of PrepareAndDispatch

    lea     r9, qword ptr [rsp+40]

    ; 3rd parameter (pointer to args on stack)

    lea     r8, qword ptr [rsp+40+104]

    ; 2nd parameter (vtbl_index)

    mov     rdx, r11

    ; 1st parameter (this) (rcx)

    call    PrepareAndDispatch

    ; restore rcx

    mov     rcx, qword ptr [rsp+88]

    ;
    ; clean up register
    ;

    add     rsp, 104+8

    ; set return address

    mov     rdx, qword ptr [rsp-8]

    ; simulate __stdcall return

    jmp     rdx

SharedStub ENDP


STUBENTRY MACRO functionname, paramcount
functionname PROC EXPORT
    mov     r11, paramcount
    jmp     SharedStub
functionname ENDP
ENDM

    STUBENTRY ?Stub3@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 3
    STUBENTRY ?Stub4@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 4
    STUBENTRY ?Stub5@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 5
    STUBENTRY ?Stub6@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 6
    STUBENTRY ?Stub7@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 7
    STUBENTRY ?Stub8@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 8
    STUBENTRY ?Stub9@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 9
    STUBENTRY ?Stub10@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 10
    STUBENTRY ?Stub11@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 11
    STUBENTRY ?Stub12@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 12
    STUBENTRY ?Stub13@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 13
    STUBENTRY ?Stub14@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 14
    STUBENTRY ?Stub15@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 15
    STUBENTRY ?Stub16@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 16
    STUBENTRY ?Stub17@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 17
    STUBENTRY ?Stub18@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 18
    STUBENTRY ?Stub19@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 19
    STUBENTRY ?Stub20@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 20
    STUBENTRY ?Stub21@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 21
    STUBENTRY ?Stub22@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 22
    STUBENTRY ?Stub23@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 23
    STUBENTRY ?Stub24@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 24
    STUBENTRY ?Stub25@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 25
    STUBENTRY ?Stub26@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 26
    STUBENTRY ?Stub27@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 27
    STUBENTRY ?Stub28@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 28
    STUBENTRY ?Stub29@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 29
    STUBENTRY ?Stub30@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 30
    STUBENTRY ?Stub31@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 31
    STUBENTRY ?Stub32@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 32
    STUBENTRY ?Stub33@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 33
    STUBENTRY ?Stub34@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 34
    STUBENTRY ?Stub35@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 35
    STUBENTRY ?Stub36@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 36
    STUBENTRY ?Stub37@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 37
    STUBENTRY ?Stub38@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 38
    STUBENTRY ?Stub39@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 39
    STUBENTRY ?Stub40@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 40
    STUBENTRY ?Stub41@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 41
    STUBENTRY ?Stub42@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 42
    STUBENTRY ?Stub43@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 43
    STUBENTRY ?Stub44@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 44
    STUBENTRY ?Stub45@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 45
    STUBENTRY ?Stub46@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 46
    STUBENTRY ?Stub47@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 47
    STUBENTRY ?Stub48@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 48
    STUBENTRY ?Stub49@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 49
    STUBENTRY ?Stub50@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 50
    STUBENTRY ?Stub51@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 51
    STUBENTRY ?Stub52@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 52
    STUBENTRY ?Stub53@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 53
    STUBENTRY ?Stub54@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 54
    STUBENTRY ?Stub55@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 55
    STUBENTRY ?Stub56@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 56
    STUBENTRY ?Stub57@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 57
    STUBENTRY ?Stub58@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 58
    STUBENTRY ?Stub59@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 59
    STUBENTRY ?Stub60@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 60
    STUBENTRY ?Stub61@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 61
    STUBENTRY ?Stub62@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 62
    STUBENTRY ?Stub63@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 63
    STUBENTRY ?Stub64@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 64
    STUBENTRY ?Stub65@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 65
    STUBENTRY ?Stub66@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 66
    STUBENTRY ?Stub67@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 67
    STUBENTRY ?Stub68@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 68
    STUBENTRY ?Stub69@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 69
    STUBENTRY ?Stub70@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 70
    STUBENTRY ?Stub71@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 71
    STUBENTRY ?Stub72@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 72
    STUBENTRY ?Stub73@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 73
    STUBENTRY ?Stub74@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 74
    STUBENTRY ?Stub75@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 75
    STUBENTRY ?Stub76@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 76
    STUBENTRY ?Stub77@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 77
    STUBENTRY ?Stub78@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 78
    STUBENTRY ?Stub79@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 79
    STUBENTRY ?Stub80@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 80
    STUBENTRY ?Stub81@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 81
    STUBENTRY ?Stub82@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 82
    STUBENTRY ?Stub83@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 83
    STUBENTRY ?Stub84@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 84
    STUBENTRY ?Stub85@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 85
    STUBENTRY ?Stub86@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 86
    STUBENTRY ?Stub87@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 87
    STUBENTRY ?Stub88@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 88
    STUBENTRY ?Stub89@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 89
    STUBENTRY ?Stub90@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 90
    STUBENTRY ?Stub91@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 91
    STUBENTRY ?Stub92@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 92
    STUBENTRY ?Stub93@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 93
    STUBENTRY ?Stub94@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 94
    STUBENTRY ?Stub95@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 95
    STUBENTRY ?Stub96@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 96
    STUBENTRY ?Stub97@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 97
    STUBENTRY ?Stub98@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 98
    STUBENTRY ?Stub99@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 99
    STUBENTRY ?Stub100@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 100
    STUBENTRY ?Stub101@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 101
    STUBENTRY ?Stub102@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 102
    STUBENTRY ?Stub103@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 103
    STUBENTRY ?Stub104@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 104
    STUBENTRY ?Stub105@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 105
    STUBENTRY ?Stub106@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 106
    STUBENTRY ?Stub107@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 107
    STUBENTRY ?Stub108@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 108
    STUBENTRY ?Stub109@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 109
    STUBENTRY ?Stub110@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 110
    STUBENTRY ?Stub111@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 111
    STUBENTRY ?Stub112@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 112
    STUBENTRY ?Stub113@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 113
    STUBENTRY ?Stub114@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 114
    STUBENTRY ?Stub115@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 115
    STUBENTRY ?Stub116@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 116
    STUBENTRY ?Stub117@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 117
    STUBENTRY ?Stub118@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 118
    STUBENTRY ?Stub119@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 119
    STUBENTRY ?Stub120@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 120
    STUBENTRY ?Stub121@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 121
    STUBENTRY ?Stub122@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 122
    STUBENTRY ?Stub123@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 123
    STUBENTRY ?Stub124@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 124
    STUBENTRY ?Stub125@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 125
    STUBENTRY ?Stub126@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 126
    STUBENTRY ?Stub127@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 127
    STUBENTRY ?Stub128@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 128
    STUBENTRY ?Stub129@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 129
    STUBENTRY ?Stub130@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 130
    STUBENTRY ?Stub131@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 131
    STUBENTRY ?Stub132@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 132
    STUBENTRY ?Stub133@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 133
    STUBENTRY ?Stub134@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 134
    STUBENTRY ?Stub135@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 135
    STUBENTRY ?Stub136@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 136
    STUBENTRY ?Stub137@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 137
    STUBENTRY ?Stub138@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 138
    STUBENTRY ?Stub139@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 139
    STUBENTRY ?Stub140@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 140
    STUBENTRY ?Stub141@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 141
    STUBENTRY ?Stub142@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 142
    STUBENTRY ?Stub143@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 143
    STUBENTRY ?Stub144@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 144
    STUBENTRY ?Stub145@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 145
    STUBENTRY ?Stub146@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 146
    STUBENTRY ?Stub147@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 147
    STUBENTRY ?Stub148@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 148
    STUBENTRY ?Stub149@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 149
    STUBENTRY ?Stub150@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 150
    STUBENTRY ?Stub151@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 151
    STUBENTRY ?Stub152@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 152
    STUBENTRY ?Stub153@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 153
    STUBENTRY ?Stub154@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 154
    STUBENTRY ?Stub155@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 155
    STUBENTRY ?Stub156@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 156
    STUBENTRY ?Stub157@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 157
    STUBENTRY ?Stub158@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 158
    STUBENTRY ?Stub159@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 159
    STUBENTRY ?Stub160@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 160
    STUBENTRY ?Stub161@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 161
    STUBENTRY ?Stub162@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 162
    STUBENTRY ?Stub163@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 163
    STUBENTRY ?Stub164@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 164
    STUBENTRY ?Stub165@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 165
    STUBENTRY ?Stub166@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 166
    STUBENTRY ?Stub167@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 167
    STUBENTRY ?Stub168@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 168
    STUBENTRY ?Stub169@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 169
    STUBENTRY ?Stub170@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 170
    STUBENTRY ?Stub171@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 171
    STUBENTRY ?Stub172@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 172
    STUBENTRY ?Stub173@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 173
    STUBENTRY ?Stub174@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 174
    STUBENTRY ?Stub175@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 175
    STUBENTRY ?Stub176@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 176
    STUBENTRY ?Stub177@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 177
    STUBENTRY ?Stub178@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 178
    STUBENTRY ?Stub179@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 179
    STUBENTRY ?Stub180@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 180
    STUBENTRY ?Stub181@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 181
    STUBENTRY ?Stub182@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 182
    STUBENTRY ?Stub183@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 183
    STUBENTRY ?Stub184@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 184
    STUBENTRY ?Stub185@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 185
    STUBENTRY ?Stub186@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 186
    STUBENTRY ?Stub187@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 187
    STUBENTRY ?Stub188@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 188
    STUBENTRY ?Stub189@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 189
    STUBENTRY ?Stub190@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 190
    STUBENTRY ?Stub191@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 191
    STUBENTRY ?Stub192@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 192
    STUBENTRY ?Stub193@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 193
    STUBENTRY ?Stub194@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 194
    STUBENTRY ?Stub195@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 195
    STUBENTRY ?Stub196@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 196
    STUBENTRY ?Stub197@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 197
    STUBENTRY ?Stub198@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 198
    STUBENTRY ?Stub199@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 199
    STUBENTRY ?Stub200@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 200
    STUBENTRY ?Stub201@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 201
    STUBENTRY ?Stub202@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 202
    STUBENTRY ?Stub203@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 203
    STUBENTRY ?Stub204@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 204
    STUBENTRY ?Stub205@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 205
    STUBENTRY ?Stub206@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 206
    STUBENTRY ?Stub207@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 207
    STUBENTRY ?Stub208@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 208
    STUBENTRY ?Stub209@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 209
    STUBENTRY ?Stub210@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 210
    STUBENTRY ?Stub211@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 211
    STUBENTRY ?Stub212@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 212
    STUBENTRY ?Stub213@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 213
    STUBENTRY ?Stub214@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 214
    STUBENTRY ?Stub215@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 215
    STUBENTRY ?Stub216@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 216
    STUBENTRY ?Stub217@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 217
    STUBENTRY ?Stub218@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 218
    STUBENTRY ?Stub219@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 219
    STUBENTRY ?Stub220@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 220
    STUBENTRY ?Stub221@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 221
    STUBENTRY ?Stub222@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 222
    STUBENTRY ?Stub223@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 223
    STUBENTRY ?Stub224@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 224
    STUBENTRY ?Stub225@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 225
    STUBENTRY ?Stub226@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 226
    STUBENTRY ?Stub227@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 227
    STUBENTRY ?Stub228@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 228
    STUBENTRY ?Stub229@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 229
    STUBENTRY ?Stub230@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 230
    STUBENTRY ?Stub231@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 231
    STUBENTRY ?Stub232@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 232
    STUBENTRY ?Stub233@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 233
    STUBENTRY ?Stub234@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 234
    STUBENTRY ?Stub235@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 235
    STUBENTRY ?Stub236@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 236
    STUBENTRY ?Stub237@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 237
    STUBENTRY ?Stub238@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 238
    STUBENTRY ?Stub239@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 239
    STUBENTRY ?Stub240@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 240
    STUBENTRY ?Stub241@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 241
    STUBENTRY ?Stub242@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 242
    STUBENTRY ?Stub243@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 243
    STUBENTRY ?Stub244@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 244
    STUBENTRY ?Stub245@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 245
    STUBENTRY ?Stub246@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 246
    STUBENTRY ?Stub247@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 247
    STUBENTRY ?Stub248@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 248
    STUBENTRY ?Stub249@nsXPTCStubBase@@UEAA?AW4tag_nsresult@@XZ, 249

END
