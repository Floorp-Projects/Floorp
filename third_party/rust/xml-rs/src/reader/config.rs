//! Contains parser configuration structure.
use std::io::Read;
use std::collections::HashMap;

use reader::EventReader;

/// Parser configuration structure.
///
/// This structure contains various configuration options which affect
/// behavior of the parser.
#[derive(Clone, PartialEq, Eq, Debug)]
pub struct ParserConfig {
    /// Whether or not should whitespace in textual events be removed. Default is false.
    ///
    /// When true, all standalone whitespace will be removed (this means no
    /// `Whitespace` events will ve emitted), and leading and trailing whitespace
    /// from `Character` events will be deleted. If after trimming `Characters`
    /// event will be empty, it will also be omitted from output stream. This is
    /// possible, however, only if `whitespace_to_characters` or
    /// `cdata_to_characters` options are set.
    ///
    /// This option does not affect CDATA events, unless `cdata_to_characters`
    /// option is also set. In that case CDATA content will also be trimmed.
    pub trim_whitespace: bool,

    /// Whether or not should whitespace be converted to characters.
    /// Default is false.
    ///
    /// If true, instead of `Whitespace` events `Characters` events with the
    /// same content will be emitted. If `trim_whitespace` is also true, these
    /// events will be trimmed to nothing and, consequently, not emitted.
    pub whitespace_to_characters: bool,

    /// Whether or not should CDATA be converted to characters.
    /// Default is false.
    ///
    /// If true, instead of `CData` events `Characters` events with the same
    /// content will be emitted. If `trim_whitespace` is also true, these events
    /// will be trimmed. If corresponding CDATA contained nothing but whitespace,
    /// this event will be omitted from the stream.
    pub cdata_to_characters: bool,

    /// Whether or not should comments be omitted. Default is true.
    ///
    /// If true, `Comment` events will not be emitted at all.
    pub ignore_comments: bool,

    /// Whether or not should sequential `Characters` events be merged.
    /// Default is true.
    ///
    /// If true, multiple sequential `Characters` events will be merged into
    /// a single event, that is, their data will be concatenated.
    ///
    /// Multiple sequential `Characters` events are only possible if either
    /// `cdata_to_characters` or `ignore_comments` are set. Otherwise character
    /// events will always be separated by other events.
    pub coalesce_characters: bool,

    /// A map of extra entities recognized by the parser. Default is an empty map.
    ///
    /// By default the XML parser recognizes the entities defined in the XML spec. Sometimes,
    /// however, it is convenient to make the parser recognize additional entities which
    /// are also not available through the DTD definitions (especially given that at the moment
    /// DTD parsing is not supported).
    pub extra_entities: HashMap<String, String>,

    /// Whether or not the parser should ignore the end of stream. Default is false.
    ///
    /// By default the parser will either error out when it encounters a premature end of
    /// stream or complete normally if the end of stream was expected. If you want to continue
    /// reading from a stream whose input is supplied progressively, you can set this option to true.
    /// In this case the parser will allow you to invoke the next() method even if a supposed end
    /// of stream has happened.
    ///
    /// Note that support for this functionality is incomplete; for example, the parser will fail if
    /// the premature end of stream happens inside PCDATA. Therefore, use this option at your own risk.
    pub ignore_end_of_stream: bool,

    /// Whether or not non-unicode entity references get replaced with the replacement character
    ///
    /// When true, any decimal or hexadecimal character reference that cannot be converted from a
    /// u32 to a char using [std::char::from_u32](https://doc.rust-lang.org/std/char/fn.from_u32.html)
    /// will be converted into the unicode REPLACEMENT CHARACTER (U+FFFD).
    pub replace_unknown_entity_references: bool,

    /// Whether or not whitespace at the root level of the document is ignored. Default is true.
    ///
    /// By default any whitespace that is not enclosed within at least one level of elements will be
    /// ignored. Setting this value to false will cause root level whitespace events to be emitted.
    pub ignore_root_level_whitespace: bool,
}

impl ParserConfig {
    /// Returns a new config with default values.
    ///
    /// You can tweak default values using builder-like pattern:
    ///
    /// ```rust
    /// use xml::reader::ParserConfig;
    ///
    /// let config = ParserConfig::new()
    ///     .trim_whitespace(true)
    ///     .ignore_comments(true)
    ///     .coalesce_characters(false);
    /// ```
    pub fn new() -> ParserConfig {
        ParserConfig {
            trim_whitespace: false,
            whitespace_to_characters: false,
            cdata_to_characters: false,
            ignore_comments: true,
            coalesce_characters: true,
            extra_entities: HashMap::new(),
            ignore_end_of_stream: false,
            replace_unknown_entity_references: false,
            ignore_root_level_whitespace: true,
        }
    }

    /// Creates an XML reader with this configuration.
    ///
    /// This is a convenience method for configuring and creating a reader at the same time:
    ///
    /// ```rust
    /// use xml::reader::ParserConfig;
    ///
    /// let mut source: &[u8] = b"...";
    ///
    /// let reader = ParserConfig::new()
    ///     .trim_whitespace(true)
    ///     .ignore_comments(true)
    ///     .coalesce_characters(false)
    ///     .create_reader(&mut source);
    /// ```
    ///
    /// This method is exactly equivalent to calling `EventReader::new_with_config()` with
    /// this configuration object.
    #[inline]
    pub fn create_reader<R: Read>(self, source: R) -> EventReader<R> {
        EventReader::new_with_config(source, self)
    }

    /// Adds a new entity mapping and returns an updated config object.
    ///
    /// This is a convenience method for adding external entities mappings to the XML parser.
    /// An example:
    ///
    /// ```rust
    /// use xml::reader::ParserConfig;
    ///
    /// let mut source: &[u8] = b"...";
    ///
    /// let reader = ParserConfig::new()
    ///     .add_entity("nbsp", " ")
    ///     .add_entity("copy", "©")
    ///     .add_entity("reg", "®")
    ///     .create_reader(&mut source);
    /// ```
    pub fn add_entity<S: Into<String>, T: Into<String>>(mut self, entity: S, value: T) -> ParserConfig {
        self.extra_entities.insert(entity.into(), value.into());
        self
    }
}

impl Default for ParserConfig {
    #[inline]
    fn default() -> ParserConfig {
        ParserConfig::new()
    }
}

gen_setters! { ParserConfig,
    trim_whitespace: val bool,
    whitespace_to_characters: val bool,
    cdata_to_characters: val bool,
    ignore_comments: val bool,
    coalesce_characters: val bool,
    ignore_end_of_stream: val bool,
    replace_unknown_entity_references: val bool,
    ignore_root_level_whitespace: val bool
}
