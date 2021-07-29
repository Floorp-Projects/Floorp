use criterion::criterion_group;
use criterion::criterion_main;
use criterion::BenchmarkId;
use criterion::Criterion;
use std::collections::HashMap;
use std::fs::File;
use std::io;
use std::io::Read;

use fluent_bundle::{FluentArgs, FluentBundle, FluentResource, FluentValue};
use fluent_syntax::ast;
use unic_langid::langid;

fn read_file(path: &str) -> Result<String, io::Error> {
    let mut f = File::open(path)?;
    let mut s = String::new();
    f.read_to_string(&mut s)?;
    Ok(s)
}

fn get_strings(tests: &[&'static str]) -> HashMap<&'static str, String> {
    let mut ftl_strings = HashMap::new();
    for test in tests {
        let path = format!("./benches/{}.ftl", test);
        ftl_strings.insert(*test, read_file(&path).expect("Couldn't load file"));
    }
    return ftl_strings;
}

fn get_ids(res: &FluentResource) -> Vec<String> {
    res.ast()
        .body
        .iter()
        .filter_map(|entry| match entry {
            ast::Entry::Message(ast::Message { id, .. }) => Some(id.name.to_owned()),
            _ => None,
        })
        .collect()
}

fn get_args(name: &str) -> Option<FluentArgs> {
    match name {
        "preferences" => {
            let mut prefs_args = FluentArgs::new();
            prefs_args.add("name", FluentValue::from("John"));
            prefs_args.add("tabCount", FluentValue::from(5));
            prefs_args.add("count", FluentValue::from(3));
            prefs_args.add("version", FluentValue::from("65.0"));
            prefs_args.add("path", FluentValue::from("/tmp"));
            prefs_args.add("num", FluentValue::from(4));
            prefs_args.add("email", FluentValue::from("john@doe.com"));
            prefs_args.add("value", FluentValue::from(4.5));
            prefs_args.add("unit", FluentValue::from("mb"));
            prefs_args.add("service-name", FluentValue::from("Mozilla Disk"));
            Some(prefs_args)
        }
        _ => None,
    }
}

fn add_functions<R>(name: &'static str, bundle: &mut FluentBundle<R>) {
    match name {
        "preferences" => {
            bundle
                .add_function("PLATFORM", |_args, _named_args| {
                    return "linux".into();
                })
                .expect("Failed to add a function to the bundle.");
        }
        _ => {}
    }
}

fn get_bundle(name: &'static str, source: &str) -> (FluentBundle<FluentResource>, Vec<String>) {
    let res = FluentResource::try_new(source.to_owned()).expect("Couldn't parse an FTL source");
    let ids = get_ids(&res);
    let lids = vec![langid!("en")];
    let mut bundle = FluentBundle::new(lids);
    bundle
        .add_resource(res)
        .expect("Couldn't add FluentResource to the FluentBundle");
    add_functions(name, &mut bundle);
    (bundle, ids)
}

fn resolver_bench(c: &mut Criterion) {
    let tests = &["simple", "preferences", "menubar", "unescape"];
    let ftl_strings = get_strings(tests);

    let mut group = c.benchmark_group("resolve");
    for name in tests {
        let source = ftl_strings.get(name).expect("Failed to find the source.");
        group.bench_with_input(BenchmarkId::from_parameter(name), &source, |b, source| {
            let (bundle, ids) = get_bundle(name, source);
            let args = get_args(name);
            b.iter(|| {
                let mut s = String::new();
                for id in &ids {
                    let msg = bundle.get_message(id).expect("Message found");
                    let mut errors = vec![];
                    if let Some(value) = msg.value {
                        let _ = bundle.write_pattern(&mut s, value, args.as_ref(), &mut errors);
                        s.clear();
                    }
                    for attr in msg.attributes {
                        let _ =
                            bundle.write_pattern(&mut s, attr.value, args.as_ref(), &mut errors);
                        s.clear();
                    }
                    assert!(errors.len() == 0, "Resolver errors: {:#?}", errors);
                }
            })
        });
    }
    group.finish();

    let mut group = c.benchmark_group("resolve_to_str");
    for name in tests {
        let source = ftl_strings.get(name).expect("Failed to find the source.");
        group.bench_with_input(BenchmarkId::from_parameter(name), &source, |b, source| {
            let (bundle, ids) = get_bundle(name, source);
            let args = get_args(name);
            b.iter(|| {
                for id in &ids {
                    let msg = bundle.get_message(id).expect("Message found");
                    let mut errors = vec![];
                    if let Some(value) = msg.value {
                        let _ = bundle.format_pattern(value, args.as_ref(), &mut errors);
                    }
                    for attr in msg.attributes {
                        let _ = bundle.format_pattern(attr.value, args.as_ref(), &mut errors);
                    }
                    assert!(errors.len() == 0, "Resolver errors: {:#?}", errors);
                }
            })
        });
    }
    group.finish();
}

criterion_group!(benches, resolver_bench);
criterion_main!(benches);
