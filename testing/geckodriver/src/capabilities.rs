use logging::LogLevel;
use marionette::LogOptions;
use mozprofile::preferences::Pref;
use mozprofile::profile::Profile;
use mozrunner::runner::platform::firefox_default_path;
use mozversion::{self, firefox_version, Version};
use regex::bytes::Regex;
use rustc_serialize::base64::FromBase64;
use rustc_serialize::json::Json;
use std::collections::BTreeMap;
use std::default::Default;
use std::error::Error;
use std::fs;
use std::io::BufWriter;
use std::io::Cursor;
use std::io;
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
            fallback_binary: fallback_binary,
            version_cache: BTreeMap::new(),
        }
    }

    fn set_binary(&mut self, capabilities: &BTreeMap<String, Json>) {
        self.chosen_binary = capabilities
            .get("moz:firefoxOptions")
            .and_then(|x| x.find("binary"))
            .and_then(|x| x.as_string())
            .map(|x| PathBuf::from(x))
            .or_else(|| self.fallback_binary.map(|x| x.clone()))
            .or_else(|| firefox_default_path())
            .and_then(|x| x.canonicalize().ok())
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
                self.version_cache
                    .insert(binary.clone(), version.clone());
            } else {
                debug!("Failed to get binary version");
            }
            rv
        } else {
            None
        }
    }

    fn version_from_binary(&self, binary: &PathBuf) -> Option<String> {
        let version_regexp = Regex::new(r#"\d+\.\d+(?:[a-z]\d+)?"#).expect("Error parsing version regexp");
        let output = Command::new(binary)
            .args(&["-version"])
            .stdout(Stdio::piped())
            .spawn()
            .and_then(|child| child.wait_with_output())
            .ok();

        if let Some(x) = output {
            version_regexp.captures(&*x.stdout)
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
    WebDriverError::new(
        ErrorStatus::SessionNotCreated,
        err.description().to_string())
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
            Ok(try!(Version::from_str(&*x).or_else(|x| Err(convert_version_error(x)))).major >= 52)
        } else {
            Ok(false)
        }
    }

    fn compare_browser_version(&mut self,
                               version: &str,
                               comparison: &str)
                               -> WebDriverResult<bool> {
        try!(Version::from_str(version).or_else(|x| Err(convert_version_error(x))))
            .matches(comparison)
            .or_else(|x| Err(convert_version_error(x)))
    }

    fn accept_proxy(&mut self, _: &Capabilities, _: &Capabilities) -> WebDriverResult<bool> {
        Ok(true)
    }

    fn validate_custom(&self, name: &str,  value: &Json) -> WebDriverResult<()> {
        if !name.starts_with("moz:") {
            return Ok(())
        }
        match name {
            "moz:firefoxOptions" => {
                let data = try_opt!(value.as_object(),
                                    ErrorStatus::InvalidArgument,
                                    "moz:firefoxOptions is not an object");
                for (key, value) in data.iter() {
                    match &**key {
                        "binary" => {
                            if !value.is_string() {
                                return Err(WebDriverError::new(
                                    ErrorStatus::InvalidArgument,
                                         "binary path is not a string"));
                            }
                        },
                        "args" => {
                            if !try_opt!(value.as_array(),
                                         ErrorStatus::InvalidArgument,
                                         "args is not an array")
                                .iter()
                                .all(|value| value.is_string()) {
                                return Err(WebDriverError::new(
                                    ErrorStatus::InvalidArgument,
                                         "args entry is not a string"));
                                }
                        },
                        "profile" => {
                            if !value.is_string() {
                                return Err(WebDriverError::new(
                                    ErrorStatus::InvalidArgument,
                                         "profile is not a string"));
                            }
                        },
                        "log" => {
                            let log_data = try_opt!(value.as_object(),
                                                    ErrorStatus::InvalidArgument,
                                                    "log value is not an object");
                            for (log_key, log_value) in log_data.iter() {
                                match &**log_key {
                                    "level" => {
                                        let level = try_opt!(log_value.as_string(),
                                                             ErrorStatus::InvalidArgument,
                                                             "log level is not a string");
                                        if LogLevel::from_str(level).is_err() {
                                            return Err(WebDriverError::new(
                                                ErrorStatus::InvalidArgument,
                                                format!("{} is not a valid log level",
                                                        level)))
                                        }
                                    }
                                    x => return Err(WebDriverError::new(
                                        ErrorStatus::InvalidArgument,
                                        format!("Invalid log field {}", x)))
                                }
                            }
                        },
                        "prefs" => {
                            let prefs_data = try_opt!(value.as_object(),
                                                    ErrorStatus::InvalidArgument,
                                                    "prefs value is not an object");
                            if !prefs_data.values()
                                .all(|x| x.is_string() || x.is_i64() || x.is_u64() || x.is_boolean()) {
                                    return Err(WebDriverError::new(
                                        ErrorStatus::InvalidArgument,
                                        "Preference values not all string or integer or boolean"));
                                }
                        }
                        x => return Err(WebDriverError::new(
                            ErrorStatus::InvalidArgument,
                            format!("Invalid moz:firefoxOptions field {}", x)))
                    }
                }
            }
            _ => return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                format!("Unrecognised option {}", name)))
        }
        Ok(())
    }

    fn accept_custom(&mut self, _: &str, _: &Json, _: &Capabilities) -> WebDriverResult<bool> {
        Ok(true)
    }
}

/// Rust representation of `moz:firefoxOptions`.
///
/// Calling `FirefoxOptions::from_capabilities(binary, capabilities)` causes
/// the encoded profile, the binary arguments, log settings, and additional
/// preferences to be checked and unmarshaled from the `moz:firefoxOptions`
/// JSON Object into a Rust representation.
#[derive(Default)]
pub struct FirefoxOptions {
    pub binary: Option<PathBuf>,
    pub profile: Option<Profile>,
    pub args: Option<Vec<String>>,
    pub log: LogOptions,
    pub prefs: Vec<(String, Pref)>,
}

impl FirefoxOptions {
    pub fn new() -> FirefoxOptions {
        Default::default()
    }

    pub fn from_capabilities(binary_path: Option<PathBuf>,
                             matched: &mut Capabilities)
                             -> WebDriverResult<FirefoxOptions> {
        let mut rv = FirefoxOptions::new();
        rv.binary = binary_path;

        if let Some(json) = matched.remove("moz:firefoxOptions") {
            let options = try!(json.as_object()
                                   .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                              "'moz:firefoxOptions' \
                                                               capability is not an object")));

            rv.profile = try!(FirefoxOptions::load_profile(&options));
            rv.args = try!(FirefoxOptions::load_args(&options));
            rv.log = try!(FirefoxOptions::load_log(&options));
            rv.prefs = try!(FirefoxOptions::load_prefs(&options));
        }

        Ok(rv)
    }

    fn load_profile(options: &Capabilities) -> WebDriverResult<Option<Profile>> {
        if let Some(profile_json) = options.get("profile") {
            let profile_base64 =
                try!(profile_json
                         .as_string()
                         .ok_or(WebDriverError::new(ErrorStatus::UnknownError,
                                                    "Profile is not a string")));
            let profile_zip = &*try!(profile_base64.from_base64());

            // Create an emtpy profile directory
            let profile = try!(Profile::new(None));
            try!(unzip_buffer(profile_zip,
                              profile
                                  .temp_dir
                                  .as_ref()
                                  .expect("Profile doesn't have a path")
                                  .path()));

            Ok(Some(profile))
        } else {
            Ok(None)
        }
    }

    fn load_args(options: &Capabilities) -> WebDriverResult<Option<Vec<String>>> {
        if let Some(args_json) = options.get("args") {
            let args_array = try!(args_json
                                      .as_array()
                                      .ok_or(WebDriverError::new(ErrorStatus::UnknownError,
                                                                 "Arguments were not an \
                                                                  array")));
            let args = try!(args_array
                                .iter()
                                .map(|x| x.as_string().map(|x| x.to_owned()))
                                .collect::<Option<Vec<String>>>()
                                .ok_or(WebDriverError::new(ErrorStatus::UnknownError,
                                                           "Arguments entries were not all \
                                                            strings")));
            Ok(Some(args))
        } else {
            Ok(None)
        }
    }

    fn load_log(options: &Capabilities) -> WebDriverResult<LogOptions> {
        if let Some(json) = options.get("log") {
            let log = try!(json.as_object()
                               .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                          "Log section is not an object")));

            let level = match log.get("level") {
                Some(json) => {
                    let s = try!(json.as_string()
                                     .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                                "Log level is not a string")));
                    Some(try!(LogLevel::from_str(s)
                                  .ok()
                                  .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                             "Log level is unknown"))))
                }
                None => None,
            };

            Ok(LogOptions { level: level })

        } else {
            Ok(Default::default())
        }
    }

    pub fn load_prefs(options: &Capabilities) -> WebDriverResult<Vec<(String, Pref)>> {
        if let Some(prefs_data) = options.get("prefs") {
            let prefs = try!(prefs_data
                                 .as_object()
                                 .ok_or(WebDriverError::new(ErrorStatus::UnknownError,
                                                            "Prefs were not an object")));
            let mut rv = Vec::with_capacity(prefs.len());
            for (key, value) in prefs.iter() {
                rv.push((key.clone(), try!(pref_from_json(value))));
            }
            Ok(rv)
        } else {
            Ok(vec![])
        }
    }
}

fn pref_from_json(value: &Json) -> WebDriverResult<Pref> {
    match value {
        &Json::String(ref x) => Ok(Pref::new(x.clone())),
        &Json::I64(x) => Ok(Pref::new(x)),
        &Json::U64(x) => Ok(Pref::new(x as i64)),
        &Json::Boolean(x) => Ok(Pref::new(x)),
        _ => Err(WebDriverError::new(ErrorStatus::UnknownError,
                                     "Could not convert pref value to string, boolean, or integer"))
    }
}

fn unzip_buffer(buf: &[u8], dest_dir: &Path) -> WebDriverResult<()> {
    let reader = Cursor::new(buf);
    let mut zip = try!(zip::ZipArchive::new(reader).map_err(|_| {
        WebDriverError::new(ErrorStatus::UnknownError, "Failed to unzip profile")
    }));

    for i in 0..zip.len() {
        let mut file = try!(zip.by_index(i).map_err(|_| {
            WebDriverError::new(ErrorStatus::UnknownError, "Processing profile zip file failed")
        }));
        let unzip_path = {
            let name = file.name();
            let is_dir = name.ends_with("/");
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
                        try!(fs::create_dir_all(dir));
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
            let dest = try!(fs::File::create(unzip_path));
            if file.size() > 0 {
                let mut writer = BufWriter::new(dest);
                try!(io::copy(&mut file, &mut writer));
            }
        }
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    extern crate mozprofile;
    extern crate rustc_serialize;

    use self::mozprofile::preferences::Pref;
    use self::rustc_serialize::base64::{CharacterSet, Config, Newline, ToBase64};
    use self::rustc_serialize::json::Json;
    use super::FirefoxOptions;
    use marionette::MarionetteHandler;
    use std::collections::BTreeMap;
    use std::default::Default;
    use std::fs::File;
    use std::io::Read;

    use webdriver::capabilities::Capabilities;

    fn example_profile() -> Json {
        let mut profile_data = Vec::with_capacity(1024);
        let mut profile = File::open("src/tests/profile.zip").unwrap();
        profile.read_to_end(&mut profile_data).unwrap();
        let base64_config = Config {
            char_set: CharacterSet::Standard,
            newline: Newline::LF,
            pad: true,
            line_length: None,
        };
        Json::String(profile_data.to_base64(base64_config))
    }

    fn make_options(firefox_opts: Capabilities) -> FirefoxOptions {
        let mut caps = Capabilities::new();
        caps.insert("moz:firefoxOptions".into(), Json::Object(firefox_opts));
        let binary = None;
        FirefoxOptions::from_capabilities(binary, &mut caps).unwrap()
    }

    #[test]
    fn test_profile() {
        let encoded_profile = example_profile();
        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("profile".into(), encoded_profile);

        let opts = make_options(firefox_opts);
        let mut profile = opts.profile.unwrap();
        let prefs = profile.user_prefs().unwrap();

        println!("{:#?}", prefs.prefs);

        assert_eq!(prefs.get("startup.homepage_welcome_url"),
                   Some(&Pref::new("data:text/html,PASS")));
    }

    #[test]
    fn test_prefs() {
        let encoded_profile = example_profile();
        let mut prefs: BTreeMap<String, Json> = BTreeMap::new();
        prefs.insert("browser.display.background_color".into(),
                     Json::String("#00ff00".into()));

        let mut firefox_opts = Capabilities::new();
        firefox_opts.insert("profile".into(), encoded_profile);
        firefox_opts.insert("prefs".into(), Json::Object(prefs));

        let opts = make_options(firefox_opts);
        let mut profile = opts.profile.unwrap();

        let handler = MarionetteHandler::new(Default::default());
        handler
            .set_prefs(2828, &mut profile, true, opts.prefs)
            .unwrap();

        let prefs_set = profile.user_prefs().unwrap();
        println!("{:#?}", prefs_set.prefs);

        assert_eq!(prefs_set.get("startup.homepage_welcome_url"),
                   Some(&Pref::new("data:text/html,PASS")));
        assert_eq!(prefs_set.get("browser.display.background_color"),
                   Some(&Pref::new("#00ff00")));
        assert_eq!(prefs_set.get("marionette.defaultPrefs.port"),
                   Some(&Pref::new(2828)));
    }
}
