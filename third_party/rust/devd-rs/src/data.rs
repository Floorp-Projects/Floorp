pub use std::collections::BTreeMap;

#[derive(Debug, Clone, PartialEq)]
pub enum Event {
    Notify { system: String, subsystem: String, kind: String, data: BTreeMap<String, String> },
    Attach { dev: String, parent: BTreeMap<String, String>, location: String },
    Detach { dev: String, parent: BTreeMap<String, String>, location: String },
    Nomatch { parent: BTreeMap<String, String>, location: String },
}
