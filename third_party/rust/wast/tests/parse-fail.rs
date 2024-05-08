//! A test suite to parse everything in `parse-fail` and assert that it matches
//! the `*.err` file it generates.
//!
//! Use `BLESS=1` in the environment to auto-update `*.err` files. Be sure to
//! look at the diff!

use libtest_mimic::{Arguments, Trial};
use std::env;
use std::path::{Path, PathBuf};

fn main() {
    let mut tests = Vec::new();
    find_tests("tests/parse-fail".as_ref(), &mut tests);
    let bless = env::var("BLESS").is_ok();

    let mut trials = Vec::new();
    for test in tests {
        let trial = Trial::test(format!("{test:?}"), move || {
            run_test(&test, bless).map_err(|e| format!("{e:?}").into())
        });
        trials.push(trial);
    }

    let mut args = Arguments::from_args();
    if cfg!(target_family = "wasm") && !cfg!(target_feature = "atomics") {
        args.test_threads = Some(1);
    }
    libtest_mimic::run(&args, trials).exit();
}

fn run_test(test: &Path, bless: bool) -> anyhow::Result<()> {
    let err = match wat::parse_file(test) {
        Ok(_) => anyhow::bail!("{} parsed successfully", test.display()),
        Err(e) => e.to_string() + "\n",
    };
    let assert = test.with_extension("wat.err");
    if bless {
        std::fs::write(assert, err.to_string())?;
        return Ok(());
    }

    // Ignore CRLF line ending and force always `\n`
    let assert = std::fs::read_to_string(assert)
        .unwrap_or(String::new())
        .replace("\r\n", "\n");

    // Compare normalize verisons which handles weirdness like path differences
    if normalize(&assert) == normalize(&err) {
        return Ok(());
    }

    anyhow::bail!(
        "errors did not match:\n\nexpected:\n\t{}\nactual:\n\t{}\n",
        tab(&assert),
        tab(&err),
    );

    fn normalize(s: &str) -> String {
        s.replace("\\", "/")
    }

    fn tab(s: &str) -> String {
        s.replace("\n", "\n\t")
    }
}

fn find_tests(path: &Path, tests: &mut Vec<PathBuf>) {
    for f in path.read_dir().unwrap() {
        let f = f.unwrap();
        if f.file_type().unwrap().is_dir() {
            find_tests(&f.path(), tests);
            continue;
        }
        match f.path().extension().and_then(|s| s.to_str()) {
            Some("wat") => {}
            _ => continue,
        }
        tests.push(f.path());
    }
}
