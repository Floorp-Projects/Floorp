use mozprofile::prefreader::PrefReaderError;
use mozprofile::profile::Profile;
use std::ascii::AsciiExt;
use std::collections::HashMap;
use std::convert::From;
use std::env;
use std::error::Error;
use std::ffi::{OsStr, OsString};
use std::fmt;
use std::io::{Error as IoError, ErrorKind, Result as IoResult};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::process;

pub trait Runner {
    type Process;

    fn arg<'a, S>(&'a mut self, arg: S) -> &'a mut Self
    where
        S: AsRef<OsStr>;

    fn args<'a, I, S>(&'a mut self, args: I) -> &'a mut Self
    where
        I: IntoIterator<Item = S>,
        S: AsRef<OsStr>;

    fn env<'a, K, V>(&'a mut self, key: K, value: V) -> &'a mut Self
    where
        K: AsRef<OsStr>,
        V: AsRef<OsStr>;

    fn envs<'a, I, K, V>(&'a mut self, envs: I) -> &'a mut Self
    where
        I: IntoIterator<Item = (K, V)>,
        K: AsRef<OsStr>,
        V: AsRef<OsStr>;

    fn stdout<'a, T>(&'a mut self, stdout: T) -> &'a mut Self
    where
        T: Into<Stdio>;

    fn stderr<'a, T>(&'a mut self, stderr: T) -> &'a mut Self
    where
        T: Into<Stdio>;

    fn start(self) -> Result<Self::Process, RunnerError>;
}

pub trait RunnerProcess {
    fn status(&mut self) -> IoResult<Option<process::ExitStatus>>;
    fn stop(&mut self) -> IoResult<process::ExitStatus>;
    fn is_running(&mut self) -> bool;
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

#[derive(Debug)]
pub struct FirefoxProcess {
    process: Child,
    profile: Profile
}

impl RunnerProcess for FirefoxProcess {
    fn status(&mut self) -> IoResult<Option<process::ExitStatus>> {
        self.process.try_wait()
    }

    fn is_running(&mut self) -> bool {
        self.status().unwrap().is_none()
    }

    fn stop(&mut self) -> IoResult<process::ExitStatus> {
        self.process.kill()?;
        self.process.wait()
    }
}

#[derive(Debug)]
pub struct FirefoxRunner {
    binary: PathBuf,
    profile: Profile,
    args: Vec<OsString>,
    envs: HashMap<OsString, OsString>,
    stdout: Option<Stdio>,
    stderr: Option<Stdio>,
}

impl FirefoxRunner {
    pub fn new(binary: &Path, profile: Profile) -> FirefoxRunner {
        let mut envs: HashMap<OsString, OsString> = HashMap::new();
        envs.insert("MOZ_NO_REMOTE".into(), "1".into());
        envs.insert("NO_EM_RESTART".into(), "1".into());

        FirefoxRunner {
            binary: binary.to_path_buf(),
            envs: envs,
            profile: profile,
            args: vec![],
            stdout: None,
            stderr: None,
        }
    }
}

impl Runner for FirefoxRunner {
    type Process = FirefoxProcess;

    fn arg<'a, S>(&'a mut self, arg: S) -> &'a mut FirefoxRunner
    where
        S: AsRef<OsStr>,
    {
        self.args.push((&arg).into());
        self
    }

    fn args<'a, I, S>(&'a mut self, args: I) -> &'a mut FirefoxRunner
    where
        I: IntoIterator<Item = S>,
        S: AsRef<OsStr>,
    {
        for arg in args {
            self.args.push((&arg).into());
        }
        self
    }

    fn env<'a, K, V>(&'a mut self, key: K, value: V) -> &'a mut FirefoxRunner
    where
        K: AsRef<OsStr>,
        V: AsRef<OsStr>,
    {
        self.envs.insert((&key).into(), (&value).into());
        self
    }

    fn envs<'a, I, K, V>(&'a mut self, envs: I) -> &'a mut FirefoxRunner
    where
        I: IntoIterator<Item = (K, V)>,
        K: AsRef<OsStr>,
        V: AsRef<OsStr>,
    {
        for (key, value) in envs {
            self.envs.insert((&key).into(), (&value).into());
        }
        self
    }

    fn stdout<'a, T>(&'a mut self, stdout: T) -> &'a mut Self
    where
        T: Into<Stdio>,
    {
        self.stdout = Some(stdout.into());
        self
    }

    fn stderr<'a, T>(&'a mut self, stderr: T) -> &'a mut Self
    where
        T: Into<Stdio>,
    {
        self.stderr = Some(stderr.into());
        self
    }

    fn start(mut self) -> Result<FirefoxProcess, RunnerError> {
        let stdout = self.stdout.unwrap_or_else(|| Stdio::inherit());
        let stderr = self.stderr.unwrap_or_else(|| Stdio::inherit());

        let mut cmd = Command::new(&self.binary);
        cmd.args(&self.args[..])
            .envs(&self.envs)
            .stdout(stdout)
            .stderr(stderr);

        if !self.args.iter().any(|x| is_profile_arg(x)) {
            cmd.arg("-profile").arg(&self.profile.path);
        }
        cmd.stdout(Stdio::inherit()).stderr(Stdio::inherit());

        self.profile.user_prefs()?.write()?;

        info!("Running command: {:?}", cmd);
        let process = cmd.spawn()?;
        Ok(FirefoxProcess {
            process: process,
            profile: self.profile
        })
    }
}

fn parse_arg_name<T>(arg: T) -> Option<String>
where
    T: AsRef<OsStr>,
{
    let arg_os_str: &OsStr = arg.as_ref();
    let arg_str = arg_os_str.to_string_lossy();

    let mut start = 0;
    let mut end = 0;

    for (i, c) in arg_str.chars().enumerate() {
        if i == 0 {
            if !platform::arg_prefix_char(c) {
                break;
            }
        } else if i == 1 {
            if name_end_char(c) {
                break;
            } else if c != '-' {
                start = i;
                end = start + 1;
            } else {
                start = i + 1;
                end = start;
            }
        } else {
            end += 1;
            if name_end_char(c) {
                end -= 1;
                break;
            }
        }
    }

    if start > 0 && end > start {
        Some(arg_str[start..end].into())
    } else {
        None
    }
}

fn name_end_char(c: char) -> bool {
    c == ' ' || c == '='
}

/// Check if an argument string affects the Firefox profile
///
/// Returns a boolean indicating whether a given string
/// contains one of the `-P`, `-Profile` or `-ProfileManager`
/// arguments, respecting the various platform-specific conventions.
pub fn is_profile_arg<T>(arg: T) -> bool
where
    T: AsRef<OsStr>,
{
    if let Some(name) = parse_arg_name(arg) {
        name.eq_ignore_ascii_case("profile") || name.eq_ignore_ascii_case("p")
            || name.eq_ignore_ascii_case("profilemanager")
    } else {
        false
    }
}

fn find_binary(name: &str) -> Option<PathBuf> {
    env::var("PATH").ok().and_then(|path_env| {
        for mut path in env::split_paths(&*path_env) {
            path.push(name);
            if path.exists() {
                return Some(path);
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

    pub fn arg_prefix_char(c: char) -> bool {
        c == '-'
    }
}

#[cfg(target_os = "macos")]
pub mod platform {
    use super::find_binary;
    use std::env;
    use std::path::PathBuf;

    pub fn firefox_default_path() -> Option<PathBuf> {
        if let Some(path) = find_binary("firefox-bin") {
            return Some(path);
        }
        let home = env::home_dir();
        for &(prefix_home, trial_path) in [
            (
                false,
                "/Applications/Firefox.app/Contents/MacOS/firefox-bin",
            ),
            (true, "Applications/Firefox.app/Contents/MacOS/firefox-bin"),
        ].iter()
        {
            let path = match (home.as_ref(), prefix_home) {
                (Some(ref home_dir), true) => home_dir.join(trial_path),
                (None, true) => continue,
                (_, false) => PathBuf::from(trial_path),
            };
            if path.exists() {
                return Some(path);
            }
        }
        None
    }

    pub fn arg_prefix_char(c: char) -> bool {
        c == '-'
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
                return Some(path);
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
                                return Ok(Some(PathBuf::from(path)));
                            }
                        }
                    }
                }
            }
        }
        Ok(None)
    }

    pub fn arg_prefix_char(c: char) -> bool {
        c == '/' || c == '-'
    }
}

#[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "windows")))]
pub mod platform {
    use std::path::PathBuf;

    pub fn firefox_default_path() -> Option<PathBuf> {
        None
    }

    pub fn arg_prefix_char(c: char) -> bool {
        c == '-'
    }
}

#[cfg(test)]
mod tests {
    use super::{is_profile_arg, parse_arg_name};

    fn parse(arg: &str, name: Option<&str>) {
        let result = parse_arg_name(arg);
        assert_eq!(result, name.map(|x| x.to_string()));
    }

    #[test]
    fn test_parse_arg_name() {
        parse("-p", Some("p"));
        parse("--p", Some("p"));
        parse("--profile foo", Some("profile"));
        parse("--profile", Some("profile"));
        parse("--", None);
        parse("", None);
        parse("-=", None);
        parse("--=", None);
        parse("-- foo", None);
        parse("foo", None);
        parse("/ foo", None);
        parse("/- foo", None);
        parse("/=foo", None);
        parse("foo", None);
        parse("-profile", Some("profile"));
        parse("-profile=foo", Some("profile"));
        parse("-profile = foo", Some("profile"));
        parse("-profile abc", Some("profile"));
    }

    #[cfg(target_os = "windows")]
    #[test]
    fn test_parse_arg_name_windows() {
        parse("/profile", Some("profile"));
    }

    #[cfg(not(target_os = "windows"))]
    #[test]
    fn test_parse_arg_name_non_windows() {
        parse("/profile", None);
    }

    #[test]
    fn test_is_profile_arg() {
        assert!(is_profile_arg("--profile"));
        assert!(is_profile_arg("-p"));
        assert!(is_profile_arg("-PROFILEMANAGER"));
        assert!(is_profile_arg("-ProfileMANAGER"));
        assert!(!is_profile_arg("-- profile"));
        assert!(!is_profile_arg("-profiled"));
        assert!(!is_profile_arg("-p1"));
        assert!(is_profile_arg("-p test"));
        assert!(is_profile_arg("-profile /foo"));
    }
}
