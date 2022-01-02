use criterion::black_box;
use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Bencher;
use criterion::Criterion;
use criterion::Fun;

use tinystr::{TinyStr16, TinyStr4, TinyStr8, TinyStrAuto};

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
            Fun::new("TinyStrAuto", $action!(TinyStrAuto)),
        ];

        $c.bench_functions(&format!("{}/4", $name), funcs, STRINGS_4);

        let funcs = vec![
            Fun::new("String", $action!(String)),
            Fun::new("TinyStr8", $action!(TinyStr8)),
            Fun::new("TinyStr16", $action!(TinyStr16)),
            Fun::new("TinyStrAuto", $action!(TinyStrAuto)),
        ];

        $c.bench_functions(&format!("{}/8", $name), funcs, STRINGS_8);

        let funcs = vec![
            Fun::new("String", $action!(String)),
            Fun::new("TinyStr16", $action!(TinyStr16)),
            Fun::new("TinyStrAuto", $action!(TinyStrAuto)),
        ];

        $c.bench_functions(&format!("{}/16", $name), funcs, STRINGS_16);
    };
}

fn construct_from_str(c: &mut Criterion) {
    macro_rules! cfs {
        ($r:ty) => {
            |b: &mut Bencher, strings: &&[&str]| {
                b.iter(|| {
                    for s in *strings {
                        let _: $r = black_box(s.parse().unwrap());
                    }
                })
            }
        };
    };

    bench_block!(c, "construct_from_str", cfs);
}

fn construct_from_bytes(c: &mut Criterion) {
    macro_rules! cfu {
        ($r:ty) => {
            |b, inputs: &&[&str]| {
                let raw: Vec<&[u8]> = inputs.iter().map(|s| s.as_bytes()).collect();
                b.iter(move || {
                    for u in &raw {
                        let _ = black_box(<$r>::from_bytes(*u).unwrap());
                    }
                })
            }
        };
    };

    let funcs = vec![
        Fun::new("TinyStr4", cfu!(TinyStr4)),
        Fun::new("TinyStr8", cfu!(TinyStr8)),
        Fun::new("TinyStr16", cfu!(TinyStr16)),
    ];

    c.bench_functions("construct_from_bytes/4", funcs, STRINGS_4);

    let funcs = vec![
        Fun::new("TinyStr8", cfu!(TinyStr8)),
        Fun::new("TinyStr16", cfu!(TinyStr16)),
    ];

    c.bench_functions("construct_from_bytes/8", funcs, STRINGS_8);

    let funcs = vec![Fun::new("TinyStr16", cfu!(TinyStr16))];

    c.bench_functions("construct_from_bytes/16", funcs, STRINGS_16);
}

fn construct_unchecked(c: &mut Criterion) {
    macro_rules! cu {
        ($tty:ty, $rty:ty) => {
            |b, inputs: &&[&str]| {
                let raw: Vec<$rty> = inputs
                    .iter()
                    .map(|s| s.parse::<$tty>().unwrap().into())
                    .collect();
                b.iter(move || {
                    for num in &raw {
                        let _ = unsafe { <$tty>::new_unchecked(black_box(*num)) };
                    }
                })
            }
        };
    };

    let funcs = vec![Fun::new("TinyStr4", cu!(TinyStr4, u32))];

    c.bench_functions("construct_unchecked/4", funcs, STRINGS_4);

    let funcs = vec![Fun::new("TinyStr8", cu!(TinyStr8, u64))];

    c.bench_functions("construct_unchecked/8", funcs, STRINGS_8);

    let funcs = vec![Fun::new("TinyStr16", cu!(TinyStr16, u128))];

    c.bench_functions("construct_unchecked/16", funcs, STRINGS_16);
}

criterion_group!(
    benches,
    construct_from_str,
    construct_from_bytes,
    construct_unchecked,
);
criterion_main!(benches);
