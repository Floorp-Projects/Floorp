/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::logging;
use hyper::Method;
use serde::de::{self, Deserialize, Deserializer};
use serde_json::{self, Value};
use std::env;
use std::fs::File;
use std::io::prelude::*;
use uuid::Uuid;
use webdriver::command::{WebDriverCommand, WebDriverExtensionCommand};
use webdriver::error::WebDriverResult;
use webdriver::httpapi::WebDriverExtensionRoute;
use webdriver::Parameters;

pub fn extension_routes() -> Vec<(Method, &'static str, GeckoExtensionRoute)> {
    return vec![
        (
            Method::GET,
            "/session/{sessionId}/moz/context",
            GeckoExtensionRoute::GetContext,
        ),
        (
            Method::POST,
            "/session/{sessionId}/moz/context",
            GeckoExtensionRoute::SetContext,
        ),
        (
            Method::POST,
            "/session/{sessionId}/moz/addon/install",
            GeckoExtensionRoute::InstallAddon,
        ),
        (
            Method::POST,
            "/session/{sessionId}/moz/addon/uninstall",
            GeckoExtensionRoute::UninstallAddon,
        ),
        (
            Method::GET,
            "/session/{sessionId}/moz/screenshot/full",
            GeckoExtensionRoute::TakeFullScreenshot,
        ),
    ];
}

#[derive(Clone, PartialEq)]
pub enum GeckoExtensionRoute {
    GetContext,
    SetContext,
    InstallAddon,
    UninstallAddon,
    TakeFullScreenshot,
}

impl WebDriverExtensionRoute for GeckoExtensionRoute {
    type Command = GeckoExtensionCommand;

    fn command(
        &self,
        _params: &Parameters,
        body_data: &Value,
    ) -> WebDriverResult<WebDriverCommand<GeckoExtensionCommand>> {
        use self::GeckoExtensionRoute::*;

        let command = match *self {
            GetContext => GeckoExtensionCommand::GetContext,
            SetContext => {
                GeckoExtensionCommand::SetContext(serde_json::from_value(body_data.clone())?)
            }
            InstallAddon => {
                GeckoExtensionCommand::InstallAddon(serde_json::from_value(body_data.clone())?)
            }
            UninstallAddon => {
                GeckoExtensionCommand::UninstallAddon(serde_json::from_value(body_data.clone())?)
            }
            TakeFullScreenshot => GeckoExtensionCommand::TakeFullScreenshot,
        };

        Ok(WebDriverCommand::Extension(command))
    }
}

#[derive(Clone)]
pub enum GeckoExtensionCommand {
    GetContext,
    SetContext(GeckoContextParameters),
    InstallAddon(AddonInstallParameters),
    UninstallAddon(AddonUninstallParameters),
    TakeFullScreenshot,
}

impl WebDriverExtensionCommand for GeckoExtensionCommand {
    fn parameters_json(&self) -> Option<Value> {
        use self::GeckoExtensionCommand::*;
        match self {
            GetContext => None,
            InstallAddon(x) => Some(serde_json::to_value(x).unwrap()),
            SetContext(x) => Some(serde_json::to_value(x).unwrap()),
            UninstallAddon(x) => Some(serde_json::to_value(x).unwrap()),
            TakeFullScreenshot => None,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize)]
pub struct AddonInstallParameters {
    pub path: String,
    pub temporary: Option<bool>,
}

impl<'de> Deserialize<'de> for AddonInstallParameters {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        #[derive(Debug, Deserialize)]
        #[serde(deny_unknown_fields)]
        struct Base64 {
            addon: String,
            temporary: Option<bool>,
        }

        #[derive(Debug, Deserialize)]
        #[serde(deny_unknown_fields)]
        struct Path {
            path: String,
            temporary: Option<bool>,
        }

        #[derive(Debug, Deserialize)]
        #[serde(untagged)]
        enum Helper {
            Base64(Base64),
            Path(Path),
        }

        let params = match Helper::deserialize(deserializer)? {
            Helper::Path(ref mut data) => AddonInstallParameters {
                path: data.path.clone(),
                temporary: data.temporary,
            },
            Helper::Base64(ref mut data) => {
                let content = base64::decode(&data.addon).map_err(de::Error::custom)?;

                let path = env::temp_dir()
                    .as_path()
                    .join(format!("addon-{}.xpi", Uuid::new_v4()));
                let mut xpi_file = File::create(&path).map_err(de::Error::custom)?;
                xpi_file
                    .write(content.as_slice())
                    .map_err(de::Error::custom)?;

                let path = match path.to_str() {
                    Some(path) => path.to_string(),
                    None => return Err(de::Error::custom("could not write addon to file")),
                };

                AddonInstallParameters {
                    path,
                    temporary: data.temporary,
                }
            }
        };

        Ok(params)
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct AddonUninstallParameters {
    pub id: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum GeckoContext {
    Content,
    Chrome,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct GeckoContextParameters {
    pub context: GeckoContext,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct XblLocatorParameters {
    pub name: String,
    pub value: String,
}

#[derive(Default, Debug, PartialEq)]
pub struct LogOptions {
    pub level: Option<logging::Level>,
}

#[cfg(test)]
mod tests {
    use serde_json::json;

    use super::*;
    use crate::test::assert_de;

    #[test]
    fn test_json_addon_install_parameters_invalid() {
        assert!(serde_json::from_str::<AddonInstallParameters>("").is_err());
        assert!(serde_json::from_value::<AddonInstallParameters>(json!(null)).is_err());
        assert!(serde_json::from_value::<AddonInstallParameters>(json!({})).is_err());
    }

    #[test]
    fn test_json_addon_install_parameters_with_path_and_temporary() {
        let params = AddonInstallParameters {
            path: "/path/to.xpi".to_string(),
            temporary: Some(true),
        };
        assert_de(&params, json!({"path": "/path/to.xpi", "temporary": true}));
    }

    #[test]
    fn test_json_addon_install_parameters_with_path() {
        let params = AddonInstallParameters {
            path: "/path/to.xpi".to_string(),
            temporary: None,
        };
        assert_de(&params, json!({"path": "/path/to.xpi"}));
    }

    #[test]
    fn test_json_addon_install_parameters_with_path_invalid_type() {
        let json = json!({"path": true, "temporary": true});
        assert!(serde_json::from_value::<AddonInstallParameters>(json).is_err());
    }

    #[test]
    fn test_json_addon_install_parameters_with_path_and_temporary_invalid_type() {
        let json = json!({"path": "/path/to.xpi", "temporary": "foo"});
        assert!(serde_json::from_value::<AddonInstallParameters>(json).is_err());
    }

    #[test]
    fn test_json_addon_install_parameters_with_addon() {
        let json = json!({"addon": "aGVsbG8=", "temporary": true});
        let data = serde_json::from_value::<AddonInstallParameters>(json).unwrap();

        assert_eq!(data.temporary, Some(true));
        let mut file = File::open(data.path).unwrap();
        let mut contents = String::new();
        file.read_to_string(&mut contents).unwrap();
        assert_eq!(contents, "hello");
    }

    #[test]
    fn test_json_addon_install_parameters_with_addon_only() {
        let json = json!({"addon": "aGVsbG8="});
        let data = serde_json::from_value::<AddonInstallParameters>(json).unwrap();

        assert_eq!(data.temporary, None);
        let mut file = File::open(data.path).unwrap();
        let mut contents = String::new();
        file.read_to_string(&mut contents).unwrap();
        assert_eq!(contents, "hello");
    }

    #[test]
    fn test_json_addon_install_parameters_with_addon_invalid_type() {
        let json = json!({"addon": true, "temporary": true});
        assert!(serde_json::from_value::<AddonInstallParameters>(json).is_err());
    }

    #[test]
    fn test_json_addon_install_parameters_with_addon_and_temporary_invalid_type() {
        let json = json!({"addon": "aGVsbG8=", "temporary": "foo"});
        assert!(serde_json::from_value::<AddonInstallParameters>(json).is_err());
    }

    #[test]
    fn test_json_install_parameters_with_temporary_only() {
        let json = json!({"temporary": true});
        assert!(serde_json::from_value::<AddonInstallParameters>(json).is_err());
    }

    #[test]
    fn test_json_addon_install_parameters_with_both_path_and_addon() {
        let json = json!({
            "path": "/path/to.xpi",
            "addon": "aGVsbG8=",
            "temporary": true,
        });
        assert!(serde_json::from_value::<AddonInstallParameters>(json).is_err());
    }

    #[test]
    fn test_json_addon_uninstall_parameters_invalid() {
        assert!(serde_json::from_str::<AddonUninstallParameters>("").is_err());
        assert!(serde_json::from_value::<AddonUninstallParameters>(json!(null)).is_err());
        assert!(serde_json::from_value::<AddonUninstallParameters>(json!({})).is_err());
    }

    #[test]
    fn test_json_addon_uninstall_parameters() {
        let params = AddonUninstallParameters {
            id: "foo".to_string(),
        };
        assert_de(&params, json!({"id": "foo"}));
    }

    #[test]
    fn test_json_addon_uninstall_parameters_id_invalid_type() {
        let json = json!({"id": true});
        assert!(serde_json::from_value::<AddonUninstallParameters>(json).is_err());
    }

    #[test]
    fn test_json_gecko_context_parameters_content() {
        let params = GeckoContextParameters {
            context: GeckoContext::Content,
        };
        assert_de(&params, json!({"context": "content"}));
    }

    #[test]
    fn test_json_gecko_context_parameters_chrome() {
        let params = GeckoContextParameters {
            context: GeckoContext::Chrome,
        };
        assert_de(&params, json!({"context": "chrome"}));
    }

    #[test]
    fn test_json_gecko_context_parameters_context_invalid() {
        type P = GeckoContextParameters;
        assert!(serde_json::from_value::<P>(json!({})).is_err());
        assert!(serde_json::from_value::<P>(json!({ "context": null })).is_err());
        assert!(serde_json::from_value::<P>(json!({"context": "foo"})).is_err());
    }
}
