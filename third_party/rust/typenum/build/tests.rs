use std::{env, fmt, fs, io, path};

use super::{gen_int, gen_uint};

fn sign(i: i64) -> char {
    if i > 0 {
        'P'
    } else if i < 0 {
        'N'
    } else {
        '_'
    }
}

struct UIntTest {
    a: u64,
    op: &'static str,
    b: Option<u64>,
    r: u64,
}

impl fmt::Display for UIntTest {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.b {
            Some(b) => write!(
                f,
                "
#[test]
#[allow(non_snake_case)]
fn test_{a}_{op}_{b}() {{
    type A = {gen_a};
    type B = {gen_b};
    type U{r} = {result};

    #[allow(non_camel_case_types)]
    type U{a}{op}U{b} = <<A as {op}<B>>::Output as Same<U{r}>>::Output;

    assert_eq!(<U{a}{op}U{b} as Unsigned>::to_u64(), <U{r} as Unsigned>::to_u64());
}}",
                gen_a = gen_uint(self.a),
                gen_b = gen_uint(b),
                r = self.r,
                result = gen_uint(self.r),
                a = self.a,
                b = b,
                op = self.op
            ),
            None => write!(
                f,
                "
#[test]
#[allow(non_snake_case)]
fn test_{a}_{op}() {{
    type A = {gen_a};
    type U{r} = {result};

    #[allow(non_camel_case_types)]
    type {op}U{a} = <<A as {op}>::Output as Same<U{r}>>::Output;
    assert_eq!(<{op}U{a} as Unsigned>::to_u64(), <U{r} as Unsigned>::to_u64());
}}",
                gen_a = gen_uint(self.a),
                r = self.r,
                result = gen_uint(self.r),
                a = self.a,
                op = self.op
            ),
        }
    }
}

fn uint_binary_test(a: u64, op: &'static str, b: u64, result: u64) -> UIntTest {
    UIntTest {
        a: a,
        op: op,
        b: Option::Some(b),
        r: result,
    }
}

// fn uint_unary_test(op: &'static str, a: u64, result: u64) -> UIntTest {
//     UIntTest { a: a, op: op, b: Option::None, r: result }
// }

struct IntBinaryTest {
    a: i64,
    op: &'static str,
    b: i64,
    r: i64,
}

impl fmt::Display for IntBinaryTest {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "
#[test]
#[allow(non_snake_case)]
fn test_{sa}{a}_{op}_{sb}{b}() {{
    type A = {gen_a};
    type B = {gen_b};
    type {sr}{r} = {result};

    #[allow(non_camel_case_types)]
    type {sa}{a}{op}{sb}{b} = <<A as {op}<B>>::Output as Same<{sr}{r}>>::Output;

    assert_eq!(<{sa}{a}{op}{sb}{b} as Integer>::to_i64(), <{sr}{r} as Integer>::to_i64());
}}",
            gen_a = gen_int(self.a),
            gen_b = gen_int(self.b),
            r = self.r.abs(),
            sr = sign(self.r),
            result = gen_int(self.r),
            a = self.a.abs(),
            b = self.b.abs(),
            sa = sign(self.a),
            sb = sign(self.b),
            op = self.op
        )
    }
}

fn int_binary_test(a: i64, op: &'static str, b: i64, result: i64) -> IntBinaryTest {
    IntBinaryTest {
        a: a,
        op: op,
        b: b,
        r: result,
    }
}

struct IntUnaryTest {
    op: &'static str,
    a: i64,
    r: i64,
}

impl fmt::Display for IntUnaryTest {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "
#[test]
#[allow(non_snake_case)]
fn test_{sa}{a}_{op}() {{
    type A = {gen_a};
    type {sr}{r} = {result};

    #[allow(non_camel_case_types)]
    type {op}{sa}{a} = <<A as {op}>::Output as Same<{sr}{r}>>::Output;
    assert_eq!(<{op}{sa}{a} as Integer>::to_i64(), <{sr}{r} as Integer>::to_i64());
}}",
            gen_a = gen_int(self.a),
            r = self.r.abs(),
            sr = sign(self.r),
            result = gen_int(self.r),
            a = self.a.abs(),
            sa = sign(self.a),
            op = self.op
        )
    }
}

fn int_unary_test(op: &'static str, a: i64, result: i64) -> IntUnaryTest {
    IntUnaryTest {
        op: op,
        a: a,
        r: result,
    }
}

fn uint_cmp_test(a: u64, b: u64) -> String {
    format!(
        "
#[test]
#[allow(non_snake_case)]
fn test_{a}_cmp_{b}() {{
    type A = {gen_a};
    type B = {gen_b};

    #[allow(non_camel_case_types)]
    type U{a}CmpU{b} = <A as Cmp<B>>::Output;
    assert_eq!(<U{a}CmpU{b} as Ord>::to_ordering(), Ordering::{result:?});
}}",
        a = a,
        b = b,
        gen_a = gen_uint(a),
        gen_b = gen_uint(b),
        result = a.cmp(&b)
    )
}

fn int_cmp_test(a: i64, b: i64) -> String {
    format!(
        "
#[test]
#[allow(non_snake_case)]
fn test_{sa}{a}_cmp_{sb}{b}() {{
    type A = {gen_a};
    type B = {gen_b};

    #[allow(non_camel_case_types)]
    type {sa}{a}Cmp{sb}{b} = <A as Cmp<B>>::Output;
    assert_eq!(<{sa}{a}Cmp{sb}{b} as Ord>::to_ordering(), Ordering::{result:?});
}}",
        a = a.abs(),
        b = b.abs(),
        sa = sign(a),
        sb = sign(b),
        gen_a = gen_int(a),
        gen_b = gen_int(b),
        result = a.cmp(&b)
    )
}

pub fn build_tests() -> Result<(), Box<::std::error::Error>> {
    // will test all permutations of number pairs up to this (and down to its opposite for ints)
    let high: i64 = 5;

    let uints = (0u64..high as u64 + 1).flat_map(|a| (a..a + 1).cycle().zip(0..high as u64 + 1));
    let ints = (-high..high + 1).flat_map(|a| (a..a + 1).cycle().zip(-high..high + 1));

    let out_dir = env::var("OUT_DIR")?;
    let dest = path::Path::new(&out_dir).join("tests.rs");
    let f = fs::File::create(&dest)?;
    let mut writer = io::BufWriter::new(&f);
    use std::io::Write;
    writer.write(
        b"
extern crate typenum;

use std::ops::*;
use std::cmp::Ordering;
use typenum::*;
",
    )?;
    use std::cmp;
    // uint operators:
    for (a, b) in uints {
        write!(writer, "{}", uint_binary_test(a, "BitAnd", b, a & b))?;
        write!(writer, "{}", uint_binary_test(a, "BitOr", b, a | b))?;
        write!(writer, "{}", uint_binary_test(a, "BitXor", b, a ^ b))?;
        write!(writer, "{}", uint_binary_test(a, "Shl", b, a << b))?;
        write!(writer, "{}", uint_binary_test(a, "Shr", b, a >> b))?;
        write!(writer, "{}", uint_binary_test(a, "Add", b, a + b))?;
        write!(writer, "{}", uint_binary_test(a, "Min", b, cmp::min(a, b)))?;
        write!(writer, "{}", uint_binary_test(a, "Max", b, cmp::max(a, b)))?;
        if a >= b {
            write!(writer, "{}", uint_binary_test(a, "Sub", b, a - b))?;
        }
        write!(writer, "{}", uint_binary_test(a, "Mul", b, a * b))?;
        if b != 0 {
            write!(writer, "{}", uint_binary_test(a, "Div", b, a / b))?;
            write!(writer, "{}", uint_binary_test(a, "Rem", b, a % b))?;
            if a % b == 0 {
                write!(writer, "{}", uint_binary_test(a, "PartialDiv", b, a / b))?;
            }
        }
        write!(writer, "{}", uint_binary_test(a, "Pow", b, a.pow(b as u32)))?;
        write!(writer, "{}", uint_cmp_test(a, b))?;
    }
    // int operators:
    for (a, b) in ints {
        write!(writer, "{}", int_binary_test(a, "Add", b, a + b))?;
        write!(writer, "{}", int_binary_test(a, "Sub", b, a - b))?;
        write!(writer, "{}", int_binary_test(a, "Mul", b, a * b))?;
        write!(writer, "{}", int_binary_test(a, "Min", b, cmp::min(a, b)))?;
        write!(writer, "{}", int_binary_test(a, "Max", b, cmp::max(a, b)))?;
        if b != 0 {
            write!(writer, "{}", int_binary_test(a, "Div", b, a / b))?;
            write!(writer, "{}", int_binary_test(a, "Rem", b, a % b))?;
            if a % b == 0 {
                write!(writer, "{}", int_binary_test(a, "PartialDiv", b, a / b))?;
            }
        }
        if b >= 0 || a.abs() == 1 {
            let result = if b < 0 {
                if a == 1 {
                    a
                } else if a == -1 {
                    a.pow((-b) as u32)
                } else {
                    unreachable!()
                }
            } else {
                a.pow(b as u32)
            };
            write!(writer, "{}", int_binary_test(a, "Pow", b, result))?;
        }
        write!(writer, "{}", int_cmp_test(a, b))?;
    }

    // int unary operators:
    for n in -high..high + 1 {
        write!(writer, "{}", int_unary_test("Neg", n, -n))?;
        write!(writer, "{}", int_unary_test("Abs", n, n.abs()))?;
    }

    writer.flush()?;

    Ok(())
}
