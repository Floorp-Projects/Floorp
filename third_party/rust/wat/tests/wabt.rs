//! Finds as many tests as we can in the `wabt` submodule and does a few things:
//!
//! * First, asserts that we can parse and encode them all to binary.
//! * Next uses `wat2wasm` to encode to binary.
//! * Finally, asserts that the two binary encodings are byte-for-byte the same.
//!
//! This also has support for handling `*.wast` files from the official test
//! suite which involve parsing as a wast file and handling assertions. Also has
//! rudimentary support for running some of the assertions.

use rayon::prelude::*;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::str;
use std::sync::atomic::{AtomicUsize, Ordering::SeqCst};
use wast::parser::ParseBuffer;
use wast::*;

fn main() {
    let tests = find_tests();
    let filter = std::env::args().nth(1);

    let tests = tests
        .par_iter()
        .filter_map(|test| {
            if let Some(filter) = &filter {
                if let Some(s) = test.file_name().and_then(|s| s.to_str()) {
                    if !s.contains(filter) {
                        return None;
                    }
                }
            }
            let contents = std::fs::read_to_string(test).unwrap();
            if skip_test(&test, &contents) {
                None
            } else {
                Some((test, contents))
            }
        })
        .collect::<Vec<_>>();

    println!("running {} test files\n", tests.len());

    let ntests = AtomicUsize::new(0);
    let errors = tests
        .par_iter()
        .filter_map(|(test, contents)| run_test(test, contents, &ntests).err())
        .collect::<Vec<_>>();

    if !errors.is_empty() {
        for msg in errors.iter() {
            eprintln!("{}", msg);
        }

        panic!("{} tests failed", errors.len())
    }

    println!(
        "test result: ok. {} directives passed\n",
        ntests.load(SeqCst)
    );
}

fn run_test(test: &Path, contents: &str, ntests: &AtomicUsize) -> anyhow::Result<()> {
    let wast = contents.contains("TOOL: wast2json")
        || contents.contains("TOOL: run-objdump-spec")
        || test.display().to_string().ends_with(".wast");
    if wast {
        return test_wast(test, contents, ntests);
    }
    let binary = wat::parse_file(test)?;
    ntests.fetch_add(1, SeqCst); // tested the parse

    // FIXME(#5) fix these tests
    if test.ends_with("invalid-elem-segment-offset.txt")
        || test.ends_with("invalid-data-segment-offset.txt")
    {
        return Ok(());
    }

    if let Some(expected) = wat2wasm(&test) {
        binary_compare(&test, 0, &binary, &expected)?;
        ntests.fetch_add(1, SeqCst); // tested the wabt compare
    }
    Ok(())
}

fn test_wast(test: &Path, contents: &str, ntests: &AtomicUsize) -> anyhow::Result<()> {
    macro_rules! adjust {
        ($e:expr) => {{
            let mut e = wast::Error::from($e);
            e.set_path(test);
            e.set_text(contents);
            e
        }};
    }
    let buf = ParseBuffer::new(contents).map_err(|e| adjust!(e))?;
    let wast = parser::parse::<Wast>(&buf).map_err(|e| adjust!(e))?;
    ntests.fetch_add(1, SeqCst); // wast parse test
    let json = wast2json(&test);

    // Pair each `Module` directive with the result of wast2json's output
    // `*.wasm` file, and then execute each test in parallel.
    let mut modules = 0;
    let directives = wast
        .directives
        .into_iter()
        .map(|directive| match directive {
            WastDirective::Module(_) => {
                modules += 1;
                (directive, json.as_ref().map(|j| &j.modules[modules - 1]))
            }
            other => (other, None),
        })
        .collect::<Vec<_>>();

    let results = directives
        .into_par_iter()
        .map(|(directive, expected)| {
            match directive {
                WastDirective::Module(mut module) => {
                    assert_eq!(expected.is_some(), json.is_some());

                    let actual = module.encode().map_err(|e| adjust!(e))?;
                    ntests.fetch_add(1, SeqCst); // testing encode
                    match module.kind {
                        ModuleKind::Text(_) => {
                            if let Some(expected) = &expected {
                                let expected = fs::read(expected)?;
                                let (line, _) = module.span.linecol_in(contents);
                                binary_compare(&test, line, &actual, &expected)?;
                                ntests.fetch_add(1, SeqCst); // testing compare
                            }
                        }
                        // Skip these for the same reason we skip
                        // `module/binary-module.txt` in `binary_compare` below.
                        ModuleKind::Binary(_) => {}
                    }
                }

                WastDirective::QuoteModule { source, span } => {
                    if let Err(e) = parse_quote_module(span, &source) {
                        let (line, col) = span.linecol_in(&contents);
                        anyhow::bail!(
                            "in test {}:{}:{} parsed with\nerror: {}",
                            test.display(),
                            line + 1,
                            col + 1,
                            e,
                        );
                    }
                }

                WastDirective::AssertMalformed {
                    span,
                    module: QuoteModule::Quote(source),
                    message,
                } => {
                    let result = parse_quote_module(span, &source);
                    let (line, col) = span.linecol_in(&contents);
                    match result {
                        Ok(()) => anyhow::bail!(
                            "\
                             test {}:{}:{} parsed successfully\n\
                             but should have failed with: {}\
                             ",
                            test.display(),
                            line + 1,
                            col + 1,
                            message,
                        ),
                        Err(e) => {
                            if error_matches(&e.to_string(), message) {
                                ntests.fetch_add(1, SeqCst);
                                return Ok(());
                            }
                            anyhow::bail!(
                                "\
                                 in test {}:{}:{} parsed with:\nerror: {}\n\
                                 but should have failed with: {:?}\
                                 ",
                                test.display(),
                                line + 1,
                                col + 1,
                                e,
                                message,
                            );
                        }
                    }
                }
                _ => {}
            }

            Ok(())
        })
        .collect::<Vec<_>>();

    let errors = results
        .into_iter()
        .filter_map(|e| e.err())
        .collect::<Vec<_>>();
    if errors.is_empty() {
        return Ok(());
    }
    let mut s = format!("{} test failures in {}:", errors.len(), test.display());
    for error in errors {
        s.push_str("\n\n\t--------------------------------\n\n\t");
        s.push_str(&error.to_string().replace("\n", "\n\t"));
    }
    anyhow::bail!("{}", s)
}

fn parse_quote_module(span: Span, source: &[&[u8]]) -> Result<(), wast::Error> {
    let mut ret = String::new();
    for src in source {
        match str::from_utf8(src) {
            Ok(s) => ret.push_str(s),
            Err(_) => {
                return Err(wast::Error::new(
                    span,
                    "malformed UTF-8 encoding".to_string(),
                ))
            }
        }
        ret.push_str(" ");
    }
    let buf = ParseBuffer::new(&ret)?;
    let mut wat = parser::parse::<Wat>(&buf)?;
    wat.module.encode()?;
    Ok(())
}

fn error_matches(error: &str, message: &str) -> bool {
    if error.contains(message) {
        return true;
    }
    if message == "unknown operator"
        || message == "unexpected token"
        || message == "wrong number of lane literals"
        || message == "type mismatch"
        || message == "malformed lane index"
        || message == "expected i8 literal"
        || message == "invalid lane length"
        || message == "unclosed annotation"
        || message == "malformed annotation id"
        || message == "alignment must be a power of two"
    {
        return error.contains("expected ")
            || error.contains("constant out of range")
            || error.contains("extra tokens remaining");
    }

    if message == "illegal character" {
        return error.contains("unexpected character");
    }

    if message == "unclosed string" {
        return error.contains("unexpected end-of-file");
    }

    if message == "invalid UTF-8 encoding" {
        return error.contains("malformed UTF-8 encoding");
    }

    if message == "duplicate identifier" {
        return error.contains("duplicate") && error.contains("identifier");
    }

    return false;
}

fn find_tests() -> Vec<PathBuf> {
    let mut tests = Vec::new();
    if !Path::new("tests/wabt").exists() {
        panic!("submodules need to be checked out");
    }
    find_tests("tests/wabt/test/desugar".as_ref(), &mut tests);
    find_tests("tests/wabt/test/dump".as_ref(), &mut tests);
    find_tests("tests/wabt/test/interp".as_ref(), &mut tests);
    find_tests("tests/wabt/test/parse".as_ref(), &mut tests);
    find_tests("tests/wabt/test/roundtrip".as_ref(), &mut tests);
    find_tests("tests/wabt/test/spec".as_ref(), &mut tests);
    find_tests("tests/wabt/test/typecheck".as_ref(), &mut tests);
    find_tests("tests/wabt/third_party/testsuite".as_ref(), &mut tests);
    find_tests("tests/regression".as_ref(), &mut tests);
    find_tests("tests/testsuite".as_ref(), &mut tests);
    tests.sort();

    return tests;

    fn find_tests(path: &Path, tests: &mut Vec<PathBuf>) {
        for f in path.read_dir().unwrap() {
            let f = f.unwrap();
            if f.file_type().unwrap().is_dir() {
                find_tests(&f.path(), tests);
                continue;
            }

            match f.path().extension().and_then(|s| s.to_str()) {
                Some("txt") | Some("wast") | Some("wat") => {}
                _ => continue,
            }
            tests.push(f.path());
        }
    }
}

fn binary_compare(
    test: &Path,
    line: usize,
    actual: &[u8],
    expected: &[u8],
) -> Result<(), anyhow::Error> {
    use wasmparser::*;

    // I tried for a bit but honestly couldn't figure out a great way to match
    // wabt's encoding of the name section. Just remove it from our asserted
    // sections and don't compare against wabt's.
    let actual = remove_name_section(actual);

    if actual == expected {
        return Ok(());
    }

    let difference = actual
        .iter()
        .enumerate()
        .zip(expected)
        .find(|((_, actual), expected)| actual != expected);
    let pos = match difference {
        Some(((pos, _), _)) => format!("at byte {} ({0:#x})", pos),
        None => format!("by being too small"),
    };
    let mut msg = format!(
        "
error: actual wasm differs {pos} from expected wasm
      --> {file}:{line}
",
        pos = pos,
        file = test.display(),
        line = line + 1,
    );

    if let Some(((pos, _), _)) = difference {
        msg.push_str(&format!("  {:4} |   {:#04x}\n", pos - 2, actual[pos - 2]));
        msg.push_str(&format!("  {:4} |   {:#04x}\n", pos - 1, actual[pos - 1]));
        msg.push_str(&format!("  {:4} | - {:#04x}\n", pos, expected[pos]));
        msg.push_str(&format!("       | + {:#04x}\n", actual[pos]));
    }

    let mut actual_parser = Parser::new(&actual);
    let mut expected_parser = Parser::new(&expected);

    let mut differences = 0;
    let mut last_dots = false;
    while differences < 5 {
        let actual_state = match read_state(&mut actual_parser) {
            Some(s) => s,
            None => break,
        };
        let expected_state = match read_state(&mut expected_parser) {
            Some(s) => s,
            None => break,
        };

        if actual_state == expected_state {
            if differences > 0 && !last_dots {
                msg.push_str(&format!("       |   ...\n"));
                last_dots = true;
            }
            continue;
        }
        last_dots = false;

        if differences == 0 {
            msg.push_str("\n\n");
        }
        msg.push_str(&format!(
            "  {:4} | - {}\n",
            expected_parser.current_position(),
            expected_state
        ));
        msg.push_str(&format!(
            "  {:4} | + {}\n",
            actual_parser.current_position(),
            actual_state
        ));
        differences += 1;
    }

    anyhow::bail!("{}", msg);

    fn read_state<'a, 'b>(parser: &'b mut Parser<'a>) -> Option<String> {
        loop {
            match parser.read() {
                // ParserState::BeginSection { code: SectionCode::DataCount, .. } => {}
                // ParserState::DataCountSectionEntry(_) => {}
                ParserState::Error(_) | ParserState::EndWasm => break None,
                other => break Some(format!("{:?}", other)),
            }
        }
    }

    fn remove_name_section(bytes: &[u8]) -> Vec<u8> {
        let mut r = ModuleReader::new(bytes).expect("should produce valid header");
        while !r.eof() {
            let start = r.current_position();
            if let Ok(s) = r.read() {
                match s.code {
                    SectionCode::Custom { name: "name", .. } => {
                        let mut bytes = bytes.to_vec();
                        bytes.drain(start..s.range().end);
                        return bytes;
                    }
                    _ => {}
                }
            }
        }
        return bytes.to_vec();
    }
}

fn wat2wasm(test: &Path) -> Option<Vec<u8>> {
    let f = tempfile::NamedTempFile::new().unwrap();
    let result = Command::new("wat2wasm")
        .arg(test)
        .arg("--enable-all")
        .arg("--no-check")
        .arg("-o")
        .arg(f.path())
        .output()
        .expect("failed to spawn `wat2wasm`");
    if result.status.success() {
        Some(fs::read(f.path()).unwrap())
    } else {
        // TODO: handle this case better
        None
    }
}

struct Wast2Json {
    _td: tempfile::TempDir,
    modules: Vec<PathBuf>,
}

fn wast2json(test: &Path) -> Option<Wast2Json> {
    let td = tempfile::TempDir::new().unwrap();
    let result = Command::new("wast2json")
        .arg(test)
        .arg("--enable-all")
        .arg("--no-check")
        .arg("-o")
        .arg(td.path().join("foo.json"))
        .output()
        .expect("failed to spawn `wat2wasm`");
    if !result.status.success() {
        // TODO: handle this case better
        return None;
    }
    let json = fs::read_to_string(td.path().join("foo.json")).unwrap();
    let json: serde_json::Value = serde_json::from_str(&json).unwrap();
    let commands = json["commands"].as_array().unwrap();
    let modules = commands
        .iter()
        .filter_map(|m| {
            if m["type"] == "module" {
                Some(td.path().join(m["filename"].as_str().unwrap()))
            } else {
                None
            }
        })
        .collect();
    Some(Wast2Json { _td: td, modules })
}

fn skip_test(test: &Path, contents: &str) -> bool {
    // Skip tests that are supposed to fail
    if contents.contains(";; ERROR") {
        return true;
    }
    // These tests are acually ones that run with the `*.wast` files from the
    // official test suite, and we slurp those up elsewhere anyway.
    if contents.contains("STDIN_FILE") {
        return true;
    }

    // We've made the opinionated decision that well-known annotations like
    // `@custom` and `@name` must be well-formed. This test, however, uses
    // `@custom` in ways the spec doesn't specify, so we skip it.
    if test.ends_with("test/parse/annotations.txt") {
        return true;
    }

    // Waiting for wabt to recognize the integer abs simd opcodes; they have been added
    // upstream in the spec test suite, https://github.com/WebAssembly/simd/pull/197,
    // but need to be pulled, filed https://github.com/WebAssembly/wabt/issues/1379.
    if test.ends_with("wabt/third_party/testsuite/proposals/simd/simd_f32x4.wast") {
        return true;
    }

    // wait for wabt to catch up on the annotations spec test
    if test.ends_with("wabt/third_party/testsuite/proposals/annotations/annotations.wast") {
        return true;
    }

    false
}
