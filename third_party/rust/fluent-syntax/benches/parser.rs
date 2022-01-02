use criterion::criterion_group;
use criterion::criterion_main;
use criterion::Criterion;
use std::collections::HashMap;
use std::fs;
use std::io;

use fluent_syntax::parser::parse_runtime;

fn read_file(path: &str) -> Result<String, io::Error> {
    fs::read_to_string(path)
}

#[cfg(feature = "all-benchmarks")]
fn get_resources(tests: &[&'static str]) -> HashMap<&'static str, String> {
    let mut ftl_strings = HashMap::new();
    for test in tests {
        let path = format!("./benches/{}", test);
        ftl_strings.insert(*test, read_file(&path).expect("Couldn't load file"));
    }
    return ftl_strings;
}

fn get_ctxs(tests: &[&'static str]) -> HashMap<&'static str, Vec<String>> {
    let mut ftl_strings = HashMap::new();
    for test in tests {
        let paths = fs::read_dir(format!("./benches/contexts/{}", test)).unwrap();
        let strings = paths
            .into_iter()
            .map(|p| {
                let p = p.unwrap().path();
                let path = p.to_str().unwrap();
                read_file(path).unwrap()
            })
            .collect::<Vec<_>>();
        ftl_strings.insert(*test, strings);
    }
    return ftl_strings;
}

fn parse_bench(c: &mut Criterion) {
    #[cfg(feature = "all-benchmarks")]
    {
        let tests = &["simple.ftl", "preferences.ftl", "menubar.ftl"];

        let mut group = c.benchmark_group("parse_resource");

        for (name, resource) in get_resources(tests) {
            group.bench_with_input(name, &resource, |b, source| {
                b.iter(|| parse_runtime(source.as_str()).expect("Parsing of the FTL failed."))
            });
        }

        group.finish();
    }

    let ctx_names = &["browser", "preferences"];

    #[cfg(feature = "all-benchmarks")]
    {
        use fluent_syntax::parser::parse;

        let mut group = c.benchmark_group("parse_ctx");

        for (name, ctx) in get_ctxs(ctx_names) {
            group.bench_with_input(name, &ctx, |b, ctx| {
                b.iter(|| {
                    for source in ctx {
                        parse(source.as_str()).expect("Parsing of the FTL failed.");
                    }
                })
            });
        }

        group.finish();
    }

    {
        let mut group = c.benchmark_group("parse_ctx_runtime");

        for (name, ctx) in get_ctxs(ctx_names) {
            group.bench_with_input(name, &ctx, |b, ctx| {
                b.iter(|| {
                    for source in ctx {
                        parse_runtime(source.as_str()).expect("Parsing of the FTL failed.");
                    }
                })
            });
        }

        group.finish();
    }

    #[cfg(feature = "all-benchmarks")]
    {
        use fluent_syntax::unicode::{unescape_unicode, unescape_unicode_to_string};

        let strings = &[
            "foo",
            "This is an example value",
            "Hello \\u00e3\\u00e9 World",
            "\\u004c\\u006f\\u0072\\u0065\\u006d \\u0069\\u0070\\u0073\\u0075\\u006d \\u0064\\u006f\\u006c\\u006f\\u0072 \\u0073\\u0069\\u0074 \\u0061\\u006d\\u0065\\u0074",
            "Let me introduce \\\"The\\\" Fluent",
            "And here's an example of \\\\ a character to be escaped",
            "But this message is completely unescape free",
            "And so is this one",
            "Maybe this one is as well completely escape free",
            "Welcome to Mozilla Firefox",
            "\\u0054\\u0068\\u0065\\u0073\\u0065 \\u0073\\u0065\\u0074\\u0074\\u0069\\u006e\\u0067\\u0073 \\u0061\\u0072\\u0065 \\u0074\\u0061\\u0069\\u006c\\u006f\\u0072\\u0065\\u0064 \\u0074\\u006f \\u0079\\u006f\\u0075\\u0072 \\u0063\\u006f\\u006d\\u0070\\u0075\\u0074\\u0065\\u0072\\u2019\\u0073 \\u0068\\u0061\\u0072\\u0064\\u0077\\u0061\\u0072\\u0065 \\u0061\\u006e\\u0064 \\u006f\\u0070\\u0065\\u0072\\u0061\\u0074\\u0069\\u006e\\u0067 \\u0073\\u0079\\u0073\\u0074\\u0065\\u006d\\u002e",
            "These settings are tailored to your computer’s hardware and operating system",
            "Use recommended performance settings",
            "\\u0041\\u0064\\u0064\\u0069\\u0074\\u0069\\u006f\\u006e\\u0061\\u006c \\u0063\\u006f\\u006e\\u0074\\u0065\\u006e\\u0074 \\u0070\\u0072\\u006f\\u0063\\u0065\\u0073\\u0073\\u0065\\u0073 \\u0063\\u0061\\u006e \\u0069\\u006d\\u0070\\u0072\\u006f\\u0076\\u0065 \\u0070\\u0065\\u0072\\u0066\\u006f\\u0072\\u006d\\u0061\\u006e\\u0063\\u0065 \\u0077\\u0068\\u0065\\u006e \\u0075\\u0073\\u0069\\u006e\\u0067 \\u006d\\u0075\\u006c\\u0074\\u0069\\u0070\\u006c\\u0065 \\u0074\\u0061\\u0062\\u0073\\u002c \\u0062\\u0075\\u0074 \\u0077\\u0069\\u006c\\u006c \\u0061\\u006c\\u0073\\u006f \\u0075\\u0073\\u0065 \\u006d\\u006f\\u0072\\u0065 \\u006d\\u0065\\u006d\\u006f\\u0072\\u0079\\u002e",
            "Additional content processes can improve performance when using multiple tabs, but will also use more memory.",
        ];

        let mut group = c.benchmark_group("unicode");

        group.bench_function("writer", |b| {
            b.iter(|| {
                let mut result = String::new();
                for s in strings {
                    unescape_unicode(&mut result, s).unwrap();
                    result.clear();
                }
            })
        });

        group.bench_function("to_string", |b| {
            b.iter(|| {
                for s in strings {
                    let _ = unescape_unicode_to_string(s);
                }
            })
        });

        group.finish();
    }
}

criterion_group!(benches, parse_bench,);
criterion_main!(benches);
