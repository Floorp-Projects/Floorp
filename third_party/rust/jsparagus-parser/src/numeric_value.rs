//! Parse NumericLiteral.

pub type ParseNumberResult = Result<f64, &'static str>;

#[derive(Debug)]
pub enum NumericLiteralBase {
    Decimal,
    Binary,
    Octal,
    Hex,
}

// The number of digits in 2**53 - 1 (integer part of f64).
// 9007199254740991
const F64_INT_DIGITS_MAX_LEN: usize = 16;
// 11111111111111111111111111111111111111111111111111111
const F64_INT_BIN_DIGITS_MAX_LEN: usize = 53;
// 377777777777777777
const F64_INT_OCT_DIGITS_MAX_LEN: usize = 18;
// 1fffffffffffff
const F64_INT_HEX_DIGITS_MAX_LEN: usize = 14;

// To avoid allocating extra buffer when '_' is present, integer cases are
// handled without Rust standard `parse` function, as long as the value
// won't overflow the integer part of f64.
fn parse_decimal_int(s: &str) -> ParseNumberResult {
    debug_assert!(!s.is_empty());

    // NOTE: Maximum length cannot be handled.
    if s.len() >= F64_INT_DIGITS_MAX_LEN {
        // Fallback to float function that can handle all cases.
        return parse_float(s);
    }

    let src = s.as_bytes();

    let mut result = 0.0;
    for &c in src {
        match c {
            b'0'..=b'9' => {
                let n = c - b'0';
                result = result * 10.0 + n as f64;
            }
            b'_' => {}
            _ => panic!("invalid syntax"),
        }
    }
    Ok(result)
}

fn parse_binary(s: &str) -> ParseNumberResult {
    debug_assert!(!s.is_empty());

    // NOTE: Maximum length can be handled.
    if s.len() > F64_INT_BIN_DIGITS_MAX_LEN {
        return Err("too long binary literal");
    }

    let src = s.as_bytes();

    let mut result = 0.0;
    for &c in src {
        match c {
            b'0'..=b'1' => {
                let n = c - b'0';
                result = result * 2.0 + n as f64;
            }
            b'_' => {}
            _ => panic!("invalid syntax"),
        }
    }
    Ok(result)
}

fn parse_octal(s: &str) -> ParseNumberResult {
    debug_assert!(!s.is_empty());

    // NOTE: Maximum length cannot be handled.
    if s.len() >= F64_INT_OCT_DIGITS_MAX_LEN {
        return Err("too long octal literal");
    }

    let src = s.as_bytes();

    let mut result = 0.0;
    for &c in src {
        match c {
            b'0'..=b'7' => {
                let n = c - b'0';
                result = result * 8.0 + n as f64;
            }
            b'_' => {}
            _ => panic!("invalid syntax"),
        }
    }
    Ok(result)
}

fn parse_hex(s: &str) -> ParseNumberResult {
    debug_assert!(!s.is_empty());

    // NOTE: Maximum length cannot be handled.
    if s.len() >= F64_INT_HEX_DIGITS_MAX_LEN {
        return Err("too long hex literal");
    }

    let src = s.as_bytes();

    let mut result = 0.0;
    for &c in src {
        match c {
            b'0'..=b'9' => {
                let n = c - b'0';
                result = result * 16.0 + n as f64;
            }
            b'A'..=b'F' => {
                let n = c - b'A' + 10;
                result = result * 16.0 + n as f64;
            }
            b'a'..=b'f' => {
                let n = c - b'a' + 10;
                result = result * 16.0 + n as f64;
            }
            b'_' => {}
            _ => panic!("invalid syntax"),
        }
    }
    Ok(result)
}

/// Parse integer NumericLiteral.
///
/// NonDecimalIntegerLiteral should contain the leading '0x' etc.
///
/// FIXME: LegacyOctalIntegerLiteral is not supported.
pub fn parse_int<'alloc>(s: &str, kind: NumericLiteralBase) -> ParseNumberResult {
    match kind {
        NumericLiteralBase::Decimal => parse_decimal_int(s),
        NumericLiteralBase::Binary => parse_binary(&s[2..]),
        NumericLiteralBase::Octal => parse_octal(&s[2..]),
        NumericLiteralBase::Hex => parse_hex(&s[2..]),
    }
}

fn parse_float_with_underscore(s: &str) -> ParseNumberResult {
    let filtered: String = s.chars().filter(|c| *c != '_').collect();

    filtered
        .parse::<f64>()
        .map_err(|_| "too long decimal literal")
}

/// Parse non-integer NumericLiteral.
pub fn parse_float(s: &str) -> ParseNumberResult {
    if s.contains('_') {
        return parse_float_with_underscore(s);
    }

    s.parse::<f64>().map_err(|_| "too long decimal literal")
}
