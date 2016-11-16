// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use freetype::{FT_UShort, FT_Short, FT_ULong, FT_Byte};

pub struct TT_OS2 {
    pub version: FT_UShort,
    pub xAvgCharWidth: FT_Short,
    pub usWeightClass: FT_UShort,
    pub usWidthClass: FT_UShort,
    pub fsType: FT_Short,
    pub ySubscriptXSize: FT_Short,
    pub ySubscriptYSize: FT_Short,
    pub ySubscriptXOffset: FT_Short,
    pub ySubscriptYOffset: FT_Short,
    pub ySuperscriptXSize: FT_Short,
    pub ySuperscriptYSize: FT_Short,
    pub ySuperscriptXOffset: FT_Short,
    pub ySuperscriptYOffset: FT_Short,
    pub yStrikeoutSize: FT_Short,
    pub yStrikeoutPosition: FT_Short,
    pub sFamilyClass: FT_Short,

    pub panose: [FT_Byte; 10],

    pub ulUnicodeRange1: FT_ULong, /* Bits 0-31   */
    pub ulUnicodeRange2: FT_ULong, /* Bits 32-63  */
    pub ulUnicodeRange3: FT_ULong, /* Bits 64-95  */
    pub ulUnicodeRange4: FT_ULong, /* Bits 96-127 */

    /* only version 1 tables */

    pub ulCodePageRange1: FT_ULong, /* Bits 0-31  */
    pub ulCodePageRange2: FT_ULong, /* Bits 32-63 */

    /* only version 2 tables */

    pub sxHeight: FT_Short,
    pub sCapHeight: FT_Short,
    pub usDefaultChar: FT_UShort,
    pub usBreakChar: FT_UShort,
    pub usMaxContext: FT_UShort,
}
