/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <Types.r>

resource 'CURS' (128, locked, preload) {
	$"0000 4000 6000 7000 7800 7C00 7E00 7F00"
	$"7F8E 7C1B 6C03 4606 060C 0300 030C",
	$"C000 E000 F000 F800 FC00 FE00 FF00 FF80"
	$"FFCE FFDB FE03 EF06 CF0C 8780 078C 0380",
	{1, 1}
};

resource 'CURS' (129, locked, preload) {
	$"0F00 30C0 4020 4620 8610 9F90 9F90 8610"
	$"4620 4020 30F0 0F38 001C 000E 0007 0002",
	$"0F00 3FC0 7FE0 7FE0 FFF0 FFF0 FFF0 FFF0"
	$"7FE0 7FE0 3FF0 0F38 001C 000E 0007 0002",
	{6, 6}
};

resource 'CURS' (130, locked, preload) {
	$"0F00 30C0 4020 4020 8010 9F90 9F90 8010"
	$"4020 4020 30F0 0F38 001C 000E 0007 0002",
	$"0F00 3FC0 7FE0 7FE0 FFF0 FFF0 FFF0 FFF0"
	$"7FE0 7FE0 3FF0 0F38 001C 000E 0007 0002",
	{6, 6}
};

resource 'CURS' (131, locked, preload) {
	$"0000 4000 6000 7000 7800 7C00 7E00 7F00"
	$"7F80 7000 6600 4900 1480 1280 0900 06",
	$"C000 E000 F000 F800 FC00 FE00 FF00 FF80"
	$"FFC0 FF80 FF00 FF80 FFC0 3FC0 1F80 0F",
	{0, 0}
};

resource 'CURS' (132, locked, preload) {
	$"0000 0660 0660 0660 0660 2664 6666 FE7F"
	$"FE7F 6666 2664 0660 0660 0660 0660",
	$"0FF0 0FF0 0FF0 0FF0 2FF4 6FF6 FFFF FFFF"
	$"FFFF FFFF 6FF6 2FF4 0FF0 0FF0 0FF0 0FF0",
	{7, 7}
};

resource 'CURS' (133, locked, preload) {
	$"0180 03C0 07E0 0180 0180 7FFE 7FFE 0000"
	$"0000 7FFE 7FFE 0180 0180 07E0 03C0 0180",
	$"03C0 07E0 0FF0 03C0 FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF 03C0 0FF0 07E0 03C0",
	{7, 7}
};

resource 'CURS' (134, locked, preload) {
	$"0000 0000 0000 0000 8001 8001 4102 3FFC"
	$"4102 8001 8001",
	$"0000 0000 0000 0000 8001 8001 4102 3FFC"
	$"4102 8001 8001",
	{7, 7}
};

resource 'CURS' (135, locked, preload) {
	$"0000 0180 03C0 07E0 0180 0180 0180 7FFE"
	$"7FFE",
	$"0180 03C0 07E0 0FF0 0FF0 03C0 FFFF FFFF"
	$"FFFF FFFF",
	{7, 7}
};

resource 'CURS' (136, locked, preload) {
	$"0000 0000 0000 0000 0000 0000 0000 7FFE"
	$"7FFE 0180 0180 0180 07E0 03C0 0180",
	$"0000 0000 0000 0000 0000 0000 FFFF FFFF"
	$"FFFF FFFF 03C0 0FF0 0FF0 07E0 03C0 0180",
	{7, 7}
};

resource 'CURS' (137, locked, preload) {
	$"0000 0000 0000 1E18 1C38 1E70 17E0 03C0"
	$"0380 0700 0E00 1C00 18",
	$"0000 0000 3F80 3F18 3E38 3E70 3FE0 33C0"
	$"2380 0700 0E00 1C00 18",
	{7, 7}
};

resource 'CURS' (138, locked, preload) {
	$"0000 0000 0000 0018 0038 0070 00E0 01C0"
	$"03C0 07E8 0E78 1C38 1878",
	$"0000 0000 0000 0018 0038 0070 00E0 01C4"
	$"03CC 07FC 0E7C 1C7C 18FC 01FC",
	{8, 8}
};

resource 'CURS' (139, locked, preload) {
	$"0000 0000 0000 1878 1C38 0E78 07E8 03C0"
	$"01C0 00E0 0070 0038 0018",
	$"0000 0000 01FC 18FC 1C7C 0E7C 07FC 03CC"
	$"01C4 00E0 0070 0038 0018",
	{7, 8}
};

resource 'CURS' (140, locked, preload) {
	$"0000 0000 0000 1800 1C00 0E00 0700 0380"
	$"03C0 17E0 1E70 1C38 1E18",
	$"0000 0000 0000 1800 1C00 0E00 0700 2380"
	$"33C0 3FE0 3E70 3E38 3F18 3F80",
	{8, 6}
};

resource 'CURS' (141, locked, preload) {
	$"0000 0180 03C0 07E0 0180 0180 0180 7FFE"
	$"7FFE 0180 0180 0180 07E0 03C0 0180",
	$"0180 03C0 07E0 0FF0 0FF0 03C0 FFFF FFFF"
	$"FFFF FFFF 03C0 0FF0 0FF0 07E0 03C0 0180",
	{7, 7}
};

resource 'CURS' (142, locked, preload) {
	$"0000 0000 0000 1878 1C38 0E78 07E8 03C0"
	$"03C0 17E0 1E70 1C38 1E18",
	$"0000 0000 387C 3CFC 3E7C 1FFC 0FFC 07E8"
	$"17E0 3FF0 3FF8 3E7C 3F3C 3E1C",
	{7, 7}
};

resource 'CURS' (143, locked, preload) {
	$"0000 0000 0000 1E18 1C38 1E70 17E0 03C0"
	$"03C0 07E8 0E78 1C38 1878",
	$"0000 0000 3E1C 3F3C 3E7C 3FF8 3FF0 17E0"
	$"07E8 0FFC 1FFC 3E7C 3CFC 387C",
	{7, 7}
};

resource 'CURS' (200, locked, preload) {
    $"0000 4000 6000 7000 7800 7C00 7E00 7F00"
    $"7FB8 7C74 6CF2 46FE 069E 035C 0338 0000",
    $"C000 E000 F000 F800 FC00 FE00 FF00 FFB8"
    $"FFFC FFFE FFFF EFFF CFFF 87FE 07FC 03B8",
    {1,1}
};

resource 'CURS' (201, locked, preload) {
    $"0000 4000 6000 7000 7800 7C00 7E00 7F00"
    $"7FB8 7C7C 6CBA 4692 06BA 037C 0338 0000",
    $"C000 E000 F000 F800 FC00 FE00 FF00 FFB8"
    $"FFFC FFFE FFFF EFFF CFFF 87FE 07FC 03B8",
    {1,1}
};

resource 'CURS' (202, locked, preload) {
    $"0000 4000 6000 7000 7800 7C00 7E00 7F00"
    $"7FB8 7C5C 6C9E 46FE 06F2 0374 0338 0000",
    $"C000 E000 F000 F800 FC00 FE00 FF00 FFB8"
    $"FFFC FFFE FFFF EFFF CFFF 87FE 07FC 03B8",
    {1,1}
};

resource 'CURS' (203, locked, preload) {
    $"0000 4000 6000 7000 7800 7C00 7E00 7F00"
    $"7FB8 7C44 6CEE 46FE 06EE 0344 0338 0000",
    $"C000 E000 F000 F800 FC00 FE00 FF00 FFB8"
    $"FFFC FFFE FFFF EFFF CFFF 87FE 07FC 03B8",
    {1,1}
};

data 'TMPL' (129, "ldes") {
	$"0756 6572 7369 6F6E 4457 5244 0452 6F77"            /* .VersionDWRD.Row */
	$"7344 5752 4407 436F 6C75 6D6E 7344 5752"            /* sDWRD.ColumnsDWR */
	$"440B 4365 6C6C 2048 6569 6768 7444 5752"            /* D.Cell HeightDWR */
	$"440A 4365 6C6C 2057 6964 7468 4457 5244"            /* DÂCell WidthDWRD */
	$"0B56 6572 7420 5363 726F 6C6C 424F 4F4C"            /* .Vert ScrollBOOL */
	$"0C48 6F72 697A 2053 6372 6F6C 6C42 4F4F"            /* .Horiz ScrollBOO */
	$"4C07 4C44 4546 2049 4444 5752 4408 4861"            /* L.LDEF IDDWRD.Ha */
	$"7320 4772 6F77 424F 4F4C"                           /* s GrowBOOL */
};

resource 'ldes' (128, purgeable) {
	versionZero {
		0,
		1,
		0,
		0,
		hasVertScroll,
		noHorizScroll,
		0,
		noGrowSpace
	}
};
