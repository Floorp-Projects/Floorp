extern crate serde;
extern crate toml;
#[macro_use]
extern crate serde_derive;

use std::collections::{BTreeMap, HashSet};
use serde::{Deserialize, Deserializer};

use toml::Value;
use toml::Value::{Table, Integer, Array, Float};

macro_rules! t {
    ($e:expr) => (match $e {
        Ok(t) => t,
        Err(e) => panic!("{} failed with {}", stringify!($e), e),
    })
}

macro_rules! equivalent {
    ($literal:expr, $toml:expr,) => ({
        let toml = $toml;
        let literal = $literal;

        // In/out of Value is equivalent
        println!("try_from");
        assert_eq!(t!(Value::try_from(literal.clone())), toml);
        println!("try_into");
        assert_eq!(literal, t!(toml.clone().try_into()));

        // Through a string equivalent
        println!("to_string(literal)");
        assert_eq!(t!(toml::to_string(&literal)), toml.to_string());
        println!("to_string(toml)");
        assert_eq!(t!(toml::to_string(&toml)), toml.to_string());
        println!("literal, from_str(toml)");
        assert_eq!(literal, t!(toml::from_str(&toml.to_string())));
        println!("toml, from_str(toml)");
        assert_eq!(toml, t!(toml::from_str(&toml.to_string())));
    })
}

macro_rules! error {
    ($ty:ty, $toml:expr, $error:expr) => ({
        println!("attempting parsing");
        match toml::from_str::<$ty>(&$toml.to_string()) {
            Ok(_) => panic!("successful"),
            Err(e) => {
                assert!(e.to_string().contains($error),
                        "bad error: {}", e);
            }
        }

        println!("attempting toml decoding");
        match $toml.try_into::<$ty>() {
            Ok(_) => panic!("successful"),
            Err(e) => {
                assert!(e.to_string().contains($error),
                        "bad error: {}", e);
            }
        }
    })
}

macro_rules! map( ($($k:ident: $v:expr),*) => ({
    let mut _m = BTreeMap::new();
    $(_m.insert(stringify!($k).to_string(), $v);)*
    _m
}) );

#[test]
fn smoke() {
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo { a: isize }

    equivalent!(
        Foo { a: 2 },
        Table(map! { a: Integer(2) }),
    );
}

#[test]
fn smoke_hyphen() {
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo {
        a_b: isize,
    }

    equivalent! {
        Foo { a_b: 2 },
        Table(map! { a_b: Integer(2) }),
    }

    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo2 {
        #[serde(rename = "a-b")]
        a_b: isize,
    }

    let mut m = BTreeMap::new();
    m.insert("a-b".to_string(), Integer(2));
    equivalent! {
        Foo2 { a_b: 2 },
        Table(m),
    }
}

#[test]
fn nested() {
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo { a: isize, b: Bar }
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Bar { a: String }

    equivalent! {
        Foo { a: 2, b: Bar { a: "test".to_string() } },
        Table(map! {
            a: Integer(2),
            b: Table(map! {
                a: Value::String("test".to_string())
            })
        }),
    }
}

#[test]
fn application_decode_error() {
    #[derive(PartialEq, Debug)]
    struct Range10(usize);
    impl<'de> Deserialize<'de> for Range10 {
         fn deserialize<D: Deserializer<'de>>(d: D) -> Result<Range10, D::Error> {
             let x: usize = try!(Deserialize::deserialize(d));
             if x > 10 {
                 Err(serde::de::Error::custom("more than 10"))
             } else {
                 Ok(Range10(x))
             }
         }
    }
    let d_good = Integer(5);
    let d_bad1 = Value::String("not an isize".to_string());
    let d_bad2 = Integer(11);

    assert_eq!(Range10(5), d_good.try_into().unwrap());

    let err1: Result<Range10, _> = d_bad1.try_into();
    assert!(err1.is_err());
    let err2: Result<Range10, _> = d_bad2.try_into();
    assert!(err2.is_err());
}

#[test]
fn array() {
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo { a: Vec<isize> }

    equivalent! {
        Foo { a: vec![1, 2, 3, 4] },
        Table(map! {
            a: Array(vec![
                Integer(1),
                Integer(2),
                Integer(3),
                Integer(4)
            ])
        }),
    };
}

#[test]
fn inner_structs_with_options() {
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo {
        a: Option<Box<Foo>>,
        b: Bar,
    }
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Bar {
        a: String,
        b: f64,
    }

    equivalent! {
        Foo {
            a: Some(Box::new(Foo {
                a: None,
                b: Bar { a: "foo".to_string(), b: 4.5 },
            })),
            b: Bar { a: "bar".to_string(), b: 1.0 },
        },
        Table(map! {
            a: Table(map! {
                b: Table(map! {
                    a: Value::String("foo".to_string()),
                    b: Float(4.5)
                })
            }),
            b: Table(map! {
                a: Value::String("bar".to_string()),
                b: Float(1.0)
            })
        }),
    }
}

#[test]
fn hashmap() {
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo {
        set: HashSet<char>,
        map: BTreeMap<String, isize>,
    }

    equivalent! {
        Foo {
            map: {
                let mut m = BTreeMap::new();
                m.insert("foo".to_string(), 10);
                m.insert("bar".to_string(), 4);
                m
            },
            set: {
                let mut s = HashSet::new();
                s.insert('a');
                s
            },
        },
        Table(map! {
            map: Table(map! {
                foo: Integer(10),
                bar: Integer(4)
            }),
            set: Array(vec![Value::String("a".to_string())])
        }),
    }
}

#[test]
fn table_array() {
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo { a: Vec<Bar>, }
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Bar { a: isize }

    equivalent! {
        Foo { a: vec![Bar { a: 1 }, Bar { a: 2 }] },
        Table(map! {
            a: Array(vec![
                Table(map!{ a: Integer(1) }),
                Table(map!{ a: Integer(2) }),
            ])
        }),
    }
}

#[test]
fn type_errors() {
    #[derive(Deserialize)]
    #[allow(dead_code)]
    struct Foo { bar: isize }

    error! {
        Foo,
        Table(map! {
            bar: Value::String("a".to_string())
        }),
        "invalid type: string \"a\", expected isize for key `bar`"
    }

    #[derive(Deserialize)]
    #[allow(dead_code)]
    struct Bar { foo: Foo }

    error! {
        Bar,
        Table(map! {
            foo: Table(map! {
                bar: Value::String("a".to_string())
            })
        }),
        "invalid type: string \"a\", expected isize for key `foo.bar`"
    }
}

#[test]
fn missing_errors() {
    #[derive(Serialize, Deserialize, PartialEq, Debug)]
    struct Foo { bar: isize }

    error! {
        Foo,
        Table(map! { }),
        "missing field `bar`"
    }
}

#[test]
fn parse_enum() {
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo { a: E }
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    #[serde(untagged)]
    enum E {
        Bar(isize),
        Baz(String),
        Last(Foo2),
    }
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo2 {
        test: String,
    }

    equivalent! {
        Foo { a: E::Bar(10) },
        Table(map! { a: Integer(10) }),
    }

    equivalent! {
        Foo { a: E::Baz("foo".to_string()) },
        Table(map! { a: Value::String("foo".to_string()) }),
    }

    equivalent! {
        Foo { a: E::Last(Foo2 { test: "test".to_string() }) },
        Table(map! { a: Table(map! { test: Value::String("test".to_string()) }) }),
    }
}

#[test]
fn parse_enum_string() {
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo { a: Sort }

    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    #[serde(rename_all = "lowercase")]
    enum Sort {
        Asc,
        Desc,
    }

    equivalent! {
        Foo { a: Sort::Desc },
        Table(map! { a: Value::String("desc".to_string()) }),
    }

}

// #[test]
// fn unused_fields() {
//     #[derive(Serialize, Deserialize, PartialEq, Debug)]
//     struct Foo { a: isize }
//
//     let v = Foo { a: 2 };
//     let mut d = Decoder::new(Table(map! {
//         a, Integer(2),
//         b, Integer(5)
//     }));
//     assert_eq!(v, t!(Deserialize::deserialize(&mut d)));
//
//     assert_eq!(d.toml, Some(Table(map! {
//         b, Integer(5)
//     })));
// }
//
// #[test]
// fn unused_fields2() {
//     #[derive(Serialize, Deserialize, PartialEq, Debug)]
//     struct Foo { a: Bar }
//     #[derive(Serialize, Deserialize, PartialEq, Debug)]
//     struct Bar { a: isize }
//
//     let v = Foo { a: Bar { a: 2 } };
//     let mut d = Decoder::new(Table(map! {
//         a, Table(map! {
//             a, Integer(2),
//             b, Integer(5)
//         })
//     }));
//     assert_eq!(v, t!(Deserialize::deserialize(&mut d)));
//
//     assert_eq!(d.toml, Some(Table(map! {
//         a, Table(map! {
//             b, Integer(5)
//         })
//     })));
// }
//
// #[test]
// fn unused_fields3() {
//     #[derive(Serialize, Deserialize, PartialEq, Debug)]
//     struct Foo { a: Bar }
//     #[derive(Serialize, Deserialize, PartialEq, Debug)]
//     struct Bar { a: isize }
//
//     let v = Foo { a: Bar { a: 2 } };
//     let mut d = Decoder::new(Table(map! {
//         a, Table(map! {
//             a, Integer(2)
//         })
//     }));
//     assert_eq!(v, t!(Deserialize::deserialize(&mut d)));
//
//     assert_eq!(d.toml, None);
// }
//
// #[test]
// fn unused_fields4() {
//     #[derive(Serialize, Deserialize, PartialEq, Debug)]
//     struct Foo { a: BTreeMap<String, String> }
//
//     let v = Foo { a: map! { a, "foo".to_string() } };
//     let mut d = Decoder::new(Table(map! {
//         a, Table(map! {
//             a, Value::String("foo".to_string())
//         })
//     }));
//     assert_eq!(v, t!(Deserialize::deserialize(&mut d)));
//
//     assert_eq!(d.toml, None);
// }
//
// #[test]
// fn unused_fields5() {
//     #[derive(Serialize, Deserialize, PartialEq, Debug)]
//     struct Foo { a: Vec<String> }
//
//     let v = Foo { a: vec!["a".to_string()] };
//     let mut d = Decoder::new(Table(map! {
//         a, Array(vec![Value::String("a".to_string())])
//     }));
//     assert_eq!(v, t!(Deserialize::deserialize(&mut d)));
//
//     assert_eq!(d.toml, None);
// }
//
// #[test]
// fn unused_fields6() {
//     #[derive(Serialize, Deserialize, PartialEq, Debug)]
//     struct Foo { a: Option<Vec<String>> }
//
//     let v = Foo { a: Some(vec![]) };
//     let mut d = Decoder::new(Table(map! {
//         a, Array(vec![])
//     }));
//     assert_eq!(v, t!(Deserialize::deserialize(&mut d)));
//
//     assert_eq!(d.toml, None);
// }
//
// #[test]
// fn unused_fields7() {
//     #[derive(Serialize, Deserialize, PartialEq, Debug)]
//     struct Foo { a: Vec<Bar> }
//     #[derive(Serialize, Deserialize, PartialEq, Debug)]
//     struct Bar { a: isize }
//
//     let v = Foo { a: vec![Bar { a: 1 }] };
//     let mut d = Decoder::new(Table(map! {
//         a, Array(vec![Table(map! {
//             a, Integer(1),
//             b, Integer(2)
//         })])
//     }));
//     assert_eq!(v, t!(Deserialize::deserialize(&mut d)));
//
//     assert_eq!(d.toml, Some(Table(map! {
//         a, Array(vec![Table(map! {
//             b, Integer(2)
//         })])
//     })));
// }

#[test]
fn empty_arrays() {
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo { a: Vec<Bar> }
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Bar;

    equivalent! {
        Foo { a: vec![] },
        Table(map! {a: Array(Vec::new())}),
    }
}

#[test]
fn empty_arrays2() {
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Foo { a: Option<Vec<Bar>> }
    #[derive(Serialize, Deserialize, PartialEq, Debug, Clone)]
    struct Bar;

    equivalent! {
        Foo { a: None },
        Table(map! {}),
    }

    equivalent!{
        Foo { a: Some(vec![]) },
        Table(map! { a: Array(vec![]) }),
    }
}

#[test]
fn extra_keys() {
    #[derive(Serialize, Deserialize)]
    struct Foo { a: isize }

    let toml = Table(map! { a: Integer(2), b: Integer(2) });
    assert!(toml.clone().try_into::<Foo>().is_ok());
    assert!(toml::from_str::<Foo>(&toml.to_string()).is_ok());
}

#[test]
fn newtypes() {
    #[derive(Deserialize, Serialize, PartialEq, Debug, Clone)]
    struct A {
        b: B
    }

    #[derive(Deserialize, Serialize, PartialEq, Debug, Clone)]
    struct B(u32);

    equivalent! {
        A { b: B(2) },
        Table(map! { b: Integer(2) }),
    }
}

#[test]
fn newtypes2() {
    #[derive(Deserialize, Serialize, PartialEq, Debug, Clone)]
	struct A {
		b: B
	}

    #[derive(Deserialize, Serialize, PartialEq, Debug, Clone)]
	struct B(Option<C>);

    #[derive(Deserialize, Serialize, PartialEq, Debug, Clone)]
	struct C {
		x: u32,
		y: u32,
		z: u32
	}

    equivalent! {
        A { b: B(Some(C { x: 0, y: 1, z: 2 })) },
        Table(map! {
            b: Table(map! {
                x: Integer(0),
                y: Integer(1),
                z: Integer(2)
            })
        }),
    }
}

#[derive(Debug, Default, PartialEq, Serialize, Deserialize)]
struct CanBeEmpty {
    a: Option<String>,
    b: Option<String>,
}

#[test]
fn table_structs_empty() {
    let text = "[bar]\n\n[baz]\n\n[bazv]\na = \"foo\"\n\n[foo]\n";
    let value: BTreeMap<String, CanBeEmpty> = toml::from_str(text).unwrap();
    let mut expected: BTreeMap<String, CanBeEmpty> = BTreeMap::new();
    expected.insert("bar".to_string(), CanBeEmpty::default());
    expected.insert("baz".to_string(), CanBeEmpty::default());
    expected.insert(
        "bazv".to_string(), 
        CanBeEmpty {a: Some("foo".to_string()), b: None},
    );
    expected.insert("foo".to_string(), CanBeEmpty::default());
    assert_eq!(value, expected);
    assert_eq!(toml::to_string(&value).unwrap(), text);
}
