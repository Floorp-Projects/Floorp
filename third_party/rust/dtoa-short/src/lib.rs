/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate dtoa;

use std::fmt::Write;
use std::{fmt, str};

/// Format the given `value` into `dest` and return the notation it uses.
#[inline]
pub fn write<W: Write, V: Floating>(dest: &mut W, value: V) -> DtoaResult {
    Floating::write(value, dest)
}

/// Form of the formatted floating-point number.
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub struct Notation {
    /// Whether it contains a decimal point.
    pub decimal_point: bool,
    /// Whether it uses E-notation.
    pub scientific: bool,
}

impl Notation {
    fn integer() -> Self {
        Notation {
            decimal_point: false,
            scientific: false,
        }
    }
}

/// Result of formatting the number.
pub type DtoaResult = Result<Notation, fmt::Error>;

pub trait Floating : dtoa::Floating {
    fn write<W: Write>(self, dest: &mut W) -> DtoaResult;
}

impl Floating for f32 {
    fn write<W: Write>(self, dest: &mut W) -> DtoaResult {
        write_with_prec(dest, self, 6)
    }
}

impl Floating for f64 {
    fn write<W: Write>(self, dest: &mut W) -> DtoaResult {
        write_with_prec(dest, self, 15)
    }
}

// dtoa's buffer is 24 bytes, so we use the same length here. We may
// need to update if dtoa changes its number in the future. See
// https://github.com/dtolnay/dtoa/blob/
// 584674a70af74521ce40350dba776ea67cfcbaa7/src/dtoa.rs#L465
const BUFFER_SIZE: usize = 24;

fn write_with_prec<W, V>(dest: &mut W, value: V, prec: usize)
    -> DtoaResult where W: Write, V: dtoa::Floating
{
    let mut buf = [b'\0'; BUFFER_SIZE + 8];
    let len = dtoa::write(&mut buf[1..], value).map_err(|_| fmt::Error)?;
    let (result, notation) = restrict_prec(&mut buf[0..len + 1], prec);
    dest.write_str(str::from_utf8(result).unwrap())?;
    Ok(notation)
}

fn restrict_prec(buf: &mut [u8], prec: usize) -> (&[u8], Notation) {
    let len = buf.len();
    debug_assert!(len <= BUFFER_SIZE, "dtoa may have changed its buffer size");
    // Put a leading zero to capture any carry.
    debug_assert!(buf[0] == b'\0', "Caller must prepare an empty byte for us");
    buf[0] = b'0';
    // Remove the sign for now. We will put it back at the end.
    let sign = match buf[1] {
        s @ b'+' | s @ b'-' => {
            buf[1] = b'0';
            Some(s)
        }
        _ => None,
    };
    // Locate dot, exponent, and the first significant digit.
    let mut pos_dot = None;
    let mut pos_exp = None;
    let mut prec_start = None;
    for i in 1..len {
        if buf[i] == b'.' {
            debug_assert!(pos_dot.is_none());
            pos_dot = Some(i);
        } else if buf[i] == b'e' {
            pos_exp = Some(i);
            // We don't change exponent part, so stop here.
            break;
        } else if prec_start.is_none() && buf[i] != b'0' {
            debug_assert!(buf[i] >= b'1' && buf[i] <= b'9');
            prec_start = Some(i);
        }
    }
    let prec_start = match prec_start {
        Some(i) => i,
        // If there is no non-zero digit at all, it is just zero.
        None => return (&buf[0..1], Notation::integer()),
    };
    // Coefficient part ends at 'e' or the length.
    let coeff_end = pos_exp.unwrap_or(len);
    // Decimal dot is effectively at the end of coefficient part if no
    // dot presents before that.
    let pos_dot = pos_dot.unwrap_or(coeff_end);
    // Find the end position of the number within the given precision.
    let prec_end = {
        let end = prec_start + prec;
        if pos_dot > prec_start && pos_dot <= end {
            end + 1
        } else {
            end
        }
    };
    let mut new_coeff_end = coeff_end;
    if prec_end < coeff_end {
        // Round to the given precision.
        let next_char = buf[prec_end];
        new_coeff_end = prec_end;
        if next_char >= b'5' {
            for i in (0..prec_end).rev() {
                if buf[i] == b'.' {
                    continue;
                }
                if buf[i] != b'9' {
                    buf[i] += 1;
                    new_coeff_end = i + 1;
                    break;
                }
                buf[i] = b'0';
            }
        }
    }
    if new_coeff_end < pos_dot {
        // If the precision isn't enough to reach the dot, set all digits
        // in-between to zero and keep the number until the dot.
        for i in new_coeff_end..pos_dot {
            buf[i] = b'0';
        }
        new_coeff_end = pos_dot;
    } else {
        // Strip any trailing zeros.
        for i in (0..new_coeff_end).rev() {
            if buf[i] != b'0' {
                if buf[i] == b'.' {
                    new_coeff_end = i;
                }
                break;
            }
            new_coeff_end = i;
        }
    }
    // Move exponent part if necessary.
    let real_end = if let Some(pos_exp) = pos_exp {
        let exp_len = len - pos_exp;
        if new_coeff_end != pos_exp {
            for i in 0..exp_len {
                buf[new_coeff_end + i] = buf[pos_exp + i];
            }
        }
        new_coeff_end + exp_len
    } else {
        new_coeff_end
    };
    // Add back the sign and strip the leading zero.
    let result = if let Some(sign) = sign {
        if buf[1] == b'0' && buf[2] != b'.' {
            buf[1] = sign;
            &buf[1..real_end]
        } else {
            debug_assert!(buf[0] == b'0');
            buf[0] = sign;
            &buf[0..real_end]
        }
    } else {
        if buf[0] == b'0' && buf[1] != b'.' {
            &buf[1..real_end]
        } else {
            &buf[0..real_end]
        }
    };
    // Generate the notation info.
    let notation = Notation {
        decimal_point: pos_dot < new_coeff_end,
        scientific: pos_exp.is_some(),
    };
    (result, notation)
}
