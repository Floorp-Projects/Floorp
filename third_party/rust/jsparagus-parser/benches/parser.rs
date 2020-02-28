use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;
use std::collections::HashMap;

use bumpalo::Bump;
use jsparagus_parser::{parse_script, ParseOptions};

fn parser_bench(c: &mut Criterion) {
    let tests = &["simple", "__finStreamer-proto"];
    let mut programs = HashMap::new();

    programs.insert("simple", include_str!("./simple.js"));
    programs.insert(
        "__finStreamer-proto",
        include_str!("./__finStreamer-proto.js"),
    );

    c.bench_function_over_inputs(
        "parser_bench",
        move |b, &name| {
            let program = &programs[name];
            b.iter(|| {
                let allocator = &Bump::new();
                let options = ParseOptions::new();
                let _ = parse_script(allocator, program, &options);
            });
        },
        tests,
    );
}

criterion_group!(benches, parser_bench);
criterion_main!(benches);
