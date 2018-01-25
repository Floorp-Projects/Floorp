use std::error::Error as StdError;
use std::fmt::{Display, Formatter, Write, Result as FmtResult};
use std::result::Result as StdResult;

use serde::ser::{self, Serialize};

#[deprecated(since="0.1.4", note="please use `to_string_pretty` with `PrettyConfig::default()` instead")]
pub mod pretty;
mod value;

/// Serializes `value` and returns it as string.
///
/// This function does not generate any newlines or nice formatting;
/// if you want that, you can use `pretty::to_string` instead.
pub fn to_string<T>(value: &T) -> Result<String>
    where T: Serialize
{
    let mut s = Serializer {
        output: String::new(),
        pretty: None,
        struct_names: false,
    };
    value.serialize(&mut s)?;
    Ok(s.output)
}

/// Serializes `value` in the recommended RON layout in a pretty way.
pub fn to_string_pretty<T>(value: &T, config: PrettyConfig) -> Result<String>
    where T: Serialize
{
    let mut s = Serializer {
        output: String::new(),
        pretty: Some((config, Pretty { indent: 0, sequence_index: Vec::new() })),
        struct_names: false,
    };
    value.serialize(&mut s)?;
    Ok(s.output)
}

/// Serialization result.
pub type Result<T> = StdResult<T, Error>;

/// Serialization error.
#[derive(Clone, Debug, PartialEq)]
pub enum Error {
    /// A custom error emitted by a serialized value.
    Message(String),
}

impl Display for Error {
    fn fmt(&self, f: &mut Formatter) -> FmtResult {
        match *self {
            Error::Message(ref e) => write!(f, "Custom message: {}", e),
        }
    }
}

impl ser::Error for Error {
    fn custom<T: Display>(msg: T) -> Self {
        Error::Message(msg.to_string())
    }
}

impl StdError for Error {
    fn description(&self) -> &str {
        match *self {
            Error::Message(ref e) => e,
        }
    }
}

/// Pretty serializer state
struct Pretty {
    indent: usize,
    sequence_index: Vec<usize>,
}

/// Pretty serializer configuration
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PrettyConfig {
    /// New line string
    pub new_line: String,
    /// Indentation string
    pub indentor: String,
    /// Separate tuple members with indentation
    pub separate_tuple_members: bool,
    /// Enumerate array items in comments
    pub enumerate_arrays: bool,
}

impl Default for PrettyConfig {
    fn default() -> Self {
        PrettyConfig {
            #[cfg(not(target_os = "windows"))]
            new_line: "\n".to_string(),
            #[cfg(target_os = "windows")]
            new_line: "\r\n".to_string(),
            indentor: "    ".to_string(),
            separate_tuple_members: false,
            enumerate_arrays: false,
        }
    }
}

/// The RON serializer.
///
/// You can just use `to_string` for deserializing a value.
/// If you want it pretty-printed, take a look at the `pretty` module.
pub struct Serializer {
    output: String,
    pretty: Option<(PrettyConfig, Pretty)>,
    struct_names: bool,
}

impl Serializer {
    /// Creates a new `Serializer`.
    ///
    /// Most of the time you can just use `to_string` or `to_string_pretty`.
    pub fn new(config: Option<PrettyConfig>, struct_names: bool) -> Self {
        Serializer {
            output: String::new(),
            pretty: config.map(|conf| (conf, Pretty { indent: 0, sequence_index: Vec::new() })),
            struct_names,
        }
    }

    /// Consumes `self` and returns the built `String`.
    pub fn into_output_string(self) -> String {
        self.output
    }

    fn separate_tuple_members(&self) -> bool {
        self.pretty.as_ref()
            .map(|&(ref config, _)| config.separate_tuple_members)
            .unwrap_or(false)
    }

    fn start_indent(&mut self) {
        if let Some((ref config, ref mut pretty)) = self.pretty {
            pretty.indent += 1;
            self.output += &config.new_line;
        }
    }

    fn indent(&mut self) {
        if let Some((ref config, ref pretty)) = self.pretty {
            self.output.extend((0..pretty.indent).map(|_| config.indentor.as_str()));
        }
    }

    fn end_indent(&mut self) {
        if let Some((ref config, ref mut pretty)) = self.pretty {
            pretty.indent -= 1;
            self.output.extend((0..pretty.indent).map(|_| config.indentor.as_str()));
        }
    }
}

impl<'a> ser::Serializer for &'a mut Serializer {
    type Ok = ();
    type Error = Error;

    type SerializeSeq = Self;
    type SerializeTuple = Self;
    type SerializeTupleStruct = Self;
    type SerializeTupleVariant = Self;
    type SerializeMap = Self;
    type SerializeStruct = Self;
    type SerializeStructVariant = Self;

    fn serialize_bool(self, v: bool) -> Result<()> {
        self.output += if v { "true" } else { "false" };
        Ok(())
    }

    fn serialize_i8(self, v: i8) -> Result<()> {
        self.serialize_i64(v as i64)
    }

    fn serialize_i16(self, v: i16) -> Result<()> {
        self.serialize_i64(v as i64)
    }

    fn serialize_i32(self, v: i32) -> Result<()> {
        self.serialize_i64(v as i64)
    }

    fn serialize_i64(self, v: i64) -> Result<()> {
        // TODO optimize
        self.output += &v.to_string();
        Ok(())
    }

    fn serialize_u8(self, v: u8) -> Result<()> {
        self.serialize_u64(v as u64)
    }

    fn serialize_u16(self, v: u16) -> Result<()> {
        self.serialize_u64(v as u64)
    }

    fn serialize_u32(self, v: u32) -> Result<()> {
        self.serialize_u64(v as u64)
    }

    fn serialize_u64(self, v: u64) -> Result<()> {
        self.output += &v.to_string();
        Ok(())
    }

    fn serialize_f32(self, v: f32) -> Result<()> {
        self.serialize_f64(v as f64)
    }

    fn serialize_f64(self, v: f64) -> Result<()> {
        self.output += &v.to_string();
        Ok(())
    }

    fn serialize_char(self, v: char) -> Result<()> {
        self.output += "'";
        if v == '\\' || v == '\'' {
            self.output.push('\\');
        }
        self.output.push(v);
        self.output += "'";
        Ok(())
    }

    fn serialize_str(self, v: &str) -> Result<()> {
        self.output += "\"";
        for char in v.chars() {
            if char == '\\' || char == '"' {
                self.output.push('\\');
            }
            self.output.push(char);
        }
        self.output += "\"";
        Ok(())
    }

    fn serialize_bytes(self, v: &[u8]) -> Result<()> {
        use serde::ser::SerializeSeq;
        //TODO: shorter version? e.g. base64 encoding in a single line
        let mut seq = self.serialize_seq(Some(v.len()))?;
        for byte in v {
            seq.serialize_element(byte)?;
        }
        seq.end()
    }

    fn serialize_none(self) -> Result<()> {
        self.output += "None";

        Ok(())
    }

    fn serialize_some<T>(self, value: &T) -> Result<()>
        where T: ?Sized + Serialize
    {
        self.output += "Some(";
        value.serialize(&mut *self)?;
        self.output += ")";

        Ok(())
    }

    fn serialize_unit(self) -> Result<()> {
        self.output += "()";

        Ok(())
    }

    fn serialize_unit_struct(self, name: &'static str) -> Result<()> {
        if self.struct_names {
            self.output += name;

            Ok(())
        } else {
            self.serialize_unit()
        }
    }

    fn serialize_unit_variant(
        self,
        _: &'static str,
        _: u32,
        variant: &'static str
    ) -> Result<()> {
        self.output += variant;

        Ok(())
    }

    fn serialize_newtype_struct<T>(self, name: &'static str, value: &T) -> Result<()>
        where T: ?Sized + Serialize
    {
        if self.struct_names {
            self.output += name;
        }

        self.output += "(";
        value.serialize(&mut *self)?;
        self.output += ")";
        Ok(())
    }

    fn serialize_newtype_variant<T>(
        self,
        _: &'static str,
        _: u32,
        variant: &'static str,
        value: &T
    ) -> Result<()>
        where T: ?Sized + Serialize
    {
        self.output += variant;
        self.output += "(";

        value.serialize(&mut *self)?;

        self.output += ")";
        Ok(())
    }

    fn serialize_seq(self, _: Option<usize>) -> Result<Self::SerializeSeq> {
        self.output += "[";

        self.start_indent();

        if let Some((_, ref mut pretty)) = self.pretty {
            pretty.sequence_index.push(0);
        }

        Ok(self)
    }

    fn serialize_tuple(self, _: usize) -> Result<Self::SerializeTuple> {
        self.output += "(";

        if self.separate_tuple_members() {
            self.start_indent();
        }

        Ok(self)
    }

    fn serialize_tuple_struct(
        self,
        name: &'static str,
        len: usize
    ) -> Result<Self::SerializeTupleStruct> {
        if self.struct_names {
            self.output += name;
        }

        self.serialize_tuple(len)
    }

    fn serialize_tuple_variant(
        self,
        _: &'static str,
        _: u32,
        variant: &'static str,
        _: usize
    ) -> Result<Self::SerializeTupleVariant> {
        self.output += variant;
        self.output += "(";

        if self.separate_tuple_members() {
            self.start_indent();
        }

        Ok(self)
    }

    fn serialize_map(self, _len: Option<usize>) -> Result<Self::SerializeMap> {
        self.output += "{";

        self.start_indent();

        Ok(self)
    }

    fn serialize_struct(
        self,
        name: &'static str,
        _: usize
    ) -> Result<Self::SerializeStruct> {
        if self.struct_names {
            self.output += name;
        }
        self.output += "(";

        self.start_indent();

        Ok(self)
    }

    fn serialize_struct_variant(
        self,
        _: &'static str,
        _: u32,
        variant: &'static str,
        _: usize
    ) -> Result<Self::SerializeStructVariant> {
        self.output += variant;
        self.output += "(";

        self.start_indent();

        Ok(self)
    }
}

impl<'a> ser::SerializeSeq for &'a mut Serializer {
    type Ok = ();
    type Error = Error;

    fn serialize_element<T>(&mut self, value: &T) -> Result<()>
        where T: ?Sized + Serialize
    {
        self.indent();

        value.serialize(&mut **self)?;
        self.output += ",";

        if let Some((ref config, ref mut pretty)) = self.pretty {
            if config.enumerate_arrays {
                assert!(config.new_line.contains('\n'));
                let index = pretty.sequence_index.last_mut().unwrap();
                //TODO: when /**/ comments are supported, prepend the index
                // to an element instead of appending it.
                write!(self.output, "// [{}]", index).unwrap();
                *index += 1;
            }
            self.output += &config.new_line;
        }

        Ok(())
    }

    fn end(self) -> Result<()> {
        self.end_indent();

        if let Some((_, ref mut pretty)) = self.pretty {
            pretty.sequence_index.pop();
        }

        self.output += "]";
        Ok(())
    }
}

impl<'a> ser::SerializeTuple for &'a mut Serializer {
    type Ok = ();
    type Error = Error;

    fn serialize_element<T>(&mut self, value: &T) -> Result<()>
        where T: ?Sized + Serialize
    {
        if self.separate_tuple_members() {
            self.indent();
        }

        value.serialize(&mut **self)?;
        self.output += ",";

        if let Some((ref config, _)) = self.pretty {
            self.output += if self.separate_tuple_members() { &config.new_line } else { " " };
        }

        Ok(())
    }

    fn end(self) -> Result<()> {
        if self.pretty.is_some() {
            if self.separate_tuple_members() {
                self.end_indent();
            } else {
                self.output.pop();
                self.output.pop();
            }
        }

        self.output += ")";

        Ok(())
    }
}

// Same thing but for tuple structs.
impl<'a> ser::SerializeTupleStruct for &'a mut Serializer {
    type Ok = ();
    type Error = Error;

    fn serialize_field<T>(&mut self, value: &T) -> Result<()>
        where T: ?Sized + Serialize
    {
        ser::SerializeTuple::serialize_element(self, value)
    }

    fn end(self) -> Result<()> {
        ser::SerializeTuple::end(self)
    }
}

impl<'a> ser::SerializeTupleVariant for &'a mut Serializer {
    type Ok = ();
    type Error = Error;

    fn serialize_field<T>(&mut self, value: &T) -> Result<()>
        where T: ?Sized + Serialize
    {
        ser::SerializeTuple::serialize_element(self, value)
    }

    fn end(self) -> Result<()> {
        ser::SerializeTuple::end(self)
    }
}

impl<'a> ser::SerializeMap for &'a mut Serializer {
    type Ok = ();
    type Error = Error;

    fn serialize_key<T>(&mut self, key: &T) -> Result<()>
        where T: ?Sized + Serialize
    {
        self.indent();

        key.serialize(&mut **self)
    }

    fn serialize_value<T>(&mut self, value: &T) -> Result<()>
        where T: ?Sized + Serialize
    {
        self.output += ":";

        if self.pretty.is_some() {
            self.output += " ";
        }

        value.serialize(&mut **self)?;
        self.output += ",";

        if let Some((ref config, _)) = self.pretty {
            self.output += &config.new_line;
        }

        Ok(())
    }

    fn end(self) -> Result<()> {
        self.end_indent();

        self.output += "}";
        Ok(())
    }
}

impl<'a> ser::SerializeStruct for &'a mut Serializer {
    type Ok = ();
    type Error = Error;

    fn serialize_field<T>(&mut self, key: &'static str, value: &T) -> Result<()>
        where T: ?Sized + Serialize
    {
        self.indent();

        self.output += key;
        self.output += ":";

        if self.pretty.is_some() {
            self.output += " ";
        }

        value.serialize(&mut **self)?;
        self.output += ",";

        if let Some((ref config, _)) = self.pretty {
            self.output += &config.new_line;
        }

        Ok(())
    }

    fn end(self) -> Result<()> {
        self.end_indent();

        self.output += ")";
        Ok(())
    }
}

impl<'a> ser::SerializeStructVariant for &'a mut Serializer {
    type Ok = ();
    type Error = Error;

    fn serialize_field<T>(&mut self, key: &'static str, value: &T) -> Result<()>
        where T: ?Sized + Serialize
    {
        ser::SerializeStruct::serialize_field(self, key, value)
    }

    fn end(self) -> Result<()> {
        ser::SerializeStruct::end(self)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Serialize)]
    struct EmptyStruct1;

    #[derive(Serialize)]
    struct EmptyStruct2 {}

    #[derive(Serialize)]
    struct MyStruct { x: f32, y: f32 }

    #[derive(Serialize)]
    enum MyEnum {
        A,
        B(bool),
        C(bool, f32),
        D { a: i32, b: i32 }
    }

    #[test]
    fn test_empty_struct() {
        assert_eq!(to_string(&EmptyStruct1).unwrap(), "()");
        assert_eq!(to_string(&EmptyStruct2 {}).unwrap(), "()");
    }

    #[test]
    fn test_struct() {
        let my_struct = MyStruct { x: 4.0, y: 7.0 };

        assert_eq!(to_string(&my_struct).unwrap(), "(x:4,y:7,)");


        #[derive(Serialize)]
        struct NewType(i32);

        assert_eq!(to_string(&NewType(42)).unwrap(), "(42)");

        #[derive(Serialize)]
        struct TupleStruct(f32, f32);

        assert_eq!(to_string(&TupleStruct(2.0, 5.0)).unwrap(), "(2,5,)");
    }

    #[test]
    fn test_option() {
        assert_eq!(to_string(&Some(1u8)).unwrap(), "Some(1)");
        assert_eq!(to_string(&None::<u8>).unwrap(), "None");
    }

    #[test]
    fn test_enum() {
        assert_eq!(to_string(&MyEnum::A).unwrap(), "A");
        assert_eq!(to_string(&MyEnum::B(true)).unwrap(), "B(true)");
        assert_eq!(to_string(&MyEnum::C(true, 3.5)).unwrap(), "C(true,3.5,)");
        assert_eq!(to_string(&MyEnum::D { a: 2, b: 3 }).unwrap(), "D(a:2,b:3,)");
    }

    #[test]
    fn test_array() {
        let empty: [i32; 0] = [];
        assert_eq!(to_string(&empty).unwrap(), "()");
        let empty_ref: &[i32] = &empty;
        assert_eq!(to_string(&empty_ref).unwrap(), "[]");

        assert_eq!(to_string(&[2, 3, 4i32]).unwrap(), "(2,3,4,)");
        assert_eq!(to_string(&(&[2, 3, 4i32] as &[i32])).unwrap(), "[2,3,4,]");
    }

    #[test]
    fn test_map() {
        use std::collections::HashMap;

        let mut map = HashMap::new();
        map.insert((true, false), 4);
        map.insert((false, false), 123);

        let s = to_string(&map).unwrap();
        s.starts_with("{");
        s.contains("(true,false,):4");
        s.contains("(false,false,):123");
        s.ends_with("}");
    }

    #[test]
    fn test_string() {
        assert_eq!(to_string(&"Some string").unwrap(), "\"Some string\"");
    }

    #[test]
    fn test_char() {
        assert_eq!(to_string(&'c').unwrap(), "'c'");
    }

    #[test]
    fn test_escape() {
        assert_eq!(to_string(&r#""Quoted""#).unwrap(), r#""\"Quoted\"""#);
    }
}
