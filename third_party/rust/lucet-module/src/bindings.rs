use failure::{format_err, Error};
use serde_json::{self, Map, Value};
use std::collections::{hash_map::Entry, HashMap};
use std::fs;
use std::path::Path;

#[derive(Debug, Clone)]
pub struct Bindings {
    bindings: HashMap<String, HashMap<String, String>>,
}

impl Bindings {
    pub fn new(bindings: HashMap<String, HashMap<String, String>>) -> Bindings {
        Self { bindings: bindings }
    }

    pub fn env(env: HashMap<String, String>) -> Bindings {
        let mut bindings = HashMap::new();
        bindings.insert("env".to_owned(), env);
        Self::new(bindings)
    }

    pub fn empty() -> Bindings {
        Self::new(HashMap::new())
    }

    pub fn from_json(v: &Value) -> Result<Bindings, Error> {
        match v.as_object() {
            Some(modules) => Self::parse_modules_json_obj(modules),
            None => Err(format_err!("top level json expected to be object"))?,
        }
    }

    pub fn from_str(s: &str) -> Result<Bindings, Error> {
        let top: Value = serde_json::from_str(s)?;
        Ok(Self::from_json(&top)?)
    }

    pub fn from_file<P: AsRef<Path>>(path: P) -> Result<Bindings, Error> {
        let contents = fs::read_to_string(path.as_ref())?;
        Ok(Self::from_str(&contents)?)
    }

    pub fn extend(&mut self, other: &Bindings) -> Result<(), Error> {
        for (modname, othermodbindings) in other.bindings.iter() {
            match self.bindings.entry(modname.clone()) {
                Entry::Occupied(mut e) => {
                    let existing = e.get_mut();
                    for (bindname, binding) in othermodbindings {
                        match existing.entry(bindname.clone()) {
                            Entry::Vacant(e) => {
                                e.insert(binding.clone());
                            }
                            Entry::Occupied(e) => {
                                if binding != e.get() {
                                    Err(format_err!(
                                        "cannot re-bind {} from {} to {}",
                                        e.key(),
                                        binding,
                                        e.get()
                                    ))?;
                                }
                            }
                        }
                    }
                }
                Entry::Vacant(e) => {
                    e.insert(othermodbindings.clone());
                }
            }
        }
        Ok(())
    }

    pub fn translate(&self, module: &str, symbol: &str) -> Result<&str, Error> {
        match self.bindings.get(module) {
            Some(m) => match m.get(symbol) {
                Some(s) => Ok(s),
                None => Err(format_err!("Unknown symbol `{}::{}`", module, symbol)),
            },
            None => Err(format_err!(
                "Unknown module for symbol `{}::{}`",
                module,
                symbol
            )),
        }
    }

    fn parse_modules_json_obj(m: &Map<String, Value>) -> Result<Self, Error> {
        let mut res = HashMap::new();
        for (modulename, values) in m {
            match values.as_object() {
                Some(methods) => {
                    let methodmap = Self::parse_methods_json_obj(methods)?;
                    res.insert(modulename.to_owned(), methodmap);
                }
                None => Err(format_err!(""))?,
            }
        }
        Ok(Self::new(res))
    }

    fn parse_methods_json_obj(m: &Map<String, Value>) -> Result<HashMap<String, String>, Error> {
        let mut res = HashMap::new();
        for (method, i) in m {
            match i.as_str() {
                Some(importbinding) => {
                    res.insert(method.to_owned(), importbinding.to_owned());
                }
                None => Err(format_err!(""))?,
            }
        }
        Ok(res)
    }

    pub fn to_string(&self) -> Result<String, Error> {
        let s = serde_json::to_string(&self.to_json())?;
        Ok(s)
    }

    pub fn to_json(&self) -> Value {
        Value::from(self.serialize_modules_json_obj())
    }

    fn serialize_modules_json_obj(&self) -> Map<String, Value> {
        let mut m = Map::new();
        for (modulename, values) in self.bindings.iter() {
            m.insert(
                modulename.to_owned(),
                Value::from(Self::serialize_methods_json_obj(values)),
            );
        }
        m
    }

    fn serialize_methods_json_obj(methods: &HashMap<String, String>) -> Map<String, Value> {
        let mut m = Map::new();
        for (methodname, symbol) in methods.iter() {
            m.insert(methodname.to_owned(), Value::from(symbol.to_owned()));
        }
        m
    }
}

#[cfg(test)]
mod tests {
    fn test_file(f: &str) -> PathBuf {
        PathBuf::from(format!("tests/bindings/{}", f))
    }

    use super::Bindings;
    use std::collections::HashMap;
    use std::path::PathBuf;

    #[test]
    fn explicit() {
        let mut explicit_map = HashMap::new();
        explicit_map.insert(String::from("hello"), String::from("goodbye"));
        let map = Bindings::env(explicit_map);

        let result = map.translate("env", "hello").unwrap();
        assert!(result == "goodbye");

        let result = map.translate("env", "nonexistent");
        if let Ok(_) = result {
            assert!(
                false,
                "explicit import map returned value for non-existent symbol"
            )
        }
    }

    #[test]
    fn explicit_from_nonexistent_file() {
        let fail_map = Bindings::from_file(&test_file("nonexistent_bindings.json"));
        assert!(
            fail_map.is_err(),
            "ImportMap::explicit_from_file did not fail on a non-existent file"
        );
    }

    #[test]
    fn explicit_from_garbage_file() {
        let fail_map = Bindings::from_file(&test_file("garbage.json"));
        assert!(
            fail_map.is_err(),
            "ImportMap::explicit_from_file did not fail on a garbage file"
        );
    }

    #[test]
    fn explicit_from_file() {
        let map = Bindings::from_file(&test_file("bindings_test.json"))
            .expect("load valid bindings from file");
        let result = map.translate("env", "hello").expect("hello has a binding");
        assert!(result == "json is cool");

        assert!(
            map.translate("env", "nonexistent").is_err(),
            "bindings from file returned value for non-existent symbol"
        );
    }
}
