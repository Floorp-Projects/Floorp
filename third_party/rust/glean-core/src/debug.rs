// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

//! # Debug options
//!
//! The debug options for Glean may be set by calling one of the `set_*` functions
//! or by setting specific environment variables.
//!
//! The environment variables will be read only once when the options are initialized.
//!
//! The possible debugging features available out of the box are:
//!
//! * **Ping logging** - logging the contents of ping requests that are correctly assembled;
//!         This may be set by calling glean.set_log_pings(value: bool)
//!         or by setting the environment variable GLEAN_LOG_PINGS="true";
//! * **Debug tagging** - Adding the X-Debug-ID header to every ping request,
//!         allowing these tagged pings to be sent to the ["Ping Debug Viewer"](https://mozilla.github.io/glean/book/dev/core/internal/debug-pings.html).
//!         This may be set by calling glean.set_debug_view_tag(value: &str)
//!         or by setting the environment variable GLEAN_DEBUG_VIEW_TAG=<some tag>;
//!
//! Bindings may implement other debugging features, e.g. sending pings on demand.

use std::env;

const GLEAN_LOG_PINGS: &str = "GLEAN_LOG_PINGS";
const GLEAN_DEBUG_VIEW_TAG: &str = "GLEAN_DEBUG_VIEW_TAG";

/// A representation of all of Glean's debug options.
#[derive(Debug)]
pub struct DebugOptions {
    /// Option to log the payload of pings that are successfully assembled into a ping request.
    pub log_pings: DebugOption<bool>,
    /// Option to add the X-Debug-ID header to every ping request.
    pub debug_view_tag: DebugOption<String>,
}

impl DebugOptions {
    pub fn new() -> Self {
        Self {
            log_pings: DebugOption::new(GLEAN_LOG_PINGS, None),
            debug_view_tag: DebugOption::new(GLEAN_DEBUG_VIEW_TAG, Some(validate_debug_view_tag)),
        }
    }
}

/// A representation of a debug option,
/// where the value can be set programmatically or come from an environment variable.
#[derive(Debug)]
pub struct DebugOption<T, F = fn(T) -> Option<T>> {
    /// The name of the environment variable related to this debug option.
    env: String,
    /// The actual value of this option.
    value: Option<T>,
    /// Optional function to validade the value parsed from the environment
    /// or passed to the `set` function.
    validation: Option<F>,
}

impl<T, F> DebugOption<T, F>
where
    T: std::str::FromStr,
    F: Fn(T) -> Option<T>,
{
    /// Create a new debug option,
    /// tries to get the initial value of the option from the environment.
    pub fn new(env: &str, validation: Option<F>) -> Self {
        let mut option = Self {
            env: env.into(),
            value: None,
            validation,
        };

        option.set_from_env();
        option
    }

    fn validate(&self, value: T) -> Option<T> {
        if let Some(f) = self.validation.as_ref() {
            f(value)
        } else {
            Some(value)
        }
    }

    fn set_from_env(&mut self) {
        match env::var(&self.env) {
            Ok(env_value) => match T::from_str(&env_value) {
                Ok(v) => {
                    self.set(v);
                }
                Err(_) => {
                    log::error!(
                        "Unable to parse debug option {}={} into {}. Ignoring.",
                        self.env,
                        env_value,
                        std::any::type_name::<T>()
                    );
                }
            },
            Err(env::VarError::NotUnicode(_)) => {
                log::error!("The value of {} is not valid unicode. Ignoring.", self.env)
            }
            // The other possible error is that the env var is not set,
            // which is not an error for us and can safely be ignored.
            Err(_) => {}
        }
    }

    /// Tries to set a value for this debug option,
    /// returns `true` if successfull.
    ///
    /// Validates the value in case a validation function is available.
    pub fn set(&mut self, value: T) -> bool {
        let validated = self.validate(value);
        if validated.is_some() {
            log::info!("Setting the debug option {}.", self.env);
            self.value = validated;
            return true;
        }
        log::info!("Invalid value for debug option {}.", self.env);
        false
    }

    /// Gets the value of this debug option.
    pub fn get(&self) -> Option<&T> {
        self.value.as_ref()
    }
}

/// The debug view tag is the value for the `X-Debug-ID` header of tagged ping requests,
/// thus is it must be a valid header value.
///
/// In other words, it must match the regex: "[a-zA-Z0-9-]{1,20}"
///
/// The regex crate isn't used here because it adds to the binary size,
/// and the Glean SDK doesn't use regular expressions anywhere else.
pub fn validate_debug_view_tag(value: String) -> Option<String> {
    if value.is_empty() {
        log::error!("Debug view tag must have at least one character.");
        return None;
    }

    let mut iter = value.chars();
    let mut count = 0;

    loop {
        match iter.next() {
            // We are done, so the whole expression is valid.
            None => return Some(value),
            // Valid characters.
            Some('-') | Some('a'..='z') | Some('A'..='Z') | Some('0'..='9') => (),
            // An invalid character
            Some(c) => {
                log::error!("Invalid character '{}' in debug view tag.", c);
                return None;
            }
        }
        count += 1;
        if count == 20 {
            log::error!("Debug view tag cannot exceed 20 characters");
            return None;
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::env;

    #[test]
    fn debug_option_is_correctly_loaded_from_env() {
        env::set_var("GLEAN_TEST_1", "test");
        let option: DebugOption<String> = DebugOption::new("GLEAN_TEST_1", None);
        assert_eq!(option.get().unwrap(), "test");
    }

    #[test]
    fn debug_option_is_correctly_validated_when_necessary() {
        fn validate(value: String) -> Option<String> {
            if value == "test" {
                Some(value)
            } else {
                None
            }
        }

        // Invalid values from the env are not set
        env::set_var("GLEAN_TEST_2", "invalid");
        let mut option: DebugOption<String> = DebugOption::new("GLEAN_TEST_2", Some(validate));
        assert!(option.get().is_none());

        // Valid values are set using the `set` function
        assert!(option.set("test".into()));
        assert_eq!(option.get().unwrap(), "test");

        // Invalid values are not set using the `set` function
        assert!(!option.set("invalid".into()));
        assert_eq!(option.get().unwrap(), "test");
    }

    #[test]
    fn validates_debug_view_tag_correctly() {
        assert!(validate_debug_view_tag("valid-value".to_string()).is_some());
        assert!(validate_debug_view_tag("-also-valid-value".to_string()).is_some());
        assert!(validate_debug_view_tag("invalid_value".to_string()).is_none());
        assert!(validate_debug_view_tag("invalid value".to_string()).is_none());
        assert!(validate_debug_view_tag("!nv@lid-val*e".to_string()).is_none());
        assert!(
            validate_debug_view_tag("invalid-value-because-way-too-long".to_string()).is_none()
        );
        assert!(validate_debug_view_tag("".to_string()).is_none());
    }
}
