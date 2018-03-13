extern crate toml;

use std::collections::BTreeMap;

use toml::Value::{String, Integer, Float, Boolean, Array, Table};

macro_rules! map( ($($k:expr => $v:expr),*) => ({
    let mut _m = BTreeMap::new();
    $(_m.insert($k.to_string(), $v);)*
    _m
}) );

#[test]
fn simple_show() {
    assert_eq!(String("foo".to_string()).to_string(),
               "\"foo\"");
    assert_eq!(Integer(10).to_string(),
               "10");
    assert_eq!(Float(10.0).to_string(),
               "10.0");
    assert_eq!(Float(2.4).to_string(),
               "2.4");
    assert_eq!(Boolean(true).to_string(),
               "true");
    assert_eq!(Array(vec![]).to_string(),
               "[]");
    assert_eq!(Array(vec![Integer(1), Integer(2)]).to_string(),
               "[1, 2]");
}

#[test]
fn table() {
    assert_eq!(Table(map! { }).to_string(),
               "");
    assert_eq!(Table(map! {
                        "test" => Integer(2),
                        "test2" => Integer(3) }).to_string(),
               "test = 2\ntest2 = 3\n");
    assert_eq!(Table(map! {
                    "test" => Integer(2),
                    "test2" => Table(map! {
                        "test" => String("wut".to_string())
                    })
               }).to_string(),
               "test = 2\n\
                \n\
                [test2]\n\
                test = \"wut\"\n");
    assert_eq!(Table(map! {
                    "test" => Integer(2),
                    "test2" => Table(map! {
                        "test" => String("wut".to_string())
                    })
               }).to_string(),
               "test = 2\n\
                \n\
                [test2]\n\
                test = \"wut\"\n");
    assert_eq!(Table(map! {
                    "test" => Integer(2),
                    "test2" => Array(vec![Table(map! {
                        "test" => String("wut".to_string())
                    })])
               }).to_string(),
               "test = 2\n\
                \n\
                [[test2]]\n\
                test = \"wut\"\n");
    assert_eq!(Table(map! {
                    "foo.bar" => Integer(2),
                    "foo\"bar" => Integer(2)
               }).to_string(),
               "\"foo\\\"bar\" = 2\n\
                \"foo.bar\" = 2\n");
    assert_eq!(Table(map! {
                    "test" => Integer(2),
                    "test2" => Array(vec![Table(map! {
                        "test" => Array(vec![Integer(2)])
                    })])
               }).to_string(),
               "test = 2\n\
                \n\
                [[test2]]\n\
                test = [2]\n");
    let table = Table(map! {
        "test" => Integer(2),
        "test2" => Array(vec![Table(map! {
            "test" => Array(vec![Array(vec![Integer(2), Integer(3)]),
            Array(vec![String("foo".to_string()), String("bar".to_string())])])
        })])
    });
    assert_eq!(table.to_string(),
               "test = 2\n\
                \n\
                [[test2]]\n\
                test = [[2, 3], [\"foo\", \"bar\"]]\n");
    assert_eq!(Table(map! {
                    "test" => Array(vec![Integer(2)]),
                    "test2" => Integer(2)
               }).to_string(),
               "test = [2]\n\
                test2 = 2\n");
}
