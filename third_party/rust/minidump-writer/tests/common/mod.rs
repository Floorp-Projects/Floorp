use std::error;
use std::io::{BufRead, BufReader, Write};
use std::process::{Child, Command, Stdio};
use std::result;

#[allow(unused)]
type Error = Box<dyn error::Error + std::marker::Send + std::marker::Sync>;
#[allow(unused)]
pub type Result<T> = result::Result<T, Error>;

#[allow(unused)]
pub fn spawn_child(command: &str, args: &[&str]) {
    let mut cmd_object = Command::new("cargo");
    let mut cmd_ref = cmd_object
        .env("RUST_BACKTRACE", "1")
        .arg("run")
        .arg("-q")
        .arg("--bin")
        .arg("test")
        .arg("--")
        .arg(command);
    for arg in args {
        cmd_ref = cmd_ref.arg(arg);
    }
    let child = cmd_ref.output().expect("failed to execute child");

    println!("Child output:");
    std::io::stdout().write_all(&child.stdout).unwrap();
    std::io::stdout().write_all(&child.stderr).unwrap();
    assert_eq!(child.status.code().expect("No return value"), 0);
}

fn start_child_and_wait_for_threads_helper(cmd: &str, num: usize) -> Child {
    let mut child = Command::new("cargo")
        .env("RUST_BACKTRACE", "1")
        .arg("run")
        .arg("-q")
        .arg("--bin")
        .arg("test")
        .arg("--")
        .arg(cmd)
        .arg(format!("{}", num))
        .stdout(Stdio::piped())
        .spawn()
        .expect("failed to execute child");

    wait_for_threads(&mut child, num);
    child
}

#[allow(unused)]
pub fn start_child_and_wait_for_threads(num: usize) -> Child {
    start_child_and_wait_for_threads_helper("spawn_and_wait", num)
}

#[allow(unused)]
pub fn start_child_and_wait_for_named_threads(num: usize) -> Child {
    start_child_and_wait_for_threads_helper("spawn_name_wait", num)
}

#[allow(unused)]
pub fn start_child_and_wait_for_create_files(num: usize) -> Child {
    start_child_and_wait_for_threads_helper("create_files_wait", num)
}

#[allow(unused)]
pub fn wait_for_threads(child: &mut Child, num: usize) {
    let mut f = BufReader::new(child.stdout.as_mut().expect("Can't open stdout"));
    let mut lines = 0;
    while lines < num {
        let mut buf = String::new();
        match f.read_line(&mut buf) {
            Ok(_) => {
                if buf == "1\n" {
                    lines += 1;
                }
            }
            Err(e) => {
                std::panic::panic_any(e);
            }
        }
    }
}

#[allow(unused)]
pub fn start_child_and_return(args: &[&str]) -> Child {
    let mut child = Command::new("cargo")
        .env("RUST_BACKTRACE", "1")
        .arg("run")
        .arg("-q")
        .arg("--bin")
        .arg("test")
        .arg("--")
        .args(args)
        .stdout(Stdio::piped())
        .spawn()
        .expect("failed to execute child");

    child
}
