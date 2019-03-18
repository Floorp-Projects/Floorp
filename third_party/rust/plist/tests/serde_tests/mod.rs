use plist::stream::Event;
use plist::stream::Event::*;
use plist::stream::Writer;
use plist::{Date, Deserializer, Error, Serializer};
use serde::de::DeserializeOwned;
use serde::ser::Serialize;
use std::fmt::Debug;
use std::time::SystemTime;

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
    fn write(&mut self, event: &Event) -> Result<(), Error> {
        self.events.push(event.clone());
        Ok(())
    }
}

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
    Frog(Result<String, bool>, Vec<f64>),
    Cat {
        age: usize,
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
}

#[test]
fn cow() {
    let cow = Animal::Cow;

    let comparison = &[
        StartDictionary(Some(1)),
        StringValue("Cow".to_owned()),
        StringValue("".to_owned()),
        EndDictionary,
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
        }],
    });

    let comparison = &[
        StartDictionary(Some(1)),
        StringValue("Dog".to_owned()),
        StartDictionary(None),
        StringValue("inner".to_owned()),
        StartArray(Some(1)),
        StartDictionary(None),
        StringValue("a".to_owned()),
        StringValue("".to_owned()),
        StringValue("b".to_owned()),
        IntegerValue(12),
        StringValue("c".to_owned()),
        StartArray(Some(2)),
        StringValue("a".to_owned()),
        StringValue("b".to_owned()),
        EndArray,
        EndDictionary,
        EndArray,
        EndDictionary,
        EndDictionary,
    ];

    assert_roundtrip(dog, Some(comparison));
}

#[test]
fn frog() {
    let frog = Animal::Frog(
        Ok("hello".to_owned()),
        vec![1.0, 2.0, 3.14159, 0.000000001, 1.27e31],
    );

    let comparison = &[
        StartDictionary(Some(1)),
        StringValue("Frog".to_owned()),
        StartArray(Some(2)),
        StartDictionary(Some(1)),
        StringValue("Ok".to_owned()),
        StringValue("hello".to_owned()),
        EndDictionary,
        StartArray(Some(5)),
        RealValue(1.0),
        RealValue(2.0),
        RealValue(3.14159),
        RealValue(0.000000001),
        RealValue(1.27e31),
        EndArray,
        EndArray,
        EndDictionary,
    ];

    assert_roundtrip(frog, Some(comparison));
}

#[test]
fn cat() {
    let cat = Animal::Cat {
        age: 12,
        name: "Paws".to_owned(),
        firmware: Some(vec![0, 1, 2, 3, 4, 5, 6, 7, 8]),
    };

    let comparison = &[
        StartDictionary(Some(1)),
        StringValue("Cat".to_owned()),
        StartDictionary(None),
        StringValue("age".to_owned()),
        IntegerValue(12),
        StringValue("name".to_owned()),
        StringValue("Paws".to_owned()),
        StringValue("firmware".to_owned()),
        StartArray(Some(9)),
        IntegerValue(0),
        IntegerValue(1),
        IntegerValue(2),
        IntegerValue(3),
        IntegerValue(4),
        IntegerValue(5),
        IntegerValue(6),
        IntegerValue(7),
        IntegerValue(8),
        EndArray,
        EndDictionary,
        EndDictionary,
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
        StartArray(Some(3)),
        IntegerValue(34),
        IntegerValue(32),
        IntegerValue(13),
        EndArray,
    ];

    assert_roundtrip(newtype, Some(comparison));
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
struct TypeWithOptions {
    a: Option<String>,
    b: Option<u32>,
    c: Option<Box<TypeWithOptions>>,
}

#[test]
fn type_with_options() {
    let inner = TypeWithOptions {
        a: None,
        b: Some(12),
        c: None,
    };

    let obj = TypeWithOptions {
        a: Some("hello".to_owned()),
        b: None,
        c: Some(Box::new(inner)),
    };

    let comparison = &[
        StartDictionary(None),
        StringValue("a".to_owned()),
        StringValue("hello".to_owned()),
        StringValue("c".to_owned()),
        StartDictionary(None),
        StringValue("b".to_owned()),
        IntegerValue(12),
        EndDictionary,
        EndDictionary,
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
    let date: Date = SystemTime::now().into();

    let obj = TypeWithDate {
        a: Some(28),
        b: Some(date.clone()),
    };

    let comparison = &[
        StartDictionary(None),
        StringValue("a".to_owned()),
        IntegerValue(28),
        StringValue("b".to_owned()),
        DateValue(date),
        EndDictionary,
    ];

    assert_roundtrip(obj, Some(comparison));
}
