use command::Parameters;
use error::{ErrorStatus, WebDriverError, WebDriverResult};
use rustc_serialize::json::{Json, ToJson};
use std::collections::BTreeMap;
use url::Url;

pub type Capabilities = BTreeMap<String, Json>;

/// Trait for objects that can be used to inspect browser capabilities
///
/// The main methods in this trait are called with a Capabilites object
/// resulting from a full set of potential capabilites for the session.
/// Given those Capabilities they return a property of the browser instance
/// that would be initiated. In many cases this will be independent of the
/// input, but in the case of e.g. browser version, it might depend on a
/// path to the binary provided as a capability.
pub trait BrowserCapabilities {
    /// Set up the Capabilites object
    ///
    /// Typically used to create any internal caches
    fn init(&mut self, &Capabilities);

    /// Name of the browser
    fn browser_name(&mut self, &Capabilities) -> WebDriverResult<Option<String>>;
    /// Version number of the browser
    fn browser_version(&mut self, &Capabilities) -> WebDriverResult<Option<String>>;
    /// Compare actual browser version to that provided in a version specifier
    ///
    /// Parameters are the actual browser version and the comparison string,
    /// respectively. The format of the comparison string is implementation-defined.
    fn compare_browser_version(&mut self, version: &str, comparison: &str) -> WebDriverResult<bool>;
    /// Name of the platform/OS
    fn platform_name(&mut self, &Capabilities) -> WebDriverResult<Option<String>>;
    /// Whether insecure certificates are supported
    fn accept_insecure_certs(&mut self, &Capabilities) -> WebDriverResult<bool>;

    fn accept_proxy(&mut self, proxy_settings: &BTreeMap<String, Json>, &Capabilities) -> WebDriverResult<bool>;

    /// Type check custom properties
    ///
    /// Check that custom properties containing ":" have the correct data types.
    /// Properties that are unrecognised must be ignored i.e. return without
    /// error.
    fn validate_custom(&self, name: &str, value: &Json) -> WebDriverResult<()>;
    /// Check if custom properties are accepted capabilites
    ///
    /// Check that custom properties containing ":" are compatible with
    /// the implementation.
    fn accept_custom(&mut self, name: &str, value: &Json, merged: &Capabilities) -> WebDriverResult<bool>;
}

/// Trait to abstract over various version of the new session parameters
///
/// This trait is expected to be implemented on objects holding the capabilities
/// from a new session command.
pub trait CapabilitiesMatching {
    /// Match the BrowserCapabilities against some candidate capabilites
    ///
    /// Takes a BrowserCapabilites object and returns a set of capabilites that
    /// are valid for that browser, if any, or None if there are no matching
    /// capabilities.
    fn match_browser<T: BrowserCapabilities>(&self, browser_capabilities: &mut T)
                                             -> WebDriverResult<Option<Capabilities>>;
}

#[derive(Debug, PartialEq)]
pub struct SpecNewSessionParameters {
    pub alwaysMatch: Capabilities,
    pub firstMatch: Vec<Capabilities>,
}

impl SpecNewSessionParameters {
    fn validate<T: BrowserCapabilities>(&self,
                                        mut capabilities: Capabilities,
                                        browser_capabilities: &T) -> WebDriverResult<Capabilities> {
        // Filter out entries with the value `null`
        let null_entries = capabilities
            .iter()
            .filter(|&(_, ref value)| **value == Json::Null)
            .map(|(k, _)| k.clone())
            .collect::<Vec<String>>();
        for key in null_entries {
            capabilities.remove(&key);
        }

        for (key, value) in capabilities.iter() {
            match &**key {
                "acceptInsecureCerts" => if !value.is_boolean() {
                        return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                       "acceptInsecureCerts was not a boolean"))
                    },
                x @ "browserName" |
                x @ "browserVersion" |
                x @ "platformName" => if !value.is_string() {
                        return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                       format!("{} was not a boolean", x)))
                    },
                "pageLoadStrategy" => {
                    try!(SpecNewSessionParameters::validate_page_load_strategy(value))
                }
                "proxy" => {
                    try!(SpecNewSessionParameters::validate_proxy(value))
                },
                "timeouts" => {
                    try!(SpecNewSessionParameters::validate_timeouts(value))
                },
                "unhandledPromptBehavior" => {
                    try!(SpecNewSessionParameters::validate_unhandled_prompt_behaviour(value))
                }
                x => {
                    if !x.contains(":") {
                        return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                       format!("{} was not a the name of a known capability or a valid extension capability", x)))
                    } else {
                        try!(browser_capabilities.validate_custom(x, value));
                    }
                }
            }
        }
        Ok(capabilities)
    }

    fn validate_page_load_strategy(value: &Json) -> WebDriverResult<()> {
        match value {
            &Json::String(ref x) => {
                match &**x {
                    "normal" |
                    "eager" |
                    "none" => {},
                    x => {
                        return Err(WebDriverError::new(
                            ErrorStatus::InvalidArgument,
                            format!("\"{}\" not a valid page load strategy", x)))
                    }
                }
            }
            _ => return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                "pageLoadStrategy was not a string"))
        }
        Ok(())
    }

    fn validate_proxy(proxy_value: &Json) -> WebDriverResult<()> {
        let obj = try_opt!(proxy_value.as_object(),
                           ErrorStatus::InvalidArgument,
                           "proxy was not an object");
        for (key, value) in obj.iter() {
            match &**key {
                "proxyType" => match value.as_string() {
                    Some("pac") |
                    Some("direct") |
                    Some("autodetect") |
                    Some("system") |
                    Some("manual") => {},
                    Some(x) => return Err(WebDriverError::new(
                        ErrorStatus::InvalidArgument,
                        format!("{} was not a valid proxyType value", x))),
                    None => return Err(WebDriverError::new(
                        ErrorStatus::InvalidArgument,
                        "proxyType value was not a string")),
                },
                "proxyAutoconfigUrl" => match value.as_string() {
                    Some(x) => {
                        try!(Url::parse(x).or(Err(WebDriverError::new(
                            ErrorStatus::InvalidArgument,
                            "proxyAutoconfigUrl was not a valid url"))));
                    },
                    None => return Err(WebDriverError::new(
                        ErrorStatus::InvalidArgument,
                        "proxyAutoconfigUrl was not a string"
                    ))
                },
                "ftpProxy" => try!(SpecNewSessionParameters::validate_host(value)),
                "httpProxy" => try!(SpecNewSessionParameters::validate_host(value)),
                "sslProxy" => try!(SpecNewSessionParameters::validate_host(value)),
                "socksProxy" => try!(SpecNewSessionParameters::validate_host(value)),
                "socksVersion" => if !value.is_number() {
                    return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                   "socksVersion was not a number"))
                },
                "socksUsername" => if !value.is_string() {
                    return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                   "socksUsername was not a string"))
                },
                "socksPassword" => if !value.is_string() {
                    return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                   "socksPassword was not a string"))
                },
                x => return Err(WebDriverError::new(
                    ErrorStatus::InvalidArgument,
                    format!("{} was not a valid proxy configuration capability", x)))
            }
        }
        Ok(())
    }

    /// Validate whether a named capability is JSON value is a string containing a host
    /// and possible port
    fn validate_host(value: &Json) -> WebDriverResult<()> {
        match value.as_string() {
            Some(host) => {
                if host.contains("://") {
                    return Err(WebDriverError::new(
                        ErrorStatus::InvalidArgument,
                        format!("{} contains a scheme", host)));
                }

                // Temporarily add a scheme so the host can be parsed as URL
                let s = String::from(format!("http://{}", host));
                let url = try!(Url::parse(s.as_str()).or(Err(WebDriverError::new(
                    ErrorStatus::InvalidArgument,
                    format!("{} is not a valid host", host)))));

                if url.username() != "" ||
                    url.password() != None ||
                    url.path() != "/" ||
                    url.query() != None ||
                    url.fragment() != None {
                        return Err(WebDriverError::new(
                            ErrorStatus::InvalidArgument,
                            format!("{} was not of the form host[:port]", host)));
                    }
            },
            None => return Err(WebDriverError::new(
                ErrorStatus::InvalidArgument,
                format!("{} was not a string", value)
            ))
        }
        Ok(())
    }

    fn validate_timeouts(value: &Json) -> WebDriverResult<()> {
        let obj = try_opt!(value.as_object(),
                           ErrorStatus::InvalidArgument,
                           "timeouts capability was not an object");
        for (key, value) in obj.iter() {
            match &**key {
                x @ "script" |
                x @ "pageLoad" |
                x @ "implicit" => {
                    let timeout = try_opt!(value.as_i64(),
                                           ErrorStatus::InvalidArgument,
                                           format!("{} timeouts value was not an integer", x));
                    if timeout < 0 {
                        return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                       format!("{} timeouts value was negative", x)))
                    }
                },
                x => return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                    format!("{} was not a valid timeouts capability", x)))
            }
        }
        Ok(())
    }

    fn validate_unhandled_prompt_behaviour(value: &Json) -> WebDriverResult<()> {
        let behaviour = try_opt!(value.as_string(),
                                 ErrorStatus::InvalidArgument,
                                 "unhandledPromptBehavior capability was not a string");
        match behaviour {
            "dismiss" |
            "accept" => {},
            x => return Err(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                format!("{} was not a valid unhandledPromptBehavior value", x)))        }
        Ok(())
    }
}

impl Parameters for SpecNewSessionParameters {
    fn from_json(body: &Json) -> WebDriverResult<SpecNewSessionParameters> {
        let data = try_opt!(body.as_object(),
                            ErrorStatus::UnknownError,
                            "Message body was not an object");

        let capabilities = try_opt!(
            try_opt!(data.get("capabilities"),
                     ErrorStatus::InvalidArgument,
                     "Missing 'capabilities' parameter").as_object(),
            ErrorStatus::InvalidArgument,
                     "'capabilities' parameter is not an object");

        let default_always_match = Json::Object(Capabilities::new());
        let always_match = try_opt!(capabilities.get("alwaysMatch")
                                   .unwrap_or(&default_always_match)
                                   .as_object(),
                                   ErrorStatus::InvalidArgument,
                                   "'alwaysMatch' parameter is not an object");
        let default_first_matches = Json::Array(vec![]);
        let first_matches = try!(
            try_opt!(capabilities.get("firstMatch")
                     .unwrap_or(&default_first_matches)
                     .as_array(),
                     ErrorStatus::InvalidArgument,
                     "'firstMatch' parameter is not an array")
                .iter()
                .map(|x| x.as_object()
                     .map(|x| x.clone())
                     .ok_or(WebDriverError::new(ErrorStatus::InvalidArgument,
                                                "'firstMatch' entry is not an object")))
                .collect::<WebDriverResult<Vec<Capabilities>>>());

        return Ok(SpecNewSessionParameters {
            alwaysMatch: always_match.clone(),
            firstMatch: first_matches
        });
    }
}

impl ToJson for SpecNewSessionParameters {
    fn to_json(&self) -> Json {
        let mut body = BTreeMap::new();
        let mut capabilities = BTreeMap::new();
        capabilities.insert("alwaysMatch".into(), self.alwaysMatch.to_json());
        capabilities.insert("firstMatch".into(), self.firstMatch.to_json());
        body.insert("capabilities".into(), capabilities.to_json());
        Json::Object(body)
    }
}

impl CapabilitiesMatching for SpecNewSessionParameters {
    fn match_browser<T: BrowserCapabilities>(&self, browser_capabilities: &mut T)
                                             -> WebDriverResult<Option<Capabilities>> {
        let default = vec![BTreeMap::new()];
        let capabilities_list = if self.firstMatch.len() > 0 {
            &self.firstMatch
        } else {
            &default
        };

        let merged_capabilities = try!(capabilities_list
            .iter()
            .map(|first_match_entry| {
                if first_match_entry.keys().any(|k| {
                    self.alwaysMatch.contains_key(k)
                }) {
                    return Err(WebDriverError::new(
                        ErrorStatus::InvalidArgument,
                        "'firstMatch' key shadowed a value in 'alwaysMatch'"));
                }
                let mut merged = self.alwaysMatch.clone();
                merged.append(&mut first_match_entry.clone());
                Ok(merged)
            })
            .map(|merged| merged.and_then(|x| self.validate(x, browser_capabilities)))
            .collect::<WebDriverResult<Vec<Capabilities>>>());

        let selected = merged_capabilities
            .iter()
            .filter_map(|merged| {
                browser_capabilities.init(merged);

                for (key, value) in merged.iter() {
                    match &**key {
                        "browserName" => {
                            let browserValue = browser_capabilities
                                .browser_name(merged)
                                .ok()
                                .and_then(|x| x);

                            if value.as_string() != browserValue.as_ref().map(|x| &**x) {
                                    return None;
                            }
                        },
                        "browserVersion" => {
                            let browserValue = browser_capabilities
                                .browser_version(merged)
                                .ok()
                                .and_then(|x| x);
                            // We already validated this was a string
                            let version_cond = value.as_string().unwrap_or("");
                            if let Some(version) = browserValue {
                                if !browser_capabilities
                                    .compare_browser_version(&*version, version_cond)
                                    .unwrap_or(false) {
                                        return None;
                                    }
                            } else {
                                return None
                            }
                        },
                        "platformName" => {
                            let browserValue = browser_capabilities
                                .platform_name(merged)
                                .ok()
                                .and_then(|x| x);
                            if value.as_string() != browserValue.as_ref().map(|x| &**x) {
                                return None;
                            }
                        }
                        "acceptInsecureCerts" => {
                            if value.as_boolean().unwrap_or(false) &&
                                !browser_capabilities
                                .accept_insecure_certs(merged)
                                .unwrap_or(false) {
                                return None;
                            }
                        },
                        "proxy" => {
                            let default = BTreeMap::new();
                            let proxy = value.as_object().unwrap_or(&default);
                            if !browser_capabilities.accept_proxy(&proxy,
                                                                  merged)
                                .unwrap_or(false) {
                                return None
                            }
                        },
                        name => {
                            if name.contains(":") {
                                if !browser_capabilities
                                    .accept_custom(name, value, merged)
                                    .unwrap_or(false) {
                                        return None
                                    }
                            } else {
                                // Accept the capability
                            }
                        }
                    }
                }

                return Some(merged)
            })
            .next()
            .map(|x| x.clone());
            Ok(selected)
    }
}

#[derive(Debug, PartialEq)]
pub struct LegacyNewSessionParameters {
    pub desired: Capabilities,
    pub required: Capabilities,
}

impl CapabilitiesMatching for LegacyNewSessionParameters {
    fn match_browser<T: BrowserCapabilities>(&self, browser_capabilities: &mut T)
                                             -> WebDriverResult<Option<Capabilities>> {
        /* For now don't do anything much, just merge the
        desired and required and return the merged list. */

        let mut capabilities: Capabilities = BTreeMap::new();
        self.required.iter()
            .chain(self.desired.iter())
            .fold(&mut capabilities,
                  |mut caps, (key, value)| {
                      if !caps.contains_key(key) {
                          caps.insert(key.clone(), value.clone());
                      }
                      caps});
        browser_capabilities.init(&capabilities);
        Ok(Some(capabilities))
    }
}

impl Parameters for LegacyNewSessionParameters {
    fn from_json(body: &Json) -> WebDriverResult<LegacyNewSessionParameters> {
        let data = try_opt!(body.as_object(),
                            ErrorStatus::UnknownError,
                            "Message body was not an object");

        let desired_capabilities =
            if let Some(capabilities) = data.get("desiredCapabilities") {
                try_opt!(capabilities.as_object(),
                         ErrorStatus::InvalidArgument,
                         "'desiredCapabilities' parameter is not an object").clone()
            } else {
                BTreeMap::new()
            };

        let required_capabilities =
            if let Some(capabilities) = data.get("requiredCapabilities") {
                try_opt!(capabilities.as_object(),
                         ErrorStatus::InvalidArgument,
                         "'requiredCapabilities' parameter is not an object").clone()
            } else {
                BTreeMap::new()
            };

        Ok(LegacyNewSessionParameters {
            desired: desired_capabilities,
            required: required_capabilities
        })
    }
}

impl ToJson for LegacyNewSessionParameters {
    fn to_json(&self) -> Json {
        let mut data = BTreeMap::new();
        data.insert("desiredCapabilities".to_owned(), self.desired.to_json());
        data.insert("requiredCapabilities".to_owned(), self.required.to_json());
        Json::Object(data)
    }
}

#[cfg(test)]
mod tests {
    use rustc_serialize::json::Json;
    use super::{WebDriverResult, SpecNewSessionParameters};

    fn validate_host(value: &str) -> WebDriverResult<()> {
        SpecNewSessionParameters::validate_host(&Json::String(value.into()))
    }

    #[test]
    fn test_validate_host() {
        validate_host("127.0.0.1").unwrap();
        validate_host("127.0.0.1:").unwrap();
        validate_host("127.0.0.1:3128").unwrap();
        validate_host("[2001:db8::1]").unwrap();
        validate_host("[2001:db8::1]:3128").unwrap();
        validate_host("localhost").unwrap();
        validate_host("localhost:3128").unwrap();
        validate_host("example.org").unwrap();
        validate_host("example.org:3128").unwrap();

        assert!(validate_host("http://example.org").is_err());  // existing scheme
        assert!(validate_host("example.org:-1").is_err());  // invalid port
        assert!(validate_host("2001:db8::1").is_err());  // missing brackets
    }
}
