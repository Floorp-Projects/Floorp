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
//! * **Source tagging** - Adding the X-Source-Tags header to every ping request,
//!         allowing pings to be tagged with custom labels.
//!         This may be set by calling glean.set_source_tags(value: Vec<String>)
//!         or by setting the environment variable GLEAN_SOURCE_TAGS=<some, tags>;
//!
//! Bindings may implement other debugging features, e.g. sending pings on demand.

use std::env;

const GLEAN_LOG_PINGS: &str = "GLEAN_LOG_PINGS";
const GLEAN_DEBUG_VIEW_TAG: &str = "GLEAN_DEBUG_VIEW_TAG";
const GLEAN_SOURCE_TAGS: &str = "GLEAN_SOURCE_TAGS";
const GLEAN_MAX_SOURCE_TAGS: usize = 5;

/// A representation of all of Glean's debug options.
pub struct DebugOptions {
    /// Option to log the payload of pings that are successfully assembled into a ping request.
    pub log_pings: DebugOption<bool>,
    /// Option to add the X-Debug-ID header to every ping request.
    pub debug_view_tag: DebugOption<String>,
    /// Option to add the X-Source-Tags header to ping requests. This will allow the data
    /// consumers to classify data depending on the applied tags.
    pub source_tags: DebugOption<Vec<String>>,
}

impl std::fmt::Debug for DebugOptions {
    fn fmt(&self, fmt: &mut std::fmt::Formatter) -> std::fmt::Result {
        fmt.debug_struct("DebugOptions")
            .field("log_pings", &self.log_pings.get())
            .field("debug_view_tag", &self.debug_view_tag.get())
            .field("source_tags", &self.source_tags.get())
            .finish()
    }
}

impl DebugOptions {
    pub fn new() -> Self {
        Self {
            log_pings: DebugOption::new(GLEAN_LOG_PINGS, get_bool_from_str, None),
            debug_view_tag: DebugOption::new(GLEAN_DEBUG_VIEW_TAG, Some, Some(validate_tag)),
            source_tags: DebugOption::new(
                GLEAN_SOURCE_TAGS,
                tokenize_string,
                Some(validate_source_tags),
            ),
        }
    }
}

/// A representation of a debug option,
/// where the value can be set programmatically or come from an environment variable.
#[derive(Debug)]
pub struct DebugOption<T, E = fn(String) -> Option<T>, V = fn(&T) -> bool> {
    /// The name of the environment variable related to this debug option.
    env: String,
    /// The actual value of this option.
    value: Option<T>,
    /// Function to extract the data of type `T` from a `String`, used when
    /// extracting data from the environment.
    extraction: E,
    /// Optional function to validate the value parsed from the environment
    /// or passed to the `set` function.
    validation: Option<V>,
}

impl<T, E, V> DebugOption<T, E, V>
where
    T: Clone,
    E: Fn(String) -> Option<T>,
    V: Fn(&T) -> bool,
{
    /// Creates a new debug option.
    ///
    /// Tries to get the initial value of the option from the environment.
    pub fn new(env: &str, extraction: E, validation: Option<V>) -> Self {
        let mut option = Self {
            env: env.into(),
            value: None,
            extraction,
            validation,
        };

        option.set_from_env();
        option
    }

    fn validate(&self, value: &T) -> bool {
        if let Some(f) = self.validation.as_ref() {
            f(value)
        } else {
            true
        }
    }

    fn set_from_env(&mut self) {
        let extract = &self.extraction;
        match env::var(&self.env) {
            Ok(env_value) => match extract(env_value.clone()) {
                Some(v) => {
                    self.set(v);
                }
                None => {
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

    /// Tries to set a value for this debug option.
    ///
    /// Validates the value in case a validation function is available.
    ///
    /// # Returns
    ///
    /// Whether the option passed validation and was succesfully set.
    pub fn set(&mut self, value: T) -> bool {
        let validated = self.validate(&value);
        if validated {
            log::info!("Setting the debug option {}.", self.env);
            self.value = Some(value);
            return true;
        }
        log::error!("Invalid value for debug option {}.", self.env);
        false
    }

    /// Gets the value of this debug option.
    pub fn get(&self) -> Option<&T> {
        self.value.as_ref()
    }
}

fn get_bool_from_str(value: String) -> Option<bool> {
    std::str::FromStr::from_str(&value).ok()
}

fn tokenize_string(value: String) -> Option<Vec<String>> {
    let trimmed = value.trim();
    if trimmed.is_empty() {
        return None;
    }

    Some(trimmed.split(',').map(|s| s.trim().to_string()).collect())
}

/// A tag is the value used in both the `X-Debug-ID` and `X-Source-Tags` headers
/// of tagged ping requests, thus is it must be a valid header value.
///
/// In other words, it must match the regex: "[a-zA-Z0-9-]{1,20}"
///
/// The regex crate isn't used here because it adds to the binary size,
/// and the Glean SDK doesn't use regular expressions anywhere else.
#[allow(clippy::ptr_arg)]
fn validate_tag(value: &String) -> bool {
    if value.is_empty() {
        log::error!("A tag must have at least one character.");
        return false;
    }

    let mut iter = value.chars();
    let mut count = 0;

    loop {
        match iter.next() {
            // We are done, so the whole expression is valid.
            None => return true,
            // Valid characters.
            Some('-') | Some('a'..='z') | Some('A'..='Z') | Some('0'..='9') => (),
            // An invalid character
            Some(c) => {
                log::error!("Invalid character '{}' in the tag.", c);
                return false;
            }
        }
        count += 1;
        if count == 20 {
            log::error!("A tag cannot exceed 20 characters.");
            return false;
        }
    }
}

/// Validate the list of source tags.
///
/// This builds upon the existing `validate_tag` function, since all the
/// tags should respect the same rules to make the pipeline happy.
#[allow(clippy::ptr_arg)]
fn validate_source_tags(tags: &Vec<String>) -> bool {
    if tags.is_empty() {
        return false;
    }

    if tags.len() > GLEAN_MAX_SOURCE_TAGS {
        log::error!(
            "A list of tags cannot contain more than {} elements.",
            GLEAN_MAX_SOURCE_TAGS
        );
        return false;
    }

    if tags.iter().any(|s| s.starts_with("glean")) {
        log::error!("Tags starting with `glean` are reserved and must not be used.");
        return false;
    }

    tags.iter().all(|x| validate_tag(x))
}

#[cfg(test)]
mod test {
    use super::*;
    use std::env;

    #[test]
    fn debug_option_is_correctly_loaded_from_env() {
        env::set_var("GLEAN_TEST_1", "test");
        let option: DebugOption<String> = DebugOption::new("GLEAN_TEST_1", Some, None);
        assert_eq!(option.get().unwrap(), "test");
    }

    #[test]
    fn debug_option_is_correctly_validated_when_necessary() {
        #[allow(clippy::ptr_arg)]
        fn validate(value: &String) -> bool {
            value == "test"
        }

        // Invalid values from the env are not set
        env::set_var("GLEAN_TEST_2", "invalid");
        let mut option: DebugOption<String> =
            DebugOption::new("GLEAN_TEST_2", Some, Some(validate));
        assert!(option.get().is_none());

        // Valid values are set using the `set` function
        assert!(option.set("test".into()));
        assert_eq!(option.get().unwrap(), "test");

        // Invalid values are not set using the `set` function
        assert!(!option.set("invalid".into()));
        assert_eq!(option.get().unwrap(), "test");
    }

    #[test]
    fn tokenize_string_splits_correctly() {
        // Valid list is properly tokenized and spaces are trimmed.
        assert_eq!(
            Some(vec!["test1".to_string(), "test2".to_string()]),
            tokenize_string("    test1,        test2  ".to_string())
        );

        // Empty strings return no item.
        assert_eq!(None, tokenize_string("".to_string()));
    }

    #[test]
    fn validates_tag_correctly() {
        assert!(validate_tag(&"valid-value".to_string()));
        assert!(validate_tag(&"-also-valid-value".to_string()));
        assert!(!validate_tag(&"invalid_value".to_string()));
        assert!(!validate_tag(&"invalid value".to_string()));
        assert!(!validate_tag(&"!nv@lid-val*e".to_string()));
        assert!(!validate_tag(
            &"invalid-value-because-way-too-long".to_string()
        ));
        assert!(!validate_tag(&"".to_string()));
    }

    #[test]
    fn validates_source_tags_correctly() {
        // Empty tags.
        assert!(!validate_source_tags(&vec!["".to_string()]));
        // Too many tags.
        assert!(!validate_source_tags(&vec![
            "1".to_string(),
            "2".to_string(),
            "3".to_string(),
            "4".to_string(),
            "5".to_string(),
            "6".to_string()
        ]));
        // Invalid tags.
        assert!(!validate_source_tags(&vec!["!nv@lid-val*e".to_string()]));
        assert!(!validate_source_tags(&vec![
            "glean-test1".to_string(),
            "test2".to_string()
        ]));
    }
}
