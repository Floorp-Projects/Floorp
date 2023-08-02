// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use criterion::{black_box, criterion_group, criterion_main, Criterion};
use std::fmt;
use writeable::LengthHint;
use writeable::Writeable;

/// A sample type implementing Writeable
struct WriteableMessage<'s> {
    message: &'s str,
}

impl Writeable for WriteableMessage<'_> {
    fn write_to<W: fmt::Write + ?Sized>(&self, sink: &mut W) -> fmt::Result {
        sink.write_str(self.message)
    }

    fn writeable_length_hint(&self) -> LengthHint {
        LengthHint::exact(self.message.len())
    }
}

writeable::impl_display_with_writeable!(WriteableMessage<'_>);

/// A sample type implementing Display
struct DisplayMessage<'s> {
    message: &'s str,
}

impl fmt::Display for DisplayMessage<'_> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(self.message)
    }
}

const SHORT_STR: &str = "short";
const MEDIUM_STR: &str = "this is a medium-length string";
const LONG_STR: &str = "this string is very very very very very very very very very very very very very very very very very very very very very very very very long";

fn overview_bench(c: &mut Criterion) {
    c.bench_function("writeable/overview", |b| {
        b.iter(|| {
            // This benchmark runs to_string on short, medium, and long strings in one batch.
            WriteableMessage {
                message: black_box(SHORT_STR),
            }
            .write_to_string();
            WriteableMessage {
                message: black_box(MEDIUM_STR),
            }
            .write_to_string();
            WriteableMessage {
                message: black_box(LONG_STR),
            }
            .write_to_string();
        });
    });

    #[cfg(feature = "bench")]
    {
        writeable_benches(c);
        writeable_dyn_benches(c);
        display_benches(c);
    }
}

#[cfg(feature = "bench")]
fn writeable_benches(c: &mut Criterion) {
    c.bench_function("writeable/to_string/short", |b| {
        b.iter(|| {
            WriteableMessage {
                message: black_box(SHORT_STR),
            }
            .write_to_string()
            .into_owned()
        });
    });
    c.bench_function("writeable/to_string/medium", |b| {
        b.iter(|| {
            WriteableMessage {
                message: black_box(MEDIUM_STR),
            }
            .write_to_string()
            .into_owned()
        });
    });
    c.bench_function("writeable/to_string/long", |b| {
        b.iter(|| {
            WriteableMessage {
                message: black_box(LONG_STR),
            }
            .write_to_string()
            .into_owned()
        });
    });
}

#[cfg(feature = "bench")]
fn writeable_dyn_benches(c: &mut Criterion) {
    // Same as `write_to_string`, but casts to a `dyn fmt::Write`
    fn writeable_dyn_to_string(w: &impl Writeable) -> String {
        let mut output = String::with_capacity(w.writeable_length_hint().capacity());
        w.write_to(&mut output as &mut dyn fmt::Write)
            .expect("impl Write for String is infallible");
        output
    }

    c.bench_function("writeable_dyn/to_string/short", |b| {
        b.iter(|| {
            writeable_dyn_to_string(&WriteableMessage {
                message: black_box(SHORT_STR),
            })
        });
    });
    c.bench_function("writeable_dyn/to_string/medium", |b| {
        b.iter(|| {
            writeable_dyn_to_string(&WriteableMessage {
                message: black_box(MEDIUM_STR),
            })
        });
    });
    c.bench_function("writeable_dyn/to_string/long", |b| {
        b.iter(|| {
            writeable_dyn_to_string(&WriteableMessage {
                message: black_box(LONG_STR),
            })
        });
    });
}

#[cfg(feature = "bench")]
fn display_benches(c: &mut Criterion) {
    c.bench_function("display/to_string/short", |b| {
        b.iter(|| {
            DisplayMessage {
                message: black_box(SHORT_STR),
            }
            .to_string()
        });
    });
    c.bench_function("display/to_string/medium", |b| {
        b.iter(|| {
            DisplayMessage {
                message: black_box(MEDIUM_STR),
            }
            .to_string()
        });
    });
    c.bench_function("display/to_string/long", |b| {
        b.iter(|| {
            DisplayMessage {
                message: black_box(LONG_STR),
            }
            .to_string()
        });
    });
}

criterion_group!(benches, overview_bench,);
criterion_main!(benches);
