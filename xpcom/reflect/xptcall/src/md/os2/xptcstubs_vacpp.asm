COMMENT | -*- Mode: asm; tab-width: 8; c-basic-offset: 4 -*-

 ***** BEGIN LICENSE BLOCK *****
 Version: MPL 1.1/GPL 2.0/LGPL 2.1

 The contents of this file are subject to the Mozilla Public License Version
 1.1 (the "License"); you may not use this file except in compliance with
 the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS IS" basis,
 WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 for the specific language governing rights and limitations under the
 License.

 The Original Code is mozilla.org Code.

 The Initial Developer of the Original Code is
 Netscape Communications Corporation.
 Portions created by the Initial Developer are Copyright (C) 2001
 the Initial Developer. All Rights Reserved.

 Contributor(s):
   John Fairhurst <john_fairhurst@iname.com>
   Henry Sobotka <sobotka@axess.com>

 Alternatively, the contents of this file may be used under the terms of
 either of the GNU General Public License Version 2 or later (the "GPL"),
 or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 in which case the provisions of the GPL or the LGPL are applicable instead
 of those above. If you wish to allow use of your version of this file only
 under the terms of either the GPL or the LGPL, and not to allow others to
 use your version of this file under the terms of the MPL, indicate your
 decision by deleting the provisions above and replace them with the notice
 and other provisions required by the GPL or the LGPL. If you do not delete
 the provisions above, a recipient may use your version of this file under
 the terms of any one of the MPL, the GPL or the LGPL.

 ***** END LICENSE BLOCK *****
        Version 1.0 (the "NPL"); you may not use this file except in
        compliance with the NPL.  You may obtain a copy of the NPL at
        http://www.mozilla.org/NPL/

        Software distributed under the NPL is distributed on an "AS IS" basis,
        WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
        for the specific language governing rights and limitations under the
        NPL.

        The Initial Developer of this code under the NPL is Netscape
        Communications Corporation.  Portions created by Netscape are
        Copyright (C) 1999 Netscape Communications Corporation.  All Rights
        Reserved.

        Contributor: Henry Sobotka <sobotka@axess.com>

        xptcallstub_vacpp.asm: ALP assembler procedure for VAC++ build of xptcall
	|

        .486P
        .MODEL FLAT, OPTLINK
        .STACK

        .CODE

        EXTERN OPTLINK PrepareAndDispatch__FP14nsXPTCStubBaseUiPUi:PROC

setentidx MACRO index

        push ecx                           ; Save parameters
        push edx                           ; Save parameters - don't need to save eax - it is the "this" ptr
        lea    ecx, dword ptr [esp+10h]    ; Load pointer to "args" into ecx
        mov    edx, index                  ; Move vtable index into edx
        sub    esp, 0ch                    ; Make room for three parameters on the stack
        call   PrepareAndDispatch__FP14nsXPTCStubBaseUiPUi 
        add    esp, 14h                    ; Reset stack pointer
        ret
        ENDM
        
; 251 STUB_ENTRY(249)

Stub249__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0f9h
Stub249__14nsXPTCStubBaseFv	endp

; 250 STUB_ENTRY(248)

Stub248__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0f8h
Stub248__14nsXPTCStubBaseFv	endp

; 249 STUB_ENTRY(247)

Stub247__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0f7h
Stub247__14nsXPTCStubBaseFv	endp

; 248 STUB_ENTRY(246)

Stub246__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0f6h
Stub246__14nsXPTCStubBaseFv	endp

; 247 STUB_ENTRY(245)

Stub245__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0f5h
Stub245__14nsXPTCStubBaseFv	endp

; 246 STUB_ENTRY(244)

Stub244__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0f4h
Stub244__14nsXPTCStubBaseFv	endp

; 245 STUB_ENTRY(243)

Stub243__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0f3h
Stub243__14nsXPTCStubBaseFv	endp

; 244 STUB_ENTRY(242)

Stub242__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0f2h
Stub242__14nsXPTCStubBaseFv	endp

; 243 STUB_ENTRY(241)

Stub241__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0f1h
Stub241__14nsXPTCStubBaseFv	endp

; 242 STUB_ENTRY(240)

Stub240__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0f0h
Stub240__14nsXPTCStubBaseFv	endp

; 241 STUB_ENTRY(239)

Stub239__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0efh
Stub239__14nsXPTCStubBaseFv	endp

; 240 STUB_ENTRY(238)

Stub238__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0eeh
Stub238__14nsXPTCStubBaseFv	endp

; 239 STUB_ENTRY(237)

Stub237__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0edh
Stub237__14nsXPTCStubBaseFv	endp

; 238 STUB_ENTRY(236)

Stub236__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0ech
Stub236__14nsXPTCStubBaseFv	endp

; 237 STUB_ENTRY(235)

Stub235__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0ebh
Stub235__14nsXPTCStubBaseFv	endp

; 236 STUB_ENTRY(234)

Stub234__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0eah
Stub234__14nsXPTCStubBaseFv	endp

; 235 STUB_ENTRY(233)

Stub233__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0e9h
Stub233__14nsXPTCStubBaseFv	endp

; 234 STUB_ENTRY(232)

Stub232__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0e8h
Stub232__14nsXPTCStubBaseFv	endp

; 233 STUB_ENTRY(231)

Stub231__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0e7h
Stub231__14nsXPTCStubBaseFv	endp

; 232 STUB_ENTRY(230)

Stub230__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0e6h
Stub230__14nsXPTCStubBaseFv	endp

; 231 STUB_ENTRY(229)

Stub229__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0e5h
Stub229__14nsXPTCStubBaseFv	endp

; 230 STUB_ENTRY(228)

Stub228__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0e4h
Stub228__14nsXPTCStubBaseFv	endp

; 229 STUB_ENTRY(227)

Stub227__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0e3h
Stub227__14nsXPTCStubBaseFv	endp

; 228 STUB_ENTRY(226)

Stub226__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0e2h
Stub226__14nsXPTCStubBaseFv	endp

; 227 STUB_ENTRY(225)

Stub225__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0e1h
Stub225__14nsXPTCStubBaseFv	endp

; 226 STUB_ENTRY(224)

Stub224__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0e0h
Stub224__14nsXPTCStubBaseFv	endp

; 225 STUB_ENTRY(223)

Stub223__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0dfh
Stub223__14nsXPTCStubBaseFv	endp

; 224 STUB_ENTRY(222)

Stub222__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0deh
Stub222__14nsXPTCStubBaseFv	endp

; 223 STUB_ENTRY(221)

Stub221__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0ddh
Stub221__14nsXPTCStubBaseFv	endp

; 222 STUB_ENTRY(220)

Stub220__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0dch
Stub220__14nsXPTCStubBaseFv	endp

; 221 STUB_ENTRY(219)

Stub219__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0dbh
Stub219__14nsXPTCStubBaseFv	endp

; 220 STUB_ENTRY(218)

Stub218__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0dah
Stub218__14nsXPTCStubBaseFv	endp

; 219 STUB_ENTRY(217)

Stub217__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0d9h
Stub217__14nsXPTCStubBaseFv	endp

; 218 STUB_ENTRY(216)

Stub216__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0d8h
Stub216__14nsXPTCStubBaseFv	endp

; 217 STUB_ENTRY(215)

Stub215__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0d7h
Stub215__14nsXPTCStubBaseFv	endp

; 216 STUB_ENTRY(214)

Stub214__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0d6h
Stub214__14nsXPTCStubBaseFv	endp

; 215 STUB_ENTRY(213)

Stub213__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0d5h
Stub213__14nsXPTCStubBaseFv	endp

; 214 STUB_ENTRY(212)

Stub212__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0d4h
Stub212__14nsXPTCStubBaseFv	endp

; 213 STUB_ENTRY(211)

Stub211__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0d3h
Stub211__14nsXPTCStubBaseFv	endp

; 212 STUB_ENTRY(210)

Stub210__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0d2h
Stub210__14nsXPTCStubBaseFv	endp

; 211 STUB_ENTRY(209)

Stub209__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0d1h
Stub209__14nsXPTCStubBaseFv	endp

; 210 STUB_ENTRY(208)

Stub208__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0d0h
Stub208__14nsXPTCStubBaseFv	endp

; 209 STUB_ENTRY(207)

Stub207__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0cfh
Stub207__14nsXPTCStubBaseFv	endp

; 208 STUB_ENTRY(206)

Stub206__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0ceh
Stub206__14nsXPTCStubBaseFv	endp

; 207 STUB_ENTRY(205)

Stub205__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0cdh
Stub205__14nsXPTCStubBaseFv	endp

; 206 STUB_ENTRY(204)

Stub204__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0cch
Stub204__14nsXPTCStubBaseFv	endp

; 205 STUB_ENTRY(203)

Stub203__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0cbh
Stub203__14nsXPTCStubBaseFv	endp

; 204 STUB_ENTRY(202)

Stub202__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0cah
Stub202__14nsXPTCStubBaseFv	endp

; 203 STUB_ENTRY(201)

Stub201__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0c9h
Stub201__14nsXPTCStubBaseFv	endp

; 202 STUB_ENTRY(200)

Stub200__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0c8h
Stub200__14nsXPTCStubBaseFv	endp

; 201 STUB_ENTRY(199)

Stub199__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0c7h
Stub199__14nsXPTCStubBaseFv	endp

; 200 STUB_ENTRY(198)

Stub198__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0c6h
Stub198__14nsXPTCStubBaseFv	endp

; 199 STUB_ENTRY(197)

Stub197__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0c5h
Stub197__14nsXPTCStubBaseFv	endp

; 198 STUB_ENTRY(196)

Stub196__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0c4h
Stub196__14nsXPTCStubBaseFv	endp

; 197 STUB_ENTRY(195)

Stub195__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0c3h
Stub195__14nsXPTCStubBaseFv	endp

; 196 STUB_ENTRY(194)

Stub194__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0c2h
Stub194__14nsXPTCStubBaseFv	endp

; 195 STUB_ENTRY(193)

Stub193__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0c1h
Stub193__14nsXPTCStubBaseFv	endp

; 194 STUB_ENTRY(192)

Stub192__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0c0h
Stub192__14nsXPTCStubBaseFv	endp

; 193 STUB_ENTRY(191)

Stub191__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0bfh
Stub191__14nsXPTCStubBaseFv	endp

; 192 STUB_ENTRY(190)

Stub190__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0beh
Stub190__14nsXPTCStubBaseFv	endp

; 191 STUB_ENTRY(189)

Stub189__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0bdh
Stub189__14nsXPTCStubBaseFv	endp

; 190 STUB_ENTRY(188)

Stub188__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0bch
Stub188__14nsXPTCStubBaseFv	endp

; 189 STUB_ENTRY(187)

Stub187__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0bbh
Stub187__14nsXPTCStubBaseFv	endp

; 188 STUB_ENTRY(186)

Stub186__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0bah
Stub186__14nsXPTCStubBaseFv	endp

; 187 STUB_ENTRY(185)

Stub185__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0b9h
Stub185__14nsXPTCStubBaseFv	endp

; 186 STUB_ENTRY(184)

Stub184__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0b8h
Stub184__14nsXPTCStubBaseFv	endp

; 185 STUB_ENTRY(183)

Stub183__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0b7h
Stub183__14nsXPTCStubBaseFv	endp

; 184 STUB_ENTRY(182)

Stub182__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0b6h
Stub182__14nsXPTCStubBaseFv	endp

; 183 STUB_ENTRY(181)

Stub181__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0b5h
Stub181__14nsXPTCStubBaseFv	endp

; 182 STUB_ENTRY(180)

Stub180__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0b4h
Stub180__14nsXPTCStubBaseFv	endp

; 181 STUB_ENTRY(179)

Stub179__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0b3h
Stub179__14nsXPTCStubBaseFv	endp

; 180 STUB_ENTRY(178)

Stub178__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0b2h
Stub178__14nsXPTCStubBaseFv	endp

; 179 STUB_ENTRY(177)

Stub177__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0b1h
Stub177__14nsXPTCStubBaseFv	endp

; 178 STUB_ENTRY(176)

Stub176__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0b0h
Stub176__14nsXPTCStubBaseFv	endp

; 177 STUB_ENTRY(175)

Stub175__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0afh
Stub175__14nsXPTCStubBaseFv	endp

; 176 STUB_ENTRY(174)

Stub174__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0aeh
Stub174__14nsXPTCStubBaseFv	endp

; 175 STUB_ENTRY(173)

Stub173__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0adh
Stub173__14nsXPTCStubBaseFv	endp

; 174 STUB_ENTRY(172)

Stub172__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0ach
Stub172__14nsXPTCStubBaseFv	endp

; 173 STUB_ENTRY(171)

Stub171__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0abh
Stub171__14nsXPTCStubBaseFv	endp

; 172 STUB_ENTRY(170)

Stub170__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0aah
Stub170__14nsXPTCStubBaseFv	endp

; 171 STUB_ENTRY(169)

Stub169__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0a9h
Stub169__14nsXPTCStubBaseFv	endp

; 170 STUB_ENTRY(168)

Stub168__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0a8h
Stub168__14nsXPTCStubBaseFv	endp

; 169 STUB_ENTRY(167)

Stub167__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0a7h
Stub167__14nsXPTCStubBaseFv	endp

; 168 STUB_ENTRY(166)

Stub166__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0a6h
Stub166__14nsXPTCStubBaseFv	endp

; 167 STUB_ENTRY(165)

Stub165__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0a5h
Stub165__14nsXPTCStubBaseFv	endp

; 166 STUB_ENTRY(164)

Stub164__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0a4h
Stub164__14nsXPTCStubBaseFv	endp

; 165 STUB_ENTRY(163)

Stub163__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0a3h
Stub163__14nsXPTCStubBaseFv	endp

; 164 STUB_ENTRY(162)

Stub162__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0a2h
Stub162__14nsXPTCStubBaseFv	endp

; 163 STUB_ENTRY(161)

Stub161__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0a1h
Stub161__14nsXPTCStubBaseFv	endp

; 162 STUB_ENTRY(160)

Stub160__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0a0h
Stub160__14nsXPTCStubBaseFv	endp

; 161 STUB_ENTRY(159)

Stub159__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 09fh
Stub159__14nsXPTCStubBaseFv	endp

; 160 STUB_ENTRY(158)

Stub158__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 09eh
Stub158__14nsXPTCStubBaseFv	endp

; 159 STUB_ENTRY(157)

Stub157__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 09dh
Stub157__14nsXPTCStubBaseFv	endp

; 158 STUB_ENTRY(156)

Stub156__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 09ch
Stub156__14nsXPTCStubBaseFv	endp

; 157 STUB_ENTRY(155)

Stub155__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 09bh
Stub155__14nsXPTCStubBaseFv	endp

; 156 STUB_ENTRY(154)

Stub154__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 09ah
Stub154__14nsXPTCStubBaseFv	endp

; 155 STUB_ENTRY(153)

Stub153__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 099h
Stub153__14nsXPTCStubBaseFv	endp

; 154 STUB_ENTRY(152)

Stub152__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 098h
Stub152__14nsXPTCStubBaseFv	endp

; 153 STUB_ENTRY(151)

Stub151__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 097h
Stub151__14nsXPTCStubBaseFv	endp

; 152 STUB_ENTRY(150)

Stub150__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 096h
Stub150__14nsXPTCStubBaseFv	endp

; 151 STUB_ENTRY(149)

Stub149__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 095h
Stub149__14nsXPTCStubBaseFv	endp

; 150 STUB_ENTRY(148)

Stub148__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 094h
Stub148__14nsXPTCStubBaseFv	endp

; 149 STUB_ENTRY(147)

Stub147__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 093h
Stub147__14nsXPTCStubBaseFv	endp

; 148 STUB_ENTRY(146)

Stub146__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 092h
Stub146__14nsXPTCStubBaseFv	endp

; 147 STUB_ENTRY(145)

Stub145__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 091h
Stub145__14nsXPTCStubBaseFv	endp

; 146 STUB_ENTRY(144)

Stub144__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 090h
Stub144__14nsXPTCStubBaseFv	endp

; 145 STUB_ENTRY(143)

Stub143__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 08fh
Stub143__14nsXPTCStubBaseFv	endp

; 144 STUB_ENTRY(142)

Stub142__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 08eh
Stub142__14nsXPTCStubBaseFv	endp

; 143 STUB_ENTRY(141)

Stub141__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 08dh
Stub141__14nsXPTCStubBaseFv	endp

; 142 STUB_ENTRY(140)

Stub140__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 08ch
Stub140__14nsXPTCStubBaseFv	endp

; 141 STUB_ENTRY(139)

Stub139__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 08bh
Stub139__14nsXPTCStubBaseFv	endp

; 140 STUB_ENTRY(138)

Stub138__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 08ah
Stub138__14nsXPTCStubBaseFv	endp

; 139 STUB_ENTRY(137)

Stub137__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 089h
Stub137__14nsXPTCStubBaseFv	endp

; 138 STUB_ENTRY(136)

Stub136__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 088h
Stub136__14nsXPTCStubBaseFv	endp

; 137 STUB_ENTRY(135)

Stub135__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 087h
Stub135__14nsXPTCStubBaseFv	endp

; 136 STUB_ENTRY(134)

Stub134__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 086h
Stub134__14nsXPTCStubBaseFv	endp

; 135 STUB_ENTRY(133)

Stub133__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 085h
Stub133__14nsXPTCStubBaseFv	endp

; 134 STUB_ENTRY(132)

Stub132__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 084h
Stub132__14nsXPTCStubBaseFv	endp

; 133 STUB_ENTRY(131)

Stub131__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 083h
Stub131__14nsXPTCStubBaseFv	endp

; 132 STUB_ENTRY(130)

Stub130__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 082h
Stub130__14nsXPTCStubBaseFv	endp

; 131 STUB_ENTRY(129)

Stub129__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 081h
Stub129__14nsXPTCStubBaseFv	endp

; 130 STUB_ENTRY(128)

Stub128__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 080h
Stub128__14nsXPTCStubBaseFv	endp

; 129 STUB_ENTRY(127)

Stub127__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 07fh
Stub127__14nsXPTCStubBaseFv	endp

; 128 STUB_ENTRY(126)

Stub126__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 07eh
Stub126__14nsXPTCStubBaseFv	endp

; 127 STUB_ENTRY(125)

Stub125__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 07dh
Stub125__14nsXPTCStubBaseFv	endp

; 126 STUB_ENTRY(124)

Stub124__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 07ch
Stub124__14nsXPTCStubBaseFv	endp

; 125 STUB_ENTRY(123)

Stub123__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 07bh
Stub123__14nsXPTCStubBaseFv	endp

; 124 STUB_ENTRY(122)

Stub122__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 07ah
Stub122__14nsXPTCStubBaseFv	endp

; 123 STUB_ENTRY(121)

Stub121__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 079h
Stub121__14nsXPTCStubBaseFv	endp

; 122 STUB_ENTRY(120)

Stub120__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 078h
Stub120__14nsXPTCStubBaseFv	endp

; 121 STUB_ENTRY(119)

Stub119__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 077h
Stub119__14nsXPTCStubBaseFv	endp

; 120 STUB_ENTRY(118)

Stub118__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 076h
Stub118__14nsXPTCStubBaseFv	endp

; 119 STUB_ENTRY(117)

Stub117__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 075h
Stub117__14nsXPTCStubBaseFv	endp

; 118 STUB_ENTRY(116)

Stub116__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 074h
Stub116__14nsXPTCStubBaseFv	endp

; 117 STUB_ENTRY(115)

Stub115__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 073h
Stub115__14nsXPTCStubBaseFv	endp

; 116 STUB_ENTRY(114)

Stub114__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 072h
Stub114__14nsXPTCStubBaseFv	endp

; 115 STUB_ENTRY(113)

Stub113__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 071h
Stub113__14nsXPTCStubBaseFv	endp

; 114 STUB_ENTRY(112)

Stub112__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 070h
Stub112__14nsXPTCStubBaseFv	endp

; 113 STUB_ENTRY(111)

Stub111__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 06fh
Stub111__14nsXPTCStubBaseFv	endp

; 112 STUB_ENTRY(110)

Stub110__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 06eh
Stub110__14nsXPTCStubBaseFv	endp

; 111 STUB_ENTRY(109)

Stub109__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 06dh
Stub109__14nsXPTCStubBaseFv	endp

; 110 STUB_ENTRY(108)

Stub108__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 06ch
Stub108__14nsXPTCStubBaseFv	endp

; 109 STUB_ENTRY(107)

Stub107__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 06bh
Stub107__14nsXPTCStubBaseFv	endp

; 108 STUB_ENTRY(106)

Stub106__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 06ah
Stub106__14nsXPTCStubBaseFv	endp

; 107 STUB_ENTRY(105)

Stub105__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 069h
Stub105__14nsXPTCStubBaseFv	endp

; 106 STUB_ENTRY(104)

Stub104__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 068h
Stub104__14nsXPTCStubBaseFv	endp

; 105 STUB_ENTRY(103)

Stub103__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 067h
Stub103__14nsXPTCStubBaseFv	endp

; 104 STUB_ENTRY(102)

Stub102__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 066h
Stub102__14nsXPTCStubBaseFv	endp

; 103 STUB_ENTRY(101)

Stub101__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 065h
Stub101__14nsXPTCStubBaseFv	endp

; 102 STUB_ENTRY(100)

Stub100__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 064h
Stub100__14nsXPTCStubBaseFv	endp

; 101 STUB_ENTRY(99)

Stub99__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 063h
Stub99__14nsXPTCStubBaseFv	endp

; 100 STUB_ENTRY(98)

Stub98__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 062h
Stub98__14nsXPTCStubBaseFv	endp

; 99 STUB_ENTRY(97)

Stub97__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 061h
Stub97__14nsXPTCStubBaseFv	endp

; 98 STUB_ENTRY(96)

Stub96__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 060h
Stub96__14nsXPTCStubBaseFv	endp

; 97 STUB_ENTRY(95)

Stub95__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 05fh
Stub95__14nsXPTCStubBaseFv	endp

; 96 STUB_ENTRY(94)

Stub94__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 05eh
Stub94__14nsXPTCStubBaseFv	endp

; 95 STUB_ENTRY(93)

Stub93__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 05dh
Stub93__14nsXPTCStubBaseFv	endp

; 94 STUB_ENTRY(92)

Stub92__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 05ch
Stub92__14nsXPTCStubBaseFv	endp

; 93 STUB_ENTRY(91)

Stub91__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 05bh
Stub91__14nsXPTCStubBaseFv	endp

; 92 STUB_ENTRY(90)

Stub90__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 05ah
Stub90__14nsXPTCStubBaseFv	endp

; 91 STUB_ENTRY(89)

Stub89__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 059h
Stub89__14nsXPTCStubBaseFv	endp

; 90 STUB_ENTRY(88)

Stub88__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 058h
Stub88__14nsXPTCStubBaseFv	endp

; 89 STUB_ENTRY(87)

Stub87__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 057h
Stub87__14nsXPTCStubBaseFv	endp

; 88 STUB_ENTRY(86)

Stub86__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 056h
Stub86__14nsXPTCStubBaseFv	endp

; 87 STUB_ENTRY(85)

Stub85__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 055h
Stub85__14nsXPTCStubBaseFv	endp

; 86 STUB_ENTRY(84)

Stub84__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 054h
Stub84__14nsXPTCStubBaseFv	endp

; 85 STUB_ENTRY(83)

Stub83__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 053h
Stub83__14nsXPTCStubBaseFv	endp

; 84 STUB_ENTRY(82)

Stub82__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 052h
Stub82__14nsXPTCStubBaseFv	endp

; 83 STUB_ENTRY(81)

Stub81__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 051h
Stub81__14nsXPTCStubBaseFv	endp

; 82 STUB_ENTRY(80)

Stub80__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 050h
Stub80__14nsXPTCStubBaseFv	endp

; 81 STUB_ENTRY(79)

Stub79__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 04fh
Stub79__14nsXPTCStubBaseFv	endp

; 80 STUB_ENTRY(78)

Stub78__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 04eh
Stub78__14nsXPTCStubBaseFv	endp

; 79 STUB_ENTRY(77)

Stub77__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 04dh
Stub77__14nsXPTCStubBaseFv	endp

; 78 STUB_ENTRY(76)

Stub76__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 04ch
Stub76__14nsXPTCStubBaseFv	endp

; 77 STUB_ENTRY(75)

Stub75__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 04bh
Stub75__14nsXPTCStubBaseFv	endp

; 76 STUB_ENTRY(74)

Stub74__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 04ah
Stub74__14nsXPTCStubBaseFv	endp

; 75 STUB_ENTRY(73)

Stub73__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 049h
Stub73__14nsXPTCStubBaseFv	endp

; 74 STUB_ENTRY(72)

Stub72__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 048h
Stub72__14nsXPTCStubBaseFv	endp

; 73 STUB_ENTRY(71)

Stub71__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 047h
Stub71__14nsXPTCStubBaseFv	endp

; 72 STUB_ENTRY(70)

Stub70__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 046h
Stub70__14nsXPTCStubBaseFv	endp

; 71 STUB_ENTRY(69)

Stub69__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 045h
Stub69__14nsXPTCStubBaseFv	endp

; 70 STUB_ENTRY(68)

Stub68__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 044h
Stub68__14nsXPTCStubBaseFv	endp

; 69 STUB_ENTRY(67)

Stub67__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 043h
Stub67__14nsXPTCStubBaseFv	endp

; 68 STUB_ENTRY(66)

Stub66__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 042h
Stub66__14nsXPTCStubBaseFv	endp

; 67 STUB_ENTRY(65)

Stub65__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 041h
Stub65__14nsXPTCStubBaseFv	endp

; 66 STUB_ENTRY(64)

Stub64__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 040h
Stub64__14nsXPTCStubBaseFv	endp

; 65 STUB_ENTRY(63)

Stub63__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 03fh
Stub63__14nsXPTCStubBaseFv	endp

; 64 STUB_ENTRY(62)

Stub62__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 03eh
Stub62__14nsXPTCStubBaseFv	endp

; 63 STUB_ENTRY(61)

Stub61__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 03dh
Stub61__14nsXPTCStubBaseFv	endp

; 62 STUB_ENTRY(60)

Stub60__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 03ch
Stub60__14nsXPTCStubBaseFv	endp

; 61 STUB_ENTRY(59)

Stub59__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 03bh
Stub59__14nsXPTCStubBaseFv	endp

; 60 STUB_ENTRY(58)

Stub58__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 03ah
Stub58__14nsXPTCStubBaseFv	endp

; 59 STUB_ENTRY(57)

Stub57__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 039h
Stub57__14nsXPTCStubBaseFv	endp

; 58 STUB_ENTRY(56)

Stub56__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 038h
Stub56__14nsXPTCStubBaseFv	endp

; 57 STUB_ENTRY(55)

Stub55__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 037h
Stub55__14nsXPTCStubBaseFv	endp

; 56 STUB_ENTRY(54)

Stub54__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 036h
Stub54__14nsXPTCStubBaseFv	endp

; 55 STUB_ENTRY(53)

Stub53__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 035h
Stub53__14nsXPTCStubBaseFv	endp

; 54 STUB_ENTRY(52)

Stub52__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 034h
Stub52__14nsXPTCStubBaseFv	endp

; 53 STUB_ENTRY(51)

Stub51__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 033h
Stub51__14nsXPTCStubBaseFv	endp

; 52 STUB_ENTRY(50)

Stub50__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 032h
Stub50__14nsXPTCStubBaseFv	endp

; 51 STUB_ENTRY(49)

Stub49__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 031h
Stub49__14nsXPTCStubBaseFv	endp

; 50 STUB_ENTRY(48)

Stub48__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 030h
Stub48__14nsXPTCStubBaseFv	endp

; 49 STUB_ENTRY(47)

Stub47__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 02fh
Stub47__14nsXPTCStubBaseFv	endp

; 48 STUB_ENTRY(46)

Stub46__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 02eh
Stub46__14nsXPTCStubBaseFv	endp

; 47 STUB_ENTRY(45)

Stub45__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 02dh
Stub45__14nsXPTCStubBaseFv	endp

; 46 STUB_ENTRY(44)

Stub44__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 02ch
Stub44__14nsXPTCStubBaseFv	endp

; 45 STUB_ENTRY(43)

Stub43__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 02bh
Stub43__14nsXPTCStubBaseFv	endp

; 44 STUB_ENTRY(42)

Stub42__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 02ah
Stub42__14nsXPTCStubBaseFv	endp

; 43 STUB_ENTRY(41)

Stub41__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 029h
Stub41__14nsXPTCStubBaseFv	endp

; 42 STUB_ENTRY(40)

Stub40__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 028h
Stub40__14nsXPTCStubBaseFv	endp

; 41 STUB_ENTRY(39)

Stub39__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 027h
Stub39__14nsXPTCStubBaseFv	endp

; 40 STUB_ENTRY(38)

Stub38__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 026h
Stub38__14nsXPTCStubBaseFv	endp

; 39 STUB_ENTRY(37)

Stub37__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 025h
Stub37__14nsXPTCStubBaseFv	endp

; 38 STUB_ENTRY(36)

Stub36__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 024h
Stub36__14nsXPTCStubBaseFv	endp

; 37 STUB_ENTRY(35)

Stub35__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 023h
Stub35__14nsXPTCStubBaseFv	endp

; 36 STUB_ENTRY(34)

Stub34__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 022h
Stub34__14nsXPTCStubBaseFv	endp

; 35 STUB_ENTRY(33)

Stub33__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 021h
Stub33__14nsXPTCStubBaseFv	endp

; 34 STUB_ENTRY(32)

Stub32__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 020h
Stub32__14nsXPTCStubBaseFv	endp

; 33 STUB_ENTRY(31)

Stub31__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 01fh
Stub31__14nsXPTCStubBaseFv	endp

; 32 STUB_ENTRY(30)

Stub30__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 01eh
Stub30__14nsXPTCStubBaseFv	endp

; 31 STUB_ENTRY(29)

Stub29__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 01dh
Stub29__14nsXPTCStubBaseFv	endp

; 30 STUB_ENTRY(28)

Stub28__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 01ch
Stub28__14nsXPTCStubBaseFv	endp

; 29 STUB_ENTRY(27)

Stub27__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 01bh
Stub27__14nsXPTCStubBaseFv	endp

; 28 STUB_ENTRY(26)

Stub26__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 01ah
Stub26__14nsXPTCStubBaseFv	endp

; 27 STUB_ENTRY(25)

Stub25__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 019h
Stub25__14nsXPTCStubBaseFv	endp

; 26 STUB_ENTRY(24)

Stub24__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 018h
Stub24__14nsXPTCStubBaseFv	endp

; 25 STUB_ENTRY(23)

Stub23__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 017h
Stub23__14nsXPTCStubBaseFv	endp

; 24 STUB_ENTRY(22)

Stub22__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 016h
Stub22__14nsXPTCStubBaseFv	endp

; 23 STUB_ENTRY(21)

Stub21__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 015h
Stub21__14nsXPTCStubBaseFv	endp

; 22 STUB_ENTRY(20)

Stub20__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 014h
Stub20__14nsXPTCStubBaseFv	endp

; 21 STUB_ENTRY(19)

Stub19__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 013h
Stub19__14nsXPTCStubBaseFv	endp

; 20 STUB_ENTRY(18)

Stub18__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 012h
Stub18__14nsXPTCStubBaseFv	endp

; 19 STUB_ENTRY(17)

Stub17__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 011h
Stub17__14nsXPTCStubBaseFv	endp

; 18 STUB_ENTRY(16)

Stub16__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 010h
Stub16__14nsXPTCStubBaseFv	endp

; 17 STUB_ENTRY(15)

Stub15__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0fh
Stub15__14nsXPTCStubBaseFv	endp

; 16 STUB_ENTRY(14)

Stub14__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0eh
Stub14__14nsXPTCStubBaseFv	endp

; 15 STUB_ENTRY(13)

Stub13__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0dh
Stub13__14nsXPTCStubBaseFv	endp

; 14 STUB_ENTRY(12)

Stub12__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0ch
Stub12__14nsXPTCStubBaseFv	endp

; 13 STUB_ENTRY(11)

Stub11__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0bh
Stub11__14nsXPTCStubBaseFv	endp

; 12 STUB_ENTRY(10)

Stub10__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 0ah
Stub10__14nsXPTCStubBaseFv	endp

; 11 STUB_ENTRY(9)

Stub9__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 09h
Stub9__14nsXPTCStubBaseFv	endp

; 10 STUB_ENTRY(8)

Stub8__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 08h
Stub8__14nsXPTCStubBaseFv	endp

; 9 STUB_ENTRY(7)

Stub7__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 07h
Stub7__14nsXPTCStubBaseFv	endp

; 8 STUB_ENTRY(6)

Stub6__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 06h
Stub6__14nsXPTCStubBaseFv	endp

; 7 STUB_ENTRY(5)

Stub5__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 05h
Stub5__14nsXPTCStubBaseFv	endp

; 6 STUB_ENTRY(4)

Stub4__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 04h
Stub4__14nsXPTCStubBaseFv	endp

; 5 STUB_ENTRY(3)

Stub3__14nsXPTCStubBaseFv	PROC OPTLINK EXPORT
        setentidx 03h
Stub3__14nsXPTCStubBaseFv	endp


        END

