
#![cfg_attr(not(feature = "std"), no_std)]

#[cfg(feature = "alloc")]
extern crate alloc;

// Can't run doctests for a no_std crate if it uses allocator (e.g. can't run
// them if we're using `alloc`), so we duplicate them here...
// See https://github.com/rust-lang/rust/issues/54010

#[cfg(feature = "alloc")]
#[test]
fn test_encode_lower() {
    assert_eq!(base16::encode_lower(b"Hello World"), "48656c6c6f20576f726c64");
    assert_eq!(base16::encode_lower(&[0xff, 0xcc, 0xaa]), "ffccaa");
}

#[cfg(feature = "alloc")]
#[test]
fn test_encode_upper() {
    assert_eq!(base16::encode_upper(b"Hello World"), "48656C6C6F20576F726C64");
    assert_eq!(base16::encode_upper(&[0xff, 0xcc, 0xaa]), "FFCCAA");
}

#[cfg(feature = "alloc")]
#[test]
fn test_encode_config() {
    let data = [1, 2, 3, 0xaa, 0xbb, 0xcc];
    assert_eq!(base16::encode_config(&data, base16::EncodeLower), "010203aabbcc");
    assert_eq!(base16::encode_config(&data, base16::EncodeUpper), "010203AABBCC");
}

#[cfg(feature = "alloc")]
#[test]
fn test_encode_config_buf() {
    use alloc::string::String;
    let messages = &["Taako, ", "Merle, ", "Magnus"];
    let mut buffer = String::new();
    for msg in messages {
        let bytes_written = base16::encode_config_buf(msg.as_bytes(),
                                                      base16::EncodeUpper,
                                                      &mut buffer);
        assert_eq!(bytes_written, msg.len() * 2);
    }
    assert_eq!(buffer, "5461616B6F2C204D65726C652C204D61676E7573");
}

#[test]
fn test_encode_config_slice() {
    // Writing to a statically sized buffer on the stack.
    let message = b"Wu-Tang Killa Bees";
    let mut buffer = [0u8; 1024];

    let wrote = base16::encode_config_slice(message,
                                            base16::EncodeLower,
                                            &mut buffer);

    assert_eq!(message.len() * 2, wrote);
    assert_eq!(core::str::from_utf8(&buffer[..wrote]).unwrap(),
               "57752d54616e67204b696c6c612042656573");

    // Appending to an existing buffer is possible too.
    let wrote2 = base16::encode_config_slice(b": The Swarm",
                                             base16::EncodeLower,
                                             &mut buffer[wrote..]);
    let write_end = wrote + wrote2;
    assert_eq!(core::str::from_utf8(&buffer[..write_end]).unwrap(),
               "57752d54616e67204b696c6c6120426565733a2054686520537761726d");
}

#[test]
fn test_encode_config_byte() {
    assert_eq!(base16::encode_byte(0xff, base16::EncodeLower), [b'f', b'f']);
    assert_eq!(base16::encode_byte(0xa0, base16::EncodeUpper), [b'A', b'0']);
    assert_eq!(base16::encode_byte(3, base16::EncodeUpper), [b'0', b'3']);
}

#[test]
fn test_encode_config_byte_l() {
    assert_eq!(base16::encode_byte_l(0xff), [b'f', b'f']);
    assert_eq!(base16::encode_byte_l(30), [b'1', b'e']);
    assert_eq!(base16::encode_byte_l(0x2d), [b'2', b'd']);
}

#[test]
fn test_encode_config_byte_u() {
    assert_eq!(base16::encode_byte_u(0xff), [b'F', b'F']);
    assert_eq!(base16::encode_byte_u(30), [b'1', b'E']);
    assert_eq!(base16::encode_byte_u(0x2d), [b'2', b'D']);
}

#[cfg(feature = "alloc")]
#[test]
fn test_decode() {
    assert_eq!(&base16::decode("48656c6c6f20576f726c64".as_bytes()).unwrap(),
               b"Hello World");
    assert_eq!(&base16::decode(b"deadBEEF").unwrap(),
               &[0xde, 0xad, 0xbe, 0xef]);
    // Error cases:
    assert_eq!(base16::decode(b"Not Hexadecimal!"),
               Err(base16::DecodeError::InvalidByte { byte: b'N', index: 0 }));
    assert_eq!(base16::decode(b"a"),
               Err(base16::DecodeError::InvalidLength { length: 1 }));
}

#[cfg(feature = "alloc")]
#[test]
fn test_decode_buf() {
    use alloc::vec::Vec;
    let mut result = Vec::new();
    assert_eq!(base16::decode_buf(b"4d61646f6b61", &mut result).unwrap(), 6);
    assert_eq!(base16::decode_buf(b"486F6D757261", &mut result).unwrap(), 6);
    assert_eq!(core::str::from_utf8(&result).unwrap(), "MadokaHomura");
}

#[test]
fn test_decode_slice() {
    let msg = "476f6f642072757374206c6962726172696573207573652073696c6c79206578616d706c6573";
    let mut buf = [0u8; 1024];
    assert_eq!(base16::decode_slice(&msg[..], &mut buf).unwrap(), 38);
    assert_eq!(&buf[..38], b"Good rust libraries use silly examples".as_ref());

    let msg2 = b"2E20416C736F2C20616E696D65207265666572656e636573";
    assert_eq!(base16::decode_slice(&msg2[..], &mut buf[38..]).unwrap(), 24);
    assert_eq!(&buf[38..62], b". Also, anime references".as_ref());
}

#[test]
fn test_decode_byte() {
    assert_eq!(base16::decode_byte(b'a'), Some(10));
    assert_eq!(base16::decode_byte(b'B'), Some(11));
    assert_eq!(base16::decode_byte(b'0'), Some(0));
    assert_eq!(base16::decode_byte(b'q'), None);
    assert_eq!(base16::decode_byte(b'x'), None);
}
