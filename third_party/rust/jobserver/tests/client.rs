use std::env;
use std::fs::File;
use std::io::Write;
use std::process::Command;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc;
use std::sync::Arc;
use std::thread;

use futures::future::{self, Future};
use futures::stream::{self, Stream};
use jobserver::Client;
use tempdir::TempDir;
use tokio_core::reactor::Core;
use tokio_process::CommandExt;

macro_rules! t {
    ($e:expr) => {
        match $e {
            Ok(e) => e,
            Err(e) => panic!("{} failed with {}", stringify!($e), e),
        }
    };
}

struct Test {
    name: &'static str,
    f: &'static dyn Fn(),
    make_args: &'static [&'static str],
    rule: &'static dyn Fn(&str) -> String,
}

const TESTS: &[Test] = &[
    Test {
        name: "no j args",
        make_args: &[],
        rule: &|me| me.to_string(),
        f: &|| {
            assert!(unsafe { Client::from_env().is_none() });
        },
    },
    Test {
        name: "no j args with plus",
        make_args: &[],
        rule: &|me| format!("+{}", me),
        f: &|| {
            assert!(unsafe { Client::from_env().is_none() });
        },
    },
    Test {
        name: "j args with plus",
        make_args: &["-j2"],
        rule: &|me| format!("+{}", me),
        f: &|| {
            assert!(unsafe { Client::from_env().is_some() });
        },
    },
    Test {
        name: "acquire",
        make_args: &["-j2"],
        rule: &|me| format!("+{}", me),
        f: &|| {
            let c = unsafe { Client::from_env().unwrap() };
            drop(c.acquire().unwrap());
            drop(c.acquire().unwrap());
        },
    },
    Test {
        name: "acquire3",
        make_args: &["-j3"],
        rule: &|me| format!("+{}", me),
        f: &|| {
            let c = unsafe { Client::from_env().unwrap() };
            let a = c.acquire().unwrap();
            let b = c.acquire().unwrap();
            drop((a, b));
        },
    },
    Test {
        name: "acquire blocks",
        make_args: &["-j2"],
        rule: &|me| format!("+{}", me),
        f: &|| {
            let c = unsafe { Client::from_env().unwrap() };
            let a = c.acquire().unwrap();
            let hit = Arc::new(AtomicBool::new(false));
            let hit2 = hit.clone();
            let (tx, rx) = mpsc::channel();
            let t = thread::spawn(move || {
                tx.send(()).unwrap();
                let _b = c.acquire().unwrap();
                hit2.store(true, Ordering::SeqCst);
            });
            rx.recv().unwrap();
            assert!(!hit.load(Ordering::SeqCst));
            drop(a);
            t.join().unwrap();
            assert!(hit.load(Ordering::SeqCst));
        },
    },
    Test {
        name: "acquire_raw",
        make_args: &["-j2"],
        rule: &|me| format!("+{}", me),
        f: &|| {
            let c = unsafe { Client::from_env().unwrap() };
            c.acquire_raw().unwrap();
            c.release_raw().unwrap();
        },
    },
];

fn main() {
    if let Ok(test) = env::var("TEST_TO_RUN") {
        return (TESTS.iter().find(|t| t.name == test).unwrap().f)();
    }

    let me = t!(env::current_exe());
    let me = me.to_str().unwrap();
    let filter = env::args().nth(1);

    let mut core = t!(Core::new());

    let futures = TESTS
        .iter()
        .filter(|test| match filter {
            Some(ref s) => test.name.contains(s),
            None => true,
        })
        .map(|test| {
            let td = t!(TempDir::new("foo"));
            let makefile = format!(
                "\
all: export TEST_TO_RUN={}
all:
\t{}
",
                test.name,
                (test.rule)(me)
            );
            t!(t!(File::create(td.path().join("Makefile"))).write_all(makefile.as_bytes()));
            let prog = env::var("MAKE").unwrap_or_else(|_| "make".to_string());
            let mut cmd = Command::new(prog);
            cmd.args(test.make_args);
            cmd.current_dir(td.path());
            future::lazy(move || {
                cmd.output_async().map(move |e| {
                    drop(td);
                    (test, e)
                })
            })
        })
        .collect::<Vec<_>>();

    println!("\nrunning {} tests\n", futures.len());

    let stream = stream::iter(futures.into_iter().map(Ok)).buffer_unordered(num_cpus::get());

    let mut failures = Vec::new();
    t!(core.run(stream.for_each(|(test, output)| {
        if output.status.success() {
            println!("test {} ... ok", test.name);
        } else {
            println!("test {} ... FAIL", test.name);
            failures.push((test, output));
        }
        Ok(())
    })));

    if failures.is_empty() {
        println!("\ntest result: ok\n");
        return;
    }

    println!("\n----------- failures");

    for (test, output) in failures {
        println!("test {}", test.name);
        let stdout = String::from_utf8_lossy(&output.stdout);
        let stderr = String::from_utf8_lossy(&output.stderr);

        println!("\texit status: {}", output.status);
        if !stdout.is_empty() {
            println!("\tstdout ===");
            for line in stdout.lines() {
                println!("\t\t{}", line);
            }
        }

        if !stderr.is_empty() {
            println!("\tstderr ===");
            for line in stderr.lines() {
                println!("\t\t{}", line);
            }
        }
    }

    std::process::exit(4);
}
