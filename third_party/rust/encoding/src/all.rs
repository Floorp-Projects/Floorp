// This is a part of rust-encoding.
// Copyright (c) 2013, Kang Seonghoon.
// See README.md and LICENSE.txt for details.

//! A list of all supported encodings. Useful for encodings fixed in the compile time.

use index_singlebyte as index;
use codec;
use types::EncodingRef;

macro_rules! unique(
    ($(#[$attr:meta])* var=$var:ident, mod=$($module:ident)::+, val=$val:ident) => (
        unique!($(#[$attr])* var=$var, mod=$($module)::+, ty=$val, val=$val);
    );
    ($(#[$attr:meta])* var=$var:ident, mod=$($module:ident)::+, ty=$ty:ident, val=$val:ident) => (
        $(#[$attr])* pub const $var: &'static $($module)::+::$ty = &$($module)::+::$val;
    );
);

macro_rules! singlebyte(
    ($(#[$attr:meta])* var=$var:ident, mod=$($module:ident)::+, name=$name:expr) => (
        singlebyte!($(#[$attr])* var=$var, mod=$($module)::+, name=$name, whatwg=None);
    );
    ($(#[$attr:meta])* var=$var:ident, mod=$($module:ident)::+, name|whatwg=$name:expr) => (
        singlebyte!($(#[$attr])* var=$var, mod=$($module)::+, name=$name, whatwg=Some($name));
    );
    ($(#[$attr:meta])* var=$var:ident, mod=$($module:ident)::+,
                       name=$name:expr, whatwg=$whatwg:expr) => (
        $(#[$attr])* pub const $var: &'static codec::singlebyte::SingleByteEncoding =
            &codec::singlebyte::SingleByteEncoding {
                name: $name,
                whatwg_name: $whatwg,
                index_forward: $($module)::+::forward,
                index_backward: $($module)::+::backward,
            };
    )
);

unique!(var=ERROR, mod=codec::error, val=ErrorEncoding);
unique!(var=ASCII, mod=codec::ascii, val=ASCIIEncoding);
singlebyte!(var=IBM866, mod=index::ibm866, name|whatwg="ibm866");
singlebyte!(var=ISO_8859_1, mod=codec::singlebyte::iso_8859_1, name="iso-8859-1");
singlebyte!(var=ISO_8859_2, mod=index::iso_8859_2, name|whatwg="iso-8859-2");
singlebyte!(var=ISO_8859_3, mod=index::iso_8859_3, name|whatwg="iso-8859-3");
singlebyte!(var=ISO_8859_4, mod=index::iso_8859_4, name|whatwg="iso-8859-4");
singlebyte!(var=ISO_8859_5, mod=index::iso_8859_5, name|whatwg="iso-8859-5");
singlebyte!(var=ISO_8859_6, mod=index::iso_8859_6, name|whatwg="iso-8859-6");
singlebyte!(var=ISO_8859_7, mod=index::iso_8859_7, name|whatwg="iso-8859-7");
singlebyte!(var=ISO_8859_8, mod=index::iso_8859_8, name|whatwg="iso-8859-8");
singlebyte!(var=ISO_8859_10, mod=index::iso_8859_10, name|whatwg="iso-8859-10");
singlebyte!(var=ISO_8859_13, mod=index::iso_8859_13, name|whatwg="iso-8859-13");
singlebyte!(var=ISO_8859_14, mod=index::iso_8859_14, name|whatwg="iso-8859-14");
singlebyte!(var=ISO_8859_15, mod=index::iso_8859_15, name|whatwg="iso-8859-15");
singlebyte!(var=ISO_8859_16, mod=index::iso_8859_16, name|whatwg="iso-8859-16");
singlebyte!(var=KOI8_R, mod=index::koi8_r, name|whatwg="koi8-r");
singlebyte!(var=KOI8_U, mod=index::koi8_u, name|whatwg="koi8-u");
singlebyte!(var=MAC_ROMAN, mod=index::macintosh, name="mac-roman", whatwg=Some("macintosh"));
singlebyte!(var=WINDOWS_874, mod=index::windows_874, name|whatwg="windows-874");
singlebyte!(var=WINDOWS_1250, mod=index::windows_1250, name|whatwg="windows-1250");
singlebyte!(var=WINDOWS_1251, mod=index::windows_1251, name|whatwg="windows-1251");
singlebyte!(var=WINDOWS_1252, mod=index::windows_1252, name|whatwg="windows-1252");
singlebyte!(var=WINDOWS_1253, mod=index::windows_1253, name|whatwg="windows-1253");
singlebyte!(var=WINDOWS_1254, mod=index::windows_1254, name|whatwg="windows-1254");
singlebyte!(var=WINDOWS_1255, mod=index::windows_1255, name|whatwg="windows-1255");
singlebyte!(var=WINDOWS_1256, mod=index::windows_1256, name|whatwg="windows-1256");
singlebyte!(var=WINDOWS_1257, mod=index::windows_1257, name|whatwg="windows-1257");
singlebyte!(var=WINDOWS_1258, mod=index::windows_1258, name|whatwg="windows-1258");
singlebyte!(var=MAC_CYRILLIC, mod=index::x_mac_cyrillic,
            name="mac-cyrillic", whatwg=Some("x-mac-cyrillic"));
unique!(var=UTF_8, mod=codec::utf_8, val=UTF8Encoding);
unique!(var=UTF_16LE, mod=codec::utf_16, ty=UTF16LEEncoding, val=UTF_16LE_ENCODING);
unique!(var=UTF_16BE, mod=codec::utf_16, ty=UTF16BEEncoding, val=UTF_16BE_ENCODING);
unique!(var=WINDOWS_949, mod=codec::korean, val=Windows949Encoding);
unique!(var=EUC_JP, mod=codec::japanese, val=EUCJPEncoding);
unique!(var=WINDOWS_31J, mod=codec::japanese, val=Windows31JEncoding);
unique!(var=ISO_2022_JP, mod=codec::japanese, val=ISO2022JPEncoding);
unique!(var=GBK, mod=codec::simpchinese, ty=GBKEncoding, val=GBK_ENCODING);
unique!(var=GB18030, mod=codec::simpchinese, ty=GB18030Encoding, val=GB18030_ENCODING);
unique!(var=HZ, mod=codec::simpchinese, val=HZEncoding);
unique!(var=BIG5_2003, mod=codec::tradchinese, val=BigFive2003Encoding);

pub mod whatwg {
    use index_singlebyte as index;
    use codec;

    singlebyte!(var=X_USER_DEFINED, mod=codec::whatwg::x_user_defined,
                name="pua-mapped-binary", whatwg=Some("x-user-defined"));
    singlebyte!(var=ISO_8859_8_I, mod=index::iso_8859_8, name|whatwg="iso-8859-8-i");
    unique!(var=REPLACEMENT, mod=codec::whatwg, val=EncoderOnlyUTF8Encoding);
}

/// Returns a list of references to the encodings available.
pub fn encodings() -> &'static [EncodingRef] {
    // TODO should be generated automatically
    const ENCODINGS: &'static [EncodingRef] = &[
        ERROR,
        ASCII,
        IBM866,
        ISO_8859_1,
        ISO_8859_2,
        ISO_8859_3,
        ISO_8859_4,
        ISO_8859_5,
        ISO_8859_6,
        ISO_8859_7,
        ISO_8859_8,
        ISO_8859_10,
        ISO_8859_13,
        ISO_8859_14,
        ISO_8859_15,
        ISO_8859_16,
        KOI8_R,
        KOI8_U,
        MAC_ROMAN,
        WINDOWS_874,
        WINDOWS_1250,
        WINDOWS_1251,
        WINDOWS_1252,
        WINDOWS_1253,
        WINDOWS_1254,
        WINDOWS_1255,
        WINDOWS_1256,
        WINDOWS_1257,
        WINDOWS_1258,
        MAC_CYRILLIC,
        UTF_8,
        UTF_16LE,
        UTF_16BE,
        WINDOWS_949,
        EUC_JP,
        WINDOWS_31J,
        ISO_2022_JP,
        GBK,
        GB18030,
        HZ,
        BIG5_2003,
        whatwg::X_USER_DEFINED,
        whatwg::ISO_8859_8_I,
        whatwg::REPLACEMENT,
    ];
    ENCODINGS
}

