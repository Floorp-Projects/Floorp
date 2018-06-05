//! Argument string parsing and matching functions for Firefox.
//!
//! Which arguments Firefox accepts and in what style depends on the platform.
//! On Windows only, arguments can be prefixed with `/` (slash), such as
//! `/foreground`.  Elsewhere, including Windows, arguments may be prefixed
//! with both single (`-foreground`) and double (`--foreground`) dashes.
//!
//! An argument's name is determined by a space or an assignment operator (`=`)
//! so that for the string `-foo=bar`, `foo` is considered the argument's
//! basename.

use std::ffi::{OsStr, OsString};

use runner::platform;

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

/// Represents a Firefox command-line argument.
#[derive(Debug, PartialEq)]
pub enum Arg {
    /// `-foreground` ensures application window gets focus, which is not the
    /// default on macOS.
    Foreground,

    /// `-no-remote` prevents remote commands to this instance of Firefox, and
    /// ensure we always start a new instance.
    NoRemote,

    /// `-P NAME` starts Firefox with a profile with a given name.
    NamedProfile,

    /// `-profile PATH` starts Firefox with the profile at the specified path.
    Profile,

    /// `-ProfileManager` starts Firefox with the profile chooser dialogue.
    ProfileManager,

    /// All other arguments.
    Other(String),

    /// Not an argument.
    None,
}

impl<'a> From<&'a OsString> for Arg {
    fn from(arg_str: &OsString) -> Arg {
        if let Some(basename) = parse_arg_name(arg_str) {
            match &*basename {
                "profile" => Arg::Profile,
                "P" => Arg::NamedProfile,
                "ProfileManager" => Arg::ProfileManager,
                "foreground" => Arg::Foreground,
                "no-remote" => Arg::NoRemote,
                _ => Arg::Other(basename),
            }
        } else {
           Arg::None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{Arg, parse_arg_name};
    use std::ffi::OsString;

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
        parse("-profile /foo", Some("profile"));
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
    fn test_arg_from_osstring() {
        assert_eq!(Arg::from(&OsString::from("-- profile")), Arg::None);
        assert_eq!(Arg::from(&OsString::from("profile")), Arg::None);
        assert_eq!(Arg::from(&OsString::from("profile -P")), Arg::None);
        assert_eq!(Arg::from(&OsString::from("-profiled")), Arg::Other("profiled".into()));
        assert_eq!(Arg::from(&OsString::from("-PROFILEMANAGER")), Arg::Other("PROFILEMANAGER".into()));

        assert_eq!(Arg::from(&OsString::from("--profile")), Arg::Profile);
        assert_eq!(Arg::from(&OsString::from("-profile foo")), Arg::Profile);

        assert_eq!(Arg::from(&OsString::from("--ProfileManager")), Arg::ProfileManager);
        assert_eq!(Arg::from(&OsString::from("-ProfileManager")), Arg::ProfileManager);

        // TODO: -Ptest is valid
        //assert_eq!(Arg::from(&OsString::from("-Ptest")), Arg::NamedProfile);
        assert_eq!(Arg::from(&OsString::from("-P")), Arg::NamedProfile);
        assert_eq!(Arg::from(&OsString::from("-P test")), Arg::NamedProfile);

        assert_eq!(Arg::from(&OsString::from("--foreground")), Arg::Foreground);
        assert_eq!(Arg::from(&OsString::from("-foreground")), Arg::Foreground);

        assert_eq!(Arg::from(&OsString::from("--no-remote")), Arg::NoRemote);
        assert_eq!(Arg::from(&OsString::from("-no-remote")), Arg::NoRemote);
    }
}
