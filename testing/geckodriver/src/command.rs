use base64;
use crate::logging;
use hyper::Method;
use regex::Captures;
use serde::de::{self, Deserialize, Deserializer};
use serde_json::{self, Value};
use std::env;
use std::fs::File;
use std::io::prelude::*;
use uuid::Uuid;
use webdriver::command::{WebDriverCommand, WebDriverExtensionCommand};
use webdriver::common::WebElement;
use webdriver::error::{ErrorStatus, WebDriverError, WebDriverResult};
use webdriver::httpapi::WebDriverExtensionRoute;

pub const CHROME_ELEMENT_KEY: &'static str = "chromeelement-9fc5-4b51-a3c8-01716eedeb04";
pub const LEGACY_ELEMENT_KEY: &'static str = "ELEMENT";

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
            "/session/{sessionId}/moz/xbl/{elementId}/anonymous_children",
            GeckoExtensionRoute::XblAnonymousChildren,
        ),
        (
            Method::POST,
            "/session/{sessionId}/moz/xbl/{elementId}/anonymous_by_attribute",
            GeckoExtensionRoute::XblAnonymousByAttribute,
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
    XblAnonymousChildren,
    XblAnonymousByAttribute,
    InstallAddon,
    UninstallAddon,
    TakeFullScreenshot,
}

impl WebDriverExtensionRoute for GeckoExtensionRoute {
    type Command = GeckoExtensionCommand;

    fn command(
        &self,
        params: &Captures,
        body_data: &Value,
    ) -> WebDriverResult<WebDriverCommand<GeckoExtensionCommand>> {
        use self::GeckoExtensionRoute::*;

        let command = match *self {
            GetContext => GeckoExtensionCommand::GetContext,
            SetContext => {
                GeckoExtensionCommand::SetContext(serde_json::from_value(body_data.clone())?)
            }
            XblAnonymousChildren => {
                let element_id = try_opt!(
                    params.name("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                let element = WebElement::new(element_id.as_str().to_string());
                GeckoExtensionCommand::XblAnonymousChildren(element)
            }
            XblAnonymousByAttribute => {
                let element_id = try_opt!(
                    params.name("elementId"),
                    ErrorStatus::InvalidArgument,
                    "Missing elementId parameter"
                );
                GeckoExtensionCommand::XblAnonymousByAttribute(
                    WebElement::new(element_id.as_str().into()),
                    serde_json::from_value(body_data.clone())?,
                )
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

#[derive(Clone, PartialEq)]
pub enum GeckoExtensionCommand {
    GetContext,
    SetContext(GeckoContextParameters),
    XblAnonymousChildren(WebElement),
    XblAnonymousByAttribute(WebElement, XblLocatorParameters),
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
            XblAnonymousByAttribute(_, x) => Some(serde_json::to_value(x).unwrap()),
            XblAnonymousChildren(_) => None,
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
        };

        #[derive(Debug, Deserialize)]
        #[serde(deny_unknown_fields)]
        struct Path {
            path: String,
            temporary: Option<bool>,
        };

        #[derive(Debug, Deserialize)]
        #[serde(untagged)]
        enum Helper {
            Base64(Base64),
            Path(Path),
        };

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
                    path: path,
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

#[derive(Default, Debug)]
pub struct LogOptions {
    pub level: Option<logging::Level>,
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test::check_deserialize;
    use std::fs::File;
    use std::io::Read;

    #[test]
    fn test_json_addon_install_parameters_null() {
        let json = r#""#;

        assert!(serde_json::from_str::<AddonInstallParameters>(&json).is_err());
    }

    #[test]
    fn test_json_addon_install_parameters_empty() {
        let json = r#"{}"#;

        assert!(serde_json::from_str::<AddonInstallParameters>(&json).is_err());
    }

    #[test]
    fn test_json_addon_install_parameters_with_path() {
        let json = r#"{"path": "/path/to.xpi", "temporary": true}"#;
        let data = AddonInstallParameters {
            path: "/path/to.xpi".to_string(),
            temporary: Some(true),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_addon_install_parameters_with_path_only() {
        let json = r#"{"path": "/path/to.xpi"}"#;
        let data = AddonInstallParameters {
            path: "/path/to.xpi".to_string(),
            temporary: None,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_addon_install_parameters_with_path_invalid_type() {
        let json = r#"{"path": true, "temporary": true}"#;

        assert!(serde_json::from_str::<AddonInstallParameters>(&json).is_err());
    }

    #[test]
    fn test_json_addon_install_parameters_with_path_and_temporary_invalid_type() {
        let json = r#"{"path": "/path/to.xpi", "temporary": "foo"}"#;

        assert!(serde_json::from_str::<AddonInstallParameters>(&json).is_err());
    }

    #[test]
    fn test_json_addon_install_parameters_with_addon() {
        let json = r#"{"addon": "aGVsbG8=", "temporary": true}"#;
        let data = serde_json::from_str::<AddonInstallParameters>(&json).unwrap();

        assert_eq!(data.temporary, Some(true));
        let mut file = File::open(data.path).unwrap();
        let mut contents = String::new();
        file.read_to_string(&mut contents).unwrap();
        assert_eq!(contents, "hello");
    }

    #[test]
    fn test_json_addon_install_parameters_with_addon_only() {
        let json = r#"{"addon": "aGVsbG8="}"#;
        let data = serde_json::from_str::<AddonInstallParameters>(&json).unwrap();

        assert_eq!(data.temporary, None);
        let mut file = File::open(data.path).unwrap();
        let mut contents = String::new();
        file.read_to_string(&mut contents).unwrap();
        assert_eq!(contents, "hello");
    }

    #[test]
    fn test_json_addon_install_parameters_with_addon_invalid_type() {
        let json = r#"{"addon": true, "temporary": true}"#;

        assert!(serde_json::from_str::<AddonInstallParameters>(&json).is_err());
    }

    #[test]
    fn test_json_addon_install_parameters_with_addon_and_temporary_invalid_type() {
        let json = r#"{"addon": "aGVsbG8=", "temporary": "foo"}"#;

        assert!(serde_json::from_str::<AddonInstallParameters>(&json).is_err());
    }

    #[test]
    fn test_json_install_parameters_with_temporary_only() {
        let json = r#"{"temporary": true}"#;

        assert!(serde_json::from_str::<AddonInstallParameters>(&json).is_err());
    }

    #[test]
    fn test_json_addon_install_parameters_with_both_path_and_addon() {
        let json = r#"{
            "path":"/path/to.xpi",
            "addon":"aGVsbG8=",
            "temporary":true
        }"#;

        assert!(serde_json::from_str::<AddonInstallParameters>(&json).is_err());
    }

    #[test]
    fn test_json_addon_uninstall_parameters_null() {
        let json = r#""#;

        assert!(serde_json::from_str::<AddonUninstallParameters>(&json).is_err());
    }

    #[test]
    fn test_json_addon_uninstall_parameters_empty() {
        let json = r#"{}"#;

        assert!(serde_json::from_str::<AddonUninstallParameters>(&json).is_err());
    }

    #[test]
    fn test_json_addon_uninstall_parameters() {
        let json = r#"{"id": "foo"}"#;
        let data = AddonUninstallParameters {
            id: "foo".to_string(),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_addon_uninstall_parameters_id_invalid_type() {
        let json = r#"{"id": true}"#;

        assert!(serde_json::from_str::<AddonUninstallParameters>(&json).is_err());
    }

    #[test]
    fn test_json_gecko_context_parameters_content() {
        let json = r#"{"context": "content"}"#;
        let data = GeckoContextParameters {
            context: GeckoContext::Content,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_gecko_context_parameters_chrome() {
        let json = r#"{"context": "chrome"}"#;
        let data = GeckoContextParameters {
            context: GeckoContext::Chrome,
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_gecko_context_parameters_context_missing() {
        let json = r#"{}"#;

        assert!(serde_json::from_str::<GeckoContextParameters>(&json).is_err());
    }

    #[test]
    fn test_json_gecko_context_parameters_context_null() {
        let json = r#"{"context": null}"#;

        assert!(serde_json::from_str::<GeckoContextParameters>(&json).is_err());
    }

    #[test]
    fn test_json_gecko_context_parameters_context_invalid_value() {
        let json = r#"{"context": "foo"}"#;

        assert!(serde_json::from_str::<GeckoContextParameters>(&json).is_err());
    }

    #[test]
    fn test_json_xbl_anonymous_by_attribute() {
        let json = r#"{
            "name": "foo",
            "value": "bar"
        }"#;

        let data = XblLocatorParameters {
            name: "foo".to_string(),
            value: "bar".to_string(),
        };

        check_deserialize(&json, &data);
    }

    #[test]
    fn test_json_xbl_anonymous_by_attribute_with_name_missing() {
        let json = r#"{
            "value": "bar"
        }"#;

        assert!(serde_json::from_str::<XblLocatorParameters>(&json).is_err());
    }

    #[test]
    fn test_json_xbl_anonymous_by_attribute_with_name_invalid_type() {
        let json = r#"{
            "name": null,
            "value": "bar"
        }"#;

        assert!(serde_json::from_str::<XblLocatorParameters>(&json).is_err());
    }

    #[test]
    fn test_json_xbl_anonymous_by_attribute_with_value_missing() {
        let json = r#"{
            "name": "foo",
        }"#;

        assert!(serde_json::from_str::<XblLocatorParameters>(&json).is_err());
    }

    #[test]
    fn test_json_xbl_anonymous_by_attribute_with_value_invalid_type() {
        let json = r#"{
            "name": "foo",
            "value": null
        }"#;

        assert!(serde_json::from_str::<XblLocatorParameters>(&json).is_err());
    }
}
