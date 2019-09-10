use mozprofile::prefreader::PrefReaderError;
use mozprofile::profile::Profile;
use std::collections::HashMap;
use std::convert::From;
use std::error::Error;
use std::ffi::{OsStr, OsString};
use std::fmt;
use std::io;
use std::io::ErrorKind;
use std::path::{Path, PathBuf};
use std::process;
use std::process::{Child, Command, Stdio};
use std::thread;
use std::time;

use crate::firefox_args::Arg;

pub trait Runner {
    type Process;

    fn arg<S>(&mut self, arg: S) -> &mut Self
    where
        S: AsRef<OsStr>;

    fn args<I, S>(&mut self, args: I) -> &mut Self
    where
        I: IntoIterator<Item = S>,
        S: AsRef<OsStr>;

    fn env<K, V>(&mut self, key: K, value: V) -> &mut Self
    where
        K: AsRef<OsStr>,
        V: AsRef<OsStr>;

    fn envs<I, K, V>(&mut self, envs: I) -> &mut Self
    where
        I: IntoIterator<Item = (K, V)>,
        K: AsRef<OsStr>,
        V: AsRef<OsStr>;

    fn stdout<T>(&mut self, stdout: T) -> &mut Self
    where
        T: Into<Stdio>;

    fn stderr<T>(&mut self, stderr: T) -> &mut Self
    where
        T: Into<Stdio>;

    fn start(self) -> Result<Self::Process, RunnerError>;
}

pub trait RunnerProcess {
    /// Attempts to collect the exit status of the process if it has already exited.
    ///
    /// This function will not block the calling thread and will only advisorily check to see if
    /// the child process has exited or not.  If the process has exited then on Unix the process ID
    /// is reaped.  This function is guaranteed to repeatedly return a successful exit status so
    /// long as the child has already exited.
    ///
    /// If the process has exited, then `Ok(Some(status))` is returned.  If the exit status is not
    /// available at this time then `Ok(None)` is returned.  If an error occurs, then that error is
    /// returned.
    fn try_wait(&mut self) -> io::Result<Option<process::ExitStatus>>;

    /// Waits for the process to exit completely, killing it if it does not stop within `timeout`,
    /// and returns the status that it exited with.
    ///
    /// Firefox' integrated background monitor observes long running threads during shutdown and
    /// kills these after 63 seconds.  If the process fails to exit within the duration of
    /// `timeout`, it is forcefully killed.
    ///
    /// This function will continue to have the same return value after it has been called at least
    /// once.
    fn wait(&mut self, timeout: time::Duration) -> io::Result<process::ExitStatus>;

    /// Determine if the process is still running.
    fn running(&mut self) -> bool;

    /// Forces the process to exit and returns the exit status.  This is
    /// equivalent to sending a SIGKILL on Unix platforms.
    fn kill(&mut self) -> io::Result<process::ExitStatus>;
}

#[derive(Debug)]
pub enum RunnerError {
    Io(io::Error),
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
            RunnerError::Io(ref err) => match err.kind() {
                ErrorKind::NotFound => "no such file or directory",
                _ => err.description(),
            },
            RunnerError::PrefReader(ref err) => err.description(),
        }
    }

    fn cause(&self) -> Option<&dyn Error> {
        Some(match *self {
            RunnerError::Io(ref err) => err as &dyn Error,
            RunnerError::PrefReader(ref err) => err as &dyn Error,
        })
    }
}

impl From<io::Error> for RunnerError {
    fn from(value: io::Error) -> RunnerError {
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
    profile: Profile,
}

impl RunnerProcess for FirefoxProcess {
    fn try_wait(&mut self) -> io::Result<Option<process::ExitStatus>> {
        self.process.try_wait()
    }

    fn wait(&mut self, timeout: time::Duration) -> io::Result<process::ExitStatus> {
        let start = time::Instant::now();
        loop {
            match self.try_wait() {
                // child has already exited, reap its exit code
                Ok(Some(status)) => return Ok(status),

                // child still running and timeout elapsed, kill it
                Ok(None) if start.elapsed() >= timeout => return self.kill(),

                // child still running, let's give it more time
                Ok(None) => thread::sleep(time::Duration::from_millis(100)),

                Err(e) => return Err(e),
            }
        }
    }

    fn running(&mut self) -> bool {
        self.try_wait().unwrap().is_none()
    }

    fn kill(&mut self) -> io::Result<process::ExitStatus> {
        debug!("Killing process {}", self.process.id());
        self.process.kill()?;
        self.process.wait()
    }
}

#[derive(Debug)]
pub struct FirefoxRunner {
    path: PathBuf,
    profile: Profile,
    args: Vec<OsString>,
    envs: HashMap<OsString, OsString>,
    stdout: Option<Stdio>,
    stderr: Option<Stdio>,
}

impl FirefoxRunner {
    /// Initialise Firefox process runner.
    ///
    /// On macOS, `path` can optionally point to an application bundle,
    /// i.e. _/Applications/Firefox.app_, as well as to an executable program
    /// such as _/Applications/Firefox.app/Content/MacOS/firefox-bin_.
    pub fn new(path: &Path, profile: Profile) -> FirefoxRunner {
        let mut envs: HashMap<OsString, OsString> = HashMap::new();
        envs.insert("MOZ_NO_REMOTE".into(), "1".into());

        FirefoxRunner {
            path: path.to_path_buf(),
            envs,
            profile,
            args: vec![],
            stdout: None,
            stderr: None,
        }
    }
}

impl Runner for FirefoxRunner {
    type Process = FirefoxProcess;

    fn arg<S>(&mut self, arg: S) -> &mut FirefoxRunner
    where
        S: AsRef<OsStr>,
    {
        self.args.push((&arg).into());
        self
    }

    fn args<I, S>(&mut self, args: I) -> &mut FirefoxRunner
    where
        I: IntoIterator<Item = S>,
        S: AsRef<OsStr>,
    {
        for arg in args {
            self.args.push((&arg).into());
        }
        self
    }

    fn env<K, V>(&mut self, key: K, value: V) -> &mut FirefoxRunner
    where
        K: AsRef<OsStr>,
        V: AsRef<OsStr>,
    {
        self.envs.insert((&key).into(), (&value).into());
        self
    }

    fn envs<I, K, V>(&mut self, envs: I) -> &mut FirefoxRunner
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

    fn stdout<T>(&mut self, stdout: T) -> &mut Self
    where
        T: Into<Stdio>,
    {
        self.stdout = Some(stdout.into());
        self
    }

    fn stderr<T>(&mut self, stderr: T) -> &mut Self
    where
        T: Into<Stdio>,
    {
        self.stderr = Some(stderr.into());
        self
    }

    fn start(mut self) -> Result<FirefoxProcess, RunnerError> {
        self.profile.user_prefs()?.write()?;

        let stdout = self.stdout.unwrap_or_else(Stdio::inherit);
        let stderr = self.stderr.unwrap_or_else(Stdio::inherit);

        let binary_path = platform::resolve_binary_path(&mut self.path);
        let mut cmd = Command::new(binary_path);
        cmd.args(&self.args[..])
            .envs(&self.envs)
            .stdout(stdout)
            .stderr(stderr);

        let mut seen_foreground = false;
        let mut seen_no_remote = false;
        let mut seen_profile = false;
        for arg in self.args.iter() {
            match arg.into() {
                Arg::Foreground => seen_foreground = true,
                Arg::NoRemote => seen_no_remote = true,
                Arg::Profile | Arg::NamedProfile | Arg::ProfileManager => seen_profile = true,
                Arg::Other(_) | Arg::None => {}
            }
        }
        if !seen_foreground {
            cmd.arg("-foreground");
        }
        if !seen_no_remote {
            cmd.arg("-no-remote");
        }
        if !seen_profile {
            cmd.arg("-profile").arg(&self.profile.path);
        }

        info!("Running command: {:?}", cmd);
        let process = cmd.spawn()?;
        Ok(FirefoxProcess {
            process,
            profile: self.profile,
        })
    }
}

#[cfg(all(not(target_os = "macos"), unix))]
pub mod platform {
    use path::find_binary;
    use std::path::PathBuf;

    pub fn resolve_binary_path(path: &mut PathBuf) -> &PathBuf {
        path
    }

    /// Searches the system path for `firefox`.
    pub fn firefox_default_path() -> Option<PathBuf> {
        find_binary("firefox")
    }

    pub fn arg_prefix_char(c: char) -> bool {
        c == '-'
    }
}

#[cfg(target_os = "macos")]
pub mod platform {
    use crate::path::{find_binary, is_binary};
    use dirs;
    use plist::Value;
    use std::path::PathBuf;

    /// Searches for the binary file inside the path passed as parameter.
    /// If the binary is not found, the path remains unaltered.
    /// Else, it gets updated by the new binary path.
    pub fn resolve_binary_path(path: &mut PathBuf) -> &PathBuf {
        if path.as_path().is_dir() {
            let mut info_plist = path.clone();
            info_plist.push("Contents");
            info_plist.push("Info.plist");
            if let Ok(plist) = Value::from_file(&info_plist) {
                if let Some(dict) = plist.as_dictionary() {
                    if let Some(binary_file) = dict.get("CFBundleExecutable") {
                        if let Value::String(s) = binary_file {
                            path.push("Contents");
                            path.push("MacOS");
                            path.push(s);
                        }
                    }
                }
            }
        }
        path
    }

    /// Searches the system path for `firefox-bin`, then looks for
    /// `Applications/Firefox.app/Contents/MacOS/firefox-bin` as well
    /// as `Applications/Firefox Nightly.app/Contents/MacOS/firefox-bin`
    /// under both `/` (system root) and the user home directory.
    pub fn firefox_default_path() -> Option<PathBuf> {
        if let Some(path) = find_binary("firefox-bin") {
            return Some(path);
        }

        let home = dirs::home_dir();
        for &(prefix_home, trial_path) in [
            (
                false,
                "/Applications/Firefox.app/Contents/MacOS/firefox-bin",
            ),
            (true, "Applications/Firefox.app/Contents/MacOS/firefox-bin"),
            (
                false,
                "/Applications/Firefox Nightly.app/Contents/MacOS/firefox-bin",
            ),
            (
                true,
                "Applications/Firefox Nightly.app/Contents/MacOS/firefox-bin",
            ),
        ]
        .iter()
        {
            let path = match (home.as_ref(), prefix_home) {
                (Some(ref home_dir), true) => home_dir.join(trial_path),
                (None, true) => continue,
                (_, false) => PathBuf::from(trial_path),
            };
            if is_binary(&path) {
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
    use path::{find_binary, is_binary};
    use std::io::Error;
    use std::path::PathBuf;
    use winreg::enums::*;
    use winreg::RegKey;

    pub fn resolve_binary_path(path: &mut PathBuf) -> &PathBuf {
        path
    }

    /// Searches the Windows registry, then the system path for `firefox.exe`.
    ///
    /// It _does not_ currently check the `HKEY_CURRENT_USER` tree.
    pub fn firefox_default_path() -> Option<PathBuf> {
        if let Ok(Some(path)) = firefox_registry_path() {
            if is_binary(&path) {
                return Some(path);
            }
        };
        find_binary("firefox.exe")
    }

    fn firefox_registry_path() -> Result<Option<PathBuf>, Error> {
        let hklm = RegKey::predef(HKEY_LOCAL_MACHINE);
        for subtree_key in ["SOFTWARE", "SOFTWARE\\WOW6432Node"].iter() {
            let subtree = hklm.open_subkey_with_flags(subtree_key, KEY_READ)?;
            let mozilla_org = match subtree.open_subkey_with_flags("mozilla.org\\Mozilla", KEY_READ)
            {
                Ok(val) => val,
                Err(_) => continue,
            };
            let current_version: String = mozilla_org.get_value("CurrentVersion")?;
            let mozilla = subtree.open_subkey_with_flags("Mozilla", KEY_READ)?;
            for key_res in mozilla.enum_keys() {
                let key = key_res?;
                let section_data = mozilla.open_subkey_with_flags(&key, KEY_READ)?;
                let version: Result<String, _> = section_data.get_value("GeckoVer");
                if let Ok(ver) = version {
                    if ver == current_version {
                        let mut bin_key = key.to_owned();
                        bin_key.push_str("\\bin");
                        if let Ok(bin_subtree) = mozilla.open_subkey_with_flags(bin_key, KEY_READ) {
                            let path_to_exe: Result<String, _> = bin_subtree.get_value("PathToExe");
                            if let Ok(path_to_exe) = path_to_exe {
                                let path = PathBuf::from(path_to_exe);
                                if is_binary(&path) {
                                    return Ok(Some(path));
                                }
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

#[cfg(not(any(unix, target_os = "windows")))]
pub mod platform {
    use std::path::PathBuf;

    /// Returns an unaltered path for all operating systems other than macOS.
    pub fn resolve_binary_path(path: &mut PathBuf) -> &PathBuf {
        path
    }

    /// Returns `None` for all other operating systems than Linux, macOS, and
    /// Windows.
    pub fn firefox_default_path() -> Option<PathBuf> {
        None
    }

    pub fn arg_prefix_char(c: char) -> bool {
        c == '-'
    }
}
