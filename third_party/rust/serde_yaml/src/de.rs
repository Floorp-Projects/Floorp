//! YAML Deserialization
//!
//! This module provides YAML deserialization with the type `Deserializer`.

use std::collections::BTreeMap;
use std::f64;
use std::fmt;
use std::io;
use std::str;

use yaml_rust::parser::{Event as YamlEvent, MarkedEventReceiver, Parser};
use yaml_rust::scanner::{Marker, TScalarStyle, TokenType};

use serde::de::IgnoredAny as Ignore;
use serde::de::{
    self, Deserialize, DeserializeOwned, DeserializeSeed, Expected, IntoDeserializer, Unexpected,
    Visitor,
};

use error::{Error, Result};
use path::Path;
use private;

pub struct Loader {
    events: Vec<(Event, Marker)>,
    /// Map from alias id to index in events.
    aliases: BTreeMap<usize, usize>,
}

impl MarkedEventReceiver for Loader {
    fn on_event(&mut self, event: YamlEvent, marker: Marker) {
        let event = match event {
            YamlEvent::Nothing
            | YamlEvent::StreamStart
            | YamlEvent::StreamEnd
            | YamlEvent::DocumentStart
            | YamlEvent::DocumentEnd => return,

            YamlEvent::Alias(id) => Event::Alias(id),
            YamlEvent::Scalar(value, style, id, tag) => {
                self.aliases.insert(id, self.events.len());
                Event::Scalar(value, style, tag)
            }
            YamlEvent::SequenceStart(id) => {
                self.aliases.insert(id, self.events.len());
                Event::SequenceStart
            }
            YamlEvent::SequenceEnd => Event::SequenceEnd,
            YamlEvent::MappingStart(id) => {
                self.aliases.insert(id, self.events.len());
                Event::MappingStart
            }
            YamlEvent::MappingEnd => Event::MappingEnd,
        };
        self.events.push((event, marker));
    }
}

#[derive(Debug, PartialEq)]
enum Event {
    Alias(usize),
    Scalar(String, TScalarStyle, Option<TokenType>),
    SequenceStart,
    SequenceEnd,
    MappingStart,
    MappingEnd,
}

struct Deserializer<'a> {
    events: &'a [(Event, Marker)],
    /// Map from alias id to index in events.
    aliases: &'a BTreeMap<usize, usize>,
    pos: &'a mut usize,
    path: Path<'a>,
    remaining_depth: u8,
}

impl<'a> Deserializer<'a> {
    fn peek(&self) -> Result<(&'a Event, Marker)> {
        match self.events.get(*self.pos) {
            Some(event) => Ok((&event.0, event.1)),
            None => Err(private::error_end_of_stream()),
        }
    }

    fn next(&mut self) -> Result<(&'a Event, Marker)> {
        self.opt_next().ok_or_else(private::error_end_of_stream)
    }

    fn opt_next(&mut self) -> Option<(&'a Event, Marker)> {
        self.events.get(*self.pos).map(|event| {
            *self.pos += 1;
            (&event.0, event.1)
        })
    }

    fn jump(&'a self, pos: &'a mut usize) -> Result<Deserializer<'a>> {
        match self.aliases.get(pos) {
            Some(&found) => {
                *pos = found;
                Ok(Deserializer {
                    events: self.events,
                    aliases: self.aliases,
                    pos: pos,
                    path: Path::Alias { parent: &self.path },
                    remaining_depth: self.remaining_depth,
                })
            }
            None => panic!("unresolved alias: {}", *pos),
        }
    }

    fn ignore_any(&mut self) -> Result<()> {
        enum Nest {
            Sequence,
            Mapping,
        }

        let mut stack = Vec::new();

        while let Some((event, _)) = self.opt_next() {
            match *event {
                Event::Alias(_) | Event::Scalar(_, _, _) => {}
                Event::SequenceStart => {
                    stack.push(Nest::Sequence);
                }
                Event::MappingStart => {
                    stack.push(Nest::Mapping);
                }
                Event::SequenceEnd => match stack.pop() {
                    Some(Nest::Sequence) => {}
                    None | Some(Nest::Mapping) => {
                        panic!("unexpected end of sequence");
                    }
                },
                Event::MappingEnd => match stack.pop() {
                    Some(Nest::Mapping) => {}
                    None | Some(Nest::Sequence) => {
                        panic!("unexpected end of mapping");
                    }
                },
            }
            if stack.is_empty() {
                return Ok(());
            }
        }

        if !stack.is_empty() {
            panic!("missing end event");
        }

        Ok(())
    }

    fn visit_sequence<'de, V>(&mut self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        let (value, len) = self.recursion_check(|de| {
            let mut seq = SeqAccess { de: de, len: 0 };
            let value = visitor.visit_seq(&mut seq)?;
            Ok((value, seq.len))
        })?;
        self.end_sequence(len)?;
        Ok(value)
    }

    fn visit_mapping<'de, V>(&mut self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        let (value, len) = self.recursion_check(|de| {
            let mut map = MapAccess {
                de: de,
                len: 0,
                key: None,
            };
            let value = visitor.visit_map(&mut map)?;
            Ok((value, map.len))
        })?;
        self.end_mapping(len)?;
        Ok(value)
    }

    fn end_sequence(&mut self, len: usize) -> Result<()> {
        let total = {
            let mut seq = SeqAccess { de: self, len: len };
            while de::SeqAccess::next_element::<Ignore>(&mut seq)?.is_some() {}
            seq.len
        };
        assert_eq!(Event::SequenceEnd, *self.next()?.0);
        if total == len {
            Ok(())
        } else {
            struct ExpectedSeq(usize);
            impl Expected for ExpectedSeq {
                fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                    if self.0 == 1 {
                        write!(formatter, "sequence of 1 element")
                    } else {
                        write!(formatter, "sequence of {} elements", self.0)
                    }
                }
            }
            Err(de::Error::invalid_length(total, &ExpectedSeq(len)))
        }
    }

    fn end_mapping(&mut self, len: usize) -> Result<()> {
        let total = {
            let mut map = MapAccess {
                de: self,
                len: len,
                key: None,
            };
            while de::MapAccess::next_entry::<Ignore, Ignore>(&mut map)?.is_some() {}
            map.len
        };
        assert_eq!(Event::MappingEnd, *self.next()?.0);
        if total == len {
            Ok(())
        } else {
            struct ExpectedMap(usize);
            impl Expected for ExpectedMap {
                fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                    if self.0 == 1 {
                        write!(formatter, "map containing 1 entry")
                    } else {
                        write!(formatter, "map containing {} entries", self.0)
                    }
                }
            }
            Err(de::Error::invalid_length(total, &ExpectedMap(len)))
        }
    }

    fn recursion_check<F: FnOnce(&mut Self) -> Result<T>, T>(&mut self, f: F) -> Result<T> {
        let previous_depth = self.remaining_depth;
        self.remaining_depth = previous_depth
            .checked_sub(1)
            .ok_or_else(private::error_recursion_limit_exceeded)?;
        let result = f(self);
        self.remaining_depth = previous_depth;
        result
    }
}

fn visit_scalar<'de, V>(
    v: &str,
    style: TScalarStyle,
    tag: &Option<TokenType>,
    visitor: V,
) -> Result<V::Value>
where
    V: Visitor<'de>,
{
    if style != TScalarStyle::Plain {
        visitor.visit_str(v)
    } else if let Some(TokenType::Tag(ref handle, ref suffix)) = *tag {
        if handle == "!!" {
            match suffix.as_ref() {
                "bool" => match v.parse::<bool>() {
                    Ok(v) => visitor.visit_bool(v),
                    Err(_) => Err(de::Error::invalid_value(Unexpected::Str(v), &"a boolean")),
                },
                "int" => match v.parse::<i64>() {
                    Ok(v) => visitor.visit_i64(v),
                    Err(_) => Err(de::Error::invalid_value(Unexpected::Str(v), &"an integer")),
                },
                "float" => match v.parse::<f64>() {
                    Ok(v) => visitor.visit_f64(v),
                    Err(_) => Err(de::Error::invalid_value(Unexpected::Str(v), &"a float")),
                },
                "null" => match v {
                    "~" | "null" => visitor.visit_unit(),
                    _ => Err(de::Error::invalid_value(Unexpected::Str(v), &"null")),
                },
                _ => visitor.visit_str(v),
            }
        } else {
            visitor.visit_str(v)
        }
    } else {
        visit_untagged_str(visitor, v)
    }
}

struct SeqAccess<'a: 'r, 'r> {
    de: &'r mut Deserializer<'a>,
    len: usize,
}

impl<'de, 'a, 'r> de::SeqAccess<'de> for SeqAccess<'a, 'r> {
    type Error = Error;

    fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>>
    where
        T: DeserializeSeed<'de>,
    {
        match *self.de.peek()?.0 {
            Event::SequenceEnd => Ok(None),
            _ => {
                let mut element_de = Deserializer {
                    events: self.de.events,
                    aliases: self.de.aliases,
                    pos: self.de.pos,
                    path: Path::Seq {
                        parent: &self.de.path,
                        index: self.len,
                    },
                    remaining_depth: self.de.remaining_depth,
                };
                self.len += 1;
                seed.deserialize(&mut element_de).map(Some)
            }
        }
    }
}

struct MapAccess<'a: 'r, 'r> {
    de: &'r mut Deserializer<'a>,
    len: usize,
    key: Option<&'a str>,
}

impl<'de, 'a, 'r> de::MapAccess<'de> for MapAccess<'a, 'r> {
    type Error = Error;

    fn next_key_seed<K>(&mut self, seed: K) -> Result<Option<K::Value>>
    where
        K: DeserializeSeed<'de>,
    {
        match *self.de.peek()?.0 {
            Event::MappingEnd => Ok(None),
            Event::Scalar(ref key, _, _) => {
                self.len += 1;
                self.key = Some(key);
                seed.deserialize(&mut *self.de).map(Some)
            }
            _ => {
                self.len += 1;
                self.key = None;
                seed.deserialize(&mut *self.de).map(Some)
            }
        }
    }

    fn next_value_seed<V>(&mut self, seed: V) -> Result<V::Value>
    where
        V: DeserializeSeed<'de>,
    {
        let mut value_de = Deserializer {
            events: self.de.events,
            aliases: self.de.aliases,
            pos: self.de.pos,
            path: if let Some(key) = self.key {
                Path::Map {
                    parent: &self.de.path,
                    key: key,
                }
            } else {
                Path::Unknown {
                    parent: &self.de.path,
                }
            },
            remaining_depth: self.de.remaining_depth,
        };
        seed.deserialize(&mut value_de)
    }
}

struct EnumAccess<'a: 'r, 'r> {
    de: &'r mut Deserializer<'a>,
    name: &'static str,
    tag: Option<&'static str>,
}

impl<'de, 'a, 'r> de::EnumAccess<'de> for EnumAccess<'a, 'r> {
    type Error = Error;
    type Variant = Deserializer<'r>;

    fn variant_seed<V>(self, seed: V) -> Result<(V::Value, Self::Variant)>
    where
        V: DeserializeSeed<'de>,
    {
        #[derive(Debug)]
        enum Nope {}

        struct BadKey {
            name: &'static str,
        }

        impl<'de> Visitor<'de> for BadKey {
            type Value = Nope;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                write!(formatter, "variant of enum `{}`", self.name)
            }
        }

        let variant = if let Some(tag) = self.tag {
            tag
        } else {
            match *self.de.next()?.0 {
                Event::Scalar(ref s, _, _) => &**s,
                _ => {
                    *self.de.pos -= 1;
                    let bad = BadKey { name: self.name };
                    return Err(de::Deserializer::deserialize_any(&mut *self.de, bad).unwrap_err());
                }
            }
        };

        let str_de = IntoDeserializer::<Error>::into_deserializer(variant);
        let ret = seed.deserialize(str_de)?;
        let variant_visitor = Deserializer {
            events: self.de.events,
            aliases: self.de.aliases,
            pos: self.de.pos,
            path: Path::Map {
                parent: &self.de.path,
                key: variant,
            },
            remaining_depth: self.de.remaining_depth,
        };
        Ok((ret, variant_visitor))
    }
}

impl<'de, 'a> de::VariantAccess<'de> for Deserializer<'a> {
    type Error = Error;

    fn unit_variant(mut self) -> Result<()> {
        Deserialize::deserialize(&mut self)
    }

    fn newtype_variant_seed<T>(mut self, seed: T) -> Result<T::Value>
    where
        T: DeserializeSeed<'de>,
    {
        seed.deserialize(&mut self)
    }

    fn tuple_variant<V>(mut self, _len: usize, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        de::Deserializer::deserialize_seq(&mut self, visitor)
    }

    fn struct_variant<V>(mut self, fields: &'static [&'static str], visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        de::Deserializer::deserialize_struct(&mut self, "", fields, visitor)
    }
}

struct UnitVariantAccess<'a: 'r, 'r> {
    de: &'r mut Deserializer<'a>,
}

impl<'de, 'a, 'r> de::EnumAccess<'de> for UnitVariantAccess<'a, 'r> {
    type Error = Error;
    type Variant = Self;

    fn variant_seed<V>(self, seed: V) -> Result<(V::Value, Self::Variant)>
    where
        V: DeserializeSeed<'de>,
    {
        Ok((seed.deserialize(&mut *self.de)?, self))
    }
}

impl<'de, 'a, 'r> de::VariantAccess<'de> for UnitVariantAccess<'a, 'r> {
    type Error = Error;

    fn unit_variant(self) -> Result<()> {
        Ok(())
    }

    fn newtype_variant_seed<T>(self, _seed: T) -> Result<T::Value>
    where
        T: DeserializeSeed<'de>,
    {
        Err(de::Error::invalid_type(
            Unexpected::UnitVariant,
            &"newtype variant",
        ))
    }

    fn tuple_variant<V>(self, _len: usize, _visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        Err(de::Error::invalid_type(
            Unexpected::UnitVariant,
            &"tuple variant",
        ))
    }

    fn struct_variant<V>(self, _fields: &'static [&'static str], _visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        Err(de::Error::invalid_type(
            Unexpected::UnitVariant,
            &"struct variant",
        ))
    }
}

fn visit_untagged_str<'de, V>(visitor: V, v: &str) -> Result<V::Value>
where
    V: Visitor<'de>,
{
    if v == "~" || v == "null" {
        return visitor.visit_unit();
    }
    if v == "true" {
        return visitor.visit_bool(true);
    }
    if v == "false" {
        return visitor.visit_bool(false);
    }
    if v.starts_with("0x") || v.starts_with("+0x") {
        let start = 2 + v.starts_with('+') as usize;
        if let Ok(n) = u64::from_str_radix(&v[start..], 16) {
            return visitor.visit_u64(n);
        }
    }
    if v.starts_with("-0x") {
        let negative = format!("-{}", &v[3..]);
        if let Ok(n) = i64::from_str_radix(&negative, 16) {
            return visitor.visit_i64(n);
        }
    }
    if v.starts_with("0o") || v.starts_with("+0o") {
        let start = 2 + v.starts_with('+') as usize;
        if let Ok(n) = u64::from_str_radix(&v[start..], 8) {
            return visitor.visit_u64(n);
        }
    }
    if v.starts_with("-0o") {
        let negative = format!("-{}", &v[3..]);
        if let Ok(n) = i64::from_str_radix(&negative, 8) {
            return visitor.visit_i64(n);
        }
    }
    if v.starts_with("0b") || v.starts_with("+0b") {
        let start = 2 + v.starts_with('+') as usize;
        if let Ok(n) = u64::from_str_radix(&v[start..], 2) {
            return visitor.visit_u64(n);
        }
    }
    if v.starts_with("-0b") {
        let negative = format!("-{}", &v[3..]);
        if let Ok(n) = i64::from_str_radix(&negative, 2) {
            return visitor.visit_i64(n);
        }
    }
    if let Ok(n) = v.parse() {
        return visitor.visit_u64(n);
    }
    serde_if_integer128! {
        if let Ok(n) = v.parse() {
            return visitor.visit_u128(n);
        }
    }
    if let Ok(n) = v.parse() {
        return visitor.visit_i64(n);
    }
    serde_if_integer128! {
        if let Ok(n) = v.parse() {
            return visitor.visit_i128(n);
        }
    }
    match trim_start_matches(v, '+') {
        ".inf" | ".Inf" | ".INF" => return visitor.visit_f64(f64::INFINITY),
        _ => (),
    }
    if v == "-.inf" || v == "-.Inf" || v == "-.INF" {
        return visitor.visit_f64(f64::NEG_INFINITY);
    }
    if v == ".nan" || v == ".NaN" || v == ".NAN" {
        return visitor.visit_f64(f64::NAN);
    }
    if let Ok(n) = v.parse() {
        return visitor.visit_f64(n);
    }
    visitor.visit_str(v)
}

#[allow(deprecated)]
fn trim_start_matches(s: &str, pat: char) -> &str {
    // str::trim_start_matches was added in 1.30, trim_left_matches deprecated
    // in 1.33. We currently support rustc back to 1.17 so we need to continue
    // to use the deprecated one.
    s.trim_left_matches(pat)
}

fn invalid_type(event: &Event, exp: &Expected) -> Error {
    enum Void {}

    struct InvalidType<'a> {
        exp: &'a Expected,
    }

    impl<'de, 'a> Visitor<'de> for InvalidType<'a> {
        type Value = Void;

        fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
            self.exp.fmt(formatter)
        }
    }

    match *event {
        Event::Alias(_) => unreachable!(),
        Event::Scalar(ref v, style, ref tag) => {
            let get_type = InvalidType { exp: exp };
            match visit_scalar(v, style, tag, get_type) {
                Ok(void) => match void {},
                Err(invalid_type) => invalid_type,
            }
        }
        Event::SequenceStart => de::Error::invalid_type(Unexpected::Seq, exp),
        Event::MappingStart => de::Error::invalid_type(Unexpected::Map, exp),
        Event::SequenceEnd => panic!("unexpected end of sequence"),
        Event::MappingEnd => panic!("unexpected end of mapping"),
    }
}

impl<'a> Deserializer<'a> {
    fn deserialize_scalar<'de, V>(&mut self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        let (next, marker) = self.next()?;
        match *next {
            Event::Alias(mut pos) => self.jump(&mut pos)?.deserialize_scalar(visitor),
            Event::Scalar(ref v, style, ref tag) => visit_scalar(v, style, tag, visitor),
            ref other => Err(invalid_type(other, &visitor)),
        }
        .map_err(|err| private::fix_marker(err, marker, self.path))
    }
}

impl<'de, 'a, 'r> de::Deserializer<'de> for &'r mut Deserializer<'a> {
    type Error = Error;

    fn deserialize_any<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        let (next, marker) = self.next()?;
        match *next {
            Event::Alias(mut pos) => self.jump(&mut pos)?.deserialize_any(visitor),
            Event::Scalar(ref v, style, ref tag) => visit_scalar(v, style, tag, visitor),
            Event::SequenceStart => self.visit_sequence(visitor),
            Event::MappingStart => self.visit_mapping(visitor),
            Event::SequenceEnd => panic!("unexpected end of sequence"),
            Event::MappingEnd => panic!("unexpected end of mapping"),
        }
        // The de::Error impl creates errors with unknown line and column. Fill
        // in the position here by looking at the current index in the input.
        .map_err(|err| private::fix_marker(err, marker, self.path))
    }

    fn deserialize_bool<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_scalar(visitor)
    }

    fn deserialize_i8<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_scalar(visitor)
    }

    fn deserialize_i16<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_scalar(visitor)
    }

    fn deserialize_i32<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_scalar(visitor)
    }

    fn deserialize_i64<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_scalar(visitor)
    }

    serde_if_integer128! {
        fn deserialize_i128<V>(self, visitor: V) -> Result<V::Value>
        where
            V: Visitor<'de>,
        {
            self.deserialize_scalar(visitor)
        }
    }

    fn deserialize_u8<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_scalar(visitor)
    }

    fn deserialize_u16<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_scalar(visitor)
    }

    fn deserialize_u32<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_scalar(visitor)
    }

    fn deserialize_u64<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_scalar(visitor)
    }

    serde_if_integer128! {
        fn deserialize_u128<V>(self, visitor: V) -> Result<V::Value>
        where
            V: Visitor<'de>,
        {
            self.deserialize_scalar(visitor)
        }
    }

    fn deserialize_f32<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_scalar(visitor)
    }

    fn deserialize_f64<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_scalar(visitor)
    }

    fn deserialize_char<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_str(visitor)
    }

    fn deserialize_str<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        let (next, marker) = self.next()?;
        match *next {
            Event::Scalar(ref v, _, _) => visitor.visit_str(v),
            Event::Alias(mut pos) => self.jump(&mut pos)?.deserialize_str(visitor),
            ref other => Err(invalid_type(other, &visitor)),
        }
        .map_err(|err: Error| private::fix_marker(err, marker, self.path))
    }

    fn deserialize_string<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_str(visitor)
    }

    fn deserialize_bytes<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_any(visitor)
    }

    fn deserialize_byte_buf<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_bytes(visitor)
    }

    /// Parses `null` as None and any other values as `Some(...)`.
    fn deserialize_option<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        let is_some = match *self.peek()?.0 {
            Event::Alias(mut pos) => {
                *self.pos += 1;
                return self.jump(&mut pos)?.deserialize_option(visitor);
            }
            Event::Scalar(ref v, style, ref tag) => {
                if style != TScalarStyle::Plain {
                    true
                } else if let Some(TokenType::Tag(ref handle, ref suffix)) = *tag {
                    if handle == "!!" && suffix == "null" {
                        if v == "~" || v == "null" {
                            false
                        } else {
                            return Err(de::Error::invalid_value(Unexpected::Str(v), &"null"));
                        }
                    } else {
                        true
                    }
                } else {
                    v != "~" && v != "null"
                }
            }
            Event::SequenceStart | Event::MappingStart => true,
            Event::SequenceEnd => panic!("unexpected end of sequence"),
            Event::MappingEnd => panic!("unexpected end of mapping"),
        };
        if is_some {
            visitor.visit_some(self)
        } else {
            *self.pos += 1;
            visitor.visit_none()
        }
    }

    fn deserialize_unit<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_scalar(visitor)
    }

    fn deserialize_unit_struct<V>(self, _name: &'static str, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_unit(visitor)
    }

    /// Parses a newtype struct as the underlying value.
    fn deserialize_newtype_struct<V>(self, _name: &'static str, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        visitor.visit_newtype_struct(self)
    }

    fn deserialize_seq<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        let (next, marker) = self.next()?;
        match *next {
            Event::Alias(mut pos) => self.jump(&mut pos)?.deserialize_seq(visitor),
            Event::SequenceStart => self.visit_sequence(visitor),
            ref other => Err(invalid_type(other, &visitor)),
        }
        .map_err(|err| private::fix_marker(err, marker, self.path))
    }

    fn deserialize_tuple<V>(self, _len: usize, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_seq(visitor)
    }

    fn deserialize_tuple_struct<V>(
        self,
        _name: &'static str,
        _len: usize,
        visitor: V,
    ) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_seq(visitor)
    }

    fn deserialize_map<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        let (next, marker) = self.next()?;
        match *next {
            Event::Alias(mut pos) => self.jump(&mut pos)?.deserialize_map(visitor),
            Event::MappingStart => self.visit_mapping(visitor),
            ref other => Err(invalid_type(other, &visitor)),
        }
        .map_err(|err| private::fix_marker(err, marker, self.path))
    }

    fn deserialize_struct<V>(
        self,
        name: &'static str,
        fields: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        let (next, marker) = self.next()?;
        match *next {
            Event::Alias(mut pos) => self
                .jump(&mut pos)?
                .deserialize_struct(name, fields, visitor),
            Event::SequenceStart => self.visit_sequence(visitor),
            Event::MappingStart => self.visit_mapping(visitor),
            ref other => Err(invalid_type(other, &visitor)),
        }
        .map_err(|err| private::fix_marker(err, marker, self.path))
    }

    /// Parses an enum as a single key:value pair where the key identifies the
    /// variant and the value gives the content. A String will also parse correctly
    /// to a unit enum value.
    fn deserialize_enum<V>(
        self,
        name: &'static str,
        variants: &'static [&'static str],
        visitor: V,
    ) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        let (next, marker) = self.peek()?;
        match *next {
            Event::Alias(mut pos) => {
                *self.pos += 1;
                self.jump(&mut pos)?
                    .deserialize_enum(name, variants, visitor)
            }
            Event::Scalar(_, _, ref t) => {
                if let Some(TokenType::Tag(ref handle, ref suffix)) = *t {
                    if handle == "!" {
                        if let Some(tag) = variants.iter().find(|v| *v == suffix) {
                            return visitor.visit_enum(EnumAccess {
                                de: self,
                                name: name,
                                tag: Some(tag),
                            });
                        }
                    }
                }
                visitor.visit_enum(UnitVariantAccess { de: self })
            }
            Event::MappingStart => {
                *self.pos += 1;
                let value = visitor.visit_enum(EnumAccess {
                    de: self,
                    name: name,
                    tag: None,
                })?;
                self.end_mapping(1)?;
                Ok(value)
            }
            Event::SequenceStart => {
                let err = de::Error::invalid_type(Unexpected::Seq, &"string or singleton map");
                Err(private::fix_marker(err, marker, self.path))
            }
            Event::SequenceEnd => panic!("unexpected end of sequence"),
            Event::MappingEnd => panic!("unexpected end of mapping"),
        }
    }

    fn deserialize_identifier<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.deserialize_str(visitor)
    }

    fn deserialize_ignored_any<V>(self, visitor: V) -> Result<V::Value>
    where
        V: Visitor<'de>,
    {
        self.ignore_any()?;
        visitor.visit_unit()
    }
}

/// Deserialize an instance of type `T` from a string of YAML text.
///
/// This conversion can fail if the structure of the Value does not match the
/// structure expected by `T`, for example if `T` is a struct type but the Value
/// contains something other than a YAML map. It can also fail if the structure
/// is correct but `T`'s implementation of `Deserialize` decides that something
/// is wrong with the data, for example required struct fields are missing from
/// the YAML map or some number is too big to fit in the expected primitive
/// type.
///
/// YAML currently does not support zero-copy deserialization.
pub fn from_str<T>(s: &str) -> Result<T>
where
    T: DeserializeOwned,
{
    let mut parser = Parser::new(s.chars());
    let mut loader = Loader {
        events: Vec::new(),
        aliases: BTreeMap::new(),
    };
    parser
        .load(&mut loader, true)
        .map_err(private::error_scanner)?;
    if loader.events.is_empty() {
        Err(private::error_end_of_stream())
    } else {
        let mut pos = 0;
        let t = Deserialize::deserialize(&mut Deserializer {
            events: &loader.events,
            aliases: &loader.aliases,
            pos: &mut pos,
            path: Path::Root,
            remaining_depth: 128,
        })?;
        if pos == loader.events.len() {
            Ok(t)
        } else {
            Err(private::error_more_than_one_document())
        }
    }
}

/// Deserialize an instance of type `T` from an IO stream of YAML.
///
/// This conversion can fail if the structure of the Value does not match the
/// structure expected by `T`, for example if `T` is a struct type but the Value
/// contains something other than a YAML map. It can also fail if the structure
/// is correct but `T`'s implementation of `Deserialize` decides that something
/// is wrong with the data, for example required struct fields are missing from
/// the YAML map or some number is too big to fit in the expected primitive
/// type.
pub fn from_reader<R, T>(mut rdr: R) -> Result<T>
where
    R: io::Read,
    T: DeserializeOwned,
{
    let mut bytes = Vec::new();
    rdr.read_to_end(&mut bytes).map_err(private::error_io)?;
    let s = str::from_utf8(&bytes).map_err(private::error_str_utf8)?;
    from_str(s)
}

/// Deserialize an instance of type `T` from bytes of YAML text.
///
/// This conversion can fail if the structure of the Value does not match the
/// structure expected by `T`, for example if `T` is a struct type but the Value
/// contains something other than a YAML map. It can also fail if the structure
/// is correct but `T`'s implementation of `Deserialize` decides that something
/// is wrong with the data, for example required struct fields are missing from
/// the YAML map or some number is too big to fit in the expected primitive
/// type.
///
/// YAML currently does not support zero-copy deserialization.
pub fn from_slice<T>(v: &[u8]) -> Result<T>
where
    T: DeserializeOwned,
{
    let s = str::from_utf8(v).map_err(private::error_str_utf8)?;
    from_str(s)
}
