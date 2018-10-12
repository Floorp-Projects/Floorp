/// The main export here is `Encoded`. See `examples/verify.rs` for usage
/// examples.

use std::{fmt, str};
use std::error::Error;
use argon2::{Argon2, ParamErr, Variant, defaults};

macro_rules! maybe {
    ($e: expr) => {
        match $e {
            None => return None,
            Some(v) => v,
        }
    };
}

const LUT64: &'static [u8; 64] =
    b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

fn lut(n: u8) -> u8 { LUT64[n as usize & 0x3f] }

fn delut(c: u8) -> Option<u8> {
    match c {
        43 => Some(62),
        47 => Some(63),
        _ if 65 <= c && c <= 90 => Some(c - 65),
        _ if 97 <= c && c <= 122 => Some(c - 71),
        _ if 48 <= c && c <= 57 => Some(c + 4),
        _ => None,
    }
}

fn quad(n: &[u8]) -> [u8; 4] {
    assert!(n.len() == 3);
    let (b, c) = (n[1] >> 4 | n[0] << 4, n[2] >> 6 | n[1] << 2);
    [lut(n[0] >> 2), lut(b), lut(c), lut(n[2])]
}

fn triplet(n: &[u8]) -> Option<[u8; 3]> {
    assert!(n.len() == 4);
    let a = maybe!(delut(n[0]));
    let b = maybe!(delut(n[1]));
    let c = maybe!(delut(n[2]));
    let d = maybe!(delut(n[3]));
    Some([a << 2 | b >> 4, b << 4 | c >> 2, c << 6 | d])
}

fn base64_no_pad(bytes: &[u8]) -> Vec<u8> {
    let mut rv = vec![];
    let mut pos = 0;
    while pos + 3 <= bytes.len() {
        rv.extend_from_slice(&quad(&bytes[pos..pos + 3]));
        pos += 3;
    }

    if bytes.len() - pos == 1 {
        rv.push(lut(bytes[pos] >> 2));
        rv.push(lut((bytes[pos] & 0x03) << 4));
    } else if bytes.len() - pos == 2 {
        rv.extend_from_slice(&quad(&[bytes[pos], bytes[pos + 1], 0]));
        rv.pop();
    }
    rv
}

fn debase64_no_pad(bytes: &[u8]) -> Option<Vec<u8>> {
    if bytes.len() % 4 != 1 && bytes.len() > 0 {
        let mut rv = vec![];
        let mut pos = 0;
        while pos + 4 <= bytes.len() {
            let s = maybe!(triplet(&bytes[pos..pos + 4]));
            rv.extend_from_slice(&s);
            pos += 4;
        }

        if bytes.len() - pos == 2 {
            let a = maybe!(delut(bytes[pos]));
            let b = maybe!(delut(bytes[pos + 1]));
            rv.push(a << 2 | b >> 4);
        } else if bytes.len() - pos == 3 {
            let a = maybe!(delut(bytes[pos]));
            let b = maybe!(delut(bytes[pos + 1]));
            let c = maybe!(delut(bytes[pos + 2]));
            rv.push(a << 2 | b >> 4);
            rv.push(b << 4 | c >> 2);
        }
        Some(rv)
    } else {
        None
    }
}

struct Parser<'a> {
    enc: &'a [u8],
    pos: usize,
}

type Parsed<T> = Result<T, usize>;

impl<'a> Parser<'a> {
    fn expect(&mut self, exp: &[u8]) -> Parsed<()> {
        assert!(self.pos < self.enc.len());
        if self.enc.len() - self.pos < exp.len() ||
           &self.enc[self.pos..self.pos + exp.len()] != exp {
            self.err()
        } else {
            self.pos += exp.len();
            Ok(())
        }
    }

    fn one_of(&mut self, chars: &[u8]) -> Parsed<u8> {
        if self.enc.len() > 0 {
            for &c in chars {
                if c == self.enc[self.pos] {
                    self.pos += 1;
                    return Ok(c);
                }
            }
        }
        self.err()
    }

    fn read_u32(&mut self) -> Parsed<u32> {
        let is_digit = |c: u8| 48 <= c && c <= 57;
        let mut end = self.pos;
        while end < self.enc.len() && is_digit(self.enc[end]) {
            end += 1;
        }
        match str::from_utf8(&self.enc[self.pos..end]) {
            Err(_) => self.err(),
            Ok(s) => {
                match s.parse() {
                    Err(_) => self.err(),
                    Ok(n) => {
                        self.pos = end;
                        Ok(n)
                    }
                }
            }
        }
    }

    fn decode64_till(&mut self, stopchar: Option<&[u8]>) -> Parsed<Vec<u8>> {
        let end = match stopchar {
            None => self.enc.len(),
            Some(c) => {
                self.enc[self.pos..]
                    .iter()
                    .take_while(|k| **k != c[0])
                    .fold(0, |c, _| c + 1) + self.pos
            }
        };
        match debase64_no_pad(&self.enc[self.pos..end]) {
            None => self.err(),
            Some(rv) => {
                self.pos = end;
                Ok(rv)
            }
        }
    }

    fn err<T>(&self) -> Parsed<T> { Err(self.pos) }
}

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum DecodeError {
    /// Byte position of first parse error
    ParseError(usize),
    /// Invalid Argon2 parameters given in encoding
    InvalidParams(ParamErr),
}

impl fmt::Display for DecodeError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::DecodeError::*;
        match *self {
            ParseError(pos) => write!(f, "Parse error at position {}", pos),
            InvalidParams(ref perr) => {
                write!(f, "Invalid hash parameters given by encoded: {}", perr)
            }
        }
    }
}

impl Error for DecodeError {
    fn description(&self) -> &str {
        match *self {
            DecodeError::ParseError(_) => "Hash string parse error.",
            DecodeError::InvalidParams(ref perr) => perr.description(),
        }
    }
}

/// Represents a single Argon2 hashing session. A hash session comprises of the
/// hash algorithm parameters, salt, key, and data used to hash a given input.
pub struct Encoded {
    params: Argon2,
    hash: Vec<u8>,
    salt: Vec<u8>,
    key: Vec<u8>,
    data: Vec<u8>,
}

macro_rules! try_unit {
    ($e: expr) => {
        match $e {
            Ok(()) => {},
            Err(e) => return Err(e)
        }
    };
}

type Packed = (Variant, u32, u32, u32, Vec<u8>, Vec<u8>, Vec<u8>, Vec<u8>);

impl Encoded {
    fn parse(encoded: &[u8]) -> Result<Packed, usize> {
        let mut p = Parser {
            enc: encoded,
            pos: 0,
        };

        try_unit!(p.expect(b"$argon2"));

        let variant = match try!(p.one_of(b"di")) {
            v if v == 'd' as u8 => Variant::Argon2d,
            v if v == 'i' as u8 => Variant::Argon2i,
            _ => unreachable!(),
        };

        try_unit!(p.expect(b"$m="));
        let kib = try!(p.read_u32());
        try_unit!(p.expect(b",t="));
        let passes = try!(p.read_u32());
        try_unit!(p.expect(b",p="));
        let lanes = try!(p.read_u32());

        let key = match p.expect(b",keyid=") {
            Err(_) => vec![],
            Ok(()) => try!(p.decode64_till(Some(b","))),
        };

        let data = match p.expect(b",data=") {
            Ok(()) => try!(p.decode64_till(Some(b"$"))),
            Err(_) => vec![],
        };

        try_unit!(p.expect(b"$"));
        let salt = try!(p.decode64_till(Some(b"$")));
        try_unit!(p.expect(b"$"));
        let hash = try!(p.decode64_till(None));
        Ok((variant, kib, passes, lanes, key, data, salt, hash))
    }

    /// Reconstruct a previous hash session from serialized bytes.
    pub fn from_u8(encoded: &[u8]) -> Result<Self, DecodeError> {
        match Self::parse(encoded) {
            Err(pos) => Err(DecodeError::ParseError(pos)),
            Ok((v, kib, passes, lanes, key, data, salt, hash)) => {
                match Argon2::new(passes, lanes, kib, v) {
                    Err(e) => Err(DecodeError::InvalidParams(e)),
                    Ok(a2) => {
                        Ok(Encoded {
                            params: a2,
                            hash: hash,
                            salt: salt,
                            key: key,
                            data: data,
                        })
                    }
                }
            }
        }
    }

    /// Serialize this hashing session into raw bytes that can later be
    /// recovered by `Encoded::from_u8`.
    #[cfg_attr(rustfmt, rustfmt_skip)]
    pub fn to_u8(&self) -> Vec<u8> {
        let vcode = |v| match v {
            Variant::Argon2i => "i",
            Variant::Argon2d => "d",
        };
        let b64 = |x| String::from_utf8(base64_no_pad(x)).unwrap()
;
        let k_ = match &b64(&self.key[..]) {
            bytes if bytes.len() > 0 => format!(",keyid={}", bytes),
            _ => String::new(),
        };
        let x_ = match &b64(&self.data[..]) {
            bytes if bytes.len() > 0 => format!(",data={}", bytes),
            _ => String::new(),
        };
        let (var, m, t, p) = self.params.params();
        format!("$argon2{}$m={},t={},p={}{}{}${}${}", vcode(var), m, t, p,
                k_, x_, b64(&self.salt[..]), b64(&self.hash))
            .into_bytes()
    }

    /// Generates a new hashing session from password, salt, and other byte
    /// input.  Parameters are:
    ///
    /// `argon`: An `Argon2` struct representative of the desired hash algorithm
    /// parameters.
    ///
    /// `p`: Password input.
    ///
    /// `s`: Salt.
    ///
    /// `k`: An optional secret value.
    ///
    /// `x`: Optional, miscellaneous associated data.
    ///
    /// Note that `p, s, k, x` must conform to the same length constraints
    /// dictated by `Argon2::hash`.
    pub fn new(argon: Argon2, p: &[u8], s: &[u8], k: &[u8], x: &[u8]) -> Self {
        let mut out = vec![0 as u8; defaults::LENGTH];
        argon.hash(&mut out[..], p, s, k, x);
        Encoded {
            params: argon,
            hash: out,
            salt: s.iter().cloned().collect(),
            key: k.iter().cloned().collect(),
            data: x.iter().cloned().collect(),
        }
    }

    /// Same as `Encoded::new`, but with the default Argon2i hash algorithm
    /// parameters.
    pub fn default2i(p: &[u8], s: &[u8], k: &[u8], x: &[u8]) -> Self {
        Self::new(Argon2::default(Variant::Argon2i), p, s, k, x)
    }

    /// Same as `Encoded::new`, but with the default _Argon2d_ hash algorithm
    /// parameters.
    pub fn default2d(p: &[u8], s: &[u8], k: &[u8], x: &[u8]) -> Self {
        Self::new(Argon2::default(Variant::Argon2d), p, s, k, x)
    }

    /// Verifies password input against the hash that was previously created in
    /// this hashing session.
    pub fn verify(&self, p: &[u8]) -> bool {
        let mut out = [0 as u8; defaults::LENGTH];
        let s = &self.salt[..];
        self.params.hash(&mut out, p, s, &self.key[..], &self.data[..]);
        constant_eq(&out, &self.hash)
    }
}

/// Compares two byte arrays for equality. Assumes that both are already of
/// equal length.
#[inline(never)]
pub fn constant_eq(xs: &[u8], ys: &[u8]) -> bool {
    if xs.len() != ys.len() {
        false
    } else {
        let rv = xs.iter().zip(ys.iter()).fold(0, |rv, (x, y)| rv | (x ^ y));
        // this kills the optimizer.
        (1 & (rv as u32).wrapping_sub(1) >> 8).wrapping_sub(1) == 0
    }
}

#[cfg(test)]
mod test {
    use super::{Encoded, base64_no_pad, debase64_no_pad};

    const BASE64_CASES: [(&'static [u8], &'static [u8]); 5] =
        [(b"any carnal pleasure.", b"YW55IGNhcm5hbCBwbGVhc3VyZS4"),
         (b"any carnal pleasure", b"YW55IGNhcm5hbCBwbGVhc3VyZQ"),
         (b"any carnal pleasur", b"YW55IGNhcm5hbCBwbGVhc3Vy"),
         (b"any carnal pleasu", b"YW55IGNhcm5hbCBwbGVhc3U"),
         (b"any carnal pleas", b"YW55IGNhcm5hbCBwbGVhcw")];

    const ENCODED: &'static [u8] =
        b"$argon2i$m=4096,t=3,p=1$dG9kbzogZnV6eiB0ZXN0cw\
          $Eh1lW3mjkhlMLRQdE7vXZnvwDXSGLBfXa6BGK4a1J3s";

    #[test]
    fn test_base64_no_pad() {
        for &(s, exp) in BASE64_CASES.iter() {
            assert_eq!(&base64_no_pad(s)[..], exp);
        }
    }

    #[test]
    fn test_debase64_no_pad() {
        for &(exp, s) in BASE64_CASES.iter() {
            assert_eq!(debase64_no_pad(s).unwrap(), exp);
        }
    }

    #[test]
    fn test_verify() {
        let v = Encoded::from_u8(ENCODED).unwrap();
        assert_eq!(v.verify(b"argon2i!"), true);
        assert_eq!(v.verify(b"nope"), false);
    }

    #[test]
    fn bad_encoded() {
        use super::DecodeError::*;
        use argon2::ParamErr::*;
        let cases: &[(&'static [u8], super::DecodeError)] =
            &[(b"$argon2y$m=4096", ParseError(7)),
              (b"$argon2i$m=-2,t=-4,p=-4$aaaaaaaa$ffffff", ParseError(11)),
              (b"$argon2i$m=0,t=0,p=0$aaaaaaaa$ffffff*", ParseError(30)),
              (b"$argon2i$m=0,t=0,p=0$aaaaaaaa$ffffff",
               InvalidParams(TooFewPasses)),
              // intentionally fail Encoded::expect with undersized input
              (b"$argon2i$m", ParseError(8))];
        for &(case, err) in cases.iter() {
            let v = Encoded::from_u8(case);
            assert!(v.is_err());
            assert_eq!(v.err().unwrap(), err);
        }
    }
}
