#[cfg(feature = "tags")]
mod tagtests {
    use serde_cbor::value::Value;
    use serde_cbor::{from_slice, to_vec};

    fn decode_hex(s: &str) -> std::result::Result<Vec<u8>, std::num::ParseIntError> {
        (0..s.len())
            .step_by(2)
            .map(|i| u8::from_str_radix(&s[i..i + 2], 16))
            .collect()
    }

    // get bytes from http://cbor.me/ trees
    fn parse_cbor_me(example: &str) -> std::result::Result<Vec<u8>, std::num::ParseIntError> {
        let hex = example
            .split("\n")
            .flat_map(|line| line.split("#").take(1))
            .collect::<Vec<&str>>()
            .join("")
            .replace(" ", "");
        decode_hex(&hex)
    }

    #[test]
    fn tagged_cbor_roundtrip() {
        let data = r#"
C1                   # tag(1)
   82                # array(2)
      C2             # tag(2)
         63          # text(3)
            666F6F   # "foo"
      C3             # tag(3)
         A1          # map(1)
            C4       # tag(4)
               61    # text(1)
                  61 # "a"
            C5       # tag(5)
               61    # text(1)
                  62 # "b"
            "#;
        let bytes1 = parse_cbor_me(&data).unwrap();
        let value1: Value = from_slice(&bytes1).unwrap();
        let bytes2 = to_vec(&value1).unwrap();
        let value2: Value = from_slice(&bytes2).unwrap();
        assert_eq!(bytes1, bytes2);
        assert_eq!(value1, value2);
    }
}
