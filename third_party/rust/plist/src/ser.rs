use serde::ser;
use std::fmt::Display;
use std::io::Write;

use date::serde_impls::DATE_NEWTYPE_STRUCT_NAME;
use stream::{self, Event, Writer};
use {Date, Error};

impl ser::Error for Error {
    fn custom<T: Display>(msg: T) -> Self {
        Error::Serde(msg.to_string())
    }
}

/// A structure that serializes Rust values plist event streams.
pub struct Serializer<W: Writer> {
    writer: W,
}

impl<W: Writer> Serializer<W> {
    pub fn new(writer: W) -> Serializer<W> {
        Serializer { writer }
    }

    fn emit(&mut self, event: Event) -> Result<(), Error> {
        self.writer.write(&event)?;
        Ok(())
    }

    pub fn into_inner(self) -> W {
        self.writer
    }

    // Emit {key: value}
    fn single_key_dict(&mut self, key: String) -> Result<(), Error> {
        self.emit(Event::StartDictionary(Some(1)))?;
        self.emit(Event::StringValue(key))?;
        Ok(())
    }

    fn single_key_dict_end(&mut self) -> Result<(), Error> {
        self.emit(Event::EndDictionary)?;
        Ok(())
    }
}

impl<'a, W: Writer> ser::Serializer for &'a mut Serializer<W> {
    type Ok = ();
    type Error = Error;

    type SerializeSeq = Compound<'a, W>;
    type SerializeTuple = Compound<'a, W>;
    type SerializeTupleStruct = Compound<'a, W>;
    type SerializeTupleVariant = Compound<'a, W>;
    type SerializeMap = Compound<'a, W>;
    type SerializeStruct = Compound<'a, W>;
    type SerializeStructVariant = Compound<'a, W>;

    fn serialize_bool(self, v: bool) -> Result<(), Self::Error> {
        self.emit(Event::BooleanValue(v))
    }

    fn serialize_i8(self, v: i8) -> Result<(), Self::Error> {
        self.serialize_i64(v.into())
    }

    fn serialize_i16(self, v: i16) -> Result<(), Self::Error> {
        self.serialize_i64(v.into())
    }

    fn serialize_i32(self, v: i32) -> Result<(), Self::Error> {
        self.serialize_i64(v.into())
    }

    fn serialize_i64(self, v: i64) -> Result<(), Self::Error> {
        self.emit(Event::IntegerValue(v))
    }

    fn serialize_u8(self, v: u8) -> Result<(), Self::Error> {
        self.serialize_u64(v.into())
    }

    fn serialize_u16(self, v: u16) -> Result<(), Self::Error> {
        self.serialize_u64(v.into())
    }

    fn serialize_u32(self, v: u32) -> Result<(), Self::Error> {
        self.serialize_u64(v.into())
    }

    fn serialize_u64(self, v: u64) -> Result<(), Self::Error> {
        self.emit(Event::IntegerValue(v as i64))
    }

    fn serialize_f32(self, v: f32) -> Result<(), Self::Error> {
        self.serialize_f64(v.into())
    }

    fn serialize_f64(self, v: f64) -> Result<(), Self::Error> {
        self.emit(Event::RealValue(v))
    }

    fn serialize_char(self, v: char) -> Result<(), Self::Error> {
        self.emit(Event::StringValue(v.to_string()))
    }

    fn serialize_str(self, v: &str) -> Result<(), Self::Error> {
        self.emit(Event::StringValue(v.to_owned()))
    }

    fn serialize_bytes(self, v: &[u8]) -> Result<(), Self::Error> {
        self.emit(Event::DataValue(v.to_owned()))
    }

    fn serialize_none(self) -> Result<(), Self::Error> {
        self.single_key_dict("None".to_owned())?;
        self.serialize_unit()?;
        self.single_key_dict_end()
    }

    fn serialize_some<T: ?Sized + ser::Serialize>(self, value: &T) -> Result<(), Self::Error> {
        self.single_key_dict("Some".to_owned())?;
        value.serialize(&mut *self)?;
        self.single_key_dict_end()
    }

    fn serialize_unit(self) -> Result<(), Self::Error> {
        // Emit empty string
        self.emit(Event::StringValue(String::new()))
    }

    fn serialize_unit_struct(self, _name: &'static str) -> Result<(), Self::Error> {
        self.serialize_unit()
    }

    fn serialize_unit_variant(
        self,
        _name: &'static str,
        _variant_index: u32,
        variant: &'static str,
    ) -> Result<(), Self::Error> {
        self.single_key_dict(variant.to_owned())?;
        self.serialize_unit()?;
        self.single_key_dict_end()?;
        Ok(())
    }

    fn serialize_newtype_struct<T: ?Sized + ser::Serialize>(
        self,
        name: &'static str,
        value: &T,
    ) -> Result<(), Self::Error> {
        if name == DATE_NEWTYPE_STRUCT_NAME {
            value.serialize(DateSerializer { ser: &mut *self })
        } else {
            value.serialize(self)
        }
    }

    fn serialize_newtype_variant<T: ?Sized + ser::Serialize>(
        self,
        _name: &'static str,
        _variant_index: u32,
        variant: &'static str,
        value: &T,
    ) -> Result<(), Self::Error> {
        self.single_key_dict(variant.to_owned())?;
        value.serialize(&mut *self)?;
        self.single_key_dict_end()
    }

    fn serialize_seq(self, len: Option<usize>) -> Result<Self::SerializeSeq, Self::Error> {
        let len = len.map(|len| len as u64);
        self.emit(Event::StartArray(len))?;
        Ok(Compound { ser: self })
    }

    fn serialize_tuple(self, len: usize) -> Result<Self::SerializeTuple, Self::Error> {
        self.serialize_seq(Some(len))
    }

    fn serialize_tuple_struct(
        self,
        _name: &'static str,
        len: usize,
    ) -> Result<Self::SerializeTupleStruct, Self::Error> {
        self.serialize_tuple(len)
    }

    fn serialize_tuple_variant(
        self,
        _name: &'static str,
        _variant_index: u32,
        variant: &'static str,
        len: usize,
    ) -> Result<Self::SerializeTupleVariant, Self::Error> {
        self.single_key_dict(variant.to_owned())?;
        self.serialize_tuple(len)
    }

    fn serialize_map(self, len: Option<usize>) -> Result<Self::SerializeMap, Self::Error> {
        let len = len.map(|len| len as u64);
        self.emit(Event::StartDictionary(len))?;
        Ok(Compound { ser: self })
    }

    fn serialize_struct(
        self,
        _name: &'static str,
        _len: usize,
    ) -> Result<Self::SerializeStruct, Self::Error> {
        // The number of struct fields is not known as fields with None values are ignored.
        self.serialize_map(None)
    }

    fn serialize_struct_variant(
        self,
        name: &'static str,
        _variant_index: u32,
        variant: &'static str,
        len: usize,
    ) -> Result<Self::SerializeStructVariant, Self::Error> {
        self.single_key_dict(variant.to_owned())?;
        self.serialize_struct(name, len)
    }
}

struct StructFieldSerializer<'a, W: 'a + Writer> {
    ser: &'a mut Serializer<W>,
    field_name: &'static str,
}

impl<'a, W: Writer> StructFieldSerializer<'a, W> {
    fn use_ser(self) -> Result<&'a mut Serializer<W>, Error> {
        // We are going to serialize something so write the struct field name.
        self.ser
            .emit(Event::StringValue(self.field_name.to_owned()))?;
        Ok(self.ser)
    }
}

impl<'a, W: Writer> ser::Serializer for StructFieldSerializer<'a, W> {
    type Ok = ();
    type Error = Error;

    type SerializeSeq = Compound<'a, W>;
    type SerializeTuple = Compound<'a, W>;
    type SerializeTupleStruct = Compound<'a, W>;
    type SerializeTupleVariant = Compound<'a, W>;
    type SerializeMap = Compound<'a, W>;
    type SerializeStruct = Compound<'a, W>;
    type SerializeStructVariant = Compound<'a, W>;

    fn serialize_bool(self, v: bool) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_bool(v)
    }

    fn serialize_i8(self, v: i8) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_i8(v)
    }

    fn serialize_i16(self, v: i16) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_i16(v)
    }

    fn serialize_i32(self, v: i32) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_i32(v)
    }

    fn serialize_i64(self, v: i64) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_i64(v)
    }

    fn serialize_u8(self, v: u8) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_u8(v)
    }

    fn serialize_u16(self, v: u16) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_u16(v)
    }

    fn serialize_u32(self, v: u32) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_u32(v)
    }

    fn serialize_u64(self, v: u64) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_u64(v)
    }

    fn serialize_f32(self, v: f32) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_f32(v)
    }

    fn serialize_f64(self, v: f64) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_f64(v)
    }

    fn serialize_char(self, v: char) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_char(v)
    }

    fn serialize_str(self, v: &str) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_str(v)
    }

    fn serialize_bytes(self, v: &[u8]) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_bytes(v)
    }

    fn serialize_none(self) -> Result<(), Self::Error> {
        // Don't write a dict for None if the Option is in a struct.
        Ok(())
    }

    fn serialize_some<T: ?Sized + ser::Serialize>(self, value: &T) -> Result<(), Self::Error> {
        let ser = self.use_ser()?;
        value.serialize(ser)
    }

    fn serialize_unit(self) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_unit()
    }

    fn serialize_unit_struct(self, name: &'static str) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_unit_struct(name)
    }

    fn serialize_unit_variant(
        self,
        name: &'static str,
        variant_index: u32,
        variant: &'static str,
    ) -> Result<(), Self::Error> {
        self.use_ser()?
            .serialize_unit_variant(name, variant_index, variant)
    }

    fn serialize_newtype_struct<T: ?Sized + ser::Serialize>(
        self,
        name: &'static str,
        value: &T,
    ) -> Result<(), Self::Error> {
        self.use_ser()?.serialize_newtype_struct(name, value)
    }

    fn serialize_newtype_variant<T: ?Sized + ser::Serialize>(
        self,
        name: &'static str,
        variant_index: u32,
        variant: &'static str,
        value: &T,
    ) -> Result<(), Self::Error> {
        self.use_ser()?
            .serialize_newtype_variant(name, variant_index, variant, value)
    }

    fn serialize_seq(self, len: Option<usize>) -> Result<Self::SerializeSeq, Self::Error> {
        self.use_ser()?.serialize_seq(len)
    }

    fn serialize_tuple(self, len: usize) -> Result<Self::SerializeTuple, Self::Error> {
        self.use_ser()?.serialize_tuple(len)
    }

    fn serialize_tuple_struct(
        self,
        name: &'static str,
        len: usize,
    ) -> Result<Self::SerializeTupleStruct, Self::Error> {
        self.use_ser()?.serialize_tuple_struct(name, len)
    }

    fn serialize_tuple_variant(
        self,
        name: &'static str,
        variant_index: u32,
        variant: &'static str,
        len: usize,
    ) -> Result<Self::SerializeTupleVariant, Self::Error> {
        self.use_ser()?
            .serialize_tuple_variant(name, variant_index, variant, len)
    }

    fn serialize_map(self, len: Option<usize>) -> Result<Self::SerializeMap, Self::Error> {
        self.use_ser()?.serialize_map(len)
    }

    fn serialize_struct(
        self,
        name: &'static str,
        len: usize,
    ) -> Result<Self::SerializeStruct, Self::Error> {
        self.use_ser()?.serialize_struct(name, len)
    }

    fn serialize_struct_variant(
        self,
        name: &'static str,
        variant_index: u32,
        variant: &'static str,
        len: usize,
    ) -> Result<Self::SerializeStructVariant, Self::Error> {
        self.use_ser()?
            .serialize_struct_variant(name, variant_index, variant, len)
    }
}

struct DateSerializer<'a, W: 'a + Writer> {
    ser: &'a mut Serializer<W>,
}

impl<'a, W: Writer> DateSerializer<'a, W> {
    fn expecting_date_error(&self) -> Error {
        ser::Error::custom("plist date string expected")
    }
}

impl<'a, W: Writer> ser::Serializer for DateSerializer<'a, W> {
    type Ok = ();
    type Error = Error;

    type SerializeSeq = ser::Impossible<(), Error>;
    type SerializeTuple = ser::Impossible<(), Error>;
    type SerializeTupleStruct = ser::Impossible<(), Error>;
    type SerializeTupleVariant = ser::Impossible<(), Error>;
    type SerializeMap = ser::Impossible<(), Error>;
    type SerializeStruct = ser::Impossible<(), Error>;
    type SerializeStructVariant = ser::Impossible<(), Error>;

    fn serialize_bool(self, _: bool) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_i8(self, _: i8) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_i16(self, _: i16) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_i32(self, _: i32) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_i64(self, _: i64) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_u8(self, _: u8) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_u16(self, _: u16) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_u32(self, _: u32) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_u64(self, _: u64) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_f32(self, _: f32) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_f64(self, _: f64) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_char(self, _: char) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_str(self, v: &str) -> Result<(), Self::Error> {
        let date = Date::from_rfc3339(v).map_err(|()| self.expecting_date_error())?;
        self.ser.emit(Event::DateValue(date))
    }

    fn serialize_bytes(self, _: &[u8]) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_none(self) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_some<T: ?Sized + ser::Serialize>(self, _: &T) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_unit(self) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_unit_struct(self, _: &'static str) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_unit_variant(
        self,
        _: &'static str,
        _: u32,
        _: &'static str,
    ) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_newtype_struct<T: ?Sized + ser::Serialize>(
        self,
        _: &'static str,
        _: &T,
    ) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_newtype_variant<T: ?Sized + ser::Serialize>(
        self,
        _: &'static str,
        _: u32,
        _: &'static str,
        _: &T,
    ) -> Result<(), Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_seq(self, _: Option<usize>) -> Result<Self::SerializeSeq, Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_tuple(self, _: usize) -> Result<Self::SerializeTuple, Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_tuple_struct(
        self,
        _: &'static str,
        _: usize,
    ) -> Result<Self::SerializeTupleStruct, Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_tuple_variant(
        self,
        _: &'static str,
        _: u32,
        _: &'static str,
        _: usize,
    ) -> Result<Self::SerializeTupleVariant, Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_map(self, _: Option<usize>) -> Result<Self::SerializeMap, Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_struct(
        self,
        _: &'static str,
        _: usize,
    ) -> Result<Self::SerializeStruct, Self::Error> {
        Err(self.expecting_date_error())
    }

    fn serialize_struct_variant(
        self,
        _: &'static str,
        _: u32,
        _: &'static str,
        _: usize,
    ) -> Result<Self::SerializeStructVariant, Self::Error> {
        Err(self.expecting_date_error())
    }
}

#[doc(hidden)]
pub struct Compound<'a, W: 'a + Writer> {
    ser: &'a mut Serializer<W>,
}

impl<'a, W: Writer> ser::SerializeSeq for Compound<'a, W> {
    type Ok = ();
    type Error = Error;

    fn serialize_element<T: ?Sized + ser::Serialize>(
        &mut self,
        value: &T,
    ) -> Result<(), Self::Error> {
        value.serialize(&mut *self.ser)
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        self.ser.emit(Event::EndArray)
    }
}

impl<'a, W: Writer> ser::SerializeTuple for Compound<'a, W> {
    type Ok = ();
    type Error = Error;

    fn serialize_element<T: ?Sized + ser::Serialize>(
        &mut self,
        value: &T,
    ) -> Result<(), Self::Error> {
        <Self as ser::SerializeSeq>::serialize_element(self, value)
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        <Self as ser::SerializeSeq>::end(self)
    }
}

impl<'a, W: Writer> ser::SerializeTupleStruct for Compound<'a, W> {
    type Ok = ();
    type Error = Error;

    fn serialize_field<T: ?Sized + ser::Serialize>(
        &mut self,
        value: &T,
    ) -> Result<(), Self::Error> {
        <Self as ser::SerializeSeq>::serialize_element(self, value)
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        <Self as ser::SerializeSeq>::end(self)
    }
}

impl<'a, W: Writer> ser::SerializeTupleVariant for Compound<'a, W> {
    type Ok = ();
    type Error = Error;

    fn serialize_field<T: ?Sized + ser::Serialize>(
        &mut self,
        value: &T,
    ) -> Result<(), Self::Error> {
        <Self as ser::SerializeSeq>::serialize_element(self, value)
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        self.ser.emit(Event::EndArray)?;
        self.ser.single_key_dict_end()
    }
}

impl<'a, W: Writer> ser::SerializeMap for Compound<'a, W> {
    type Ok = ();
    type Error = Error;

    fn serialize_key<T: ?Sized + ser::Serialize>(&mut self, key: &T) -> Result<(), Self::Error> {
        key.serialize(&mut *self.ser)
    }

    fn serialize_value<T: ?Sized + ser::Serialize>(
        &mut self,
        value: &T,
    ) -> Result<(), Self::Error> {
        value.serialize(&mut *self.ser)
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        self.ser.emit(Event::EndDictionary)
    }
}

impl<'a, W: Writer> ser::SerializeStruct for Compound<'a, W> {
    type Ok = ();
    type Error = Error;

    fn serialize_field<T: ?Sized + ser::Serialize>(
        &mut self,
        key: &'static str,
        value: &T,
    ) -> Result<(), Self::Error> {
        // We don't want to serialize None if the Option is a struct field as this is how null
        // fields are represented in plists.
        value.serialize(StructFieldSerializer {
            field_name: key,
            ser: &mut *self.ser,
        })
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        <Self as ser::SerializeMap>::end(self)
    }
}

impl<'a, W: Writer> ser::SerializeStructVariant for Compound<'a, W> {
    type Ok = ();
    type Error = Error;

    fn serialize_field<T: ?Sized + ser::Serialize>(
        &mut self,
        key: &'static str,
        value: &T,
    ) -> Result<(), Self::Error> {
        <Self as ser::SerializeStruct>::serialize_field(self, key, value)
    }

    fn end(self) -> Result<Self::Ok, Self::Error> {
        self.ser.emit(Event::EndDictionary)?;
        self.ser.single_key_dict_end()
    }
}

/// Serializes the given data structure as an XML encoded plist file.
pub fn to_writer_xml<W: Write, T: ser::Serialize>(writer: W, value: &T) -> Result<(), Error> {
    let writer = stream::XmlWriter::new(writer);
    let mut ser = Serializer::new(writer);
    value.serialize(&mut ser)
}
