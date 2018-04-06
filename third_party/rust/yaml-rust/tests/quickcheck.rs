extern crate yaml_rust;
#[macro_use]
extern crate quickcheck;

use quickcheck::TestResult;
use yaml_rust::{Yaml, YamlLoader, YamlEmitter};
use std::error::Error;

quickcheck! {
    fn test_check_weird_keys(xs: Vec<String>) -> TestResult {
        let mut out_str = String::new();
        {
            let mut emitter = YamlEmitter::new(&mut out_str);
            emitter.dump(&Yaml::Array(xs.into_iter().map(Yaml::String).collect())).unwrap();
        }
        if let Err(err) = YamlLoader::load_from_str(&out_str) {
            return TestResult::error(err.description());
        }
        TestResult::passed()
    }
}
