use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;
use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::Rc;

use bumpalo::Bump;
use jsparagus_ast::source_atom_set::SourceAtomSet;
use jsparagus_parser::{parse_script, ParseOptions};
use jsparagus_ast::source_slice_list::SourceSliceList;

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
                let atoms = Rc::new(RefCell::new(SourceAtomSet::new()));
                let slices = Rc::new(RefCell::new(SourceSliceList::new()));
                let _ = parse_script(allocator, program, &options, atoms, slices);
            });
        },
        tests,
    );
}

criterion_group!(benches, parser_bench);
criterion_main!(benches);
