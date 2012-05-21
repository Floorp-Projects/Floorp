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

    STUBENTRY ?Stub3@nsXPTCStubBase@@UEAAIXZ, 3
    STUBENTRY ?Stub4@nsXPTCStubBase@@UEAAIXZ, 4
    STUBENTRY ?Stub5@nsXPTCStubBase@@UEAAIXZ, 5
    STUBENTRY ?Stub6@nsXPTCStubBase@@UEAAIXZ, 6
    STUBENTRY ?Stub7@nsXPTCStubBase@@UEAAIXZ, 7
    STUBENTRY ?Stub8@nsXPTCStubBase@@UEAAIXZ, 8
    STUBENTRY ?Stub9@nsXPTCStubBase@@UEAAIXZ, 9
    STUBENTRY ?Stub10@nsXPTCStubBase@@UEAAIXZ, 10
    STUBENTRY ?Stub11@nsXPTCStubBase@@UEAAIXZ, 11
    STUBENTRY ?Stub12@nsXPTCStubBase@@UEAAIXZ, 12
    STUBENTRY ?Stub13@nsXPTCStubBase@@UEAAIXZ, 13
    STUBENTRY ?Stub14@nsXPTCStubBase@@UEAAIXZ, 14
    STUBENTRY ?Stub15@nsXPTCStubBase@@UEAAIXZ, 15
    STUBENTRY ?Stub16@nsXPTCStubBase@@UEAAIXZ, 16
    STUBENTRY ?Stub17@nsXPTCStubBase@@UEAAIXZ, 17
    STUBENTRY ?Stub18@nsXPTCStubBase@@UEAAIXZ, 18
    STUBENTRY ?Stub19@nsXPTCStubBase@@UEAAIXZ, 19
    STUBENTRY ?Stub20@nsXPTCStubBase@@UEAAIXZ, 20
    STUBENTRY ?Stub21@nsXPTCStubBase@@UEAAIXZ, 21
    STUBENTRY ?Stub22@nsXPTCStubBase@@UEAAIXZ, 22
    STUBENTRY ?Stub23@nsXPTCStubBase@@UEAAIXZ, 23
    STUBENTRY ?Stub24@nsXPTCStubBase@@UEAAIXZ, 24
    STUBENTRY ?Stub25@nsXPTCStubBase@@UEAAIXZ, 25
    STUBENTRY ?Stub26@nsXPTCStubBase@@UEAAIXZ, 26
    STUBENTRY ?Stub27@nsXPTCStubBase@@UEAAIXZ, 27
    STUBENTRY ?Stub28@nsXPTCStubBase@@UEAAIXZ, 28
    STUBENTRY ?Stub29@nsXPTCStubBase@@UEAAIXZ, 29
    STUBENTRY ?Stub30@nsXPTCStubBase@@UEAAIXZ, 30
    STUBENTRY ?Stub31@nsXPTCStubBase@@UEAAIXZ, 31
    STUBENTRY ?Stub32@nsXPTCStubBase@@UEAAIXZ, 32
    STUBENTRY ?Stub33@nsXPTCStubBase@@UEAAIXZ, 33
    STUBENTRY ?Stub34@nsXPTCStubBase@@UEAAIXZ, 34
    STUBENTRY ?Stub35@nsXPTCStubBase@@UEAAIXZ, 35
    STUBENTRY ?Stub36@nsXPTCStubBase@@UEAAIXZ, 36
    STUBENTRY ?Stub37@nsXPTCStubBase@@UEAAIXZ, 37
    STUBENTRY ?Stub38@nsXPTCStubBase@@UEAAIXZ, 38
    STUBENTRY ?Stub39@nsXPTCStubBase@@UEAAIXZ, 39
    STUBENTRY ?Stub40@nsXPTCStubBase@@UEAAIXZ, 40
    STUBENTRY ?Stub41@nsXPTCStubBase@@UEAAIXZ, 41
    STUBENTRY ?Stub42@nsXPTCStubBase@@UEAAIXZ, 42
    STUBENTRY ?Stub43@nsXPTCStubBase@@UEAAIXZ, 43
    STUBENTRY ?Stub44@nsXPTCStubBase@@UEAAIXZ, 44
    STUBENTRY ?Stub45@nsXPTCStubBase@@UEAAIXZ, 45
    STUBENTRY ?Stub46@nsXPTCStubBase@@UEAAIXZ, 46
    STUBENTRY ?Stub47@nsXPTCStubBase@@UEAAIXZ, 47
    STUBENTRY ?Stub48@nsXPTCStubBase@@UEAAIXZ, 48
    STUBENTRY ?Stub49@nsXPTCStubBase@@UEAAIXZ, 49
    STUBENTRY ?Stub50@nsXPTCStubBase@@UEAAIXZ, 50
    STUBENTRY ?Stub51@nsXPTCStubBase@@UEAAIXZ, 51
    STUBENTRY ?Stub52@nsXPTCStubBase@@UEAAIXZ, 52
    STUBENTRY ?Stub53@nsXPTCStubBase@@UEAAIXZ, 53
    STUBENTRY ?Stub54@nsXPTCStubBase@@UEAAIXZ, 54
    STUBENTRY ?Stub55@nsXPTCStubBase@@UEAAIXZ, 55
    STUBENTRY ?Stub56@nsXPTCStubBase@@UEAAIXZ, 56
    STUBENTRY ?Stub57@nsXPTCStubBase@@UEAAIXZ, 57
    STUBENTRY ?Stub58@nsXPTCStubBase@@UEAAIXZ, 58
    STUBENTRY ?Stub59@nsXPTCStubBase@@UEAAIXZ, 59
    STUBENTRY ?Stub60@nsXPTCStubBase@@UEAAIXZ, 60
    STUBENTRY ?Stub61@nsXPTCStubBase@@UEAAIXZ, 61
    STUBENTRY ?Stub62@nsXPTCStubBase@@UEAAIXZ, 62
    STUBENTRY ?Stub63@nsXPTCStubBase@@UEAAIXZ, 63
    STUBENTRY ?Stub64@nsXPTCStubBase@@UEAAIXZ, 64
    STUBENTRY ?Stub65@nsXPTCStubBase@@UEAAIXZ, 65
    STUBENTRY ?Stub66@nsXPTCStubBase@@UEAAIXZ, 66
    STUBENTRY ?Stub67@nsXPTCStubBase@@UEAAIXZ, 67
    STUBENTRY ?Stub68@nsXPTCStubBase@@UEAAIXZ, 68
    STUBENTRY ?Stub69@nsXPTCStubBase@@UEAAIXZ, 69
    STUBENTRY ?Stub70@nsXPTCStubBase@@UEAAIXZ, 70
    STUBENTRY ?Stub71@nsXPTCStubBase@@UEAAIXZ, 71
    STUBENTRY ?Stub72@nsXPTCStubBase@@UEAAIXZ, 72
    STUBENTRY ?Stub73@nsXPTCStubBase@@UEAAIXZ, 73
    STUBENTRY ?Stub74@nsXPTCStubBase@@UEAAIXZ, 74
    STUBENTRY ?Stub75@nsXPTCStubBase@@UEAAIXZ, 75
    STUBENTRY ?Stub76@nsXPTCStubBase@@UEAAIXZ, 76
    STUBENTRY ?Stub77@nsXPTCStubBase@@UEAAIXZ, 77
    STUBENTRY ?Stub78@nsXPTCStubBase@@UEAAIXZ, 78
    STUBENTRY ?Stub79@nsXPTCStubBase@@UEAAIXZ, 79
    STUBENTRY ?Stub80@nsXPTCStubBase@@UEAAIXZ, 80
    STUBENTRY ?Stub81@nsXPTCStubBase@@UEAAIXZ, 81
    STUBENTRY ?Stub82@nsXPTCStubBase@@UEAAIXZ, 82
    STUBENTRY ?Stub83@nsXPTCStubBase@@UEAAIXZ, 83
    STUBENTRY ?Stub84@nsXPTCStubBase@@UEAAIXZ, 84
    STUBENTRY ?Stub85@nsXPTCStubBase@@UEAAIXZ, 85
    STUBENTRY ?Stub86@nsXPTCStubBase@@UEAAIXZ, 86
    STUBENTRY ?Stub87@nsXPTCStubBase@@UEAAIXZ, 87
    STUBENTRY ?Stub88@nsXPTCStubBase@@UEAAIXZ, 88
    STUBENTRY ?Stub89@nsXPTCStubBase@@UEAAIXZ, 89
    STUBENTRY ?Stub90@nsXPTCStubBase@@UEAAIXZ, 90
    STUBENTRY ?Stub91@nsXPTCStubBase@@UEAAIXZ, 91
    STUBENTRY ?Stub92@nsXPTCStubBase@@UEAAIXZ, 92
    STUBENTRY ?Stub93@nsXPTCStubBase@@UEAAIXZ, 93
    STUBENTRY ?Stub94@nsXPTCStubBase@@UEAAIXZ, 94
    STUBENTRY ?Stub95@nsXPTCStubBase@@UEAAIXZ, 95
    STUBENTRY ?Stub96@nsXPTCStubBase@@UEAAIXZ, 96
    STUBENTRY ?Stub97@nsXPTCStubBase@@UEAAIXZ, 97
    STUBENTRY ?Stub98@nsXPTCStubBase@@UEAAIXZ, 98
    STUBENTRY ?Stub99@nsXPTCStubBase@@UEAAIXZ, 99
    STUBENTRY ?Stub100@nsXPTCStubBase@@UEAAIXZ, 100
    STUBENTRY ?Stub101@nsXPTCStubBase@@UEAAIXZ, 101
    STUBENTRY ?Stub102@nsXPTCStubBase@@UEAAIXZ, 102
    STUBENTRY ?Stub103@nsXPTCStubBase@@UEAAIXZ, 103
    STUBENTRY ?Stub104@nsXPTCStubBase@@UEAAIXZ, 104
    STUBENTRY ?Stub105@nsXPTCStubBase@@UEAAIXZ, 105
    STUBENTRY ?Stub106@nsXPTCStubBase@@UEAAIXZ, 106
    STUBENTRY ?Stub107@nsXPTCStubBase@@UEAAIXZ, 107
    STUBENTRY ?Stub108@nsXPTCStubBase@@UEAAIXZ, 108
    STUBENTRY ?Stub109@nsXPTCStubBase@@UEAAIXZ, 109
    STUBENTRY ?Stub110@nsXPTCStubBase@@UEAAIXZ, 110
    STUBENTRY ?Stub111@nsXPTCStubBase@@UEAAIXZ, 111
    STUBENTRY ?Stub112@nsXPTCStubBase@@UEAAIXZ, 112
    STUBENTRY ?Stub113@nsXPTCStubBase@@UEAAIXZ, 113
    STUBENTRY ?Stub114@nsXPTCStubBase@@UEAAIXZ, 114
    STUBENTRY ?Stub115@nsXPTCStubBase@@UEAAIXZ, 115
    STUBENTRY ?Stub116@nsXPTCStubBase@@UEAAIXZ, 116
    STUBENTRY ?Stub117@nsXPTCStubBase@@UEAAIXZ, 117
    STUBENTRY ?Stub118@nsXPTCStubBase@@UEAAIXZ, 118
    STUBENTRY ?Stub119@nsXPTCStubBase@@UEAAIXZ, 119
    STUBENTRY ?Stub120@nsXPTCStubBase@@UEAAIXZ, 120
    STUBENTRY ?Stub121@nsXPTCStubBase@@UEAAIXZ, 121
    STUBENTRY ?Stub122@nsXPTCStubBase@@UEAAIXZ, 122
    STUBENTRY ?Stub123@nsXPTCStubBase@@UEAAIXZ, 123
    STUBENTRY ?Stub124@nsXPTCStubBase@@UEAAIXZ, 124
    STUBENTRY ?Stub125@nsXPTCStubBase@@UEAAIXZ, 125
    STUBENTRY ?Stub126@nsXPTCStubBase@@UEAAIXZ, 126
    STUBENTRY ?Stub127@nsXPTCStubBase@@UEAAIXZ, 127
    STUBENTRY ?Stub128@nsXPTCStubBase@@UEAAIXZ, 128
    STUBENTRY ?Stub129@nsXPTCStubBase@@UEAAIXZ, 129
    STUBENTRY ?Stub130@nsXPTCStubBase@@UEAAIXZ, 130
    STUBENTRY ?Stub131@nsXPTCStubBase@@UEAAIXZ, 131
    STUBENTRY ?Stub132@nsXPTCStubBase@@UEAAIXZ, 132
    STUBENTRY ?Stub133@nsXPTCStubBase@@UEAAIXZ, 133
    STUBENTRY ?Stub134@nsXPTCStubBase@@UEAAIXZ, 134
    STUBENTRY ?Stub135@nsXPTCStubBase@@UEAAIXZ, 135
    STUBENTRY ?Stub136@nsXPTCStubBase@@UEAAIXZ, 136
    STUBENTRY ?Stub137@nsXPTCStubBase@@UEAAIXZ, 137
    STUBENTRY ?Stub138@nsXPTCStubBase@@UEAAIXZ, 138
    STUBENTRY ?Stub139@nsXPTCStubBase@@UEAAIXZ, 139
    STUBENTRY ?Stub140@nsXPTCStubBase@@UEAAIXZ, 140
    STUBENTRY ?Stub141@nsXPTCStubBase@@UEAAIXZ, 141
    STUBENTRY ?Stub142@nsXPTCStubBase@@UEAAIXZ, 142
    STUBENTRY ?Stub143@nsXPTCStubBase@@UEAAIXZ, 143
    STUBENTRY ?Stub144@nsXPTCStubBase@@UEAAIXZ, 144
    STUBENTRY ?Stub145@nsXPTCStubBase@@UEAAIXZ, 145
    STUBENTRY ?Stub146@nsXPTCStubBase@@UEAAIXZ, 146
    STUBENTRY ?Stub147@nsXPTCStubBase@@UEAAIXZ, 147
    STUBENTRY ?Stub148@nsXPTCStubBase@@UEAAIXZ, 148
    STUBENTRY ?Stub149@nsXPTCStubBase@@UEAAIXZ, 149
    STUBENTRY ?Stub150@nsXPTCStubBase@@UEAAIXZ, 150
    STUBENTRY ?Stub151@nsXPTCStubBase@@UEAAIXZ, 151
    STUBENTRY ?Stub152@nsXPTCStubBase@@UEAAIXZ, 152
    STUBENTRY ?Stub153@nsXPTCStubBase@@UEAAIXZ, 153
    STUBENTRY ?Stub154@nsXPTCStubBase@@UEAAIXZ, 154
    STUBENTRY ?Stub155@nsXPTCStubBase@@UEAAIXZ, 155
    STUBENTRY ?Stub156@nsXPTCStubBase@@UEAAIXZ, 156
    STUBENTRY ?Stub157@nsXPTCStubBase@@UEAAIXZ, 157
    STUBENTRY ?Stub158@nsXPTCStubBase@@UEAAIXZ, 158
    STUBENTRY ?Stub159@nsXPTCStubBase@@UEAAIXZ, 159
    STUBENTRY ?Stub160@nsXPTCStubBase@@UEAAIXZ, 160
    STUBENTRY ?Stub161@nsXPTCStubBase@@UEAAIXZ, 161
    STUBENTRY ?Stub162@nsXPTCStubBase@@UEAAIXZ, 162
    STUBENTRY ?Stub163@nsXPTCStubBase@@UEAAIXZ, 163
    STUBENTRY ?Stub164@nsXPTCStubBase@@UEAAIXZ, 164
    STUBENTRY ?Stub165@nsXPTCStubBase@@UEAAIXZ, 165
    STUBENTRY ?Stub166@nsXPTCStubBase@@UEAAIXZ, 166
    STUBENTRY ?Stub167@nsXPTCStubBase@@UEAAIXZ, 167
    STUBENTRY ?Stub168@nsXPTCStubBase@@UEAAIXZ, 168
    STUBENTRY ?Stub169@nsXPTCStubBase@@UEAAIXZ, 169
    STUBENTRY ?Stub170@nsXPTCStubBase@@UEAAIXZ, 170
    STUBENTRY ?Stub171@nsXPTCStubBase@@UEAAIXZ, 171
    STUBENTRY ?Stub172@nsXPTCStubBase@@UEAAIXZ, 172
    STUBENTRY ?Stub173@nsXPTCStubBase@@UEAAIXZ, 173
    STUBENTRY ?Stub174@nsXPTCStubBase@@UEAAIXZ, 174
    STUBENTRY ?Stub175@nsXPTCStubBase@@UEAAIXZ, 175
    STUBENTRY ?Stub176@nsXPTCStubBase@@UEAAIXZ, 176
    STUBENTRY ?Stub177@nsXPTCStubBase@@UEAAIXZ, 177
    STUBENTRY ?Stub178@nsXPTCStubBase@@UEAAIXZ, 178
    STUBENTRY ?Stub179@nsXPTCStubBase@@UEAAIXZ, 179
    STUBENTRY ?Stub180@nsXPTCStubBase@@UEAAIXZ, 180
    STUBENTRY ?Stub181@nsXPTCStubBase@@UEAAIXZ, 181
    STUBENTRY ?Stub182@nsXPTCStubBase@@UEAAIXZ, 182
    STUBENTRY ?Stub183@nsXPTCStubBase@@UEAAIXZ, 183
    STUBENTRY ?Stub184@nsXPTCStubBase@@UEAAIXZ, 184
    STUBENTRY ?Stub185@nsXPTCStubBase@@UEAAIXZ, 185
    STUBENTRY ?Stub186@nsXPTCStubBase@@UEAAIXZ, 186
    STUBENTRY ?Stub187@nsXPTCStubBase@@UEAAIXZ, 187
    STUBENTRY ?Stub188@nsXPTCStubBase@@UEAAIXZ, 188
    STUBENTRY ?Stub189@nsXPTCStubBase@@UEAAIXZ, 189
    STUBENTRY ?Stub190@nsXPTCStubBase@@UEAAIXZ, 190
    STUBENTRY ?Stub191@nsXPTCStubBase@@UEAAIXZ, 191
    STUBENTRY ?Stub192@nsXPTCStubBase@@UEAAIXZ, 192
    STUBENTRY ?Stub193@nsXPTCStubBase@@UEAAIXZ, 193
    STUBENTRY ?Stub194@nsXPTCStubBase@@UEAAIXZ, 194
    STUBENTRY ?Stub195@nsXPTCStubBase@@UEAAIXZ, 195
    STUBENTRY ?Stub196@nsXPTCStubBase@@UEAAIXZ, 196
    STUBENTRY ?Stub197@nsXPTCStubBase@@UEAAIXZ, 197
    STUBENTRY ?Stub198@nsXPTCStubBase@@UEAAIXZ, 198
    STUBENTRY ?Stub199@nsXPTCStubBase@@UEAAIXZ, 199
    STUBENTRY ?Stub200@nsXPTCStubBase@@UEAAIXZ, 200
    STUBENTRY ?Stub201@nsXPTCStubBase@@UEAAIXZ, 201
    STUBENTRY ?Stub202@nsXPTCStubBase@@UEAAIXZ, 202
    STUBENTRY ?Stub203@nsXPTCStubBase@@UEAAIXZ, 203
    STUBENTRY ?Stub204@nsXPTCStubBase@@UEAAIXZ, 204
    STUBENTRY ?Stub205@nsXPTCStubBase@@UEAAIXZ, 205
    STUBENTRY ?Stub206@nsXPTCStubBase@@UEAAIXZ, 206
    STUBENTRY ?Stub207@nsXPTCStubBase@@UEAAIXZ, 207
    STUBENTRY ?Stub208@nsXPTCStubBase@@UEAAIXZ, 208
    STUBENTRY ?Stub209@nsXPTCStubBase@@UEAAIXZ, 209
    STUBENTRY ?Stub210@nsXPTCStubBase@@UEAAIXZ, 210
    STUBENTRY ?Stub211@nsXPTCStubBase@@UEAAIXZ, 211
    STUBENTRY ?Stub212@nsXPTCStubBase@@UEAAIXZ, 212
    STUBENTRY ?Stub213@nsXPTCStubBase@@UEAAIXZ, 213
    STUBENTRY ?Stub214@nsXPTCStubBase@@UEAAIXZ, 214
    STUBENTRY ?Stub215@nsXPTCStubBase@@UEAAIXZ, 215
    STUBENTRY ?Stub216@nsXPTCStubBase@@UEAAIXZ, 216
    STUBENTRY ?Stub217@nsXPTCStubBase@@UEAAIXZ, 217
    STUBENTRY ?Stub218@nsXPTCStubBase@@UEAAIXZ, 218
    STUBENTRY ?Stub219@nsXPTCStubBase@@UEAAIXZ, 219
    STUBENTRY ?Stub220@nsXPTCStubBase@@UEAAIXZ, 220
    STUBENTRY ?Stub221@nsXPTCStubBase@@UEAAIXZ, 221
    STUBENTRY ?Stub222@nsXPTCStubBase@@UEAAIXZ, 222
    STUBENTRY ?Stub223@nsXPTCStubBase@@UEAAIXZ, 223
    STUBENTRY ?Stub224@nsXPTCStubBase@@UEAAIXZ, 224
    STUBENTRY ?Stub225@nsXPTCStubBase@@UEAAIXZ, 225
    STUBENTRY ?Stub226@nsXPTCStubBase@@UEAAIXZ, 226
    STUBENTRY ?Stub227@nsXPTCStubBase@@UEAAIXZ, 227
    STUBENTRY ?Stub228@nsXPTCStubBase@@UEAAIXZ, 228
    STUBENTRY ?Stub229@nsXPTCStubBase@@UEAAIXZ, 229
    STUBENTRY ?Stub230@nsXPTCStubBase@@UEAAIXZ, 230
    STUBENTRY ?Stub231@nsXPTCStubBase@@UEAAIXZ, 231
    STUBENTRY ?Stub232@nsXPTCStubBase@@UEAAIXZ, 232
    STUBENTRY ?Stub233@nsXPTCStubBase@@UEAAIXZ, 233
    STUBENTRY ?Stub234@nsXPTCStubBase@@UEAAIXZ, 234
    STUBENTRY ?Stub235@nsXPTCStubBase@@UEAAIXZ, 235
    STUBENTRY ?Stub236@nsXPTCStubBase@@UEAAIXZ, 236
    STUBENTRY ?Stub237@nsXPTCStubBase@@UEAAIXZ, 237
    STUBENTRY ?Stub238@nsXPTCStubBase@@UEAAIXZ, 238
    STUBENTRY ?Stub239@nsXPTCStubBase@@UEAAIXZ, 239
    STUBENTRY ?Stub240@nsXPTCStubBase@@UEAAIXZ, 240
    STUBENTRY ?Stub241@nsXPTCStubBase@@UEAAIXZ, 241
    STUBENTRY ?Stub242@nsXPTCStubBase@@UEAAIXZ, 242
    STUBENTRY ?Stub243@nsXPTCStubBase@@UEAAIXZ, 243
    STUBENTRY ?Stub244@nsXPTCStubBase@@UEAAIXZ, 244
    STUBENTRY ?Stub245@nsXPTCStubBase@@UEAAIXZ, 245
    STUBENTRY ?Stub246@nsXPTCStubBase@@UEAAIXZ, 246
    STUBENTRY ?Stub247@nsXPTCStubBase@@UEAAIXZ, 247
    STUBENTRY ?Stub248@nsXPTCStubBase@@UEAAIXZ, 248
    STUBENTRY ?Stub249@nsXPTCStubBase@@UEAAIXZ, 249

END
