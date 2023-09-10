use std::io;

use base64::Engine;
use serde::{ser, ser::Serialize};
use serde_derive::{Deserialize, Serialize};

use crate::{
    error::{Error, Result},
    extensions::Extensions,
    options::Options,
    parse::{
        is_ident_first_char, is_ident_other_char, is_ident_raw_char, LargeSInt, LargeUInt,
        BASE64_ENGINE,
    },
};

#[cfg(test)]
mod tests;
mod value;

/// Serializes `value` into `writer`.
///
/// This function does not generate any newlines or nice formatting;
/// if you want that, you can use [`to_writer_pretty`] instead.
pub fn to_writer<W, T>(writer: W, value: &T) -> Result<()>
where
    W: io::Write,
    T: ?Sized + Serialize,
{
    Options::default().to_writer(writer, value)
}

/// Serializes `value` into `writer` in a pretty way.
pub fn to_writer_pretty<W, T>(writer: W, value: &T, config: PrettyConfig) -> Result<()>
where
    W: io::Write,
    T: ?Sized + Serialize,
{
    Options::default().to_writer_pretty(writer, value, config)
}

/// Serializes `value` and returns it as string.
///
/// This function does not generate any newlines or nice formatting;
/// if you want that, you can use [`to_string_pretty`] instead.
pub fn to_string<T>(value: &T) -> Result<String>
where
    T: ?Sized + Serialize,
{
    Options::default().to_string(value)
}

/// Serializes `value` in the recommended RON layout in a pretty way.
pub fn to_string_pretty<T>(value: &T, config: PrettyConfig) -> Result<String>
where
    T: ?Sized + Serialize,
{
    Options::default().to_string_pretty(value, config)
}

/// Pretty serializer state
struct Pretty {
    indent: usize,
    sequence_index: Vec<usize>,
}

/// Pretty serializer configuration.
///
/// # Examples
///
/// ```
/// use ron::ser::PrettyConfig;
///
/// let my_config = PrettyConfig::new()
///     .depth_limit(4)
///     // definitely superior (okay, just joking)
///     .indentor("\t".to_owned());
/// ```
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(default)]
#[non_exhaustive]
pub struct PrettyConfig {
    /// Limit the pretty-ness up to the given depth.
    pub depth_limit: usize,
    /// New line string
    pub new_line: String,
    /// Indentation string
    pub indentor: String,
    /// Separator string
    pub separator: String,
    // Whether to emit struct names
    pub struct_names: bool,
    /// Separate tuple members with indentation
    pub separate_tuple_members: bool,
    /// Enumerate array items in comments
    pub enumerate_arrays: bool,
    /// Enable extensions. Only configures 'implicit_some',
    ///  'unwrap_newtypes', and 'unwrap_variant_newtypes' for now.
    pub extensions: Extensions,
    /// Enable compact arrays
    pub compact_arrays: bool,
}

impl PrettyConfig {
    /// Creates a default [`PrettyConfig`].
    pub fn new() -> Self {
        Default::default()
    }

    /// Limits the pretty-formatting based on the number of indentations.
    /// I.e., with a depth limit of 5, starting with an element of depth
    /// (indentation level) 6, everything will be put into the same line,
    /// without pretty formatting.
    ///
    /// Default: [usize::MAX]
    pub fn depth_limit(mut self, depth_limit: usize) -> Self {
        self.depth_limit = depth_limit;

        self
    }

    /// Configures the newlines used for serialization.
    ///
    /// Default: `\r\n` on Windows, `\n` otherwise
    pub fn new_line(mut self, new_line: String) -> Self {
        self.new_line = new_line;

        self
    }

    /// Configures the string sequence used for indentation.
    ///
    /// Default: 4 spaces
    pub fn indentor(mut self, indentor: String) -> Self {
        self.indentor = indentor;

        self
    }

    /// Configures the string sequence used to separate items inline.
    ///
    /// Default: 1 space
    pub fn separator(mut self, separator: String) -> Self {
        self.separator = separator;

        self
    }

    /// Configures whether to emit struct names.
    ///
    /// Default: `false`
    pub fn struct_names(mut self, struct_names: bool) -> Self {
        self.struct_names = struct_names;

        self
    }

    /// Configures whether tuples are single- or multi-line.
    /// If set to `true`, tuples will have their fields indented and in new
    /// lines. If set to `false`, tuples will be serialized without any
    /// newlines or indentations.
    ///
    /// Default: `false`
    pub fn separate_tuple_members(mut self, separate_tuple_members: bool) -> Self {
        self.separate_tuple_members = separate_tuple_members;

        self
    }

    /// Configures whether a comment shall be added to every array element,
    /// indicating the index.
    ///
    /// Default: `false`
    pub fn enumerate_arrays(mut self, enumerate_arrays: bool) -> Self {
        self.enumerate_arrays = enumerate_arrays;

        self
    }

    /// Configures whether every array should be a single line (`true`)
    /// or a multi line one (`false`).
    ///
    /// When `false`, `["a","b"]` (as well as any array) will serialize to
    /// ```
    /// [
    ///   "a",
    ///   "b",
    /// ]
    /// # ;
    /// ```
    /// When `true`, `["a","b"]` (as well as any array) will serialize to
    /// ```
    /// ["a","b"]
    /// # ;
    /// ```
    ///
    /// Default: `false`
    pub fn compact_arrays(mut self, compact_arrays: bool) -> Self {
        self.compact_arrays = compact_arrays;

        self
    }

    /// Configures extensions
    ///
    /// Default: [Extensions::empty()]
    pub fn extensions(mut self, extensions: Extensions) -> Self {
        self.extensions = extensions;

        self
    }
}

impl Default for PrettyConfig {
    fn default() -> Self {
        PrettyConfig {
            depth_limit: usize::MAX,
            new_line: if cfg!(not(target_os = "windows")) {
                String::from("\n")
            } else {
                String::from("\r\n")
            },
            indentor: String::from("    "),
            separator: String::from(" "),
            struct_names: false,
            separate_tuple_members: false,
            enumerate_arrays: false,
            extensions: Extensions::empty(),
            compact_arrays: false,
        }
    }
}

/// The RON serializer.
///
/// You can just use [`to_string`] for deserializing a value.
/// If you want it pretty-printed, take a look at [`to_string_pretty`].
pub struct Serializer<W: io::Write> {
    output: W,
    pretty: Option<(PrettyConfig, Pretty)>,
    default_extensions: Extensions,
    is_empty: Option<bool>,
    newtype_variant: bool,
    recursion_limit: Option<usize>,
}

impl<W: io::Write> Serializer<W> {
    /// Creates a new [`Serializer`].
    ///
    /// Most of the time you can just use [`to_string`] or
    /// [`to_string_pretty`].
    pub fn new(writer: W, config: Option<PrettyConfig>) -> Result<Self> {
        Self::with_options(writer, config, Options::default())
    }

    /// Creates a new [`Serializer`].
    ///
    /// Most of the time you can just use [`to_string`] or
    /// [`to_string_pretty`].
    pub fn with_options(
        mut writer: W,
        config: Option<PrettyConfig>,
        options: Options,
    ) -> Result<Self> {
        if let Some(conf) = &config {
            let non_default_extensions = !options.default_extensions;

            if (non_default_extensions & conf.extensions).contains(Extensions::IMPLICIT_SOME) {
                writer.write_all(b"#![enable(implicit_some)]")?;
                writer.write_all(conf.new_line.as_bytes())?;
            };
            if (non_default_extensions & conf.extensions).contains(Extensions::UNWRAP_NEWTYPES) {
                writer.write_all(b"#![enable(unwrap_newtypes)]")?;
                writer.write_all(conf.new_line.as_bytes())?;
            };
            if (non_default_extensions & conf.extensions)
                .contains(Extensions::UNWRAP_VARIANT_NEWTYPES)
            {
                writer.write_all(b"#![enable(unwrap_variant_newtypes)]")?;
                writer.write_all(conf.new_line.as_bytes())?;
            };
        };
        Ok(Serializer {
            output: writer,
            pretty: config.map(|conf| {
                (
                    conf,
                    Pretty {
                        indent: 0,
                        sequence_index: Vec::new(),
                    },
                )
            }),
            default_extensions: options.default_extensions,
            is_empty: None,
            newtype_variant: false,
            recursion_limit: options.recursion_limit,
        })
    }

    fn separate_tuple_members(&self) -> bool {
        self.pretty
            .as_ref()
            .map_or(false, |&(ref config, _)| config.separate_tuple_members)
    }

    fn compact_arrays(&self) -> bool {
        self.pretty
            .as_ref()
            .map_or(false, |&(ref config, _)| config.compact_arrays)
    }

    fn extensions(&self) -> Extensions {
        self.default_extensions
            | self
                .pretty
                .as_ref()
                .map_or(Extensions::empty(), |&(ref config, _)| config.extensions)
    }

    fn start_indent(&mut self) -> Result<()> {
        if let Some((ref config, ref mut pretty)) = self.pretty {
            pretty.indent += 1;
            if pretty.indent <= config.depth_limit {
                let is_empty = self.is_empty.unwrap_or(false);

                if !is_empty {
                    self.output.write_all(config.new_line.as_bytes())?;
                }
            }
        }
        Ok(())
    }

    fn indent(&mut self) -> io::Result<()> {
        if let Some((ref config, ref pretty)) = self.pretty {
            if pretty.indent <= config.depth_limit {
                for _ in 0..pretty.indent {
                    self.output.write_all(config.indentor.as_bytes())?;
                }
            }
        }
        Ok(())
    }

    fn end_indent(&mut self) -> io::Result<()> {
        if let Some((ref config, ref mut pretty)) = self.pretty {
            if pretty.indent <= config.depth_limit {
                let is_empty = self.is_empty.unwrap_or(false);

                if !is_empty {
                    for _ in 1..pretty.indent {
                        self.output.write_all(config.indentor.as_bytes())?;
                    }
                }
            }
            pretty.indent -= 1;

            self.is_empty = None;
        }
        Ok(())
    }

    fn serialize_escaped_str(&mut self, value: &str) -> io::Result<()> {
        self.output.write_all(b"\"")?;
        let mut scalar = [0u8; 4];
        for c in value.chars().flat_map(|c| c.escape_debug()) {
            self.output
                .write_all(c.encode_utf8(&mut scalar).as_bytes())?;
        }
        self.output.write_all(b"\"")?;
        Ok(())
    }

    fn serialize_sint(&mut self, value: impl Into<LargeSInt>) -> Result<()> {
        // TODO optimize
        write!(self.output, "{}", value.into())?;

        Ok(())
    }

    fn serialize_uint(&mut self, value: impl Into<LargeUInt>) -> Result<()> {
        // TODO optimize
        write!(self.output, "{}", value.into())?;

        Ok(())
    }

    fn write_identifier(&mut self, name: &str) -> Result<()> {
        if name.is_empty() || !name.as_bytes().iter().copied().all(is_ident_raw_char) {
            return Err(Error::InvalidIdentifier(name.into()));
        }
        let mut bytes = name.as_bytes().iter().copied();
        if !bytes.next().map_or(false, is_ident_first_char) || !bytes.all(is_ident_other_char) {
            self.output.write_all(b"r#")?;
        }
        self.output.write_all(name.as_bytes())?;
        Ok(())
    }

    fn struct_names(&self) -> bool {
        self.pretty
            .as_ref()
            .map(|(pc, _)| pc.struct_names)
            .unwrap_or(false)
    }
}

macro_rules! guard_recursion {
    ($self:expr => $expr:expr) => {{
        if let Some(limit) = &mut $self.recursion_limit {
            if let Some(new_limit) = limit.checked_sub(1) {
                *limit = new_limit;
            } else {
                return Err(Error::ExceededRecursionLimit);
            }
        }

        let result = $expr;

        if let Some(limit) = &mut $self.recursion_limit {
            *limit = limit.saturating_add(1);
        }

        result
    }};
}

impl<'a, W: io::Write> ser::Serializer for &'a mut Serializer<W> {
    type Error = Error;
    type Ok = ();
    type SerializeMap = Compound<'a, W>;
    type SerializeSeq = Compound<'a, W>;
    type SerializeStruct = Compound<'a, W>;
    type SerializeStructVariant = Compound<'a, W>;
    type SerializeTuple = Compound<'a, W>;
    type SerializeTupleStruct = Compound<'a, W>;
    type SerializeTupleVariant = Compound<'a, W>;

    fn serialize_bool(self, v: bool) -> Result<()> {
        self.output.write_all(if v { b"true" } else { b"false" })?;
        Ok(())
    }

    fn serialize_i8(self, v: i8) -> Result<()> {
        self.serialize_sint(v)
    }

    fn serialize_i16(self, v: i16) -> Result<()> {
        self.serialize_sint(v)
    }

    fn serialize_i32(self, v: i32) -> Result<()> {
        self.serialize_sint(v)
    }

    fn serialize_i64(self, v: i64) -> Result<()> {
        self.serialize_sint(v)
    }

    #[cfg(feature = "integer128")]
    fn serialize_i128(self, v: i128) -> Result<()> {
        self.serialize_sint(v)
    }

    fn serialize_u8(self, v: u8) -> Result<()> {
        self.serialize_uint(v)
    }

    fn serialize_u16(self, v: u16) -> Result<()> {
        self.serialize_uint(v)
    }

    fn serialize_u32(self, v: u32) -> Result<()> {
        self.serialize_uint(v)
    }

    fn serialize_u64(self, v: u64) -> Result<()> {
        self.serialize_uint(v)
    }

    #[cfg(feature = "integer128")]
    fn serialize_u128(self, v: u128) -> Result<()> {
        self.serialize_uint(v)
    }

    fn serialize_f32(self, v: f32) -> Result<()> {
        write!(self.output, "{}", v)?;
        if v.fract() == 0.0 {
            write!(self.output, ".0")?;
        }
        Ok(())
    }

    fn serialize_f64(self, v: f64) -> Result<()> {
        write!(self.output, "{}", v)?;
        if v.fract() == 0.0 {
            write!(self.output, ".0")?;
        }
        Ok(())
    }

    fn serialize_char(self, v: char) -> Result<()> {
        self.output.write_all(b"'")?;
        if v == '\\' || v == '\'' {
            self.output.write_all(b"\\")?;
        }
        write!(self.output, "{}", v)?;
        self.output.write_all(b"'")?;
        Ok(())
    }

    fn serialize_str(self, v: &str) -> Result<()> {
        self.serialize_escaped_str(v)?;

        Ok(())
    }

    fn serialize_bytes(self, v: &[u8]) -> Result<()> {
        self.serialize_str(BASE64_ENGINE.encode(v).as_str())
    }

    fn serialize_none(self) -> Result<()> {
        self.output.write_all(b"None")?;

        Ok(())
    }

    fn serialize_some<T>(self, value: &T) -> Result<()>
    where
        T: ?Sized + Serialize,
    {
        let implicit_some = self.extensions().contains(Extensions::IMPLICIT_SOME);
        if !implicit_some {
            self.output.write_all(b"Some(")?;
        }
        guard_recursion! { self => value.serialize(&mut *self)? };
        if !implicit_some {
            self.output.write_all(b")")?;
        }

        Ok(())
    }

    fn serialize_unit(self) -> Result<()> {
        if !self.newtype_variant {
            self.output.write_all(b"()")?;
        }

        self.newtype_variant = false;

        Ok(())
    }

    fn serialize_unit_struct(self, name: &'static str) -> Result<()> {
        if self.struct_names() && !self.newtype_variant {
            self.write_identifier(name)?;

            Ok(())
        } else {
            self.serialize_unit()
        }
    }

    fn serialize_unit_variant(self, _: &'static str, _: u32, variant: &'static str) -> Result<()> {
        self.write_identifier(variant)?;

        Ok(())
    }

    fn serialize_newtype_struct<T>(self, name: &'static str, value: &T) -> Result<()>
    where
        T: ?Sized + Serialize,
    {
        if self.extensions().contains(Extensions::UNWRAP_NEWTYPES) || self.newtype_variant {
            self.newtype_variant = false;

            return guard_recursion! { self => value.serialize(&mut *self) };
        }

        if self.struct_names() {
            self.write_identifier(name)?;
        }

        self.output.write_all(b"(")?;
        guard_recursion! { self => value.serialize(&mut *self)? };
        self.output.write_all(b")")?;
        Ok(())
    }

    fn serialize_newtype_variant<T>(
        self,
        _: &'static str,
        _: u32,
        variant: &'static str,
        value: &T,
    ) -> Result<()>
    where
        T: ?Sized + Serialize,
    {
        self.write_identifier(variant)?;
        self.output.write_all(b"(")?;

        self.newtype_variant = self
            .extensions()
            .contains(Extensions::UNWRAP_VARIANT_NEWTYPES);

        guard_recursion! { self => value.serialize(&mut *self)? };

        self.newtype_variant = false;

        self.output.write_all(b")")?;
        Ok(())
    }

    fn serialize_seq(self, len: Option<usize>) -> Result<Self::SerializeSeq> {
        self.newtype_variant = false;

        self.output.write_all(b"[")?;

        if let Some(len) = len {
            self.is_empty = Some(len == 0);
        }

        if !self.compact_arrays() {
            self.start_indent()?;
        }

        if let Some((_, ref mut pretty)) = self.pretty {
            pretty.sequence_index.push(0);
        }

        Compound::try_new(self, false)
    }

    fn serialize_tuple(self, len: usize) -> Result<Self::SerializeTuple> {
        let old_newtype_variant = self.newtype_variant;
        self.newtype_variant = false;

        if !old_newtype_variant {
            self.output.write_all(b"(")?;
        }

        if self.separate_tuple_members() {
            self.is_empty = Some(len == 0);

            self.start_indent()?;
        }

        Compound::try_new(self, old_newtype_variant)
    }

    fn serialize_tuple_struct(
        self,
        name: &'static str,
        len: usize,
    ) -> Result<Self::SerializeTupleStruct> {
        if self.struct_names() && !self.newtype_variant {
            self.write_identifier(name)?;
        }

        self.serialize_tuple(len)
    }

    fn serialize_tuple_variant(
        self,
        _: &'static str,
        _: u32,
        variant: &'static str,
        len: usize,
    ) -> Result<Self::SerializeTupleVariant> {
        self.newtype_variant = false;

        self.write_identifier(variant)?;
        self.output.write_all(b"(")?;

        if self.separate_tuple_members() {
            self.is_empty = Some(len == 0);

            self.start_indent()?;
        }

        Compound::try_new(self, false)
    }

    fn serialize_map(self, len: Option<usize>) -> Result<Self::SerializeMap> {
        self.newtype_variant = false;

        self.output.write_all(b"{")?;

        if let Some(len) = len {
            self.is_empty = Some(len == 0);
        }

        self.start_indent()?;

        Compound::try_new(self, false)
    }

    fn serialize_struct(self, name: &'static str, len: usize) -> Result<Self::SerializeStruct> {
        let old_newtype_variant = self.newtype_variant;
        self.newtype_variant = false;

        if !old_newtype_variant {
            if self.struct_names() {
                self.write_identifier(name)?;
            }
            self.output.write_all(b"(")?;
        }

        self.is_empty = Some(len == 0);
        self.start_indent()?;

        Compound::try_new(self, old_newtype_variant)
    }

    fn serialize_struct_variant(
        self,
        _: &'static str,
        _: u32,
        variant: &'static str,
        len: usize,
    ) -> Result<Self::SerializeStructVariant> {
        self.newtype_variant = false;

        self.write_identifier(variant)?;
        self.output.write_all(b"(")?;

        self.is_empty = Some(len == 0);
        self.start_indent()?;

        Compound::try_new(self, false)
    }
}

enum State {
    First,
    Rest,
}

#[doc(hidden)]
pub struct Compound<'a, W: io::Write> {
    ser: &'a mut Serializer<W>,
    state: State,
    newtype_variant: bool,
}

impl<'a, W: io::Write> Compound<'a, W> {
    fn try_new(ser: &'a mut Serializer<W>, newtype_variant: bool) -> Result<Self> {
        if let Some(limit) = &mut ser.recursion_limit {
            if let Some(new_limit) = limit.checked_sub(1) {
                *limit = new_limit;
            } else {
                return Err(Error::ExceededRecursionLimit);
            }
        }

        Ok(Compound {
            ser,
            state: State::First,
            newtype_variant,
        })
    }
}

impl<'a, W: io::Write> Drop for Compound<'a, W> {
    fn drop(&mut self) {
        if let Some(limit) = &mut self.ser.recursion_limit {
            *limit = limit.saturating_add(1);
        }
    }
}

impl<'a, W: io::Write> ser::SerializeSeq for Compound<'a, W> {
    type Error = Error;
    type Ok = ();

    fn serialize_element<T>(&mut self, value: &T) -> Result<()>
    where
        T: ?Sized + Serialize,
    {
        if let State::First = self.state {
            self.state = State::Rest;
        } else {
            self.ser.output.write_all(b",")?;
            if let Some((ref config, ref mut pretty)) = self.ser.pretty {
                if pretty.indent <= config.depth_limit && !config.compact_arrays {
                    self.ser.output.write_all(config.new_line.as_bytes())?;
                } else {
                    self.ser.output.write_all(config.separator.as_bytes())?;
                }
            }
        }

        if !self.ser.compact_arrays() {
            self.ser.indent()?;
        }

        if let Some((ref mut config, ref mut pretty)) = self.ser.pretty {
            if pretty.indent <= config.depth_limit && config.enumerate_arrays {
                let index = pretty.sequence_index.last_mut().unwrap();
                write!(self.ser.output, "/*[{}]*/ ", index)?;
                *index += 1;
            }
        }

        guard_recursion! { self.ser => value.serialize(&mut *self.ser)? };

        Ok(())
    }

    fn end(self) -> Result<()> {
        if let State::Rest = self.state {
            if let Some((ref config, ref mut pretty)) = self.ser.pretty {
                if pretty.indent <= config.depth_limit && !config.compact_arrays {
                    self.ser.output.write_all(b",")?;
                    self.ser.output.write_all(config.new_line.as_bytes())?;
                }
            }
        }

        if !self.ser.compact_arrays() {
            self.ser.end_indent()?;
        }

        if let Some((_, ref mut pretty)) = self.ser.pretty {
            pretty.sequence_index.pop();
        }

        // seq always disables `self.newtype_variant`
        self.ser.output.write_all(b"]")?;
        Ok(())
    }
}

impl<'a, W: io::Write> ser::SerializeTuple for Compound<'a, W> {
    type Error = Error;
    type Ok = ();

    fn serialize_element<T>(&mut self, value: &T) -> Result<()>
    where
        T: ?Sized + Serialize,
    {
        if let State::First = self.state {
            self.state = State::Rest;
        } else {
            self.ser.output.write_all(b",")?;
            if let Some((ref config, ref pretty)) = self.ser.pretty {
                if pretty.indent <= config.depth_limit && self.ser.separate_tuple_members() {
                    self.ser.output.write_all(config.new_line.as_bytes())?;
                } else {
                    self.ser.output.write_all(config.separator.as_bytes())?;
                }
            }
        }

        if self.ser.separate_tuple_members() {
            self.ser.indent()?;
        }

        guard_recursion! { self.ser => value.serialize(&mut *self.ser)? };

        Ok(())
    }

    fn end(self) -> Result<()> {
        if let State::Rest = self.state {
            if let Some((ref config, ref pretty)) = self.ser.pretty {
                if self.ser.separate_tuple_members() && pretty.indent <= config.depth_limit {
                    self.ser.output.write_all(b",")?;
                    self.ser.output.write_all(config.new_line.as_bytes())?;
                }
            }
        }
        if self.ser.separate_tuple_members() {
            self.ser.end_indent()?;
        }

        if !self.newtype_variant {
            self.ser.output.write_all(b")")?;
        }

        Ok(())
    }
}

// Same thing but for tuple structs.
impl<'a, W: io::Write> ser::SerializeTupleStruct for Compound<'a, W> {
    type Error = Error;
    type Ok = ();

    fn serialize_field<T>(&mut self, value: &T) -> Result<()>
    where
        T: ?Sized + Serialize,
    {
        ser::SerializeTuple::serialize_element(self, value)
    }

    fn end(self) -> Result<()> {
        ser::SerializeTuple::end(self)
    }
}

impl<'a, W: io::Write> ser::SerializeTupleVariant for Compound<'a, W> {
    type Error = Error;
    type Ok = ();

    fn serialize_field<T>(&mut self, value: &T) -> Result<()>
    where
        T: ?Sized + Serialize,
    {
        ser::SerializeTuple::serialize_element(self, value)
    }

    fn end(self) -> Result<()> {
        ser::SerializeTuple::end(self)
    }
}

impl<'a, W: io::Write> ser::SerializeMap for Compound<'a, W> {
    type Error = Error;
    type Ok = ();

    fn serialize_key<T>(&mut self, key: &T) -> Result<()>
    where
        T: ?Sized + Serialize,
    {
        if let State::First = self.state {
            self.state = State::Rest;
        } else {
            self.ser.output.write_all(b",")?;

            if let Some((ref config, ref pretty)) = self.ser.pretty {
                if pretty.indent <= config.depth_limit {
                    self.ser.output.write_all(config.new_line.as_bytes())?;
                } else {
                    self.ser.output.write_all(config.separator.as_bytes())?;
                }
            }
        }
        self.ser.indent()?;
        guard_recursion! { self.ser => key.serialize(&mut *self.ser) }
    }

    fn serialize_value<T>(&mut self, value: &T) -> Result<()>
    where
        T: ?Sized + Serialize,
    {
        self.ser.output.write_all(b":")?;

        if let Some((ref config, _)) = self.ser.pretty {
            self.ser.output.write_all(config.separator.as_bytes())?;
        }

        guard_recursion! { self.ser => value.serialize(&mut *self.ser)? };

        Ok(())
    }

    fn end(self) -> Result<()> {
        if let State::Rest = self.state {
            if let Some((ref config, ref pretty)) = self.ser.pretty {
                if pretty.indent <= config.depth_limit {
                    self.ser.output.write_all(b",")?;
                    self.ser.output.write_all(config.new_line.as_bytes())?;
                }
            }
        }
        self.ser.end_indent()?;
        // map always disables `self.newtype_variant`
        self.ser.output.write_all(b"}")?;
        Ok(())
    }
}

impl<'a, W: io::Write> ser::SerializeStruct for Compound<'a, W> {
    type Error = Error;
    type Ok = ();

    fn serialize_field<T>(&mut self, key: &'static str, value: &T) -> Result<()>
    where
        T: ?Sized + Serialize,
    {
        if let State::First = self.state {
            self.state = State::Rest;
        } else {
            self.ser.output.write_all(b",")?;

            if let Some((ref config, ref pretty)) = self.ser.pretty {
                if pretty.indent <= config.depth_limit {
                    self.ser.output.write_all(config.new_line.as_bytes())?;
                } else {
                    self.ser.output.write_all(config.separator.as_bytes())?;
                }
            }
        }
        self.ser.indent()?;
        self.ser.write_identifier(key)?;
        self.ser.output.write_all(b":")?;

        if let Some((ref config, _)) = self.ser.pretty {
            self.ser.output.write_all(config.separator.as_bytes())?;
        }

        guard_recursion! { self.ser => value.serialize(&mut *self.ser)? };

        Ok(())
    }

    fn end(self) -> Result<()> {
        if let State::Rest = self.state {
            if let Some((ref config, ref pretty)) = self.ser.pretty {
                if pretty.indent <= config.depth_limit {
                    self.ser.output.write_all(b",")?;
                    self.ser.output.write_all(config.new_line.as_bytes())?;
                }
            }
        }
        self.ser.end_indent()?;
        if !self.newtype_variant {
            self.ser.output.write_all(b")")?;
        }
        Ok(())
    }
}

impl<'a, W: io::Write> ser::SerializeStructVariant for Compound<'a, W> {
    type Error = Error;
    type Ok = ();

    fn serialize_field<T>(&mut self, key: &'static str, value: &T) -> Result<()>
    where
        T: ?Sized + Serialize,
    {
        ser::SerializeStruct::serialize_field(self, key, value)
    }

    fn end(self) -> Result<()> {
        ser::SerializeStruct::end(self)
    }
}
