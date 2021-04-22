/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

use crate::runner::platform;

/// Parse an argument string into a name and value
///
/// Given an argument like `"--arg=value"` this will split it into
/// `(Some("arg"), Some("value")). For a case like `"--arg"` it will
/// return `(Some("arg"), None)` and where the input doesn't look like
/// an argument e.g. `"value"` it will return `(None, Some("value"))`
fn parse_arg_name_value<T>(arg: T) -> (Option<String>, Option<String>)
where
    T: AsRef<OsStr>,
{
    let arg_os_str: &OsStr = arg.as_ref();
    let arg_str = arg_os_str.to_string_lossy();

    let mut name_start = 0;
    let mut name_end = 0;

    // Look for an argument name at the start of the
    // string
    for (i, c) in arg_str.chars().enumerate() {
        if i == 0 {
            if !platform::arg_prefix_char(c) {
                break;
            }
        } else if i == 1 {
            if name_end_char(c) {
                break;
            } else if c != '-' {
                name_start = i;
                name_end = name_start + 1;
            } else {
                name_start = i + 1;
                name_end = name_start;
            }
        } else {
            name_end += 1;
            if name_end_char(c) {
                name_end -= 1;
                break;
            }
        }
    }

    let name = if name_start > 0 && name_end > name_start {
        Some(arg_str[name_start..name_end].into())
    } else {
        None
    };

    // If there are characters in the string after the argument, read
    // them as the value, excluding the seperator (e.g. "=") if
    // present.
    let mut value_start = name_end;
    let value_end = arg_str.len();
    let value = if value_start < value_end {
        if let Some(c) = arg_str[value_start..value_end].chars().next() {
            if name_end_char(c) {
                value_start += 1;
            }
        }
        Some(arg_str[value_start..value_end].into())
    } else {
        None
    };
    (name, value)
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

impl Arg {
    fn new(name: &str) -> Arg {
        match &*name {
            "profile" => Arg::Profile,
            "P" => Arg::NamedProfile,
            "ProfileManager" => Arg::ProfileManager,
            "foreground" => Arg::Foreground,
            "no-remote" => Arg::NoRemote,
            _ => Arg::Other(name.into()),
        }
    }
}

impl<'a> From<&'a OsString> for Arg {
    fn from(arg_str: &OsString) -> Arg {
        if let (Some(name), _) = parse_arg_name_value(arg_str) {
            Arg::new(&name)
        } else {
            Arg::None
        }
    }
}

/// Parse an iterator over arguments into an vector of (name, value)
/// tuples
///
/// Each entry in the input argument will produce a single item in the
/// output. Because we don't know anything about the specific
/// arguments, something that doesn't parse as a named argument may
/// either be the value of a previous named argument, or may be a
/// positional argument.
pub fn parse_args<'a>(
    args: impl Iterator<Item = &'a OsString>,
) -> Vec<(Option<Arg>, Option<String>)> {
    args.map(|arg| parse_arg_name_value(arg))
        .map(|(name, value)| {
            if let Some(arg_name) = name {
                (Some(Arg::new(&arg_name)), value)
            } else {
                (None, value)
            }
        })
        .collect()
}

/// Given an iterator over all arguments, get the value of an argument
///
/// This assumes that the argument takes a single value and that is
/// either provided as a single argument entry
/// (e.g. `["--name=value"]`) or as the following argument
/// (e.g. `["--name", "value"])
pub fn get_arg_value<'a>(
    mut parsed_args: impl Iterator<Item = &'a (Option<Arg>, Option<String>)>,
    arg: Arg,
) -> Option<String> {
    let mut found_value = None;
    for (arg_name, arg_value) in &mut parsed_args {
        if let (Some(name), value) = (arg_name, arg_value) {
            if *name == arg {
                found_value = value.clone();
                break;
            }
        }
    }
    if found_value.is_none() {
        // If there wasn't a value, check if the following argument is a value
        if let Some((None, value)) = parsed_args.next() {
            found_value = value.clone();
        }
    }
    found_value
}

#[cfg(test)]
mod tests {
    use super::{get_arg_value, parse_arg_name_value, parse_args, Arg};
    use std::ffi::OsString;

    fn parse(arg: &str, name: Option<&str>) {
        let (result, _) = parse_arg_name_value(arg);
        assert_eq!(result, name.map(|x| x.to_string()));
    }

    #[test]
    fn test_parse_arg_name_value() {
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
    fn test_parse_arg_name_value_windows() {
        parse("/profile", Some("profile"));
    }

    #[cfg(not(target_os = "windows"))]
    #[test]
    fn test_parse_arg_name_value_non_windows() {
        parse("/profile", None);
    }

    #[test]
    fn test_arg_from_osstring() {
        assert_eq!(Arg::from(&OsString::from("-- profile")), Arg::None);
        assert_eq!(Arg::from(&OsString::from("profile")), Arg::None);
        assert_eq!(Arg::from(&OsString::from("profile -P")), Arg::None);
        assert_eq!(
            Arg::from(&OsString::from("-profiled")),
            Arg::Other("profiled".into())
        );
        assert_eq!(
            Arg::from(&OsString::from("-PROFILEMANAGER")),
            Arg::Other("PROFILEMANAGER".into())
        );

        assert_eq!(Arg::from(&OsString::from("--profile")), Arg::Profile);
        assert_eq!(Arg::from(&OsString::from("-profile foo")), Arg::Profile);

        assert_eq!(
            Arg::from(&OsString::from("--ProfileManager")),
            Arg::ProfileManager
        );
        assert_eq!(
            Arg::from(&OsString::from("-ProfileManager")),
            Arg::ProfileManager
        );

        // TODO: -Ptest is valid
        //assert_eq!(Arg::from(&OsString::from("-Ptest")), Arg::NamedProfile);
        assert_eq!(Arg::from(&OsString::from("-P")), Arg::NamedProfile);
        assert_eq!(Arg::from(&OsString::from("-P test")), Arg::NamedProfile);

        assert_eq!(Arg::from(&OsString::from("--foreground")), Arg::Foreground);
        assert_eq!(Arg::from(&OsString::from("-foreground")), Arg::Foreground);

        assert_eq!(Arg::from(&OsString::from("--no-remote")), Arg::NoRemote);
        assert_eq!(Arg::from(&OsString::from("-no-remote")), Arg::NoRemote);
    }

    #[test]
    fn test_get_arg_value() {
        let args = vec!["-P", "ProfileName", "--profile=/path/", "--no-remote"]
            .iter()
            .map(|x| OsString::from(x))
            .collect::<Vec<OsString>>();
        let parsed_args = parse_args(args.iter());
        assert_eq!(
            get_arg_value(parsed_args.iter(), Arg::NamedProfile),
            Some("ProfileName".into())
        );
        assert_eq!(
            get_arg_value(parsed_args.iter(), Arg::Profile),
            Some("/path/".into())
        );
        assert_eq!(get_arg_value(parsed_args.iter(), Arg::NoRemote), None);

        let args = vec!["--profile=", "-P test"]
            .iter()
            .map(|x| OsString::from(x))
            .collect::<Vec<OsString>>();
        let parsed_args = parse_args(args.iter());
        assert_eq!(
            get_arg_value(parsed_args.iter(), Arg::NamedProfile),
            Some("test".into())
        );
        assert_eq!(
            get_arg_value(parsed_args.iter(), Arg::Profile),
            Some("".into())
        );
    }
}
