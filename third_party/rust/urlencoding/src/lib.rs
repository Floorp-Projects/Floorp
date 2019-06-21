use std::str;
use std::string::FromUtf8Error;

pub fn encode(data: &str) -> String {
    let mut escaped = String::new();
    for b in data.as_bytes().iter() {
        match *b as char {
            // Accepted characters
            'A'...'Z' | 'a'...'z' | '0'...'9' | '-' | '_' | '.' | '~' => escaped.push(*b as char),

            // Everything else is percent-encoded
            b => escaped.push_str(format!("%{:02X}", b as u32).as_str()),
        };
    }
    return escaped;
}

pub fn decode(data: &str) -> Result<String, FromUrlEncodingError> {
    validate_urlencoded_str(data)?;
    let mut unescaped_bytes: Vec<u8> = Vec::new();
    let mut bytes = data.bytes();
    // If validate_urlencoded_str returned Ok, then we know
    // every '%' is followed by 2 hex characters
    while let Some(b) = bytes.next() {
        match b as char {
            '%' => {
                let bytes_to_decode = &[bytes.next().unwrap(), bytes.next().unwrap()];
                let hex_str = str::from_utf8(bytes_to_decode).unwrap();
                unescaped_bytes.push(u8::from_str_radix(hex_str, 16).unwrap());
            },
            _ => {
                // Assume whoever did the encoding intended what we got
                unescaped_bytes.push(b);
            }
        }
    }
    String::from_utf8(unescaped_bytes).or_else(|e| Err(FromUrlEncodingError::Utf8CharacterError {
        error: e,
    }))
}

// Validates every '%' character is followed by exactly 2 hex
// digits.
fn validate_urlencoded_str(data: &str) -> Result<(), FromUrlEncodingError> {
    let mut iter = data.char_indices();
    while let Some((idx, chr)) = iter.next() {
        match chr {
            '%' => {
                validate_percent_encoding(&mut iter, idx)?;
            },
            _ => continue,
        }
    }
    Ok(())
}

// Validates the next two characters returned by the provided iterator are
// hexadecimal digits.
fn validate_percent_encoding(iter: &mut str::CharIndices, current_idx: usize) -> Result<(), FromUrlEncodingError> {
    for _ in 0..2 {
        match iter.next() {
            // Only hex digits are valid
            Some((_, c)) if c.is_digit(16) => {
                continue
            },
            Some((i, c)) => return Err(FromUrlEncodingError::UriCharacterError {
                character: c,
                index: i,
            }),
            // We got a '%' without 2 characters after it, so mark the '%' as bad
            None => return Err(FromUrlEncodingError::UriCharacterError {
                character: '%',
                index: current_idx,
            }),
        }
    }
    Ok(())
}

#[derive(Debug)]
pub enum FromUrlEncodingError {
    UriCharacterError { character: char, index: usize },
    Utf8CharacterError { error: FromUtf8Error },
}

#[cfg(test)]
mod tests {
    use super::encode;
    use super::decode;
    use super::FromUrlEncodingError;

    #[test]
    fn it_encodes_successfully() {
        let expected = "this%20that";
        assert_eq!(expected, encode("this that"));
    }

    #[test]
    fn it_encodes_successfully_emoji() {
        let emoji_string = "👾 Exterminate!";
        let expected = "%F0%9F%91%BE%20Exterminate%21";
        assert_eq!(expected, encode(emoji_string));
    }

    #[test]
    fn it_decodes_successfully() {
        let expected = String::from("this that");
        let encoded = "this%20that";
        assert_eq!(expected, decode(encoded).unwrap());
    }

    #[test]
    fn it_decodes_successfully_emoji() {
        let expected = String::from("👾 Exterminate!");
        let encoded = "%F0%9F%91%BE%20Exterminate%21";
        assert_eq!(expected, decode(encoded).unwrap());
    }

    #[test]
    fn it_decodes_unsuccessfully_emoji() {
        let bad_encoded_string = "👾 Exterminate!";

        assert_eq!(bad_encoded_string, decode(bad_encoded_string).unwrap());
    }

    #[test]
    fn it_decodes_unsuccessfuly_bad_percent_01() {
        let bad_encoded_string = "this%2that";
        let expected_idx = 6;
        let expected_char = 't';

        match decode(bad_encoded_string).unwrap_err() {
            FromUrlEncodingError::UriCharacterError { index: i, character: c } => {
                assert_eq!(expected_idx, i);
                assert_eq!(expected_char, c)
            },
            _ => panic!()
        }
    }

    #[test]
    fn it_decodes_unsuccessfuly_bad_percent_02() {
        let bad_encoded_string = "this%20that%";
        let expected_idx = 11;
        let expected_char = '%';

        match decode(bad_encoded_string).unwrap_err() {
            FromUrlEncodingError::UriCharacterError { index: i, character: c } => {
                assert_eq!(expected_idx, i);
                assert_eq!(expected_char, c)
            },
            _ => panic!()
        }
    }

    #[test]
    fn it_decodes_unsuccessfuly_bad_percent_03() {
        let bad_encoded_string = "this%20that%2";
        let expected_idx = 11;
        let expected_char = '%';

        match decode(bad_encoded_string).unwrap_err() {
            FromUrlEncodingError::UriCharacterError { index: i, character: c } => {
                assert_eq!(expected_idx, i);
                assert_eq!(expected_char, c)
            },
            _ => panic!()
        }
    }
}
