/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde::ser::{Serialize, Serializer};
use serde::{Deserialize, Deserializer};
use std::collections::HashMap;
use std::fmt::{self, Display, Formatter};

pub static ELEMENT_KEY: &str = "element-6066-11e4-a52e-4f735466cecf";
pub static FRAME_KEY: &str = "frame-075b-4da1-b6ba-e579c2d3230a";
pub static SHADOW_KEY: &str = "shadow-6066-11e4-a52e-4f735466cecf";
pub static WINDOW_KEY: &str = "window-fcc6-11e5-b4f8-330a88ab9d7f";

pub static MAX_SAFE_INTEGER: u64 = 9_007_199_254_740_991;

pub type Parameters = HashMap<String, String>;

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Cookie {
    pub name: String,
    pub value: String,
    pub path: Option<String>,
    pub domain: Option<String>,
    #[serde(default)]
    pub secure: bool,
    #[serde(rename = "httpOnly")]
    pub http_only: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub expiry: Option<Date>,
    #[serde(skip_serializing_if = "Option::is_none", rename = "sameSite")]
    pub same_site: Option<String>,
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct CredentialParameters {
    #[serde(rename = "credentialId")]
    pub credential_id: String,
    #[serde(rename = "isResidentCredential")]
    pub is_resident_credential: bool,
    #[serde(rename = "rpId")]
    pub rp_id: String,
    #[serde(rename = "privateKey")]
    pub private_key: String,
    #[serde(rename = "userHandle")]
    #[serde(default)]
    pub user_handle: String,
    #[serde(rename = "signCount")]
    pub sign_count: u64,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Date(pub u64);

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum FrameId {
    Short(u16),
    #[serde(
        rename = "element-6066-11e4-a52e-4f735466cecf",
        serialize_with = "serialize_webelement_id"
    )]
    Element(WebElement),
    Top,
}

// TODO(Henrik): Remove when ToMarionette trait has been fixed (Bug 1481776)
fn serialize_webelement_id<S>(element: &WebElement, serializer: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    element.serialize(serializer)
}

#[derive(Copy, Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum LocatorStrategy {
    #[serde(rename = "css selector")]
    CSSSelector,
    #[serde(rename = "link text")]
    LinkText,
    #[serde(rename = "partial link text")]
    PartialLinkText,
    #[serde(rename = "tag name")]
    TagName,
    #[serde(rename = "xpath")]
    XPath,
}

#[derive(Clone, Debug, PartialEq)]
pub struct ShadowRoot(pub String);

// private
#[derive(Serialize, Deserialize)]
struct ShadowRootObject {
    #[serde(rename = "shadow-6066-11e4-a52e-4f735466cecf")]
    id: String,
}

impl Serialize for ShadowRoot {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        ShadowRootObject { id: self.0.clone() }.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for ShadowRoot {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        Deserialize::deserialize(deserializer).map(|ShadowRootObject { id }| ShadowRoot(id))
    }
}

impl Display for ShadowRoot {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct WebElement(pub String);

// private
#[derive(Serialize, Deserialize)]
struct WebElementObject {
    #[serde(rename = "element-6066-11e4-a52e-4f735466cecf")]
    id: String,
}

impl Serialize for WebElement {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        WebElementObject { id: self.0.clone() }.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for WebElement {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        Deserialize::deserialize(deserializer).map(|WebElementObject { id }| WebElement(id))
    }
}

impl Display for WebElement {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct WebFrame(pub String);

// private
#[derive(Serialize, Deserialize)]
struct WebFrameObject {
    #[serde(rename = "frame-075b-4da1-b6ba-e579c2d3230a")]
    id: String,
}

impl Serialize for WebFrame {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        WebFrameObject { id: self.0.clone() }.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for WebFrame {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        Deserialize::deserialize(deserializer).map(|WebFrameObject { id }| WebFrame(id))
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct WebWindow(pub String);

// private
#[derive(Serialize, Deserialize)]
struct WebWindowObject {
    #[serde(rename = "window-fcc6-11e5-b4f8-330a88ab9d7f")]
    id: String,
}

impl Serialize for WebWindow {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        WebWindowObject { id: self.0.clone() }.serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for WebWindow {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        Deserialize::deserialize(deserializer).map(|WebWindowObject { id }| WebWindow(id))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test::{assert_ser, assert_ser_de};
    use serde_json::{self, json};

    #[test]
    fn test_json_date() {
        assert_ser_de(&Date(1234), json!(1234));
    }

    #[test]
    fn test_json_date_invalid() {
        assert!(serde_json::from_value::<Date>(json!("2018-01-01")).is_err());
    }

    #[test]
    fn test_json_frame_id_short() {
        assert_ser_de(&FrameId::Short(1234), json!(1234));
    }

    #[test]
    fn test_json_frame_id_webelement() {
        assert_ser(
            &FrameId::Element(WebElement("elem".into())),
            json!({ELEMENT_KEY: "elem"}),
        );
    }

    #[test]
    fn test_json_frame_id_invalid() {
        assert!(serde_json::from_value::<FrameId>(json!(true)).is_err());
    }

    #[test]
    fn test_json_locator_strategy_css_selector() {
        assert_ser_de(&LocatorStrategy::CSSSelector, json!("css selector"));
    }

    #[test]
    fn test_json_locator_strategy_link_text() {
        assert_ser_de(&LocatorStrategy::LinkText, json!("link text"));
    }

    #[test]
    fn test_json_locator_strategy_partial_link_text() {
        assert_ser_de(
            &LocatorStrategy::PartialLinkText,
            json!("partial link text"),
        );
    }

    #[test]
    fn test_json_locator_strategy_tag_name() {
        assert_ser_de(&LocatorStrategy::TagName, json!("tag name"));
    }

    #[test]
    fn test_json_locator_strategy_xpath() {
        assert_ser_de(&LocatorStrategy::XPath, json!("xpath"));
    }

    #[test]
    fn test_json_locator_strategy_invalid() {
        assert!(serde_json::from_value::<LocatorStrategy>(json!("foo")).is_err());
    }

    #[test]
    fn test_json_shadowroot() {
        assert_ser_de(&ShadowRoot("shadow".into()), json!({SHADOW_KEY: "shadow"}));
    }

    #[test]
    fn test_json_shadowroot_invalid() {
        assert!(serde_json::from_value::<ShadowRoot>(json!({"invalid":"shadow"})).is_err());
    }

    #[test]
    fn test_json_webelement() {
        assert_ser_de(&WebElement("elem".into()), json!({ELEMENT_KEY: "elem"}));
    }

    #[test]
    fn test_json_webelement_invalid() {
        assert!(serde_json::from_value::<WebElement>(json!({"invalid": "elem"})).is_err());
    }

    #[test]
    fn test_json_webframe() {
        assert_ser_de(&WebFrame("frame".into()), json!({FRAME_KEY: "frame"}));
    }

    #[test]
    fn test_json_webframe_invalid() {
        assert!(serde_json::from_value::<WebFrame>(json!({"invalid": "frame"})).is_err());
    }

    #[test]
    fn test_json_webwindow() {
        assert_ser_de(&WebWindow("window".into()), json!({WINDOW_KEY: "window"}));
    }

    #[test]
    fn test_json_webwindow_invalid() {
        assert!(serde_json::from_value::<WebWindow>(json!({"invalid": "window"})).is_err());
    }
}
