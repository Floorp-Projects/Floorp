use std::error;
use std::io::{BufRead, BufReader, Write};
use std::process::{Child, Command, Stdio};
use std::result;

#[allow(unused)]
type Error = Box<dyn error::Error + std::marker::Send + std::marker::Sync>;
#[allow(unused)]
pub type Result<T> = result::Result<T, Error>;

fn build_command() -> Command {
    let mut cmd = Command::new("cargo");

    cmd.env("RUST_BACKTRACE", "1")
        .args(["run", "-q", "--bin", "test"]);

    // In normal cases where the host and target are the same this won't matter,
    // but tests will fail if you are eg running in a cross container which will
    // likely be x86_64 but may be targetting aarch64 or i686, which will result
    // in tests failing, or at the least not testing what you think
    cmd.args(["--target", current_platform::CURRENT_PLATFORM, "--"]);

    cmd
}

#[allow(unused)]
pub fn spawn_child(command: &str, args: &[&str]) {
    let mut cmd = build_command();
    cmd.arg(command).args(args);

    let child = cmd.output().expect("failed to execute child");

    println!("Child output:");
    std::io::stdout().write_all(&child.stdout).unwrap();
    std::io::stdout().write_all(&child.stderr).unwrap();
    assert_eq!(child.status.code().expect("No return value"), 0);
}

fn start_child_and_wait_for_threads_helper(command: &str, num: usize) -> Child {
    let mut cmd = build_command();
    cmd.arg(command).arg(num.to_string());
    cmd.stdout(Stdio::piped());

    let mut child = cmd.spawn().expect("failed to spawn cargo");
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
    let mut cmd = build_command();
    cmd.args(args);

    cmd.stdout(Stdio::piped())
        .spawn()
        .expect("failed to execute child")
}
