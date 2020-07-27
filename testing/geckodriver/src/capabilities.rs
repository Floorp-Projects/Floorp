use crate::command::LogOptions;
use crate::logging::Level;
use base64;
use mozprofile::preferences::Pref;
use mozprofile::profile::Profile;
use mozrunner::runner::platform::firefox_default_path;
use mozversion::{self, firefox_version, Version};
use regex::bytes::Regex;
use serde_json::{Map, Value};
use std::collections::BTreeMap;
use std::default::Default;
use std::fs;
use std::io;
use std::io::BufWriter;
use std::io::Cursor;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::str::{self, FromStr};
use webdriver::capabilities::{BrowserCapabilities, Capabilities};
use webdriver::error::{ErrorStatus, WebDriverError, WebDriverResult};
use zip;

/// Provides matching of `moz:firefoxOptions` and resolution of which Firefox
/// binary to use.
///
/// `FirefoxCapabilities` is constructed with the fallback binary, should
/// `moz:firefoxOptions` not contain a binary entry.  This may either be the
/// system Firefox installation or an override, for example given to the
/// `--binary` flag of geckodriver.
pub struct FirefoxCapabilities<'a> {
    pub chosen_binary: Option<PathBuf>,
    fallback_binary: Option<&'a PathBuf>,
    version_cache: BTreeMap<PathBuf, String>,
}

impl<'a> FirefoxCapabilities<'a> {
    pub fn new(fallback_binary: Option<&'a PathBuf>) -> FirefoxCapabilities<'a> {
        FirefoxCapabilities {
            chosen_binary: None,
            fallback_binary,
            version_cache: BTreeMap::new(),
        }
    }

    fn set_binary(&mut self, capabilities: &Map<String, Value>) {
        self.chosen_binary = capabilities
            .get("moz:firefoxOptions")
            .and_then(|x| x.get("binary"))
            .and_then(|x| x.as_str())
            .map(PathBuf::from)
            .or_else(|| self.fallback_binary.cloned())
            .or_else(firefox_default_path)
    }

    fn version(&mut self) -> Option<String> {
        if let Some(ref binary) = self.chosen_binary {
            if let Some(value) = self.version_cache.get(binary) {
                return Some((*value).clone());
            }
            debug!("Trying to read firefox version from ini files");
            let rv = firefox_version(&*binary)
                .ok()
                .and_then(|x| x.version_string)
                .or_else(|| {
                    debug!("Trying to read firefox version from binary");
                    self.version_from_binary(binary)
                });
            if let Some(ref version) = rv {
                debug!("Found version {}", version);
                self.version_cache.insert(binary.clone(), version.clone());
            } else {
                debug!("Failed to get binary version");
            }
            rv
        } else {
            None
        }
    }

    fn version_from_binary(&self, binary: &PathBuf) -> Option<String> {
        let version_regexp =
            Regex::new(r#"\d+\.\d+(?:[a-z]\d+)?"#).expect("Error parsing version regexp");
        let output = Command::new(binary)
            .args(&["-version"])
            .stdout(Stdio::piped())
            .spawn()
            .and_then(|child| child.wait_with_output())
            .ok();

        if let Some(x) = output {
            version_regexp
                .captures(&*x.stdout)
                .and_then(|captures| captures.get(0))
                .and_then(|m| str::from_utf8(m.as_bytes()).ok())
                .map(|x| x.into())
        } else {
            None
        }
    }
}

// TODO: put this in webdriver-rust
fn convert_version_error(err: mozversion::Error) -> WebDriverError {
    WebDriverError::new(ErrorStatus::SessionNotCreated, err.to_string())
}

impl<'a> BrowserCapabilities for FirefoxCapabilities<'a> {
    fn init(&mut self, capabilities: &Capabilities) {
        self.set_binary(capabilities);
    }

    fn browser_name(&mut self, _: &Capabilities) -> WebDriverResult<Option<String>> {
        Ok(Some("firefox".into()))
    }

    fn browser_version(&mut self, _: &Capabilities) -> WebDriverResult<Option<String>> {
        Ok(self.version())
    }

    fn platform_name(&mut self, _: &Capabilities) -> WebDriverResult<Option<String>> {
        Ok(if cfg!(target_os = "windows") {
            Some("windows".into())
        } else if cfg!(target_os = "macos") {
            Some("mac".into())
        } else if cfg!(target_os = "linux") {
            Some("linux".into())
        } else {
            None
        })
    }

    fn accept_insecure_certs(&mut self, _: &Capabilities) -> WebDriverResult<bool> {
        let version_str = self.version();
        if let Some(x) = version_str {
            Ok(Version::from_str(&*x)
                .or_else(|x| Err(convert_version_error(x)))?
                .major
                >= 52)
        } else {
            Ok(false)
        }
    }

    fn set_window_rect(&mut self, _: &Capabilities) -> WebDriverResult<bool> {
        Ok(true)
    }

    fn compare_browser_version(
        &mut self,
        version: &str,
        comparison: &str,
    ) -> WebDriverResult<bool> {
        Version::from_str(version)
            .or_else(|x| Err(convert_version_error(x)))?
            .matches(comparison)
            .or_else(|x| Err(convert_version_error(x)))
    }

    fn strict_file_interactability(&mut self, _: &Capabilities) -> WebDriverResult<bool> {
        Ok(true)
    }

    fn accept_proxy(&mut self, _: &Capabilities, _: &Capabilities) -> WebDriverResult<bool> {
        Ok(true)
    }

    fn validate_custom(&self, name: &str, value: &Value) -> WebDriverResult<()> {
        if !name.starts_with("moz:") {
            return Ok(());
        }
        match name {
            "moz:firefoxOptions" => {
                let data = try_opt!(
                    value.as_object(),
                    ErrorStatus::InvalidArgument,
                    "moz:firefoxOptions is not an object"
                );
                for (key, value) in data.iter() {
                    match &**key {
                        "androidActivity"
                        | "androidDeviceSerial"
                        | "androidPackage"
                        | "binary"
                        | "profile" => {
                            if !value.is_string() {
                                return Err(WebDriverError::new(
                                    ErrorStatus::InvalidArgument,
                                    format!("{} is not a string", &**key),
                                ));
                            }
                        }
                        "androidIntentArguments" | "args" => {
                            if !try_opt!(
                                value.as_array(),
                                ErrorStatus::InvalidArgument,
                                format!("{} is not an array", &**key)
                            )
                            .iter()
                            .all(|value| value.is_string())
                            {
                                return Err(WebDriverError::new(
                                    ErrorStatus::InvalidArgument,
                                    format!("{} entry is not a string", &**key),
                                ));
                            }
                        }
                        "env" => {
                            let env_data = try_opt!(
                                value.as_object(),
                                ErrorStatus::InvalidArgument,
                                "env value is not an object"
                            );
                            if !env_data.values().all(Value::is_string) {
                                return Err(WebDriverError::new(
                                    ErrorStatus::InvalidArgument,
                                    "Environment values were not all strings",
                                ));
                            }
                        }
                        "log" => {
                            let log_data = try_opt!(
                                value.as_object(),
                                ErrorStatus::InvalidArgument,
                                "log value is not an object"
                            );
                            for (log_key, log_value) in log_data.iter() {
                                match &**log_key {
                                    "level" => {
                                        let level = try_opt!(
                                            log_value.as_str(),
                                            ErrorStatus::InvalidArgument,
                                            "log level is not a string"
                                        );
                                        if Level::from_str(level).is_err() {
                                            return Err(WebDriverError::new(
                                                ErrorStatus::InvalidArgument,
                                                format!("Not a valid log level: {}", level),
                                            ));
                                        }
                                    }
                                    x => {
                                        return Err(WebDriverError::new(
                                            ErrorStatus::InvalidArgument,
                                            format!("Invalid log field {}", x),
                                        ))
                                    }
                                }
                            }
                        }
                        "prefs" => {
                            let prefs_data = try_opt!(
                                value.as_object(),
                                ErrorStatus::InvalidArgument,
                                "prefs value is not an object"
                            );
                            let is_pref_value_type = |x: &Value| {
                                x.is_string() || x.is_i64() || x.is_u64() || x.is_boolean()
                            };
                            if !prefs_data.values().all(is_pref_value_type) {
                                return Err(WebDriverError::new(
                                    ErrorStatus::InvalidArgument,
                                    "Preference values not all string or integer or boolean",
                                ));
                            }
                        }
                        x => {
                            return Err(WebDriverError::new(
                                ErrorStatus::InvalidArgument,
                                format!("Invalid moz:firefoxOptions field {}", x),
                            ))
                        }
                    }
                }
            }
            "moz:useNonSpecCompliantPointerOrigin" => {
                if !value.is_boolean() {
                    return Err(WebDriverError::new(
                        ErrorStatus::InvalidArgument,
                        "moz:useNonSpecCompliantPointerOrigin is not a boolean",
                    ));
                }
            }
            "moz:webdriverClick" => {
                if !value.is_boolean() {
                    return Err(WebDriverError::new(
                        ErrorStatus::InvalidArgument,
                        "moz:webdriverClick is not a boolean",
                    ));
                }
            }
            _ => {
                return Err(WebDriverError::new(
                    ErrorStatus::InvalidArgument,
                    format!("Unrecognised option {}", name),
                ))
            }
        }
        Ok(())
    }

    fn accept_custom(&mut self, _: &str, _: &Value, _: &Capabilities) -> WebDriverResult<bool> {
        Ok(true)
    }
}

/// Android-specific options in the `moz:firefoxOptions` struct.
/// These map to "androidCamelCase", following [chromedriver's Android-specific
/// Capabilities](http://chromedriver.chromium.org/getting-started/getting-started---android).
#[derive(Default, Clone, Debug, PartialEq)]
pub struct AndroidOptions {
    pub activity: Option<String>,
    pub device_serial: Option<String>,
    pub intent_arguments: Option<Vec<String>>,
    pub package: String,
}

impl AndroidOptions {
    fn new(package: String) -> AndroidOptions {
        AndroidOptions {
            package,
            ..Default::default()
        }
    }
}

/// Rust representation of `moz:firefoxOptions`.
///
/// Calling `FirefoxOptions::from_capabilities(binary, capabilities)` causes
/// the encoded profile, the binary arguments, log settings, and additional
/// preferences to be checked and unmarshaled from the `moz:firefoxOptions`
/// JSON Object into a Rust representation.
#[derive(Default, Debug)]
pub struct FirefoxOptions {
    pub binary: Option<PathBuf>,
    pub profile: Option<Profile>,
    pub args: Option<Vec<String>>,
    pub env: Option<Vec<(String, String)>>,
    pub log: LogOptions,
    pub prefs: Vec<(String, Pref)>,
    pub android: Option<AndroidOptions>,
}

impl FirefoxOptions {
    pub fn new() -> FirefoxOptions {
        Default::default()
    }

    pub fn from_capabilities(
        binary_path: Option<PathBuf>,
        matched: &mut Capabilities,
    ) -> WebDriverResult<FirefoxOptions> {
        let mut rv = FirefoxOptions::new();
        rv.binary = binary_path;

        if let Some(json) = matched.remove("moz:firefoxOptions") {
            let options = json.as_object().ok_or_else(|| {
                WebDriverError::new(
                    ErrorStatus::InvalidArgument,
                    "'moz:firefoxOptions' \
                 capability is not an object",
                )
            })?;

            rv.android = FirefoxOptions::load_android(&options)?;
            rv.args = FirefoxOptions::load_args(&options)?;
            rv.env = FirefoxOptions::load_env(&options)?;
            rv.log = FirefoxOptions::load_log(&options)?;
            rv.prefs = FirefoxOptions::load_prefs(&options)?;
            rv.profile = FirefoxOptions::load_profile(&options)?;
        }

        Ok(rv)
    }

    fn load_profile(options: &Capabilities) -> WebDriverResult<Option<Profile>> {
        if let Some(profile_json) = options.get("profile") {
            let profile_base64 = profile_json.as_str().ok_or_else(|| {
                WebDriverError::new(ErrorStatus::InvalidArgument, "Profile is not a string")
            })?;
            let profile_zip = &*base64::decode(profile_base64)?;

            // Create an emtpy profile directory
            let profile = Profile::new()?;
            unzip_buffer(
                profile_zip,
                profile
                    .temp_dir
                    .as_ref()
                    .expect("Profile doesn't have a path")
                    .path(),
            )?;

            Ok(Some(profile))
        } else {
            Ok(None)
        }
    }

    fn load_args(options: &Capabilities) -> WebDriverResult<Option<Vec<String>>> {
        if let Some(args_json) = options.get("args") {
            let args_array = args_json.as_array().ok_or_else(|| {
                WebDriverError::new(
                    ErrorStatus::InvalidArgument,
                    "Arguments were not an \
                 array",
                )
            })?;
            let args = args_array
                .iter()
                .map(|x| x.as_str().map(|x| x.to_owned()))
                .collect::<Option<Vec<String>>>()
                .ok_or_else(|| {
                    WebDriverError::new(
                        ErrorStatus::InvalidArgument,
                        "Arguments entries were not all strings",
                    )
                })?;
            Ok(Some(args))
        } else {
            Ok(None)
        }
    }

    pub fn load_env(options: &Capabilities) -> WebDriverResult<Option<Vec<(String, String)>>> {
        if let Some(env_data) = options.get("env") {
            let env = env_data.as_object().ok_or_else(|| {
                WebDriverError::new(ErrorStatus::InvalidArgument, "Env was not an object")
            })?;
            let mut rv = Vec::with_capacity(env.len());
            for (key, value) in env.iter() {
                rv.push((
                    key.clone(),
                    value
                        .as_str()
                        .ok_or_else(|| {
                            WebDriverError::new(
                                ErrorStatus::InvalidArgument,
                                "Env value is not a string",
                            )
                        })?
                        .to_string(),
                ));
            }
            Ok(Some(rv))
        } else {
            Ok(None)
        }
    }

    fn load_log(options: &Capabilities) -> WebDriverResult<LogOptions> {
        if let Some(json) = options.get("log") {
            let log = json.as_object().ok_or_else(|| {
                WebDriverError::new(ErrorStatus::InvalidArgument, "Log section is not an object")
            })?;

            let level = match log.get("level") {
                Some(json) => {
                    let s = json.as_str().ok_or_else(|| {
                        WebDriverError::new(
                            ErrorStatus::InvalidArgument,
                            "Log level is not a string",
                        )
                    })?;
                    Some(Level::from_str(s).ok().ok_or_else(|| {
                        WebDriverError::new(ErrorStatus::InvalidArgument, "Log level is unknown")
                    })?)
                }
                None => None,
            };

            Ok(LogOptions { level })
        } else {
            Ok(Default::default())
        }
    }

    pub fn load_prefs(options: &Capabilities) -> WebDriverResult<Vec<(String, Pref)>> {
        if let Some(prefs_data) = options.get("prefs") {
            let prefs = prefs_data.as_object().ok_or_else(|| {
                WebDriverError::new(ErrorStatus::InvalidArgument, "Prefs were not an object")
            })?;
            let mut rv = Vec::with_capacity(prefs.len());
            for (key, value) in prefs.iter() {
                rv.push((key.clone(), pref_from_json(value)?));
            }
            Ok(rv)
        } else {
            Ok(vec![])
        }
    }

    pub fn load_android(options: &Capabilities) -> WebDriverResult<Option<AndroidOptions>> {
        if let Some(package_json) = options.get("androidPackage") {
            let package = package_json
                .as_str()
                .ok_or_else(|| {
                    WebDriverError::new(
                        ErrorStatus::InvalidArgument,
                        "androidPackage is not a string",
                    )
                })?
                .to_owned();

            // https://developer.android.com/studio/build/application-id
            let package_regexp =
                Regex::new(r#"^([a-zA-Z][a-zA-Z0-9_]*\.){1,}([a-zA-Z][a-zA-Z0-9_]*)$"#).unwrap();
            if !package_regexp.is_match(package.as_bytes()) {
                return Err(WebDriverError::new(
                    ErrorStatus::InvalidArgument,
                    "Not a valid androidPackage name",
                ));
            }

            let mut android = AndroidOptions::new(package);

            android.activity = match options.get("androidActivity") {
                Some(json) => {
                    let activity = json
                        .as_str()
                        .ok_or_else(|| {
                            WebDriverError::new(
                                ErrorStatus::InvalidArgument,
                                "androidActivity is not a string",
                            )
                        })?
                        .to_owned();

                    if activity.contains("/") {
                        return Err(WebDriverError::new(
                            ErrorStatus::InvalidArgument,
                            "androidActivity should not contain '/",
                        ));
                    }

                    Some(activity)
                }
                None => None,
            };

            android.device_serial = match options.get("androidDeviceSerial") {
                Some(json) => Some(
                    json.as_str()
                        .ok_or_else(|| {
                            WebDriverError::new(
                                ErrorStatus::InvalidArgument,
                                "androidDeviceSerial is not a string",
                            )
                        })?
                        .to_owned(),
                ),
                None => None,
            };

            android.intent_arguments = match options.get("androidIntentArguments") {
                Some(json) => {
                    let args_array = json.as_array().ok_or_else(|| {
                        WebDriverError::new(
                            ErrorStatus::InvalidArgument,
                            "androidIntentArguments is not an array",
                        )
                    })?;
                    let args = args_array
                        .iter()
                        .map(|x| x.as_str().map(|x| x.to_owned()))
                        .collect::<Option<Vec<String>>>()
                        .ok_or_else(|| {
                            WebDriverError::new(
                                ErrorStatus::InvalidArgument,
                                "androidIntentArguments entries are not all strings",
                            )
                        })?;

                    Some(args)
                }
                None => None,
            };

            Ok(Some(android))
        } else {
            Ok(None)
        }
    }
}

fn pref_from_json(value: &Value) -> WebDriverResult<Pref> {
    match *value {
        Value::String(ref x) => Ok(Pref::new(x.clone())),
        Value::Number(ref x) => Ok(Pref::new(x.as_i64().unwrap())),
        Value::Bool(x) => Ok(Pref::new(x)),
        _ => Err(WebDriverError::new(
            ErrorStatus::UnknownError,
            "Could not convert pref value to string, boolean, or integer",
        )),
    }
}

fn unzip_buffer(buf: &[u8], dest_dir: &Path) -> WebDriverResult<()> {
    let reader = Cursor::new(buf);
    let mut zip = zip::ZipArchive::new(reader)
        .map_err(|_| WebDriverError::new(ErrorStatus::UnknownError, "Failed to unzip profile"))?;

    for i in 0..zip.len() {
        let mut file = zip.by_index(i).map_err(|_| {
            WebDriverError::new(
                ErrorStatus::UnknownError,
                "Processing profile zip file failed",
            )
        })?;
        let unzip_path = {
            let name = file.name();
            let is_dir = name.ends_with('/');
            let rel_path = Path::new(name);
            let dest_path = dest_dir.join(rel_path);

            {
                let create_dir = if is_dir {
                    Some(dest_path.as_path())
                } else {
                    dest_path.parent()
                };
                if let Some(dir) = create_dir {
                    if !dir.exists() {
                        debug!("Creating profile directory tree {}", dir.to_string_lossy());
                        fs::create_dir_all(dir)?;
                    }
                }
            }

            if is_dir {
                None
            } else {
                Some(dest_path)
            }
        };

        if let Some(unzip_path) = unzip_path {
            debug!("Extracting profile to {}", unzip_path.to_string_lossy());
            let dest = fs::File::create(unzip_path)?;
            if file.size() > 0 {
                let mut writer = BufWriter::new(dest);
                io::copy(&mut file, &mut writer)?;
            }
        }
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    extern crate mozprofile;

    use super::*;
    use crate::marionette::MarionetteHandler;

    use self::mozprofile::preferences::Pref;
    use serde_json::json;
    use std::default::Default;
    use std::fs::File;
    use std::io::Read;

    use webdriver::capabilities::Capabilities;

    fn example_profile() -> Value {
        let mut profile_data = Vec::with_capacity(1024);
        let mut profile = File::open("src/tests/profile.zip").unwrap();
        profile.read_to_end(&mut profile_data).unwrap();
        Value::String(base64::encode(&profile_data))
    }

    fn make_options(firefox_opts: Capabilities) -> WebDriverResult<FirefoxOptions> {
        let mut caps = Capabilities::new();
        caps.insert("moz:firefoxOptions".into(), Value::Object(firefox_opts));

        FirefoxOptions::from_capabilities(None, &mut caps)
    }

    #[test]
    fn fx_options_default() {
        let opts = FirefoxOptions::new();
        assert_eq!(opts.android, None);
        assert_eq!(opts.args, None);
        assert_eq!(opts.binary, None);
        assert_eq!(opts.log, LogOptions { level: None });
        assert_eq!(opts.prefs, vec![]);
        // Profile doesn't support PartialEq
        // assert_eq!(opts.profile, None);
    }

    #[test]
    fn fx_options_from_capabilities_no_binary_and_caps() {
        let mut caps = Capabilities::new();

        let opts = FirefoxOptions::from_capabilities(None, &mut caps).unwrap();
        assert_eq!(opts.android, None);
        assert_eq!(opts.args, None);
        assert_eq!(opts.binary, None);
        assert_eq!(opts.log, LogOptions { level: None });
        assert_eq!(opts.prefs, vec![]);
    }

    #[test]
    fn fx_options_from_capabilities_with_binary_and_caps() {
        let mut caps = Capabilities::new();
        caps.insert(
            "moz:firefoxOptions".into(),
            Value::Object(Capabilities::new()),
        );

        let binary = PathBuf::from("foo");

        let opts = FirefoxOptions::from_capabilities(Some(binary.clone()), &mut caps).unwrap();
        assert_eq!(opts.android, None);
        assert_eq!(opts.args, None);
        assert_eq!(opts.binary, Some(binary));
        assert_eq!(opts.log, LogOptions { level: None });
        assert_eq!(opts.prefs, vec![]);
    }

    #[test]
    fn fx_options_from_capabilities_with_invalid_caps() {
        let mut caps = Capabilities::new();
        caps.insert("moz:firefoxOptions".into(), json!(42));

        FirefoxOptions::from_capabilities(None, &mut caps)
            .expect_err("Firefox options need to be of type object");
    }

    #[test]
    fn fx_options_android_no_package() {
        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("androidAvtivity".into(), json!("foo"));

        let opts = make_options(firefox_opts).expect("valid firefox options");
        assert_eq!(opts.android, None);
    }

    #[test]
    fn fx_options_android_package_valid_value() {
        for value in ["foo.bar", "foo.bar.cheese.is.good", "Foo.Bar_9"].iter() {
            let mut firefox_opts = Capabilities::new();
            firefox_opts.insert("androidPackage".into(), json!(value));

            let opts = make_options(firefox_opts).expect("valid firefox options");
            assert_eq!(opts.android, Some(AndroidOptions::new(value.to_string())));
        }
    }

    #[test]
    fn fx_options_android_package_invalid_type() {
        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("androidPackage".into(), json!(42));

        make_options(firefox_opts).expect_err("invalid firefox options");
    }

    #[test]
    fn fx_options_android_package_invalid_value() {
        for value in ["../foo", "\\foo\n", "foo", "_foo", "0foo"].iter() {
            let mut firefox_opts = Capabilities::new();
            firefox_opts.insert("androidPackage".into(), json!(value));
            make_options(firefox_opts).expect_err("invalid firefox options");
        }
    }

    #[test]
    fn fx_options_android_activity_valid_value() {
        for value in ["cheese", "Cheese_9"].iter() {
            let mut firefox_opts = Capabilities::new();
            firefox_opts.insert("androidPackage".into(), json!("foo.bar"));
            firefox_opts.insert("androidActivity".into(), json!(value));

            let opts = make_options(firefox_opts).expect("valid firefox options");
            let android_opts = AndroidOptions {
                package: "foo.bar".to_owned(),
                activity: Some(value.to_string()),
                ..Default::default()
            };
            assert_eq!(opts.android, Some(android_opts));
        }
    }

    #[test]
    fn fx_options_android_activity_invalid_type() {
        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("androidPackage".into(), json!("foo.bar"));
        firefox_opts.insert("androidActivity".into(), json!(42));

        make_options(firefox_opts).expect_err("invalid firefox options");
    }

    #[test]
    fn fx_options_android_activity_invalid_value() {
        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("androidPackage".into(), json!("foo.bar"));
        firefox_opts.insert("androidActivity".into(), json!("foo.bar/cheese"));

        make_options(firefox_opts).expect_err("invalid firefox options");
    }

    #[test]
    fn fx_options_android_device_serial() {
        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("androidPackage".into(), json!("foo.bar"));
        firefox_opts.insert("androidDeviceSerial".into(), json!("cheese"));

        let opts = make_options(firefox_opts).expect("valid firefox options");
        let android_opts = AndroidOptions {
            package: "foo.bar".to_owned(),
            device_serial: Some("cheese".to_owned()),
            ..Default::default()
        };
        assert_eq!(opts.android, Some(android_opts));
    }

    #[test]
    fn fx_options_android_serial_invalid() {
        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("androidPackage".into(), json!("foo.bar"));
        firefox_opts.insert("androidDeviceSerial".into(), json!(42));

        make_options(firefox_opts).expect_err("invalid firefox options");
    }

    #[test]
    fn fx_options_android_intent_arguments() {
        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("androidPackage".into(), json!("foo.bar"));
        firefox_opts.insert("androidIntentArguments".into(), json!(["lorem", "ipsum"]));

        let opts = make_options(firefox_opts).expect("valid firefox options");
        let android_opts = AndroidOptions {
            package: "foo.bar".to_owned(),
            intent_arguments: Some(vec!["lorem".to_owned(), "ipsum".to_owned()]),
            ..Default::default()
        };
        assert_eq!(opts.android, Some(android_opts));
    }

    #[test]
    fn fx_options_android_intent_arguments_no_array() {
        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("androidPackage".into(), json!("foo.bar"));
        firefox_opts.insert("androidIntentArguments".into(), json!(42));

        make_options(firefox_opts).expect_err("invalid firefox options");
    }

    #[test]
    fn fx_options_android_intent_arguments_invalid_value() {
        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("androidPackage".into(), json!("foo.bar"));
        firefox_opts.insert("androidIntentArguments".into(), json!(["lorem", 42]));

        make_options(firefox_opts).expect_err("invalid firefox options");
    }

    #[test]
    fn fx_options_env() {
        let mut env: Map<String, Value> = Map::new();
        env.insert("TEST_KEY_A".into(), Value::String("test_value_a".into()));
        env.insert("TEST_KEY_B".into(), Value::String("test_value_b".into()));

        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("env".into(), env.into());

        let mut opts = make_options(firefox_opts).expect("valid firefox options");
        for sorted in opts.env.iter_mut() {
            sorted.sort()
        }
        assert_eq!(
            opts.env,
            Some(vec![
                ("TEST_KEY_A".into(), "test_value_a".into()),
                ("TEST_KEY_B".into(), "test_value_b".into()),
            ])
        );
    }

    #[test]
    fn fx_options_env_invalid_container() {
        let env = Value::Number(1.into());

        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("env".into(), env.into());

        make_options(firefox_opts).expect_err("invalid firefox options");
    }

    #[test]
    fn fx_options_env_invalid_value() {
        let mut env: Map<String, Value> = Map::new();
        env.insert("TEST_KEY".into(), Value::Number(1.into()));

        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("env".into(), env.into());

        make_options(firefox_opts).expect_err("invalid firefox options");
    }

    #[test]
    fn test_profile() {
        let encoded_profile = example_profile();
        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("profile".into(), encoded_profile);

        let opts = make_options(firefox_opts).expect("valid firefox options");
        let mut profile = opts.profile.expect("valid firefox profile");
        let prefs = profile.user_prefs().expect("valid preferences");

        println!("{:#?}", prefs.prefs);

        assert_eq!(
            prefs.get("startup.homepage_welcome_url"),
            Some(&Pref::new("data:text/html,PASS"))
        );
    }

    #[test]
    fn test_prefs() {
        let encoded_profile = example_profile();
        let mut prefs: Map<String, Value> = Map::new();
        prefs.insert(
            "browser.display.background_color".into(),
            Value::String("#00ff00".into()),
        );

        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("profile".into(), encoded_profile);
        firefox_opts.insert("prefs".into(), Value::Object(prefs));

        let opts = make_options(firefox_opts).expect("valid profile and prefs");
        let mut profile = opts.profile.expect("valid firefox profile");

        let handler = MarionetteHandler::new(Default::default());
        handler
            .set_prefs(2828, &mut profile, true, opts.prefs)
            .expect("set preferences");

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
}
