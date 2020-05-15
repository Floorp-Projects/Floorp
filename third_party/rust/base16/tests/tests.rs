
#![cfg_attr(not(feature = "std"), no_std)]

#[cfg(feature = "alloc")]
#[macro_use]
extern crate alloc;

use base16::*;

const ALL_LOWER: &[&str] = &[
    "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0a", "0b",
    "0c", "0d", "0e", "0f", "10", "11", "12", "13", "14", "15", "16", "17",
    "18", "19", "1a", "1b", "1c", "1d", "1e", "1f", "20", "21", "22", "23",
    "24", "25", "26", "27", "28", "29", "2a", "2b", "2c", "2d", "2e", "2f",
    "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3a", "3b",
    "3c", "3d", "3e", "3f", "40", "41", "42", "43", "44", "45", "46", "47",
    "48", "49", "4a", "4b", "4c", "4d", "4e", "4f", "50", "51", "52", "53",
    "54", "55", "56", "57", "58", "59", "5a", "5b", "5c", "5d", "5e", "5f",
    "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6a", "6b",
    "6c", "6d", "6e", "6f", "70", "71", "72", "73", "74", "75", "76", "77",
    "78", "79", "7a", "7b", "7c", "7d", "7e", "7f", "80", "81", "82", "83",
    "84", "85", "86", "87", "88", "89", "8a", "8b", "8c", "8d", "8e", "8f",
    "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9a", "9b",
    "9c", "9d", "9e", "9f", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
    "a8", "a9", "aa", "ab", "ac", "ad", "ae", "af", "b0", "b1", "b2", "b3",
    "b4", "b5", "b6", "b7", "b8", "b9", "ba", "bb", "bc", "bd", "be", "bf",
    "c0", "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8", "c9", "ca", "cb",
    "cc", "cd", "ce", "cf", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
    "d8", "d9", "da", "db", "dc", "dd", "de", "df", "e0", "e1", "e2", "e3",
    "e4", "e5", "e6", "e7", "e8", "e9", "ea", "eb", "ec", "ed", "ee", "ef",
    "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "fa", "fb",
    "fc", "fd", "fe", "ff",
];

const ALL_UPPER: &[&str] = &[
    "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B",
    "0C", "0D", "0E", "0F", "10", "11", "12", "13", "14", "15", "16", "17",
    "18", "19", "1A", "1B", "1C", "1D", "1E", "1F", "20", "21", "22", "23",
    "24", "25", "26", "27", "28", "29", "2A", "2B", "2C", "2D", "2E", "2F",
    "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3A", "3B",
    "3C", "3D", "3E", "3F", "40", "41", "42", "43", "44", "45", "46", "47",
    "48", "49", "4A", "4B", "4C", "4D", "4E", "4F", "50", "51", "52", "53",
    "54", "55", "56", "57", "58", "59", "5A", "5B", "5C", "5D", "5E", "5F",
    "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6A", "6B",
    "6C", "6D", "6E", "6F", "70", "71", "72", "73", "74", "75", "76", "77",
    "78", "79", "7A", "7B", "7C", "7D", "7E", "7F", "80", "81", "82", "83",
    "84", "85", "86", "87", "88", "89", "8A", "8B", "8C", "8D", "8E", "8F",
    "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9A", "9B",
    "9C", "9D", "9E", "9F", "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
    "A8", "A9", "AA", "AB", "AC", "AD", "AE", "AF", "B0", "B1", "B2", "B3",
    "B4", "B5", "B6", "B7", "B8", "B9", "BA", "BB", "BC", "BD", "BE", "BF",
    "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CA", "CB",
    "CC", "CD", "CE", "CF", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
    "D8", "D9", "DA", "DB", "DC", "DD", "DE", "DF", "E0", "E1", "E2", "E3",
    "E4", "E5", "E6", "E7", "E8", "E9", "EA", "EB", "EC", "ED", "EE", "EF",
    "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "FA", "FB",
    "FC", "FD", "FE", "FF",
];

#[cfg(feature = "alloc")]
#[test]
fn test_exhaustive_bytes_encode() {
    for i in 0..256 {
        assert_eq!(&encode_lower(&[i as u8]), ALL_LOWER[i]);
        assert_eq!(&encode_upper(&[i as u8]), ALL_UPPER[i]);
    }
}

#[cfg(feature = "alloc")]
#[test]
fn test_exhaustive_bytes_decode() {
    for i in 0..16 {
        for j in 0..16 {
            let all_cases = format!("{0:x}{1:x}{0:x}{1:X}{0:X}{1:x}{0:X}{1:X}", i, j);
            let byte = i * 16 + j;
            let expect = &[byte, byte, byte, byte];
            assert_eq!(&decode(&all_cases).unwrap(), expect,
                        "Failed for {}", all_cases);
        }
    }
    for b in 0..256 {
        let i = b as u8;
        let expected = match i {
            b'0' | b'1' | b'2' | b'3' | b'4' | b'5' | b'6' | b'7' | b'8' | b'9' => Ok(vec![i - b'0']),
            b'a' | b'b' | b'c' | b'd' | b'e' | b'f' => Ok(vec![i - b'a' + 10]),
            b'A' | b'B' | b'C' | b'D' | b'E' | b'F' => Ok(vec![i - b'A' + 10]),
            _ => Err(DecodeError::InvalidByte { byte: i, index: 1 })
        };
        assert_eq!(decode(&[b'0', i]), expected);
    }
}

#[cfg(feature = "alloc")]
#[test]
fn test_decode_errors() {
    let mut buf = decode(b"686f6d61646f6b61").unwrap();
    let orig = buf.clone();

    assert_eq!(buf.len(), 8);

    assert_eq!(decode_buf(b"abc", &mut buf),
                Err(DecodeError::InvalidLength { length: 3 }));
    assert_eq!(buf, orig);

    assert_eq!(decode_buf(b"6d61646f686f6d75g_", &mut buf),
                Err(DecodeError::InvalidByte { byte: b'g', index: 16 }));
    assert_eq!(buf, orig);
}

#[test]
#[should_panic]
fn test_panic_slice_encode() {
    let mut slice = [0u8; 8];
    encode_config_slice(b"Yuasa", EncodeLower, &mut slice);
}

#[test]
#[should_panic]
fn test_panic_slice_decode() {
    let mut slice = [0u8; 32];
    let input = b"4920646f6e277420636172652074686174206d7563682061626f757420504d4d4d20544248";
    let _ignore = decode_slice(&input[..], &mut slice);
}

#[test]
fn test_enc_slice_exact_fit() {
    let mut slice = [0u8; 12];
    let res = encode_config_slice(b"abcdef", EncodeLower, &mut slice);
    assert_eq!(res, 12);
    assert_eq!(&slice, b"616263646566")
}

#[test]
fn test_exhaustive_encode_byte() {
    for i in 0..256 {
        let byte = i as u8;
        let su = ALL_UPPER[byte as usize].as_bytes();
        let sl = ALL_LOWER[byte as usize].as_bytes();
        let tu = encode_byte(byte, EncodeUpper);
        let tl = encode_byte(byte, EncodeLower);

        assert_eq!(tu[0], su[0]);
        assert_eq!(tu[1], su[1]);

        assert_eq!(tl[0], sl[0]);
        assert_eq!(tl[1], sl[1]);

        assert_eq!(tu, encode_byte_u(byte));
        assert_eq!(tl, encode_byte_l(byte));
    }
}

const HEX_TO_VALUE: &[(u8, u8)] = &[
    (b'0', 0x0), (b'1', 0x1), (b'2', 0x2), (b'3', 0x3), (b'4', 0x4),
    (b'5', 0x5), (b'6', 0x6), (b'7', 0x7), (b'8', 0x8), (b'9', 0x9),
    (b'a', 0xa), (b'b', 0xb), (b'c', 0xc), (b'd', 0xd), (b'e', 0xe), (b'f', 0xf),
    (b'A', 0xA), (b'B', 0xB), (b'C', 0xC), (b'D', 0xD), (b'E', 0xE), (b'F', 0xF),
];

#[test]
fn test_exhaustive_decode_byte() {
    let mut expected = [None::<u8>; 256];
    for &(k, v) in HEX_TO_VALUE {
        expected[k as usize] = Some(v);
    }
    for i in 0..256 {
        assert_eq!(decode_byte(i as u8), expected[i]);
    }
}
