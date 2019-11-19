use criterion::black_box;
use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Bencher;
use criterion::Criterion;
use criterion::Fun;

use tinystr::{TinyStr16, TinyStr4, TinyStr8};

static STRINGS_4: &[&str] = &[
    "US", "GB", "AR", "Hans", "CN", "AT", "PL", "FR", "AT", "Cyrl", "SR", "NO", "FR", "MK", "UK",
];

static STRINGS_8: &[&str] = &[
    "Latn", "windows", "AR", "Hans", "macos", "AT", "pl", "FR", "en", "Cyrl", "SR", "NO", "419",
    "und", "UK",
];

static STRINGS_16: &[&str] = &[
    "Latn",
    "windows",
    "AR",
    "Hans",
    "macos",
    "AT",
    "infiniband",
    "FR",
    "en",
    "Cyrl",
    "FromIntegral",
    "NO",
    "419",
    "MacintoshOSX2019",
    "UK",
];

macro_rules! bench_block {
    ($c:expr, $name:expr, $action:ident) => {
        let funcs = vec![
            Fun::new("String", $action!(String)),
            Fun::new("TinyStr4", $action!(TinyStr4)),
            Fun::new("TinyStr8", $action!(TinyStr8)),
            Fun::new("TinyStr16", $action!(TinyStr16)),
        ];

        $c.bench_functions(&format!("{}/4", $name), funcs, STRINGS_4);

        let funcs = vec![
            Fun::new("String", $action!(String)),
            Fun::new("TinyStr8", $action!(TinyStr8)),
            Fun::new("TinyStr16", $action!(TinyStr16)),
        ];

        $c.bench_functions(&format!("{}/8", $name), funcs, STRINGS_8);

        let funcs = vec![
            Fun::new("String", $action!(String)),
            Fun::new("TinyStr16", $action!(TinyStr16)),
        ];

        $c.bench_functions(&format!("{}/16", $name), funcs, STRINGS_16);
    };
}

macro_rules! convert_to_ascii {
    ($ty:ty, $action:ident) => {
        |b: &mut Bencher, inputs: &&[&str]| {
            let raw: Vec<$ty> = inputs.iter().map(|s| s.parse::<$ty>().unwrap()).collect();
            b.iter(move || {
                for s in &raw {
                    let _ = black_box(s.$action());
                }
            })
        }
    };
}

fn convert_to_ascii_lowercase(c: &mut Criterion) {
    macro_rules! ctal {
        ($ty:ty) => {
            convert_to_ascii!($ty, to_ascii_lowercase)
        };
    }

    bench_block!(c, "convert_to_ascii_lowercase", ctal);
}

fn convert_to_ascii_uppercase(c: &mut Criterion) {
    macro_rules! ctau {
        ($ty:ty) => {
            convert_to_ascii!($ty, to_ascii_uppercase)
        };
    }

    bench_block!(c, "convert_to_ascii_uppercase", ctau);
}

trait ExtToAsciiTitlecase {
    #[inline(always)]
    fn to_ascii_titlecase(&self) -> String;
}

impl ExtToAsciiTitlecase for str {
    fn to_ascii_titlecase(&self) -> String {
        let mut result = self.to_ascii_lowercase();
        result[0..1].make_ascii_uppercase();
        result
    }
}

fn convert_to_ascii_titlecase(c: &mut Criterion) {
    macro_rules! ctat {
        ($ty:ty) => {
            convert_to_ascii!($ty, to_ascii_titlecase)
        };
    }

    bench_block!(c, "convert_to_ascii_titlecase", ctat);
}

trait ExtIsAsciiAlphanumeric {
    #[inline(always)]
    fn is_ascii_alphanumeric(&self) -> bool;
}

impl ExtIsAsciiAlphanumeric for str {
    fn is_ascii_alphanumeric(&self) -> bool {
        self.chars().all(|c| c.is_ascii_alphanumeric())
    }
}

fn test_is_ascii_alphanumeric(c: &mut Criterion) {
    macro_rules! tiaa {
        ($ty:ty) => {
            |b: &mut Bencher, inputs: &&[&str]| {
                let raw: Vec<$ty> = inputs.iter().map(|s| s.parse::<$ty>().unwrap()).collect();
                b.iter(move || {
                    for s in &raw {
                        let _ = black_box(s.is_ascii_alphanumeric());
                    }
                })
            }
        };
    }

    bench_block!(c, "test_is_ascii_alphanumeric", tiaa);
}

fn test_eq(c: &mut Criterion) {
    macro_rules! te {
        ($ty:ty) => {
            |b: &mut Bencher, inputs: &&[&str]| {
                let raw: Vec<$ty> = inputs.iter().map(|s| s.parse::<$ty>().unwrap()).collect();
                b.iter(move || {
                    for s in &raw {
                        for l in &raw {
                            let _ = black_box(s == l);
                        }
                    }
                })
            }
        };
    }

    bench_block!(c, "test_eq", te);
}

criterion_group!(
    benches,
    convert_to_ascii_lowercase,
    convert_to_ascii_uppercase,
    convert_to_ascii_titlecase,
    test_is_ascii_alphanumeric,
    test_eq,
);
criterion_main!(benches);
