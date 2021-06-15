use serde::{de::DeserializeOwned, ser::Serialize};
use std::{collections::BTreeMap, fmt::Debug};

use crate::{
    stream::{private::Sealed, Event, Writer},
    Date, Deserializer, Error, Integer, Serializer, Uid,
};

struct VecWriter {
    events: Vec<Event>,
}

impl VecWriter {
    pub fn new() -> VecWriter {
        VecWriter { events: Vec::new() }
    }

    pub fn into_inner(self) -> Vec<Event> {
        self.events
    }
}

impl Writer for VecWriter {
    fn write_start_array(&mut self, len: Option<u64>) -> Result<(), Error> {
        self.events.push(Event::StartArray(len));
        Ok(())
    }

    fn write_start_dictionary(&mut self, len: Option<u64>) -> Result<(), Error> {
        self.events.push(Event::StartDictionary(len));
        Ok(())
    }

    fn write_end_collection(&mut self) -> Result<(), Error> {
        self.events.push(Event::EndCollection);
        Ok(())
    }

    fn write_boolean(&mut self, value: bool) -> Result<(), Error> {
        self.events.push(Event::Boolean(value));
        Ok(())
    }

    fn write_data(&mut self, value: &[u8]) -> Result<(), Error> {
        self.events.push(Event::Data(value.to_owned()));
        Ok(())
    }

    fn write_date(&mut self, value: Date) -> Result<(), Error> {
        self.events.push(Event::Date(value));
        Ok(())
    }

    fn write_integer(&mut self, value: Integer) -> Result<(), Error> {
        self.events.push(Event::Integer(value));
        Ok(())
    }

    fn write_real(&mut self, value: f64) -> Result<(), Error> {
        self.events.push(Event::Real(value));
        Ok(())
    }

    fn write_string(&mut self, value: &str) -> Result<(), Error> {
        self.events.push(Event::String(value.to_owned()));
        Ok(())
    }

    fn write_uid(&mut self, value: Uid) -> Result<(), Error> {
        self.events.push(Event::Uid(value));
        Ok(())
    }
}

impl Sealed for VecWriter {}

fn new_serializer() -> Serializer<VecWriter> {
    Serializer::new(VecWriter::new())
}

fn new_deserializer(events: Vec<Event>) -> Deserializer<Vec<Result<Event, Error>>> {
    let result_events = events.into_iter().map(Ok).collect();
    Deserializer::new(result_events)
}

fn assert_roundtrip<T>(obj: T, comparison: Option<&[Event]>)
where
    T: Debug + DeserializeOwned + PartialEq + Serialize,
{
    let mut se = new_serializer();

    obj.serialize(&mut se).unwrap();

    let events = se.into_inner().into_inner();

    if let Some(comparison) = comparison {
        assert_eq!(&events[..], comparison);
    }

    let mut de = new_deserializer(events);

    let new_obj = T::deserialize(&mut de).unwrap();

    assert_eq!(new_obj, obj);
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
enum Animal {
    Cow,
    Dog(DogOuter),
    Frog(Result<String, bool>, Option<Vec<f64>>),
    Cat {
        age: Integer,
        name: String,
        firmware: Option<Vec<u8>>,
    },
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
struct DogOuter {
    inner: Vec<DogInner>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
struct DogInner {
    a: (),
    b: usize,
    c: Vec<String>,
    d: Option<Uid>,
}

#[test]
fn cow() {
    let cow = Animal::Cow;

    let comparison = &[
        Event::StartDictionary(Some(1)),
        Event::String("Cow".to_owned()),
        Event::String("".to_owned()),
        Event::EndCollection,
    ];

    assert_roundtrip(cow, Some(comparison));
}

#[test]
fn dog() {
    let dog = Animal::Dog(DogOuter {
        inner: vec![DogInner {
            a: (),
            b: 12,
            c: vec!["a".to_string(), "b".to_string()],
            d: Some(Uid::new(42)),
        }],
    });

    let comparison = &[
        Event::StartDictionary(Some(1)),
        Event::String("Dog".to_owned()),
        Event::StartDictionary(None),
        Event::String("inner".to_owned()),
        Event::StartArray(Some(1)),
        Event::StartDictionary(None),
        Event::String("a".to_owned()),
        Event::String("".to_owned()),
        Event::String("b".to_owned()),
        Event::Integer(12.into()),
        Event::String("c".to_owned()),
        Event::StartArray(Some(2)),
        Event::String("a".to_owned()),
        Event::String("b".to_owned()),
        Event::EndCollection,
        Event::String("d".to_owned()),
        Event::Uid(Uid::new(42)),
        Event::EndCollection,
        Event::EndCollection,
        Event::EndCollection,
        Event::EndCollection,
    ];

    assert_roundtrip(dog, Some(comparison));
}

#[test]
fn frog() {
    let frog = Animal::Frog(
        Ok("hello".to_owned()),
        Some(vec![1.0, 2.0, 3.14159, 0.000000001, 1.27e31]),
    );

    let comparison = &[
        Event::StartDictionary(Some(1)),
        Event::String("Frog".to_owned()),
        Event::StartArray(Some(2)),
        Event::StartDictionary(Some(1)),
        Event::String("Ok".to_owned()),
        Event::String("hello".to_owned()),
        Event::EndCollection,
        Event::StartDictionary(Some(1)),
        Event::String("Some".to_owned()),
        Event::StartArray(Some(5)),
        Event::Real(1.0),
        Event::Real(2.0),
        Event::Real(3.14159),
        Event::Real(0.000000001),
        Event::Real(1.27e31),
        Event::EndCollection,
        Event::EndCollection,
        Event::EndCollection,
        Event::EndCollection,
    ];

    assert_roundtrip(frog, Some(comparison));
}

#[test]
fn cat_with_firmware() {
    let cat = Animal::Cat {
        age: 12.into(),
        name: "Paws".to_owned(),
        firmware: Some(vec![0, 1, 2, 3, 4, 5, 6, 7, 8]),
    };

    let comparison = &[
        Event::StartDictionary(Some(1)),
        Event::String("Cat".to_owned()),
        Event::StartDictionary(None),
        Event::String("age".to_owned()),
        Event::Integer(12.into()),
        Event::String("name".to_owned()),
        Event::String("Paws".to_owned()),
        Event::String("firmware".to_owned()),
        Event::StartArray(Some(9)),
        Event::Integer(0.into()),
        Event::Integer(1.into()),
        Event::Integer(2.into()),
        Event::Integer(3.into()),
        Event::Integer(4.into()),
        Event::Integer(5.into()),
        Event::Integer(6.into()),
        Event::Integer(7.into()),
        Event::Integer(8.into()),
        Event::EndCollection,
        Event::EndCollection,
        Event::EndCollection,
    ];

    assert_roundtrip(cat, Some(comparison));
}

#[test]
fn cat_without_firmware() {
    let cat = Animal::Cat {
        age: Integer::from(-12),
        name: "Paws".to_owned(),
        firmware: None,
    };

    let comparison = &[
        Event::StartDictionary(Some(1)),
        Event::String("Cat".to_owned()),
        Event::StartDictionary(None),
        Event::String("age".to_owned()),
        Event::Integer(Integer::from(-12)),
        Event::String("name".to_owned()),
        Event::String("Paws".to_owned()),
        Event::EndCollection,
        Event::EndCollection,
    ];

    assert_roundtrip(cat, Some(comparison));
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
struct NewtypeStruct(NewtypeInner);

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
struct NewtypeInner(u8, u8, u8);

#[test]
fn newtype_struct() {
    let newtype = NewtypeStruct(NewtypeInner(34, 32, 13));

    let comparison = &[
        Event::StartArray(Some(3)),
        Event::Integer(34.into()),
        Event::Integer(32.into()),
        Event::Integer(13.into()),
        Event::EndCollection,
    ];

    assert_roundtrip(newtype, Some(comparison));
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
struct TypeWithOptions {
    a: Option<String>,
    b: Option<Option<u32>>,
    c: Option<Box<TypeWithOptions>>,
}

#[test]
fn type_with_options() {
    let inner = TypeWithOptions {
        a: None,
        b: Some(Some(12)),
        c: None,
    };

    let obj = TypeWithOptions {
        a: Some("hello".to_owned()),
        b: Some(None),
        c: Some(Box::new(inner)),
    };

    let comparison = &[
        Event::StartDictionary(None),
        Event::String("a".to_owned()),
        Event::String("hello".to_owned()),
        Event::String("b".to_owned()),
        Event::StartDictionary(Some(1)),
        Event::String("None".to_owned()),
        Event::String("".to_owned()),
        Event::EndCollection,
        Event::String("c".to_owned()),
        Event::StartDictionary(None),
        Event::String("b".to_owned()),
        Event::StartDictionary(Some(1)),
        Event::String("Some".to_owned()),
        Event::Integer(12.into()),
        Event::EndCollection,
        Event::EndCollection,
        Event::EndCollection,
    ];

    assert_roundtrip(obj, Some(comparison));
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
struct TypeWithDate {
    a: Option<i32>,
    b: Option<Date>,
}

#[test]
fn type_with_date() {
    let date = Date::from_rfc3339("1920-01-01T00:10:00Z").unwrap();

    let obj = TypeWithDate {
        a: Some(28),
        b: Some(date.clone()),
    };

    let comparison = &[
        Event::StartDictionary(None),
        Event::String("a".to_owned()),
        Event::Integer(28.into()),
        Event::String("b".to_owned()),
        Event::Date(date),
        Event::EndCollection,
    ];

    assert_roundtrip(obj, Some(comparison));
}

#[test]
fn option_some() {
    let obj = Some(12);

    let comparison = &[Event::Integer(12.into())];

    assert_roundtrip(obj, Some(comparison));
}

#[test]
fn option_none() {
    let obj: Option<u32> = None;

    let comparison = &[];

    assert_roundtrip(obj, Some(comparison));
}

#[test]
fn option_some_some() {
    let obj = Some(Some(12));

    let comparison = &[
        Event::StartDictionary(Some(1)),
        Event::String("Some".to_owned()),
        Event::Integer(12.into()),
        Event::EndCollection,
    ];

    assert_roundtrip(obj, Some(comparison));
}

#[test]
fn option_some_none() {
    let obj: Option<Option<u32>> = Some(None);

    let comparison = &[
        Event::StartDictionary(Some(1)),
        Event::String("None".to_owned()),
        Event::String("".to_owned()),
        Event::EndCollection,
    ];

    assert_roundtrip(obj, Some(comparison));
}

#[test]
fn option_dictionary_values() {
    let mut obj = BTreeMap::new();
    obj.insert("a".to_owned(), None);
    obj.insert("b".to_owned(), Some(None));
    obj.insert("c".to_owned(), Some(Some(144)));

    let comparison = &[
        Event::StartDictionary(Some(3)),
        Event::String("a".to_owned()),
        Event::StartDictionary(Some(1)),
        Event::String("None".to_owned()),
        Event::String("".to_owned()),
        Event::EndCollection,
        Event::String("b".to_owned()),
        Event::StartDictionary(Some(1)),
        Event::String("Some".to_owned()),
        Event::StartDictionary(Some(1)),
        Event::String("None".to_owned()),
        Event::String("".to_owned()),
        Event::EndCollection,
        Event::EndCollection,
        Event::String("c".to_owned()),
        Event::StartDictionary(Some(1)),
        Event::String("Some".to_owned()),
        Event::StartDictionary(Some(1)),
        Event::String("Some".to_owned()),
        Event::Integer(144.into()),
        Event::EndCollection,
        Event::EndCollection,
        Event::EndCollection,
    ];

    assert_roundtrip(obj, Some(comparison));
}

#[test]
fn option_dictionary_keys() {
    let mut obj = BTreeMap::new();
    obj.insert(None, 1);
    obj.insert(Some(None), 2);
    obj.insert(Some(Some(144)), 3);

    let comparison = &[
        Event::StartDictionary(Some(3)),
        Event::StartDictionary(Some(1)),
        Event::String("None".to_owned()),
        Event::String("".to_owned()),
        Event::EndCollection,
        Event::Integer(1.into()),
        Event::StartDictionary(Some(1)),
        Event::String("Some".to_owned()),
        Event::StartDictionary(Some(1)),
        Event::String("None".to_owned()),
        Event::String("".to_owned()),
        Event::EndCollection,
        Event::EndCollection,
        Event::Integer(2.into()),
        Event::StartDictionary(Some(1)),
        Event::String("Some".to_owned()),
        Event::StartDictionary(Some(1)),
        Event::String("Some".to_owned()),
        Event::Integer(144.into()),
        Event::EndCollection,
        Event::EndCollection,
        Event::Integer(3.into()),
        Event::EndCollection,
    ];

    assert_roundtrip(obj, Some(comparison));
}

#[test]
fn option_array() {
    let obj = vec![None, Some(None), Some(Some(144))];

    let comparison = &[
        Event::StartArray(Some(3)),
        Event::StartDictionary(Some(1)),
        Event::String("None".to_owned()),
        Event::String("".to_owned()),
        Event::EndCollection,
        Event::StartDictionary(Some(1)),
        Event::String("Some".to_owned()),
        Event::StartDictionary(Some(1)),
        Event::String("None".to_owned()),
        Event::String("".to_owned()),
        Event::EndCollection,
        Event::EndCollection,
        Event::StartDictionary(Some(1)),
        Event::String("Some".to_owned()),
        Event::StartDictionary(Some(1)),
        Event::String("Some".to_owned()),
        Event::Integer(144.into()),
        Event::EndCollection,
        Event::EndCollection,
        Event::EndCollection,
    ];

    assert_roundtrip(obj, Some(comparison));
}
