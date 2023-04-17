/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::android::AndroidHandler;
use crate::capabilities::{FirefoxOptions, ProfileType};
use crate::logging;
use crate::prefs;
use mozprofile::preferences::Pref;
use mozprofile::profile::{PrefFile, Profile};
use mozrunner::runner::{FirefoxProcess, FirefoxRunner, Runner, RunnerProcess};
use std::fs;
use std::io::Read;
use std::path::{Path, PathBuf};
use std::time;
use webdriver::error::{ErrorStatus, WebDriverError, WebDriverResult};

/// A running Gecko instance.
#[derive(Debug)]
#[allow(clippy::large_enum_variant)]
pub(crate) enum Browser {
    Local(LocalBrowser),
    Remote(RemoteBrowser),

    /// An existing browser instance not controlled by GeckoDriver
    Existing(u16),
}

impl Browser {
    pub(crate) fn close(self, wait_for_shutdown: bool) -> WebDriverResult<()> {
        match self {
            Browser::Local(x) => x.close(wait_for_shutdown),
            Browser::Remote(x) => x.close(),
            Browser::Existing(_) => Ok(()),
        }
    }

    pub(crate) fn marionette_port(&mut self) -> WebDriverResult<Option<u16>> {
        match self {
            Browser::Local(x) => x.marionette_port(),
            Browser::Remote(x) => x.marionette_port(),
            Browser::Existing(x) => Ok(Some(*x)),
        }
    }

    pub(crate) fn update_marionette_port(&mut self, port: u16) {
        match self {
            Browser::Local(x) => x.update_marionette_port(port),
            Browser::Remote(x) => x.update_marionette_port(port),
            Browser::Existing(x) => {
                if port != *x {
                    error!(
                        "Cannot re-assign Marionette port when connected to an existing browser"
                    );
                }
            }
        }
    }
}

#[derive(Debug)]
/// A local Firefox process, running on this (host) device.
pub(crate) struct LocalBrowser {
    marionette_port: u16,
    prefs_backup: Option<PrefsBackup>,
    process: FirefoxProcess,
    profile_path: Option<PathBuf>,
}

impl LocalBrowser {
    pub(crate) fn new(
        options: FirefoxOptions,
        marionette_port: u16,
        jsdebugger: bool,
        profile_root: Option<&Path>,
    ) -> WebDriverResult<LocalBrowser> {
        let binary = options.binary.ok_or_else(|| {
            WebDriverError::new(
                ErrorStatus::SessionNotCreated,
                "Expected browser binary location, but unable to find \
             binary in default location, no \
             'moz:firefoxOptions.binary' capability provided, and \
             no binary flag set on the command line",
            )
        })?;

        let is_custom_profile = matches!(options.profile, ProfileType::Path(_));

        let mut profile = match options.profile {
            ProfileType::Named => None,
            ProfileType::Path(x) => Some(x),
            ProfileType::Temporary => Some(Profile::new(profile_root)?),
        };

        let (profile_path, prefs_backup) = if let Some(ref mut profile) = profile {
            let profile_path = profile.path.clone();
            let prefs_backup = set_prefs(
                marionette_port,
                profile,
                is_custom_profile,
                options.prefs,
                jsdebugger,
            )
            .map_err(|e| {
                WebDriverError::new(
                    ErrorStatus::SessionNotCreated,
                    format!("Failed to set preferences: {}", e),
                )
            })?;
            (Some(profile_path), prefs_backup)
        } else {
            warn!("Unable to set geckodriver prefs when using a named profile");
            (None, None)
        };

        let mut runner = FirefoxRunner::new(&binary, profile);

        runner.arg("--marionette");
        if jsdebugger {
            runner.arg("--jsdebugger");
        }
        if let Some(args) = options.args.as_ref() {
            runner.args(args);
        }

        // https://developer.mozilla.org/docs/Environment_variables_affecting_crash_reporting
        runner
            .env("MOZ_CRASHREPORTER", "1")
            .env("MOZ_CRASHREPORTER_NO_REPORT", "1")
            .env("MOZ_CRASHREPORTER_SHUTDOWN", "1");

        let process = match runner.start() {
            Ok(process) => process,
            Err(e) => {
                if let Some(backup) = prefs_backup {
                    backup.restore();
                }
                return Err(WebDriverError::new(
                    ErrorStatus::SessionNotCreated,
                    format!("Failed to start browser {}: {}", binary.display(), e),
                ));
            }
        };

        Ok(LocalBrowser {
            marionette_port,
            prefs_backup,
            process,
            profile_path,
        })
    }

    fn close(mut self, wait_for_shutdown: bool) -> WebDriverResult<()> {
        if wait_for_shutdown {
            // TODO(https://bugzil.la/1443922):
            // Use toolkit.asyncshutdown.crash_timout pref
            let duration = time::Duration::from_secs(70);
            match self.process.wait(duration) {
                Ok(x) => debug!("Browser process stopped: {}", x),
                Err(e) => error!("Failed to stop browser process: {}", e),
            }
        }
        self.process.kill()?;

        // Restoring the prefs if the browser fails to stop perhaps doesn't work anyway
        if let Some(prefs_backup) = self.prefs_backup {
            prefs_backup.restore();
        };

        Ok(())
    }

    fn marionette_port(&mut self) -> WebDriverResult<Option<u16>> {
        if self.marionette_port != 0 {
            return Ok(Some(self.marionette_port));
        }

        if let Some(profile_path) = self.profile_path.as_ref() {
            return Ok(read_marionette_port(profile_path));
        }

        // This should be impossible, but it isn't enforced
        Err(WebDriverError::new(
            ErrorStatus::SessionNotCreated,
            "Port not known when using named profile",
        ))
    }

    fn update_marionette_port(&mut self, port: u16) {
        self.marionette_port = port;
    }

    pub(crate) fn check_status(&mut self) -> Option<String> {
        match self.process.try_wait() {
            Ok(Some(status)) => Some(
                status
                    .code()
                    .map(|c| c.to_string())
                    .unwrap_or_else(|| "signal".into()),
            ),
            Ok(None) => None,
            Err(_) => Some("{unknown}".into()),
        }
    }
}

fn read_marionette_port(profile_path: &Path) -> Option<u16> {
    let port_file = profile_path.join("MarionetteActivePort");
    let mut port_str = String::with_capacity(6);
    let mut file = match fs::File::open(&port_file) {
        Ok(file) => file,
        Err(_) => {
            trace!("Failed to open {}", &port_file.to_string_lossy());
            return None;
        }
    };
    if let Err(e) = file.read_to_string(&mut port_str) {
        trace!("Failed to read {}: {}", &port_file.to_string_lossy(), e);
        return None;
    };
    println!("Read port: {}", port_str);
    let port = port_str.parse::<u16>().ok();
    if port.is_none() {
        warn!("Failed fo convert {} to u16", &port_str);
    }
    port
}

#[derive(Debug)]
/// A remote instance, running on a (target) Android device.
pub(crate) struct RemoteBrowser {
    handler: AndroidHandler,
    marionette_port: u16,
    prefs_backup: Option<PrefsBackup>,
}

impl RemoteBrowser {
    pub(crate) fn new(
        options: FirefoxOptions,
        marionette_port: u16,
        websocket_port: Option<u16>,
        profile_root: Option<&Path>,
    ) -> WebDriverResult<RemoteBrowser> {
        let android_options = options.android.unwrap();

        let handler = AndroidHandler::new(&android_options, marionette_port, websocket_port)?;

        // Profile management.
        let (mut profile, is_custom_profile) = match options.profile {
            ProfileType::Named => {
                return Err(WebDriverError::new(
                    ErrorStatus::SessionNotCreated,
                    "Cannot use a named profile on Android",
                ));
            }
            ProfileType::Path(x) => (x, true),
            ProfileType::Temporary => (Profile::new(profile_root)?, false),
        };

        let prefs_backup = set_prefs(
            handler.marionette_target_port,
            &mut profile,
            is_custom_profile,
            options.prefs,
            false,
        )
        .map_err(|e| {
            WebDriverError::new(
                ErrorStatus::SessionNotCreated,
                format!("Failed to set preferences: {}", e),
            )
        })?;

        handler.prepare(&profile, options.args, options.env.unwrap_or_default())?;

        handler.launch()?;

        Ok(RemoteBrowser {
            handler,
            marionette_port,
            prefs_backup,
        })
    }

    fn close(self) -> WebDriverResult<()> {
        self.handler.force_stop()?;

        // Restoring the prefs if the browser fails to stop perhaps doesn't work anyway
        if let Some(prefs_backup) = self.prefs_backup {
            prefs_backup.restore();
        };

        Ok(())
    }

    fn marionette_port(&mut self) -> WebDriverResult<Option<u16>> {
        Ok(Some(self.marionette_port))
    }

    fn update_marionette_port(&mut self, port: u16) {
        self.marionette_port = port;
    }
}

fn set_prefs(
    port: u16,
    profile: &mut Profile,
    custom_profile: bool,
    extra_prefs: Vec<(String, Pref)>,
    js_debugger: bool,
) -> WebDriverResult<Option<PrefsBackup>> {
    let prefs = profile.user_prefs().map_err(|_| {
        WebDriverError::new(
            ErrorStatus::UnknownError,
            "Unable to read profile preferences file",
        )
    })?;

    let backup_prefs = if custom_profile && prefs.path.exists() {
        Some(PrefsBackup::new(prefs)?)
    } else {
        None
    };

    for &(name, ref value) in prefs::DEFAULT.iter() {
        if !custom_profile || !prefs.contains_key(name) {
            prefs.insert(name.to_string(), (*value).clone());
        }
    }

    prefs.insert_slice(&extra_prefs[..]);

    if js_debugger {
        prefs.insert("devtools.browsertoolbox.panel", Pref::new("jsdebugger"));
        prefs.insert("devtools.debugger.remote-enabled", Pref::new(true));
        prefs.insert("devtools.chrome.enabled", Pref::new(true));
        prefs.insert("devtools.debugger.prompt-connection", Pref::new(false));
    }

    prefs.insert("marionette.port", Pref::new(port));
    prefs.insert("remote.log.level", logging::max_level().into());

    prefs.write().map_err(|e| {
        WebDriverError::new(
            ErrorStatus::UnknownError,
            format!("Unable to write Firefox profile: {}", e),
        )
    })?;
    Ok(backup_prefs)
}

#[derive(Debug)]
struct PrefsBackup {
    orig_path: PathBuf,
    backup_path: PathBuf,
}

impl PrefsBackup {
    fn new(prefs: &PrefFile) -> WebDriverResult<PrefsBackup> {
        let mut prefs_backup_path = prefs.path.clone();
        let mut counter = 0;
        while {
            let ext = if counter > 0 {
                format!("geckodriver_backup_{}", counter)
            } else {
                "geckodriver_backup".to_string()
            };
            prefs_backup_path.set_extension(ext);
            prefs_backup_path.exists()
        } {
            counter += 1
        }
        debug!("Backing up prefs to {:?}", prefs_backup_path);
        fs::copy(&prefs.path, &prefs_backup_path)?;

        Ok(PrefsBackup {
            orig_path: prefs.path.clone(),
            backup_path: prefs_backup_path,
        })
    }

    fn restore(self) {
        if self.backup_path.exists() {
            let _ = fs::rename(self.backup_path, self.orig_path);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::set_prefs;
    use crate::browser::read_marionette_port;
    use crate::capabilities::{FirefoxOptions, ProfileType};
    use base64::prelude::BASE64_STANDARD;
    use base64::Engine;
    use mozprofile::preferences::{Pref, PrefValue};
    use mozprofile::profile::Profile;
    use serde_json::{Map, Value};
    use std::fs::File;
    use std::io::{Read, Write};
    use std::path::Path;
    use tempfile::tempdir;

    fn example_profile() -> Value {
        let mut profile_data = Vec::with_capacity(1024);
        let mut profile = File::open("src/tests/profile.zip").unwrap();
        profile.read_to_end(&mut profile_data).unwrap();
        Value::String(BASE64_STANDARD.encode(&profile_data))
    }

    // This is not a pretty test, mostly due to the nature of
    // mozprofile's and MarionetteHandler's APIs, but we have had
    // several regressions related to remote.log.level.
    #[test]
    fn test_remote_log_level() {
        let mut profile = Profile::new(None).unwrap();
        set_prefs(2828, &mut profile, false, vec![], false).ok();
        let user_prefs = profile.user_prefs().unwrap();

        let pref = user_prefs.get("remote.log.level").unwrap();
        let value = match pref.value {
            PrefValue::String(ref s) => s,
            _ => panic!(),
        };
        for (i, ch) in value.chars().enumerate() {
            if i == 0 {
                assert!(ch.is_uppercase());
            } else {
                assert!(ch.is_lowercase());
            }
        }
    }

    #[test]
    fn test_prefs() {
        let marionette_settings = Default::default();

        let encoded_profile = example_profile();
        let mut prefs: Map<String, Value> = Map::new();
        prefs.insert(
            "browser.display.background_color".into(),
            Value::String("#00ff00".into()),
        );

        let mut firefox_opts = Map::new();
        firefox_opts.insert("profile".into(), encoded_profile);
        firefox_opts.insert("prefs".into(), Value::Object(prefs));

        let mut caps = Map::new();
        caps.insert("moz:firefoxOptions".into(), Value::Object(firefox_opts));

        let opts = FirefoxOptions::from_capabilities(None, &marionette_settings, &mut caps)
            .expect("Valid profile and prefs");

        let mut profile = match opts.profile {
            ProfileType::Path(profile) => profile,
            _ => panic!("Expected ProfileType::Path"),
        };

        set_prefs(2828, &mut profile, true, opts.prefs, false).expect("set preferences");

        let prefs_set = profile.user_prefs().expect("valid user preferences");
        println!("{:#?}", prefs_set.prefs);

        assert_eq!(
            prefs_set.get("startup.homepage_welcome_url"),
            Some(&Pref::new("data:text/html,PASS"))
        );
        assert_eq!(
            prefs_set.get("browser.display.background_color"),
            Some(&Pref::new("#00ff00"))
        );
        assert_eq!(prefs_set.get("marionette.port"), Some(&Pref::new(2828)));
    }

    #[test]
    fn test_pref_backup() {
        let mut profile = Profile::new(None).unwrap();

        // Create some prefs in the profile
        let initial_prefs = profile.user_prefs().unwrap();
        initial_prefs.insert("geckodriver.example", Pref::new("example"));
        initial_prefs.write().unwrap();

        let prefs_path = initial_prefs.path.clone();

        let mut conflicting_backup_path = initial_prefs.path.clone();
        conflicting_backup_path.set_extension("geckodriver_backup");
        println!("{:?}", conflicting_backup_path);
        let mut file = File::create(&conflicting_backup_path).unwrap();
        file.write_all(b"test").unwrap();
        assert!(conflicting_backup_path.exists());

        let mut initial_prefs_data = String::new();
        File::open(&prefs_path)
            .expect("Initial prefs exist")
            .read_to_string(&mut initial_prefs_data)
            .unwrap();

        let backup = set_prefs(2828, &mut profile, true, vec![], false)
            .unwrap()
            .unwrap();
        let user_prefs = profile.user_prefs().unwrap();

        assert!(user_prefs.path.exists());
        let mut backup_path = user_prefs.path.clone();
        backup_path.set_extension("geckodriver_backup_1");

        assert!(backup_path.exists());

        // Ensure the actual prefs contain both the existing ones and the ones we added
        let pref = user_prefs.get("marionette.port").unwrap();
        assert_eq!(pref.value, PrefValue::Int(2828));

        let pref = user_prefs.get("geckodriver.example").unwrap();
        assert_eq!(pref.value, PrefValue::String("example".into()));

        // Ensure the backup prefs don't contain the new settings
        let mut backup_data = String::new();
        File::open(&backup_path)
            .expect("Backup prefs exist")
            .read_to_string(&mut backup_data)
            .unwrap();
        assert_eq!(backup_data, initial_prefs_data);

        backup.restore();

        assert!(!backup_path.exists());
        let mut final_prefs_data = String::new();
        File::open(&prefs_path)
            .expect("Initial prefs exist")
            .read_to_string(&mut final_prefs_data)
            .unwrap();
        assert_eq!(final_prefs_data, initial_prefs_data);
    }

    #[test]
    fn test_local_read_marionette_port() {
        fn create_port_file(profile_path: &Path, data: &[u8]) {
            let port_path = profile_path.join("MarionetteActivePort");
            let mut file = File::create(&port_path).unwrap();
            file.write_all(data).unwrap();
        }

        let profile_dir = tempdir().unwrap();
        let profile_path = profile_dir.path();
        assert_eq!(read_marionette_port(profile_path), None);
        assert_eq!(read_marionette_port(profile_path), None);
        create_port_file(profile_path, b"");
        assert_eq!(read_marionette_port(profile_path), None);
        create_port_file(profile_path, b"1234");
        assert_eq!(read_marionette_port(profile_path), Some(1234));
        create_port_file(profile_path, b"1234abc");
        assert_eq!(read_marionette_port(profile_path), None);
    }
}
