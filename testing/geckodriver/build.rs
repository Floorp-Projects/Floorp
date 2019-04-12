// Writes build information to ${OUT_DIR}/build-info.rs which is included in
// the program during compilation:
//
// ```no_run
// const COMMIT_HASH: Option<&'static str> = Some("c31a366");
// const COMMIT_DATE: Option<&'static str> = Some("1988-05-10");
// ```
//
// The values are `None` if running hg failed, e.g. if it is not installed or
// if we are not in an hg repo.

use std::env;
use std::ffi::OsStr;
use std::fs::File;
use std::io;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::Command;

fn main() -> io::Result<()> {
    let cur_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let build_info = get_build_info(&cur_dir);

    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    let mut fh = File::create(out_dir.join("build-info.rs"))?;
    writeln!(
        fh,
        "const COMMIT_HASH: Option<&'static str> = {:?};",
        build_info.hash()
    )?;
    writeln!(
        fh,
        "const COMMIT_DATE: Option<&'static str> = {:?};",
        build_info.date()
    )?;

    Ok(())
}

fn get_build_info(dir: &Path) -> Box<BuildInfo> {
    if Path::exists(&dir.join(".hg")) {
        Box::new(Hg {})
    } else if Path::exists(&dir.join(".git")) {
        Box::new(Git {})
    } else {
        if let Some(parent) = dir.parent() {
            get_build_info(parent)
        } else {
            eprintln!("unable to detect vcs");
            Box::new(Noop {})
        }
    }
}

trait BuildInfo {
    fn hash(&self) -> Option<String>;
    fn date(&self) -> Option<String>;
}

struct Hg;

impl Hg {
    fn exec<I, S>(&self, args: I) -> Option<String>
    where
        I: IntoIterator<Item = S>,
        S: AsRef<OsStr>,
    {
        Command::new("hg")
            .env("HGPLAIN", "1")
            .args(args)
            .output()
            .ok()
            .and_then(|r| String::from_utf8(r.stdout).ok())
            .map(|s| s.trim_end().into())
    }
}

impl BuildInfo for Hg {
    fn hash(&self) -> Option<String> {
        self.exec(&["log", "-r.", "-T{node|short}"])
    }

    fn date(&self) -> Option<String> {
        self.exec(&["log", "-r.", "-T{date|isodate}"])
    }
}

struct Git;

impl Git {
    fn exec<I, S>(&self, args: I) -> Option<String>
    where
        I: IntoIterator<Item = S>,
        S: AsRef<OsStr>,
    {
        Command::new("git")
            .env("GIT_CONFIG_NOSYSTEM", "1")
            .args(args)
            .output()
            .ok()
            .and_then(|r| String::from_utf8(r.stdout).ok())
            .map(|s| s.trim_end().into())
    }

    fn to_hg_sha(&self, git_sha: String) -> Option<String> {
        self.exec(&["cinnabar", "git2hg", &git_sha])
    }
}

impl BuildInfo for Git {
    fn hash(&self) -> Option<String> {
        self.exec(&["rev-parse", "HEAD"])
            .and_then(|sha| self.to_hg_sha(sha))
            .map(|mut s| {
              s.truncate(12);
              s
            })
    }

    fn date(&self) -> Option<String> {
        self.exec(&["log", "-1", "--date=short", "--pretty=format:%cd"])
    }
}

struct Noop;

impl BuildInfo for Noop {
    fn hash(&self) -> Option<String> {
        None
    }
    fn date(&self) -> Option<String> {
        None
    }
}
