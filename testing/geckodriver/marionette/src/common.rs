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
    pub implicit: u64,
    #[serde(rename = "pageLoad", alias = "page load")]
    pub page_load: u64,
    pub script: Option<u64>,
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test::{assert_de, assert_ser_de, ELEMENT_KEY};
    use serde_json::json;

    #[test]
    fn test_web_element() {
        let data = WebElement {
            element: "foo".into(),
        };
        assert_ser_de(&data, json!({ELEMENT_KEY: "foo"}));
    }

    #[test]
    fn test_timeouts() {
        let data = Timeouts {
            implicit: 1000,
            page_load: 200000,
            script: Some(60000),
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
}
