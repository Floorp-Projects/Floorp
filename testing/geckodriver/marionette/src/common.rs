use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct BoolValue {
    value: bool,
}

impl BoolValue {
    pub fn new(val: bool) -> Self {
        BoolValue { value: val }
    }
}

// TODO(nupur): Bug 1567165 - Make WebElement in Marionette a unit struct
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct WebElement {
    #[serde(rename = "element-6066-11e4-a52e-4f735466cecf")]
    pub element: String,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Timeouts {
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub implicit: Option<u64>,
    #[serde(
        default,
        rename = "pageLoad",
        alias = "page load",
        skip_serializing_if = "Option::is_none"
    )]
    pub page_load: Option<u64>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub script: Option<Option<u64>>,
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test::{assert_de, assert_ser, assert_ser_de, ELEMENT_KEY};
    use serde_json::json;

    #[test]
    fn test_web_element() {
        let data = WebElement {
            element: "foo".into(),
        };
        assert_ser_de(&data, json!({ELEMENT_KEY: "foo"}));
    }

    #[test]
    fn test_timeouts_with_all_params() {
        let data = Timeouts {
            implicit: Some(1000),
            page_load: Some(200000),
            script: Some(Some(60000)),
        };
        assert_ser_de(
            &data,
            json!({"implicit":1000,"pageLoad":200000,"script":60000}),
        );
        assert_de(
            &data,
            json!({"implicit":1000,"page load":200000,"script":60000}),
        );
    }

    #[test]
    fn test_timeouts_with_missing_params() {
        let data = Timeouts {
            implicit: Some(1000),
            page_load: None,
            script: None,
        };
        assert_ser_de(&data, json!({"implicit":1000}));
    }

    #[test]
    fn test_timeouts_setting_script_none() {
        let data = Timeouts {
            implicit: Some(1000),
            page_load: None,
            script: Some(None),
        };
        assert_ser(&data, json!({"implicit":1000, "script":null}));
    }
}
