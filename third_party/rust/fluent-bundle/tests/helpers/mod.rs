use std::collections::HashMap;
use std::fs::File;
use std::io;
use std::io::Read;

use serde::{Deserialize, Serialize};

#[macro_export]
macro_rules! test {
    ($desc:stmt, $closure:block) => {{
        $closure
    }};
}

#[macro_export]
macro_rules! assert_format {
    ($bundle:expr, $id:expr, $args:expr, $expected:expr) => {
        let msg = $bundle.get_message($id).expect("Message doesn't exist");
        let mut errors = vec![];
        assert!(msg.value.is_some());
        assert_eq!(
            $bundle.format_pattern(&msg.value.unwrap(), $args, &mut errors),
            $expected
        );
        assert!(errors.is_empty());
    };
    ($bundle:expr, $id:expr, $args:expr, $expected:expr, $errors:expr) => {
        let msg = $bundle.get_message($id).expect("Message doesn't exist.");
        let mut errors = vec![];
        assert!(msg.value.is_some());
        assert_eq!(
            $bundle.format_pattern(&msg.value.unwrap(), $args, &mut errors),
            $expected
        );
        assert_eq!(errors, $errors);
    };
}

#[macro_export]
macro_rules! assert_format_none {
    ($bundle:expr, $id:expr) => {
        let msg = $bundle.get_message($id).expect("Message doesn't exist");
        assert!(msg.value.is_none());
    };
}

#[macro_export]
macro_rules! assert_format_attr {
    ($bundle:expr, $id:expr, $name:expr, $args:expr, $expected:expr) => {
        let msg = $bundle.get_message($id).expect("Message doesn't exists");
        let mut errors = vec![];
        let attr = msg.attributes.get($name).expect("Attribute exists");
        assert_eq!($bundle.format_pattern(&attr, $args, &mut errors), $expected);
        assert!(errors.is_empty());
    };
}

#[macro_export]
macro_rules! assert_get_resource_from_str {
    ($source:expr) => {
        FluentResource::try_new($source.to_owned()).expect("Failed to parse an FTL resource.")
    };
}

#[macro_export]
macro_rules! assert_get_bundle {
    ($res:expr) => {{
        let mut bundle: FluentBundle<&FluentResource> = FluentBundle::new(&["x-testing"]);
        bundle.set_use_isolating(false);
        bundle
            .add_resource($res)
            .expect("Failed to add FluentResource to FluentBundle.");
        bundle
    }};
}

pub fn get_fixture(path: &str) -> Result<TestFixture, io::Error> {
    let mut f = File::open(path)?;
    let mut s = String::new();
    f.read_to_string(&mut s)?;
    Ok(serde_yaml::from_str(&s).expect("Parsing YAML failed."))
}

pub fn get_defaults(path: &str) -> Result<TestDefaults, io::Error> {
    let mut f = File::open(path)?;
    let mut s = String::new();
    f.read_to_string(&mut s)?;
    Ok(serde_yaml::from_str(&s).expect("Parsing YAML failed."))
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub struct TestBundle {
    pub name: Option<String>,
    pub locales: Option<Vec<String>>,
    pub resources: Option<Vec<String>>,
    #[serde(rename = "useIsolating")]
    pub use_isolating: Option<bool>,
    pub functions: Option<Vec<String>>,
    pub transform: Option<String>,
    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub errors: Vec<TestError>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub struct TestResource {
    pub name: Option<String>,
    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub errors: Vec<TestError>,
    pub source: String,
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub struct TestSetup {
    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub bundles: Vec<TestBundle>,
    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub resources: Vec<TestResource>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub struct TestError {
    #[serde(rename = "type")]
    pub error_type: String,
    pub desc: Option<String>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
#[serde(untagged)]
pub enum TestArgumentValue {
    String(String),
    Number(f64),
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub struct TestAssert {
    pub bundle: Option<String>,
    pub id: String,
    pub attribute: Option<String>,
    pub value: Option<String>,
    pub args: Option<HashMap<String, TestArgumentValue>>,
    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub errors: Vec<TestError>,
    pub missing: Option<bool>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub struct Test {
    pub name: String,
    pub skip: Option<bool>,

    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub bundles: Vec<TestBundle>,
    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub resources: Vec<TestResource>,

    pub asserts: Vec<TestAssert>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub struct TestSuite {
    pub name: String,
    pub skip: Option<bool>,

    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub bundles: Vec<TestBundle>,
    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub resources: Vec<TestResource>,

    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub tests: Vec<Test>,
    #[serde(skip_serializing_if = "Vec::is_empty", default)]
    pub suites: Vec<TestSuite>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub struct TestFixture {
    pub suites: Vec<TestSuite>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub struct BundleDefaults {
    #[serde(rename = "useIsolating")]
    pub use_isolating: Option<bool>,
    pub transform: Option<String>,
    pub locales: Option<Vec<String>>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub struct TestDefaults {
    pub bundle: BundleDefaults,
}
