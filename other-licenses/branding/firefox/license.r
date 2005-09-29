// See /System/Library/Frameworks/CoreServices.framework/Frameworks/CarbonCore.framework/Headers/Script.h for language IDs.
data 'LPic' (5000) {
  // Default language ID, 0 = English
  $"0000"
  // Number of entries in list
  $"0001"

  // Entry 1
  // Language ID, 0 = English
  $"0000"
  // Resource ID, 0 = STR#/TEXT/styl 5000
  $"0000"
  // Multibyte language, 0 = no
  $"0000"
};

resource 'STR#' (5000, "English") {
  {
    // Language (unused?) = English
    "English",
    // Accept (Agree)
    "Accept",
    // Decline (Disagree)
    "Decline",
    // Print, ellipsis is 0xC9
    "Print…",
    // Save As, ellipsis is 0xC9
    "Save As…",
    // Descriptive text, curly quotes are 0xD2 and 0xD3
    "You are about to install Mozilla Firefox.\n"
    "\n"
    "Please read the license agreement.  If you agree to its terms and accept, click “Accept” to access the software.  Otherwise, click “Decline” to cancel."
  };
};

// Beware of 1024(?) byte (character?) line length limitation.  Split up long
// lines.
// If straight quotes are used ("), remember to escape them (\").
// Newline is \n, to leave a blank line, use two of them.
// 0xD2 and 0xD3 are curly double-quotes ("), 0xD4 and 0xD5 are curly
//   single quotes ('), 0xD5 is also the apostrophe.
data 'TEXT' (5000, "English") {
  "FOR TRANSLATIONS OF THIS LICENSE INTO SELECTED LANGUAGES, PLEASE VISIT WWW.MOZILLA.ORG/LICENSING.\n"
  "\n"
  "MOZILLA FIREFOX END-USER SOFTWARE LICENSE AGREEMENT\n"
  "Version 1.1\n"
  "\n"
  "A SOURCE CODE VERSION OF CERTAIN FIREFOX BROWSER FUNCTIONALITY THAT YOU MAY USE, MODIFY AND DISTRIBUTE IS AVAILABLE TO YOU FREE-OF-CHARGE FROM WWW.MOZILLA.ORG UNDER THE MOZILLA PUBLIC LICENSE and other open source software licenses.\n"
  "\n"
  "The accompanying executable code version of Mozilla Firefox and related documentation (the “Product”) is made available to you under the terms of this MOZILLA FIREFOX END-USER SOFTWARE LICENSE AGREEMENT (THE “AGREEMENT”). BY CLICKING THE “ACCEPT” BUTTON, OR BY INSTALLING OR USING THE MOZILLA FIREFOX BROWSER, YOU ARE CONSENTING TO BE BOUND BY THE AGREEMENT. IF YOU DO NOT AGREE TO THE TERMS AND CONDITIONS OF THIS AGREEMENT, DO NOT CLICK THE “ACCEPT” BUTTON, AND DO NOT INSTALL OR USE ANY PART OF THE MOZILLA FIREFOX BROWSER.\n"
  "\n"
  "DURING THE MOZILLA FIREFOX INSTALLATION PROCESS, AND AT LATER TIMES, YOU MAY BE GIVEN THE OPTION OF INSTALLING ADDITIONAL COMPONENTS FROM THIRD-PARTY SOFTWARE PROVIDERS. THE INSTALLATION AND USE OF THOSE THIRD-PARTY COMPONENTS MAY BE GOVERNED BY ADDITIONAL LICENSE AGREEMENTS.\n"
  "\n"
  "1. LICENSE GRANT. The Mozilla Corporation grants you a non-exclusive license to use the executable code version of the Product. This Agreement will also govern any software upgrades provided by Mozilla that replace and/or supplement the original Product, unless such upgrades are accompanied by a separate license, in which case the terms of that license will govern.\n"
  "\n"
  "2. TERMINATION. If you breach this Agreement your right to use the Product will terminate immediately and without notice, but all provisions of this Agreement except the License Grant (Paragraph 1) will survive termination and continue in effect. Upon termination, you must destroy all copies of the Product.\n"
  "\n"
  "3. PROPRIETARY RIGHTS. Portions of the Product are available in source code form under the terms of the Mozilla Public License and other open source licenses (collectively, “Open Source Licenses”) at http://www.mozilla.org. Nothing in this Agreement will be construed to limit any rights granted under the Open Source Licenses. Subject to the foregoing, Mozilla, for itself and on behalf of its licensors, hereby reserves all intellectual property rights in the Product, except for the rights expressly granted in this Agreement. You may not remove or alter any trademark, logo, copyright or other proprietary notice in or on the Product. This license does not grant you any right to use the trademarks, service marks or logos of Mozilla or its licensors.\n"
  "\n"
  "4. DISCLAIMER OF WARRANTY. THE PRODUCT IS PROVIDED “AS IS” WITH ALL FAULTS. TO THE EXTENT PERMITTED BY LAW, MOZILLA AND MOZILLA’S DISTRIBUTORS, LICENSORS HEREBY DISCLAIM ALL WARRANTIES, WHETHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION WARRANTIES THAT THE PRODUCT IS FREE OF DEFECTS, MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE AND NON-INFRINGING. YOU BEAR ENTIRE RISK AS TO SELECTING THE PRODUCT FOR YOUR PURPOSES AND AS TO THE QUALITY AND PERFORMANCE OF THE PRODUCT. THIS LIMITATION WILL APPLY NOTWITHSTANDING THE FAILURE OF ESSENTIAL PURPOSE OF ANY REMEDY. SOME JURISDICTIONS DO NOT ALLOW THE EXCLUSION OR LIMITATION OF IMPLIED WARRANTIES, SO THIS DISCLAIMER MAY NOT APPLY TO YOU.\n"
  "\n"
  "5. LIMITATION OF LIABILITY. EXCEPT AS REQUIRED BY LAW, MOZILLA AND ITS DISTRIBUTORS, DIRECTORS, LICENSORS, CONTRIBUTORS AND AGENTS (COLLECTIVELY, THE “MOZILLA GROUP”) WILL NOT BE LIABLE FOR ANY INDIRECT, SPECIAL, INCIDENTAL, CONSEQUENTIAL OR EXEMPLARY DAMAGES ARISING OUT OF OR IN ANY WAY RELATING TO THIS AGREEMENT OR THE USE OF OR INABILITY TO USE THE PRODUCT, INCLUDING WITHOUT LIMITATION DAMAGES FOR LOSS OF GOODWILL, WORK STOPPAGE, LOST PROFITS, LOSS OF DATA, AND COMPUTER FAILURE OR MALFUNCTION, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES AND REGARDLESS OF THE THEORY (CONTRACT, TORT OR OTHERWISE) UPON WHICH SUCH CLAIM IS BASED. THE MOZILLA GROUP’S COLLECTIVE LIABILITY UNDER THIS AGREEMENT WILL NOT EXCEED THE GREATER OF $500 (FIVE HUNDRED DOLLARS) AND THE FEES PAID BY YOU UNDER THIS LICENSE (IF ANY). SOME JURISDICTIONS DO NOT ALLOW THE EXCLUSION OR LIMITATION OF INCIDENTAL, CONSEQUENTIAL OR SPECIAL DAMAGES, SO THIS EXCLUSION AND LIMITATION MAY NOT APPLY TO YOU.\n"
  "\n"
  "6. EXPORT CONTROLS. This license is subject to all applicable export restrictions. You must comply with all export and import laws and restrictions and regulations of any United States or foreign agency or authority relating to the Product and its use.\n"
  "\n"
  "7. U.S. GOVERNMENT END-USERS. The Product is a “commercial item,” as that term is defined in 48 C.F.R. 2.101, consisting of “commercial computer software” and “commercial computer software documentation,” as such terms are used in 48 C.F.R. 12.212 (Sept. 1995) and 48 C.F.R. 227.7202 (June 1995). Consistent with 48 C.F.R. 12.212, 48 C.F.R. 27.405(b)(2) (June 1998) and 48 C.F.R. 227.7202, all U.S. Government End Users acquire the Product with only those rights as set forth herein.\n"
  "\n"
  "8. MISCELLANEOUS. (a) This Agreement constitutes the entire agreement between Mozilla and you concerning the subject matter hereof, and it may only be modified by a written amendment signed by an authorized executive of Mozilla. (b) Except to the extent applicable law, if any, provides otherwise, this Agreement will be governed by the laws of the state of California, U.S.A., excluding its conflict of law provisions. (c) This Agreement will not be governed by the United Nations Convention on Contracts for the International Sale of Goods. (d) If any part of this Agreement is held invalid or unenforceable, that part will be construed to reflect the parties’ original intent, and the remaining portions will remain in full force and effect. "
  "(e) A waiver by either party of any term or condition of this Agreement or any breach thereof, in any one instance, will not waive such term or condition or any subsequent breach thereof. (f) Except as required by law, the controlling language of this Agreement is English. (g) You may assign your rights under this Agreement to any party that consents to, and agrees to be bound by, its terms; the Mozilla Corporation may assign its rights under this Agreement without condition. (h) This Agreement will be binding upon and will inure to the benefit of the parties, their successors and permitted assigns.\n"
};

data 'styl' (5000, "English") {
  // Number of styles following = 3
  $"0003"

  // Style 1.  This is used to display the message about translations.
  // Start character = 0
  $"0000 0000"
  // Height = 16
  $"0010"
  // Ascent = 12
  $"000C"
  // Font family = 1024 (Lucida Grande)
  $"0400"
  // Style bitfield, 0x1=bold 0x2=italic 0x4=underline 0x8=outline
  // 0x10=shadow 0x20=condensed 0x40=extended
  $"00"
  // Style, unused?
  $"02"
  // Size = 12 point
  $"000C"
  // Color, RGB
  $"0000 0000 0000"

  // Style 2.  This is used to display the header lines in bold text.
  // Start character = 99
  $"0000 0063"
  // Height = 16
  $"0010"
  // Ascent = 12
  $"000C"
  // Font family = 1024 (Lucida Grande)
  $"0400"
  // Style bitfield, 0x1=bold 0x2=italic 0x4=underline 0x8=outline
  // 0x10=shadow 0x20=condensed 0x40=extended
  $"01"
  // Style, unused?
  $"02"
  // Size = 12 point
  $"000C"
  // Color, RGB
  $"0000 0000 0000"

  // Style 3.  This is used to display the body.
  // Start character = 151
  $"0000 0097"
  // Height = 16
  $"0010"
  // Ascent = 12
  $"000C"
  // Font family = 1024 (Lucida Grande)
  $"0400"
  // Style bitfield, 0x1=bold 0x2=italic 0x4=underline 0x8=outline
  // 0x10=shadow 0x20=condensed 0x40=extended
  $"00"
  // Style, unused?
  $"02"
  // Size = 12 point
  $"000C"
  // Color, RGB
  $"0000 0000 0000"
};
