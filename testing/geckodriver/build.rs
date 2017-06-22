/// Writes build information to ${OUT_DIR}/build-info.rs which is included in
/// the program during compilation:
///
/// ```no_run
/// const COMMIT_HASH: Option<&'static str> = Some("c31a366");
/// const COMMIT_DATE: Option<&'static str> = Some("1988-05-10");
/// ```
///
/// The values are `None` if running hg failed, e.g. if it is not installed or
/// if we are not in an hg repo.

use std::env;
use std::ffi::OsStr;
use std::fs::File;
use std::io::Write;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    let out_dir = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    let mut fh = File::create(out_dir.join("build-info.rs")).unwrap();
    writeln!(
        fh,
        "const COMMIT_HASH: Option<&'static str> = {:?};",
        commit_hash()
    ).unwrap();
    writeln!(
        fh,
        "const COMMIT_DATE: Option<&'static str> = {:?};",
        commit_date()
    ).unwrap();
}

fn commit_hash() -> Option<String> {
    exec(&"hg", &["log", "-r.", "-T '{node|short}'"]).or_else(
        || {
            exec(&"git", &["rev-parse", "HEAD"]).and_then(hg2git_sha)
        },
    )
}

fn commit_date() -> Option<String> {
    exec(&"hg", &["log", "-r.", "-T '{date|isodate}'"]).or_else(|| {
        exec(
            &"git",
            &["log", "-1", "--date=short", "--pretty=format:%cd"],
        )
    })
}

fn exec<S, I>(program: S, args: I) -> Option<String>
where
    S: AsRef<OsStr>,
    I: IntoIterator<Item = S>,
{
    let mut cmd = Command::new(program);
    for arg in args {
        cmd.arg(arg.as_ref());
    }
    cmd.output()
        .ok()
        .and_then(|r| if r.status.success() {
            Some(r.stdout)
        } else {
            None
        })
        .and_then(|o| String::from_utf8(o).ok())
        .map(|s| s.trim_right().into())
}

fn hg2git_sha(hg_sha: String) -> Option<String> {
    exec(&"git", &["cinnabar", "git2hg", &hg_sha])
}
