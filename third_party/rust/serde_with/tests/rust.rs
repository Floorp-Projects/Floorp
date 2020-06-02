extern crate fnv;
extern crate pretty_assertions;
extern crate serde;
extern crate serde_derive;
extern crate serde_json;
extern crate serde_with;

use fnv::FnvHashMap;
use pretty_assertions::assert_eq;
use serde_derive::{Deserialize, Serialize};
use serde_json::error::Category;
use serde_with::CommaSeparator;
use std::collections::{BTreeMap, HashMap, LinkedList, VecDeque};

#[test]
fn string_collection() {
    #[derive(Debug, Deserialize)]
    struct S {
        #[serde(with = "serde_with::rust::StringWithSeparator::<CommaSeparator>")]
        s: Vec<String>,
    }
    let from = r#"[
        { "s": "A,B,C,D" },
        { "s": ",," },
        { "s": "AVeryLongString" }
    ]"#;

    let res: Vec<S> = serde_json::from_str(from).unwrap();
    assert_eq!(
        vec![
            "A".to_string(),
            "B".to_string(),
            "C".to_string(),
            "D".to_string(),
        ],
        res[0].s
    );
    assert_eq!(
        vec!["".to_string(), "".to_string(), "".to_string()],
        res[1].s
    );
    assert_eq!(vec!["AVeryLongString".to_string()], res[2].s);
}

#[test]
fn string_collection_non_existing() {
    #[derive(Debug, Deserialize, Serialize)]
    struct S {
        #[serde(with = "serde_with::rust::StringWithSeparator::<CommaSeparator>")]
        s: Vec<String>,
    }
    let from = r#"[
        { "s": "" }
    ]"#;

    let res: Vec<S> = serde_json::from_str(from).unwrap();
    assert_eq!(Vec::<String>::new(), res[0].s);

    assert_eq!(r#"{"s":""}"#, serde_json::to_string(&res[0]).unwrap());
}

#[test]
fn prohibit_duplicate_value_hashset() {
    use std::{collections::HashSet, iter::FromIterator};
    #[derive(Debug, Eq, PartialEq, Deserialize)]
    struct Doc {
        #[serde(with = "::serde_with::rust::sets_duplicate_value_is_error")]
        set: HashSet<usize>,
    }

    let s = r#"{"set": [1, 2, 3, 4]}"#;
    let v = Doc {
        set: HashSet::from_iter(vec![1, 2, 3, 4]),
    };
    assert_eq!(v, serde_json::from_str(s).unwrap());

    let s = r#"{"set": [1, 2, 3, 4, 1]}"#;
    let res: Result<Doc, _> = serde_json::from_str(s);
    assert!(res.is_err());
}

#[test]
fn prohibit_duplicate_value_btreeset() {
    use std::{collections::BTreeSet, iter::FromIterator};
    #[derive(Debug, Eq, PartialEq, Deserialize)]
    struct Doc {
        #[serde(with = "::serde_with::rust::sets_duplicate_value_is_error")]
        set: BTreeSet<usize>,
    }

    let s = r#"{"set": [1, 2, 3, 4]}"#;
    let v = Doc {
        set: BTreeSet::from_iter(vec![1, 2, 3, 4]),
    };
    assert_eq!(v, serde_json::from_str(s).unwrap());

    let s = r#"{"set": [1, 2, 3, 4, 1]}"#;
    let res: Result<Doc, _> = serde_json::from_str(s);
    assert!(res.is_err());
}

#[test]
fn prohibit_duplicate_value_hashmap() {
    use std::collections::HashMap;
    #[derive(Debug, Eq, PartialEq, Deserialize)]
    struct Doc {
        #[serde(with = "::serde_with::rust::maps_duplicate_key_is_error")]
        map: HashMap<usize, usize>,
    }

    // Different value and key always works
    let s = r#"{"map": {"1": 1, "2": 2, "3": 3}}"#;
    let mut v = Doc {
        map: HashMap::new(),
    };
    v.map.insert(1, 1);
    v.map.insert(2, 2);
    v.map.insert(3, 3);
    assert_eq!(v, serde_json::from_str(s).unwrap());

    // Same value for different keys is ok
    let s = r#"{"map": {"1": 1, "2": 1, "3": 1}}"#;
    let mut v = Doc {
        map: HashMap::new(),
    };
    v.map.insert(1, 1);
    v.map.insert(2, 1);
    v.map.insert(3, 1);
    assert_eq!(v, serde_json::from_str(s).unwrap());

    // Duplicate keys are an error
    let s = r#"{"map": {"1": 1, "2": 2, "1": 3}}"#;
    let res: Result<Doc, _> = serde_json::from_str(s);
    assert!(res.is_err());
}

#[test]
fn prohibit_duplicate_value_btreemap() {
    use std::collections::BTreeMap;
    #[derive(Debug, Eq, PartialEq, Deserialize)]
    struct Doc {
        #[serde(with = "::serde_with::rust::maps_duplicate_key_is_error")]
        map: BTreeMap<usize, usize>,
    }

    // Different value and key always works
    let s = r#"{"map": {"1": 1, "2": 2, "3": 3}}"#;
    let mut v = Doc {
        map: BTreeMap::new(),
    };
    v.map.insert(1, 1);
    v.map.insert(2, 2);
    v.map.insert(3, 3);
    assert_eq!(v, serde_json::from_str(s).unwrap());

    // Same value for different keys is ok
    let s = r#"{"map": {"1": 1, "2": 1, "3": 1}}"#;
    let mut v = Doc {
        map: BTreeMap::new(),
    };
    v.map.insert(1, 1);
    v.map.insert(2, 1);
    v.map.insert(3, 1);
    assert_eq!(v, serde_json::from_str(s).unwrap());

    // Duplicate keys are an error
    let s = r#"{"map": {"1": 1, "2": 2, "1": 3}}"#;
    let res: Result<Doc, _> = serde_json::from_str(s);
    assert!(res.is_err());
}

#[test]
fn duplicate_key_first_wins_hashmap() {
    use std::collections::HashMap;
    #[derive(Debug, Eq, PartialEq, Deserialize)]
    struct Doc {
        #[serde(with = "::serde_with::rust::maps_first_key_wins")]
        map: HashMap<usize, usize>,
    }

    // Different value and key always works
    let s = r#"{"map": {"1": 1, "2": 2, "3": 3}}"#;
    let mut v = Doc {
        map: HashMap::new(),
    };
    v.map.insert(1, 1);
    v.map.insert(2, 2);
    v.map.insert(3, 3);
    assert_eq!(v, serde_json::from_str(s).unwrap());

    // Same value for different keys is ok
    let s = r#"{"map": {"1": 1, "2": 1, "3": 1}}"#;
    let mut v = Doc {
        map: HashMap::new(),
    };
    v.map.insert(1, 1);
    v.map.insert(2, 1);
    v.map.insert(3, 1);
    assert_eq!(v, serde_json::from_str(s).unwrap());

    // Duplicate keys, the first one is used
    let s = r#"{"map": {"1": 1, "2": 2, "1": 3}}"#;
    let mut v = Doc {
        map: HashMap::new(),
    };
    v.map.insert(1, 1);
    v.map.insert(2, 2);
    assert_eq!(v, serde_json::from_str(s).unwrap());
}

#[test]
fn duplicate_key_first_wins_btreemap() {
    use std::collections::BTreeMap;
    #[derive(Debug, Eq, PartialEq, Deserialize)]
    struct Doc {
        #[serde(with = "::serde_with::rust::maps_first_key_wins")]
        map: BTreeMap<usize, usize>,
    }

    // Different value and key always works
    let s = r#"{"map": {"1": 1, "2": 2, "3": 3}}"#;
    let mut v = Doc {
        map: BTreeMap::new(),
    };
    v.map.insert(1, 1);
    v.map.insert(2, 2);
    v.map.insert(3, 3);
    assert_eq!(v, serde_json::from_str(s).unwrap());

    // Same value for different keys is ok
    let s = r#"{"map": {"1": 1, "2": 1, "3": 1}}"#;
    let mut v = Doc {
        map: BTreeMap::new(),
    };
    v.map.insert(1, 1);
    v.map.insert(2, 1);
    v.map.insert(3, 1);
    assert_eq!(v, serde_json::from_str(s).unwrap());

    // Duplicate keys, the first one is used
    let s = r#"{"map": {"1": 1, "2": 2, "1": 3}}"#;
    let mut v = Doc {
        map: BTreeMap::new(),
    };
    v.map.insert(1, 1);
    v.map.insert(2, 2);
    assert_eq!(v, serde_json::from_str(s).unwrap());
}

#[test]
fn test_hashmap_as_tuple_list() {
    #[derive(Debug, Deserialize, Serialize, PartialEq, Default)]
    struct S {
        #[serde(with = "serde_with::rust::hashmap_as_tuple_list")]
        s: HashMap<String, u8>,
    }
    let from = r#"{
        "s": [
            ["ABC", 1],
            ["Hello", 0],
            ["World", 20]
        ]
    }"#;
    let mut expected = S::default();
    expected.s.insert("ABC".to_string(), 1);
    expected.s.insert("Hello".to_string(), 0);
    expected.s.insert("World".to_string(), 20);

    let res: S = serde_json::from_str(from).unwrap();
    assert_eq!(expected, res);

    let from = r#"{
  "s": [
    [
      "Hello",
      0
    ]
  ]
}"#;
    let mut expected = S::default();
    expected.s.insert("Hello".to_string(), 0);

    let res: S = serde_json::from_str(from).unwrap();
    assert_eq!(expected, res);
    // We can only do this with a HashMap of size 1 as otherwise the iteration order is unspecified
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());

    let from = r#"{
  "s": []
}"#;
    let expected = S::default();
    let res: S = serde_json::from_str(from).unwrap();
    assert!(res.s.is_empty());
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());

    // Test parse error
    let from = r#"{
  "s": [ [1] ]
}"#;
    let res: Result<S, _> = serde_json::from_str(from);
    assert!(res.is_err());
    let err = res.unwrap_err();
    println!("{:?}", err);
    assert!(err.is_data());
}

#[test]
fn test_hashmap_as_tuple_list_fnv() {
    #[derive(Debug, Deserialize, Serialize, PartialEq, Default)]
    struct S {
        #[serde(with = "serde_with::rust::hashmap_as_tuple_list")]
        s: FnvHashMap<String, u8>,
    }
    let from = r#"{
        "s": [
            ["ABC", 1],
            ["Hello", 0],
            ["World", 20]
        ]
    }"#;
    let mut expected = S::default();
    expected.s.insert("ABC".to_string(), 1);
    expected.s.insert("Hello".to_string(), 0);
    expected.s.insert("World".to_string(), 20);

    let res: S = serde_json::from_str(from).unwrap();
    assert_eq!(expected, res);

    let from = r#"{
  "s": [
    [
      "Hello",
      0
    ]
  ]
}"#;
    let mut expected = S::default();
    expected.s.insert("Hello".to_string(), 0);

    let res: S = serde_json::from_str(from).unwrap();
    assert_eq!(expected, res);
    // We can only do this with a HashMap of size 1 as otherwise the iteration order is unspecified
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());

    let from = r#"{
  "s": []
}"#;
    let expected = S::default();
    let res: S = serde_json::from_str(from).unwrap();
    assert!(res.s.is_empty());
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());

    // Test parse error
    let from = r#"{
  "s": [ [1] ]
}"#;
    let res: Result<S, _> = serde_json::from_str(from);
    assert!(res.is_err());
    let err = res.unwrap_err();
    println!("{:?}", err);
    assert!(err.is_data());
}

#[test]
fn test_btreemap_as_tuple_list() {
    #[derive(Debug, Deserialize, Serialize, PartialEq, Default)]
    struct S {
        #[serde(with = "serde_with::rust::btreemap_as_tuple_list")]
        s: BTreeMap<String, u8>,
    }
    let from = r#"{
  "s": [
    [
      "ABC",
      1
    ],
    [
      "Hello",
      0
    ],
    [
      "World",
      20
    ]
  ]
}"#;
    let mut expected = S::default();
    expected.s.insert("ABC".to_string(), 1);
    expected.s.insert("Hello".to_string(), 0);
    expected.s.insert("World".to_string(), 20);

    let res: S = serde_json::from_str(from).unwrap();
    assert_eq!(expected, res);
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());

    let from = r#"{
  "s": [
    [
      "Hello",
      0
    ]
  ]
}"#;
    let mut expected = S::default();
    expected.s.insert("Hello".to_string(), 0);

    let res: S = serde_json::from_str(from).unwrap();
    assert_eq!(expected, res);
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());

    let from = r#"{
  "s": []
}"#;
    let expected = S::default();
    let res: S = serde_json::from_str(from).unwrap();
    assert!(res.s.is_empty());
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());

    // Test parse error
    let from = r#"{
  "s": [ [1] ]
}"#;
    let res: Result<S, _> = serde_json::from_str(from);
    assert!(res.is_err());
    let err = res.unwrap_err();
    println!("{:?}", err);
    assert!(err.is_data());
}

#[test]
fn tuple_list_as_map_vec() {
    #[derive(Debug, Deserialize, Serialize, Default)]
    struct S {
        #[serde(with = "serde_with::rust::tuple_list_as_map")]
        s: Vec<(Wrapper<i32>, Wrapper<String>)>,
    }

    #[derive(Clone, Debug, Serialize, Deserialize)]
    #[serde(transparent)]
    struct Wrapper<T>(T);

    let from = r#"{
  "s": {
    "1": "Hi",
    "2": "Cake",
    "99": "Lie"
  }
}"#;
    let mut expected = S::default();
    expected.s.push((Wrapper(1), Wrapper("Hi".into())));
    expected.s.push((Wrapper(2), Wrapper("Cake".into())));
    expected.s.push((Wrapper(99), Wrapper("Lie".into())));

    let res: S = serde_json::from_str(from).unwrap();
    for ((exp_k, exp_v), (res_k, res_v)) in expected.s.iter().zip(&res.s) {
        assert_eq!(exp_k.0, res_k.0);
        assert_eq!(exp_v.0, res_v.0);
    }
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());

    let from = r#"{
  "s": {}
}"#;
    let expected = S::default();

    let res: S = serde_json::from_str(from).unwrap();
    assert!(res.s.is_empty());
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());

    let from = r#"{
  "s": []
}"#;
    let res: Result<S, _> = serde_json::from_str(from);
    let res = res.unwrap_err();
    assert_eq!(Category::Data, res.classify());
    assert_eq!(
        "invalid type: sequence, expected a map at line 2 column 7",
        res.to_string()
    );

    let from = r#"{
  "s": null
}"#;
    let res: Result<S, _> = serde_json::from_str(from);
    let res = res.unwrap_err();
    assert_eq!(Category::Data, res.classify());
    assert_eq!(
        "invalid type: null, expected a map at line 2 column 11",
        res.to_string()
    );
}

#[test]
fn tuple_list_as_map_linkedlist() {
    #[derive(Debug, Deserialize, Serialize, Default)]
    struct S {
        #[serde(with = "serde_with::rust::tuple_list_as_map")]
        s: LinkedList<(Wrapper<i32>, Wrapper<String>)>,
    }

    #[derive(Clone, Debug, Serialize, Deserialize)]
    #[serde(transparent)]
    struct Wrapper<T>(T);

    let from = r#"{
  "s": {
    "1": "Hi",
    "2": "Cake",
    "99": "Lie"
  }
}"#;
    let mut expected = S::default();
    expected.s.push_back((Wrapper(1), Wrapper("Hi".into())));
    expected.s.push_back((Wrapper(2), Wrapper("Cake".into())));
    expected.s.push_back((Wrapper(99), Wrapper("Lie".into())));

    let res: S = serde_json::from_str(from).unwrap();
    for ((exp_k, exp_v), (res_k, res_v)) in expected.s.iter().zip(&res.s) {
        assert_eq!(exp_k.0, res_k.0);
        assert_eq!(exp_v.0, res_v.0);
    }
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());

    let from = r#"{
  "s": {}
}"#;
    let expected = S::default();

    let res: S = serde_json::from_str(from).unwrap();
    assert!(res.s.is_empty());
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());
}

#[test]
fn tuple_list_as_map_vecdeque() {
    #[derive(Debug, Deserialize, Serialize, Default)]
    struct S {
        #[serde(with = "serde_with::rust::tuple_list_as_map")]
        s: VecDeque<(Wrapper<i32>, Wrapper<String>)>,
    }

    #[derive(Clone, Debug, Serialize, Deserialize)]
    #[serde(transparent)]
    struct Wrapper<T>(T);

    let from = r#"{
  "s": {
    "1": "Hi",
    "2": "Cake",
    "99": "Lie"
  }
}"#;
    let mut expected = S::default();
    expected.s.push_back((Wrapper(1), Wrapper("Hi".into())));
    expected.s.push_back((Wrapper(2), Wrapper("Cake".into())));
    expected.s.push_back((Wrapper(99), Wrapper("Lie".into())));

    let res: S = serde_json::from_str(from).unwrap();
    for ((exp_k, exp_v), (res_k, res_v)) in expected.s.iter().zip(&res.s) {
        assert_eq!(exp_k.0, res_k.0);
        assert_eq!(exp_v.0, res_v.0);
    }
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());

    let from = r#"{
  "s": {}
}"#;
    let expected = S::default();

    let res: S = serde_json::from_str(from).unwrap();
    assert!(res.s.is_empty());
    assert_eq!(from, serde_json::to_string_pretty(&expected).unwrap());
}
