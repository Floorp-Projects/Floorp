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
