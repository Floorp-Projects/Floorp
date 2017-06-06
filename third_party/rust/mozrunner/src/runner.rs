use mozprofile::prefreader::PrefReaderError;
use mozprofile::profile::Profile;
use std::convert::From;
use std::env;
use std::error::Error;
use std::fmt;
use std::io::{Result as IoResult, Error as IoError, ErrorKind};
use std::path::{Path, PathBuf};
use std::process;
use std::process::{Command, Stdio};

pub trait Runner {
    fn start(&mut self) -> Result<(), RunnerError>;

    fn args(&mut self) -> &mut Vec<String>;

    fn build_command(&self, &mut Command);

    fn is_running(&self) -> bool;

    fn stop(&mut self) -> IoResult<Option<process::ExitStatus>>;
}

#[derive(Debug)]
pub enum RunnerError {
    Io(IoError),
    PrefReader(PrefReaderError),
}

impl fmt::Display for RunnerError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.description().fmt(f)
    }
}

impl Error for RunnerError {
    fn description(&self) -> &str {
        match *self {
            RunnerError::Io(ref err) => {
                match err.kind() {
                    ErrorKind::NotFound => "no such file or directory",
                    _ => err.description(),
                }
            }
            RunnerError::PrefReader(ref err) => err.description(),
        }
    }

    fn cause(&self) -> Option<&Error> {
        Some(match *self {
            RunnerError::Io(ref err) => err as &Error,
            RunnerError::PrefReader(ref err) => err as &Error,
        })
    }
}

impl From<IoError> for RunnerError {
    fn from(value: IoError) -> RunnerError {
        RunnerError::Io(value)
    }
}

impl From<PrefReaderError> for RunnerError {
    fn from(value: PrefReaderError) -> RunnerError {
        RunnerError::PrefReader(value)
    }
}

pub struct FirefoxRunner {
    pub binary: PathBuf,
    args: Vec<String>,
    process: Option<process::Child>,
    pub ret_code: Option<process::ExitStatus>,
    pub profile: Profile
}

impl FirefoxRunner {
    pub fn new(binary: &Path, profile: Option<Profile>) -> IoResult<FirefoxRunner> {
        let prof = match profile {
            Some(p) => p,
            None => try!(Profile::new(None))
        };

        Ok(FirefoxRunner {
            binary: binary.to_path_buf(),
            process: None,
            ret_code: None,
            args: Vec::new(),
            profile: prof
        })
    }
}

impl Runner for FirefoxRunner {
    fn start(&mut self) -> Result<(), RunnerError> {
        let mut cmd = Command::new(&self.binary);
        self.build_command(&mut cmd);

        let prefs = try!(self.profile.user_prefs());
        try!(prefs.write());

        let process = try!(cmd.spawn());
        self.process = Some(process);
        Ok(())
    }

    fn args(&mut self) -> &mut Vec<String> {
        &mut self.args
    }

    fn build_command(&self, command: &mut Command) {
        command
            .env("MOZ_NO_REMOTE", "1")
            .env("NO_EM_RESTART", "1")
            .args(&self.args[..])
            .arg("-profile").arg(&self.profile.path)
            .stdout(Stdio::inherit())
            .stderr(Stdio::inherit());
    }

    fn is_running(&self) -> bool {
        self.process.is_some() && self.ret_code.is_none()
    }

    fn stop(&mut self) -> IoResult<Option<process::ExitStatus>> {
        match self.process.as_mut() {
            Some(p) => {
                try!(p.kill());
                let status = try!(p.wait());
                self.ret_code = Some(status);
            },
            None => {}
        };
        Ok(self.ret_code)
    }
}

fn find_binary(name: &str) -> Option<PathBuf> {
    env::var("PATH")
        .ok()
        .and_then(|path_env| {
            for mut path in env::split_paths(&*path_env) {
                path.push(name);
                if path.exists() {
                    return Some(path)
                }
            }
            None
        })
}

#[cfg(target_os = "linux")]
pub mod platform {
    use super::find_binary;
    use std::path::PathBuf;

    pub fn firefox_default_path() -> Option<PathBuf> {
        find_binary("firefox")
    }
}

#[cfg(target_os = "macos")]
pub mod platform {
    use super::find_binary;
    use std::env;
    use std::path::PathBuf;

    pub fn firefox_default_path() -> Option<PathBuf> {
        if let Some(path) = find_binary("firefox-bin") {
            return Some(path)
        }
        let home = env::home_dir();
        for &(prefix_home, trial_path) in [
            (false, "/Applications/Firefox.app/Contents/MacOS/firefox-bin"),
            (true, "Applications/Firefox.app/Contents/MacOS/firefox-bin")].iter() {
            let path = match (home.as_ref(), prefix_home) {
                (Some(ref home_dir), true) => home_dir.join(trial_path),
                (None, true) => continue,
                (_, false) => PathBuf::from(trial_path)
            };
            if path.exists() {
                return Some(path)
            }
        }
        None
    }
}

#[cfg(target_os = "windows")]
pub mod platform {
    use super::find_binary;
    use std::io::Error;
    use std::path::PathBuf;
    use winreg::RegKey;
    use winreg::enums::*;

    pub fn firefox_default_path() -> Option<PathBuf> {
        let opt_path = firefox_registry_path().unwrap_or(None);
        if let Some(path) = opt_path {
            if path.exists() {
                return Some(path)
            }
        };
        find_binary("firefox.exe")
    }

    fn firefox_registry_path() -> Result<Option<PathBuf>, Error> {
        let hklm = RegKey::predef(HKEY_LOCAL_MACHINE);
        for subtree_key in ["SOFTWARE", "SOFTWARE\\WOW6432Node"].iter() {
            let subtree = try!(hklm.open_subkey_with_flags(subtree_key, KEY_READ));
            let mozilla_org = match subtree.open_subkey_with_flags("mozilla.org\\Mozilla", KEY_READ) {
                Ok(val) => val,
                Err(_) => continue
            };
            let current_version: String = try!(mozilla_org.get_value("CurrentVersion"));
            let mozilla = try!(subtree.open_subkey_with_flags("Mozilla", KEY_READ));
            for key_res in mozilla.enum_keys() {
                let key = try!(key_res);
                let section_data = try!(mozilla.open_subkey_with_flags(&key, KEY_READ));
                let version: Result<String, _> = section_data.get_value("GeckoVer");
                if let Ok(ver) = version {
                    if ver == current_version {
                        let mut bin_key = key.to_owned();
                        bin_key.push_str("\\bin");
                        if let Ok(bin_subtree) = mozilla.open_subkey_with_flags(bin_key, KEY_READ) {
                            let path: Result<String, _> = bin_subtree.get_value("PathToExe");
                            if let Ok(path) = path {
                                return Ok(Some(PathBuf::from(path)))
                            }
                        }
                    }
                }
            }
        }
        Ok(None)
    }
}

#[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "windows")))]
pub mod platform {
    use std::path::PathBuf;

    pub fn firefox_default_path() -> Option<PathBuf> {
        None
    }
}
