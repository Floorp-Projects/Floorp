use std::collections::BTreeMap;
use std::fs;
use std::io;
use std::io::BufWriter;
use std::io::Cursor;
use std::path::{Path, PathBuf};
use std::str::FromStr;

use marionette::LogOptions;
use mozprofile::preferences::Pref;
use mozprofile::profile::Profile;
use rustc_serialize::base64::FromBase64;
use rustc_serialize::json::Json;
use webdriver::command::NewSessionParameters;
use webdriver::error::{ErrorStatus, WebDriverError, WebDriverResult};
use zip;

use logging::LogLevel;

#[derive(Default)]
pub struct FirefoxOptions {
    pub binary: Option<PathBuf>,
    pub profile: Option<Profile>,
    pub args: Option<Vec<String>>,
    pub log: LogOptions,
    pub prefs: Vec<(String, Pref)>
}

impl FirefoxOptions {
    pub fn from_capabilities(capabilities: &mut NewSessionParameters) -> WebDriverResult<FirefoxOptions> {
        if let Some(options) = capabilities.consume("moz:firefoxOptions") {
            let firefox_options = try!(options
                                       .as_object()
                                       .ok_or(WebDriverError::new(
                                           ErrorStatus::InvalidArgument,
                                           "'moz:firefoxOptions' capability was not an object")));
            let binary = try!(FirefoxOptions::load_binary(&firefox_options));
            let profile = try!(FirefoxOptions::load_profile(&firefox_options));
            let args = try!(FirefoxOptions::load_args(&firefox_options));
            let log = try!(FirefoxOptions::load_log(&firefox_options));
            let prefs = try!(FirefoxOptions::load_prefs(&firefox_options));

            Ok(FirefoxOptions {
                binary: binary,
                profile: profile,
                args: args,
                log: log,
                prefs: prefs,
            })
        } else {
            Ok(Default::default())
        }
    }

    fn load_binary(options: &BTreeMap<String, Json>) -> WebDriverResult<Option<PathBuf>> {
        if let Some(path) = options.get("binary") {
            Ok(Some(PathBuf::from(try!(path
                                       .as_string()
                                       .ok_or(WebDriverError::new(
                                           ErrorStatus::InvalidArgument,
                                           "'binary' capability was not a string"))))))
        } else {
            Ok(None)
        }
    }

    fn load_profile(options: &BTreeMap<String, Json>) -> WebDriverResult<Option<Profile>> {
        if let Some(profile_json) = options.get("profile") {
            let profile_base64 = try!(profile_json
                                      .as_string()
                                      .ok_or(
                                          WebDriverError::new(ErrorStatus::UnknownError,
                                                              "Profile was not a string")));
            let profile_zip = &*try!(profile_base64.from_base64());

            // Create an emtpy profile directory
            let profile = try!(Profile::new(None));
            try!(unzip_buffer(profile_zip,
                              profile.temp_dir
                              .as_ref()
                              .expect("Profile doesn't have a path")
                              .path()));

            Ok(Some(profile))
        } else {
            Ok(None)
        }
    }

    fn load_args(options: &BTreeMap<String, Json>) -> WebDriverResult<Option<Vec<String>>> {
        if let Some(args_json) = options.get("args") {
            let args_array = try!(args_json.as_array()
                                  .ok_or(WebDriverError::new(ErrorStatus::UnknownError,
                                                             "Arguments were not an array")));
            let args = try!(args_array
                            .iter()
                            .map(|x| x.as_string().map(|x| x.to_owned()))
                            .collect::<Option<Vec<String>>>()
                            .ok_or(WebDriverError::new(
                                ErrorStatus::UnknownError,
                                "Arguments entries were not all strings")));
            Ok(Some(args))
        } else {
            Ok(None)
        }
    }

    fn load_log(options: &BTreeMap<String, Json>) -> WebDriverResult<LogOptions> {
        if let Some(json) = options.get("log") {
            let log = try!(json.as_object()
                .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument, "Log section is not an object")));

            let level = match log.get("level") {
                Some(json) => {
                    let s = try!(json.as_string()
                        .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument, "Log level is not a string")));
                    Some(try!(LogLevel::from_str(s).ok()
                        .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument, "Log level is unknown"))))
                },
                None => None,
            };

            Ok(LogOptions { level: level })

        } else {
            Ok(Default::default())
        }
    }

    pub fn load_prefs(options: &BTreeMap<String, Json>) -> WebDriverResult<Vec<(String, Pref)>> {
        if let Some(prefs_data) = options.get("prefs") {
            let prefs = try!(prefs_data
                             .as_object()
                             .ok_or(WebDriverError::new(ErrorStatus::UnknownError,"Prefs were not an object")));
            let mut rv = Vec::with_capacity(prefs.len());
            for (key, value) in prefs.iter() {
                rv.push((key.clone(), try!(pref_from_json(value))));
            };
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
            let mut writer = BufWriter::new(dest);
            try!(io::copy(&mut file, &mut writer));
        }
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    extern crate mozprofile;
    extern crate rustc_serialize;

    use std::collections::BTreeMap;
    use std::default::Default;
    use std::fs::File;
    use std::io::Read;

    use self::mozprofile::preferences::Pref;
    use self::rustc_serialize::base64::{ToBase64, Config, CharacterSet, Newline};
    use self::rustc_serialize::json::Json;

    use webdriver::command::NewSessionParameters;
    use marionette::MarionetteHandler;
    use super::FirefoxOptions;

    fn example_profile() -> Json {
        let mut profile_data = Vec::with_capacity(1024);
        let mut profile = File::open("src/tests/profile.zip").unwrap();
        profile.read_to_end(&mut profile_data).unwrap();
        let base64_config = Config {
            char_set: CharacterSet::Standard,
            newline: Newline::LF,
            pad: true,
            line_length: None
        };
        Json::String(profile_data.to_base64(base64_config))
    }

    fn capabilities() -> NewSessionParameters {
        let desired: BTreeMap<String, Json> = BTreeMap::new();
        let required: BTreeMap<String, Json> = BTreeMap::new();
        NewSessionParameters {
            desired: desired,
            required: required
        }
    }

    #[test]
    fn test_profile() {
        let encoded_profile = example_profile();

        let mut capabilities = capabilities();
        let mut firefox_options: BTreeMap<String, Json> = BTreeMap::new();
        firefox_options.insert("profile".into(), encoded_profile);
        capabilities.required.insert("moz:firefoxOptions".into(), Json::Object(firefox_options));

        let options = FirefoxOptions::from_capabilities(&mut capabilities).unwrap();
        let mut profile = options.profile.unwrap();
        let prefs = profile.user_prefs().unwrap();

        println!("{:?}",prefs.prefs);

        assert_eq!(prefs.get("startup.homepage_welcome_url"),
                   Some(&Pref::new("data:text/html,PASS")));
    }

    #[test]
    fn test_prefs() {
        let encoded_profile = example_profile();

        let mut capabilities = capabilities();
        let mut firefox_options: BTreeMap<String, Json> = BTreeMap::new();
        firefox_options.insert("profile".into(), encoded_profile);
        let mut prefs: BTreeMap<String, Json> = BTreeMap::new();
        prefs.insert("browser.display.background_color".into(), Json::String("#00ff00".into()));
        firefox_options.insert("prefs".into(), Json::Object(prefs));
        capabilities.required.insert("moz:firefoxOptions".into(), Json::Object(firefox_options));


        let options = FirefoxOptions::from_capabilities(&mut capabilities).unwrap();
        let mut profile = options.profile.unwrap();

        let handler = MarionetteHandler::new(Default::default());
        handler.set_prefs(2828, &mut profile, true, options.prefs).unwrap();

        let prefs_set = profile.user_prefs().unwrap();
        println!("{:?}",prefs_set.prefs);
        assert_eq!(prefs_set.get("startup.homepage_welcome_url"),
                   Some(&Pref::new("data:text/html,PASS")));
        assert_eq!(prefs_set.get("browser.display.background_color"),
                   Some(&Pref::new("#00ff00")));
        assert_eq!(prefs_set.get("marionette.defaultPrefs.port"),
                   Some(&Pref::new(2828)));
    }
}
