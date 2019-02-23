use std::collections::HashMap;
use std::error::Error as StdError;
use std::fmt::{self, Debug};
use std::io::{self, Write};
use std::str::FromStr;
use std::result;

use regex::{Captures, Regex};
use serde::de;
use serde::de::IntoDeserializer;

use parse::Parser;
use synonym::SynonymMap;

use self::Value::{Switch, Counted, Plain, List};
use self::Error::{Usage, Argv, NoMatch, Deserialize, WithProgramUsage, Help, Version};

use cap_or_empty;

/// Represents the different types of Docopt errors.
///
/// This error type has a lot of variants. In the common case, you probably
/// don't care why Docopt has failed, and would rather just quit the program
/// and show an error message instead. The `exit` method defined on the `Error`
/// type will do just that. It will also set the exit code appropriately
/// (no error for `--help` or `--version`, but an error code for bad usage,
/// bad argv, no match or bad decode).
///
/// ### Example
///
/// Generally, you want to parse the usage string, try to match the argv
/// and then quit the program if there was an error reported at any point
/// in that process. This can be achieved like so:
///
/// ```no_run
/// use docopt::Docopt;
///
/// const USAGE: &'static str = "
/// Usage: ...
/// ";
///
/// let args = Docopt::new(USAGE)
///                   .and_then(|d| d.parse())
///                   .unwrap_or_else(|e| e.exit());
/// ```
#[derive(Debug)]
pub enum Error {
    /// Parsing the usage string failed.
    ///
    /// This error can only be triggered by the programmer, i.e., the writer
    /// of the Docopt usage string. This error is usually indicative of a bug
    /// in your program.
    Usage(String),

    /// Parsing the argv specified failed.
    ///
    /// The payload is a string describing why the arguments provided could not
    /// be parsed.
    ///
    /// This is distinct from `NoMatch` because it will catch errors like
    /// using flags that aren't defined in the usage string.
    Argv(String),

    /// The given argv parsed successfully, but it did not match any example
    /// usage of the program.
    ///
    /// Regrettably, there is no descriptive message describing *why* the
    /// given argv didn't match any of the usage strings.
    NoMatch,

    /// This indicates a problem deserializing a successful argv match into a
    /// deserializable value.
    Deserialize(String),

    /// Parsing failed, and the program usage should be printed next to the
    /// failure message. Typically this wraps `Argv` and `NoMatch` errors.
    WithProgramUsage(Box<Error>, String),

    /// Decoding or parsing failed because the command line specified that the
    /// help message should be printed.
    Help,

    /// Decoding or parsing failed because the command line specified that the
    /// version should be printed
    ///
    /// The version is included as a payload to this variant.
    Version(String),
}

impl Error {
    /// Return whether this was a fatal error or not.
    ///
    /// Non-fatal errors include requests to print the help or version
    /// information of a program, while fatal errors include those such as
    /// failing to decode or parse.
    pub fn fatal(&self) -> bool {
        match *self {
            Help | Version(..) => false,
            Usage(..) | Argv(..) | NoMatch | Deserialize(..) => true,
            WithProgramUsage(ref b, _) => b.fatal(),
        }
    }

    /// Print this error and immediately exit the program.
    ///
    /// If the error is non-fatal (e.g., `Help` or `Version`), then the
    /// error is printed to stdout and the exit status will be `0`. Otherwise,
    /// when the error is fatal, the error is printed to stderr and the
    /// exit status will be `1`.
    pub fn exit(&self) -> ! {
        if self.fatal() {
            werr!("{}\n", self);
            ::std::process::exit(1)
        } else {
            let _ = writeln!(&mut io::stdout(), "{}", self);
            ::std::process::exit(0)
        }
    }
}

type Result<T> = result::Result<T, Error>;

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            WithProgramUsage(ref other, ref usage) => {
                let other = other.to_string();
                if other.is_empty() {
                    write!(f, "{}", usage)
                } else {
                    write!(f, "{}\n\n{}", other, usage)
                }
            }
            Help => write!(f, ""),
            NoMatch => write!(f, "Invalid arguments."),
            Usage(ref s) |
            Argv(ref s) |
            Deserialize(ref s) |
            Version(ref s) => write!(f, "{}", s),
        }
    }
}

impl StdError for Error {
    fn description(&self) -> &str {
        match *self {
            Usage(..) => "invalid usage string",
            Argv(..) => "failed to parse specified argv",
            NoMatch => "could not match specified argv",
            Deserialize(..) => "failed to deserialize",
            WithProgramUsage(..) => "failed to parse specified argv",
            Help => "help message requested",
            Version(..) => "version message requested",
        }
    }

    fn cause(&self) -> Option<&StdError> {
        match *self {
            WithProgramUsage(ref cause, _) => Some(&**cause),
            _ => None,
        }
    }
}

impl de::Error for Error {
    fn custom<T: fmt::Display>(msg: T) -> Self {
        Error::Deserialize(msg.to_string())
    }
}

/// The main Docopt type, which is constructed with a Docopt usage string.
///
/// This can be used to match command line arguments to produce a `ArgvMap`.
#[derive(Clone, Debug)]
pub struct Docopt {
    p: Parser,
    argv: Option<Vec<String>>,
    options_first: bool,
    help: bool,
    version: Option<String>,
}

impl Docopt {
    /// Parse the Docopt usage string given.
    ///
    /// The `Docopt` value returned may be used immediately to parse command
    /// line arguments with a default configuration.
    ///
    /// If there was a problem parsing the usage string, a `Usage` error
    /// is returned.
    pub fn new<S>(usage: S) -> Result<Docopt>
            where S: ::std::ops::Deref<Target=str> {
        Parser::new(usage.deref())
               .map_err(Usage)
               .map(|p| Docopt {
                   p: p,
                   argv: None,
                   options_first: false,
                   help: true,
                   version: None,
                })
    }

    /// Parse and deserialize the given argv.
    ///
    /// This is a convenience method for
    /// `parse().and_then(|vals| vals.deserialize())`.
    ///
    /// For details on how decoding works, please see the documentation for
    /// `ArgvMap`.
    pub fn deserialize<'a, 'de: 'a, D>(&'a self) -> Result<D>
        where D: de::Deserialize<'de>
    {
        self.parse().and_then(|vals| vals.deserialize())
    }

    /// Parse command line arguments and try to match them against a usage
    /// pattern specified in the Docopt string.
    ///
    /// If there is a match, then an `ArgvMap` is returned, which maps
    /// flags, commands and arguments to values.
    ///
    /// If parsing the command line arguments fails, then an `Argv` error is
    /// returned. If parsing succeeds but there is no match, then a `NoMatch`
    /// error is returned. Both of these errors are always returned inside a
    /// `WithProgramUsage` error.
    ///
    /// If special handling of `help` or `version` is enabled (the former is
    /// enabled by default), then `Help` or `Version` errors are returned
    /// if `--help` or `--version` is present.
    pub fn parse(&self) -> Result<ArgvMap> {
        let argv = self.argv.clone().unwrap_or_else(Docopt::get_argv);
        let vals =
            self.p.parse_argv(argv, self.options_first)
                .map_err(|s| self.err_with_usage(Argv(s)))
                .and_then(|argv|
                    match self.p.matches(&argv) {
                        Some(m) => Ok(ArgvMap { map: m }),
                        None => Err(self.err_with_usage(NoMatch)),
                    })?;
        if self.help && vals.get_bool("--help") {
            return Err(self.err_with_full_doc(Help));
        }
        match self.version {
            Some(ref v) if vals.get_bool("--version") => {
                return Err(Version(v.clone()))
            }
            _ => {},
        }
        Ok(vals)
    }

    /// Set the argv to be used for Docopt parsing.
    ///
    /// By default, when no argv is set, and it is automatically taken from
    /// `std::env::args()`.
    ///
    /// The `argv` given *must* be the full set of `argv` passed to the
    /// program. e.g., `["cp", "src", "dest"]` is right while `["src", "dest"]`
    /// is wrong.
    pub fn argv<I, S>(mut self, argv: I) -> Docopt
               where I: IntoIterator<Item=S>, S: AsRef<str> {
        self.argv = Some(
            argv.into_iter().skip(1).map(|s| s.as_ref().to_owned()).collect()
        );
        self
    }

    /// Enables the "options first" Docopt behavior.
    ///
    /// The options first behavior means that all flags *must* appear before
    /// position arguments. That is, after the first position argument is
    /// seen, all proceeding arguments are interpreted as positional
    /// arguments unconditionally.
    pub fn options_first(mut self, yes: bool) -> Docopt {
        self.options_first = yes;
        self
    }

    /// Enables automatic handling of `--help`.
    ///
    /// When this is enabled and `--help` appears anywhere in the arguments,
    /// then a `Help` error will be returned. You may then use the `exit`
    /// method on the error value to conveniently quit the program (which will
    /// print the full usage string to stdout).
    ///
    /// Note that for this to work, `--help` must be a valid pattern.
    ///
    /// When disabled, there is no special handling of `--help`.
    pub fn help(mut self, yes: bool) -> Docopt {
        self.help = yes;
        self
    }

    /// Enables automatic handling of `--version`.
    ///
    /// When this is enabled and `--version` appears anywhere in the arguments,
    /// then a `Version(s)` error will be returned, where `s` is the string
    /// given here. You may then use the `exit` method on the error value to
    /// convenient quit the program (which will print the version to stdout).
    ///
    /// When disabled (a `None` value), there is no special handling of
    /// `--version`.
    pub fn version(mut self, version: Option<String>) -> Docopt {
        self.version = version;
        self
    }

    #[doc(hidden)]
    pub fn parser(&self) -> &Parser {
        &self.p
    }

    fn err_with_usage(&self, e: Error) -> Error {
        WithProgramUsage(
            Box::new(e), self.p.usage.trim().into())
    }

    fn err_with_full_doc(&self, e: Error) -> Error {
        WithProgramUsage(
            Box::new(e), self.p.full_doc.trim().into())
    }

    fn get_argv() -> Vec<String> {
        // Hmm, we should probably handle a Unicode decode error here... ---AG
        ::std::env::args().skip(1).collect()
    }
}

/// A map containing matched values from command line arguments.
///
/// The keys are just as specified in Docopt: `--flag` for a long flag or
/// `-f` for a short flag. (If `-f` is a synonym for `--flag`, then either
/// key will work.) `ARG` or `<arg>` specify a positional argument and `cmd`
/// specifies a command.
#[derive(Clone)]
pub struct ArgvMap {
    #[doc(hidden)]
    pub map: SynonymMap<String, Value>,
}

impl ArgvMap {
    /// Tries to deserialize the map of values into a struct.
    ///
    /// This method should always be called to deserialize a `ArgvMap` into
    /// a struct. All fields of the struct must map to a corresponding key
    /// in the `ArgvMap`. To this end, each member must have a special prefix
    /// corresponding to the different kinds of patterns in Docopt. There are
    /// three prefixes: `flag_`, `arg_` and `cmd_` which respectively
    /// correspond to short/long flags, positional arguments and commands.
    ///
    /// If a Docopt item has a `-` in its name, then it is converted to an `_`.
    ///
    /// # Example
    ///
    /// ```rust
    /// # extern crate docopt;
    /// #[macro_use]
    /// extern crate serde_derive;
    /// # extern crate serde;
    /// # fn main() {
    /// use docopt::Docopt;
    ///
    /// const USAGE: &'static str = "
    /// Usage: cargo [options] (build | test)
    ///        cargo --help
    ///
    /// Options: -v, --verbose
    ///          -h, --help
    /// ";
    ///
    /// #[derive(Deserialize)]
    /// struct Args {
    ///   cmd_build: bool,
    ///   cmd_test: bool,
    ///   flag_verbose: bool,
    ///   flag_h: bool,
    /// }
    ///
    /// let argv = || vec!["cargo", "build", "-v"].into_iter();
    /// let args: Args = Docopt::new(USAGE)
    ///                         .and_then(|d| d.argv(argv()).deserialize())
    ///                         .unwrap_or_else(|e| e.exit());
    /// assert!(args.cmd_build && !args.cmd_test
    ///         && args.flag_verbose && !args.flag_h);
    /// # }
    /// ```
    ///
    /// Note that in the above example, `flag_h` is used but `flag_help`
    /// could also be used. (In fact, both could be used at the same time.)
    ///
    /// In this example, only the `bool` type was used, but any type satisfying
    /// the `Deserialize` trait is valid.
    pub fn deserialize<'de, T: de::Deserialize<'de>>(self) -> Result<T> {
        de::Deserialize::deserialize(&mut Deserializer {
                                              vals: self,
                                              stack: vec![],
                                          })
    }

    /// Finds the value corresponding to `key` and calls `as_bool()` on it.
    /// If the key does not exist, `false` is returned.
    pub fn get_bool(&self, key: &str) -> bool {
        self.find(key).map_or(false, |v| v.as_bool())
    }

    /// Finds the value corresponding to `key` and calls `as_count()` on it.
    /// If the key does not exist, `0` is returned.
    pub fn get_count(&self, key: &str) -> u64 {
        self.find(key).map_or(0, |v| v.as_count())
    }

    /// Finds the value corresponding to `key` and calls `as_str()` on it.
    /// If the key does not exist, `""` is returned.
    pub fn get_str(&self, key: &str) -> &str {
        self.find(key).map_or("", |v| v.as_str())
    }

    /// Finds the value corresponding to `key` and calls `as_vec()` on it.
    /// If the key does not exist, `vec!()` is returned.
    pub fn get_vec(&self, key: &str) -> Vec<&str> {
        self.find(key).map(|v| v.as_vec()).unwrap_or(vec!())
    }

    /// Return the raw value corresponding to some `key`.
    ///
    /// `key` should be a string in the traditional Docopt format. e.g.,
    /// `<arg>` or `--flag`.
    pub fn find(&self, key: &str) -> Option<&Value> {
        self.map.find(&key.into())
    }

    /// Return the number of values, not including synonyms.
    pub fn len(&self) -> usize {
        self.map.len()
    }

    /// Converts a Docopt key to a struct field name.
    /// This makes a half-hearted attempt at making the key a valid struct
    /// field name (like replacing `-` with `_`), but it does not otherwise
    /// guarantee that the result is a valid struct field name.
    #[doc(hidden)]
    pub fn key_to_struct_field(name: &str) -> String {
        lazy_static! {
            static ref RE: Regex = regex!(
                r"^(?:--?(?P<flag>\S+)|(?:(?P<argu>\p{Lu}+)|<(?P<argb>[^>]+)>)|(?P<cmd>\S+))$"
            );
        }
        fn sanitize(name: &str) -> String {
            name.replace("-", "_")
        }

        RE.replace(name, |cap: &Captures| {
            let (flag, cmd) = (
                cap_or_empty(cap, "flag"),
                cap_or_empty(cap, "cmd"),
            );
            let (argu, argb) = (
                cap_or_empty(cap, "argu"),
                cap_or_empty(cap, "argb"),
            );
            let (prefix, name) =
                if !flag.is_empty() {
                    ("flag_", flag)
                } else if !argu.is_empty() {
                    ("arg_", argu)
                } else if !argb.is_empty() {
                    ("arg_", argb)
                } else if !cmd.is_empty() {
                    ("cmd_", cmd)
                } else {
                    panic!("Unknown ArgvMap key: '{}'", name)
                };
            let mut prefix = prefix.to_owned();
            prefix.push_str(&sanitize(name));
            prefix
        }).into_owned()
    }

    /// Converts a struct field name to a Docopt key.
    #[doc(hidden)]
    pub fn struct_field_to_key(field: &str) -> String {
        lazy_static! {
            static ref FLAG: Regex = regex!(r"^flag_");
            static ref ARG: Regex = regex!(r"^arg_");
            static ref LETTERS: Regex = regex!(r"^\p{Lu}+$");
            static ref CMD: Regex = regex!(r"^cmd_");
        }
        fn desanitize(name: &str) -> String {
            name.replace("_", "-")
        }
        let name =
            if field.starts_with("flag_") {
                let name = FLAG.replace(field, "");
                let mut pre_name = (if name.len() == 1 { "-" } else { "--" })
                                   .to_owned();
                pre_name.push_str(&*name);
                pre_name
            } else if field.starts_with("arg_") {
                let name = ARG.replace(field, "").into_owned();
                if LETTERS.is_match(&name) {
                    name
                } else {
                    let mut pre_name = "<".to_owned();
                    pre_name.push_str(&*name);
                    pre_name.push('>');
                    pre_name
                }
            } else if field.starts_with("cmd_") {
                CMD.replace(field, "").into_owned()
            } else {
                panic!("Unrecognized struct field: '{}'", field)
            };
        desanitize(&*name)
    }
}

impl fmt::Debug for ArgvMap {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if self.len() == 0 {
            return write!(f, "{{EMPTY}}");
        }

        // This is a little crazy, but we want to group synonyms with
        // their keys and sort them for predictable output.
        let reverse: HashMap<&String, &String> =
            self.map.synonyms().map(|(from, to)| (to, from)).collect();
        let mut keys: Vec<&String> = self.map.keys().collect();
        keys.sort();
        let mut first = true;
        for &k in &keys {
            if !first { write!(f, "\n")?; } else { first = false; }
            match reverse.get(&k) {
                None => {
                    write!(f, "{} => {:?}", k, self.map.get(k))?
                }
                Some(s) => {
                    write!(f, "{}, {} => {:?}", s, k, self.map.get(k))?
                }
            }
        }
        Ok(())
    }
}

/// A matched command line value.
///
/// The value can be a boolean, counted repetition, a plain string or a list
/// of strings.
///
/// The various `as_{bool,count,str,vec}` methods provide convenient access
/// to values without destructuring manually.
#[derive(Clone, Debug, PartialEq)]
pub enum Value {
    /// A boolean value from a flag that has no argument.
    ///
    /// The presence of a flag means `true` and the absence of a flag
    /// means `false`.
    Switch(bool),

    /// The number of occurrences of a repeated flag.
    Counted(u64),

    /// A positional or flag argument.
    ///
    /// This is `None` when the positional argument or flag is not present.
    /// Note that it is possible to have `Some("")` for a present but empty
    /// argument.
    Plain(Option<String>),

    /// A List of positional or flag arguments.
    ///
    /// This list may be empty when no arguments or flags are present.
    List(Vec<String>),
}

impl Value {
    /// Returns the value as a bool.
    ///
    /// Counted repetitions are `false` if `0` and `true` otherwise.
    /// Plain strings are `true` if present and `false` otherwise.
    /// Lists are `true` if non-empty and `false` otherwise.
    pub fn as_bool(&self) -> bool {
        match *self {
            Switch(b) => b,
            Counted(n) => n > 0,
            Plain(None) => false,
            Plain(Some(_)) => true,
            List(ref vs) => !vs.is_empty(),
        }
    }

    /// Returns the value as a count of the number of times it occurred.
    ///
    /// Booleans are `1` if `true` and `0` otherwise.
    /// Plain strings are `1` if present and `0` otherwise.
    /// Lists correspond to its length.
    pub fn as_count(&self) -> u64 {
        match *self {
            Switch(b) => if b { 1 } else { 0 },
            Counted(n) => n,
            Plain(None) => 0,
            Plain(Some(_)) => 1,
            List(ref vs) => vs.len() as u64,
        }
    }

    /// Returns the value as a string.
    ///
    /// All values return an empty string except for a non-empty plain string.
    pub fn as_str(&self) -> &str {
        match *self {
            Switch(_) | Counted(_) | Plain(None) | List(_) => "",
            Plain(Some(ref s)) => &**s,
        }
    }

    /// Returns the value as a list of strings.
    ///
    /// Booleans, repetitions and empty strings correspond to an empty list.
    /// Plain strings correspond to a list of length `1`.
    pub fn as_vec(&self) -> Vec<&str> {
        match *self {
            Switch(_) | Counted(_) | Plain(None) => vec![],
            Plain(Some(ref s)) => vec![&**s],
            List(ref vs) => vs.iter().map(|s| &**s).collect(),
        }
    }
}

/// Deserializer for `ArgvMap` into your own `Deserialize`able types.
///
/// In general, you shouldn't have to use this type directly. It is exposed
/// in case you want to write a generic function that produces a deserializable
/// value. For example, here's a function that takes a usage string, an argv
/// and produces a deserializable value:
///
/// ```rust
/// # extern crate docopt;
/// extern crate serde;
/// # fn main() {
/// use docopt::Docopt;
/// use serde::de::Deserialize;
///
/// fn deserialize<'de, D: Deserialize<'de>>(usage: &str, argv: &[&str])
///                         -> Result<D, docopt::Error> {
///     Docopt::new(usage)
///            .and_then(|d| d.argv(argv.iter()).deserialize())
/// }
/// # }
pub struct Deserializer<'de> {
    vals: ArgvMap,
    stack: Vec<DeserializerItem<'de>>,
}

#[derive(Debug)]
struct DeserializerItem<'de> {
    key: String,
    struct_field: &'de str,
    val: Option<Value>,
}

macro_rules! derr(
    ($($arg:tt)*) => (return Err(Deserialize(format!($($arg)*))))
);

impl<'de> Deserializer<'de> {
    fn push(&mut self, struct_field: &'de str) {
        let key = ArgvMap::struct_field_to_key(struct_field);
        self.stack
            .push(DeserializerItem {
                      key: key.clone(),
                      struct_field: struct_field,
                      val: self.vals.find(&*key).cloned(),
                  });
    }

    fn pop(&mut self) -> Result<DeserializerItem> {
        match self.stack.pop() {
            None => derr!("Could not deserialize value into unknown key."),
            Some(it) => Ok(it),
        }
    }

    fn pop_key_val(&mut self) -> Result<(String, Value)> {
        let it = self.pop()?;
        match it.val {
            None => {
                derr!("Could not find argument '{}' (from struct field '{}').
Note that each struct field must have the right key prefix, which must
be one of `cmd_`, `flag_` or `arg_`.",
                      it.key,
                      it.struct_field)
            }
            Some(v) => Ok((it.key, v)),
        }
    }

    fn pop_val(&mut self) -> Result<Value> {
        let (_, v) = self.pop_key_val()?;
        Ok(v)
    }

    fn to_number<T>(&mut self, expect: &str) -> Result<T>
        where T: FromStr + ToString,
              <T as FromStr>::Err: Debug
    {
        let (k, v) = self.pop_key_val()?;
        match v {
            Counted(n) => Ok(n.to_string().parse().unwrap()), // lol
            _ => {
                if v.as_str().trim().is_empty() {
                    Ok("0".parse().unwrap()) // lol
                } else {
                    match v.as_str().parse() {
                        Err(_) => {
                            derr!("Could not deserialize '{}' to {} for '{}'.",
                                  v.as_str(),
                                  expect,
                                  k)
                        }
                        Ok(v) => Ok(v),
                    }
                }
            }
        }
    }

    fn to_float(&mut self, expect: &str) -> Result<f64> {
        let (k, v) = self.pop_key_val()?;
        match v {
            Counted(n) => Ok(n as f64),
            _ => {
                match v.as_str().parse() {
                    Err(_) => {
                        derr!("Could not deserialize '{}' to {} for '{}'.",
                              v.as_str(),
                              expect,
                              k)
                    }
                    Ok(v) => Ok(v),
                }
            }
        }
    }
}

macro_rules! deserialize_num {
    ($name:ident, $method:ident, $ty:ty) => (
        fn $name<V>(self, visitor: V) -> Result<V::Value>
            where V: de::Visitor<'de>
        {
            visitor.$method(self.to_number::<$ty>(stringify!($ty)).map(|n| n as $ty)?)
        }
    );
}

impl<'a, 'de> ::serde::Deserializer<'de> for &'a mut Deserializer<'de> {
    type Error = Error;

    fn deserialize_any<V>(self, _visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        unimplemented!()
    }

    fn deserialize_bool<V>(self, visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        visitor.visit_bool(self.pop_val().map(|v| v.as_bool())?)
    }

    // wish for stable macro concat_idents!
    deserialize_num!(deserialize_i8, visit_i8, i8);
    deserialize_num!(deserialize_i16, visit_i16, i16);
    deserialize_num!(deserialize_i32, visit_i32, i32);
    deserialize_num!(deserialize_i64, visit_i64, i64);
    deserialize_num!(deserialize_u8, visit_u8, u8);
    deserialize_num!(deserialize_u16, visit_u16, u16);
    deserialize_num!(deserialize_u32, visit_u32, u32);
    deserialize_num!(deserialize_u64, visit_u64, u64);

    fn deserialize_f32<V>(self, visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        visitor.visit_f32(self.to_float("f32").map(|n| n as f32)?)
    }

    fn deserialize_f64<V>(self, visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        visitor.visit_f64(self.to_float("f64")?)
    }

    fn deserialize_char<V>(self, visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        let (k, v) = self.pop_key_val()?;
        let vstr = v.as_str();
        match vstr.chars().count() {
            1 => visitor.visit_char(vstr.chars().next().unwrap()),
            _ => derr!("Could not deserialize '{}' into char for '{}'.", vstr, k),
        }
    }

    fn deserialize_str<V>(self, visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        let s = self.pop_val()?;
        visitor.visit_str(s.as_str())
    }

    fn deserialize_string<V>(self, visitor:V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        self.deserialize_str(visitor)
    }

    fn deserialize_bytes<V>(self, _visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        unimplemented!()
    }

    fn deserialize_byte_buf<V>(self, _visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        unimplemented!()
    }

    fn  deserialize_option<V>(self, visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        let is_some = match self.stack.last() {
            None => derr!("Could not deserialize value into unknown key."),
            Some(it) => it.val.as_ref().map_or(false, |v| v.as_bool()),
        };
        if is_some {
            visitor.visit_some(self)
        } else {
            visitor.visit_none()
        }
    }

    fn deserialize_unit<V>(self, _visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        // I don't know what the right thing is here, so just fail for now.
        panic!("I don't know how to read into a nil value.")
    }

    fn deserialize_unit_struct<V>(self, _name: &'static str, visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        visitor.visit_unit()
    }

    fn deserialize_newtype_struct<V>(self, _name: &'static str, visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        visitor.visit_newtype_struct(self)
    }

    fn deserialize_tuple<V>(self, _len: usize, _visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        unimplemented!()
    }

    fn deserialize_tuple_struct<V>(self,
                                   _name: &'static str,
                                   _len: usize,
                                   _visitor: V)
                                   -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        unimplemented!()
    }

    fn deserialize_map<V>(self, _visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        unimplemented!()
    }

    fn deserialize_seq<V>(mut self, visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        let (key, struct_field, val) = match self.stack.pop() {
            None => derr!("Could not deserialize value into unknown key."),
            Some(DeserializerItem {key, struct_field, val}) => (key, struct_field, val),
        };
        let list = val.unwrap_or(List(vec![]));
        let vals = list.as_vec();
        for val in vals.iter().rev() {
            self.stack
                .push(DeserializerItem {
                          key: key.clone(),
                          struct_field: struct_field,
                          val: Some(Plain(Some((*val).into()))),
                      });
        }
        visitor.visit_seq(SeqDeserializer::new(&mut self, vals.len()))
    }

    fn deserialize_struct<V>(mut self,
                             _: &str,
                             fields: &'static [&'static str],
                             visitor: V)
                             -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        visitor.visit_seq(StructDeserializer::new(&mut self, fields))
    }

    fn deserialize_enum<V>(self, _name: &str, variants: &[&str], visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        let v = self.pop_val()?.as_str().to_lowercase();
        let s = match variants.iter().find(|&n| n.to_lowercase() == v) {
            Some(s) => s,
            None => {
                derr!("Could not match '{}' with any of \
                           the allowed variants: {:?}",
                      v,
                      variants)
            }
        };
        visitor.visit_enum(s.into_deserializer())
    }

    fn deserialize_identifier<V>(self, visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        self.deserialize_str(visitor)
    }

    fn deserialize_ignored_any<V>(self, visitor: V) -> Result<V::Value>
        where V: de::Visitor<'de>
    {
        self.deserialize_any(visitor)
    }
}

struct SeqDeserializer<'a, 'de: 'a> {
    de: &'a mut Deserializer<'de>,
    len: usize,
}

impl<'a, 'de> SeqDeserializer<'a, 'de> {
    fn new(de: &'a mut Deserializer<'de>, len: usize) -> Self {
        SeqDeserializer { de: de, len: len }
    }
}

impl<'a, 'de> de::SeqAccess<'de> for SeqDeserializer<'a, 'de> {
    type Error = Error;

    fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>>
        where T: de::DeserializeSeed<'de>
    {
        if self.len == 0 {
            return Ok(None);
        }
        self.len -= 1;
        seed.deserialize(&mut *self.de).map(Some)
    }

    fn size_hint(&self) -> Option<usize> {
        return Some(self.len);
    }
}

struct StructDeserializer<'a, 'de: 'a> {
    de: &'a mut Deserializer<'de>,
    fields: &'static [&'static str],
}

impl<'a, 'de> StructDeserializer<'a, 'de> {
    fn new(de: &'a mut Deserializer<'de>, fields: &'static [&'static str]) -> Self {
        StructDeserializer {
            de: de,
            fields: fields,
        }
    }
}

impl<'a, 'de> de::SeqAccess<'de> for StructDeserializer<'a, 'de> {
    type Error = Error;

    fn next_element_seed<T>(&mut self, seed: T) -> Result<Option<T::Value>>
        where T: de::DeserializeSeed<'de>
    {
        if self.fields.len() == 0 {
            return Ok(None);
        }
        self.de.push(self.fields[0]);
        self.fields = &self.fields[1..];
        seed.deserialize(&mut *self.de).map(Some)
    }

    fn size_hint(&self) -> Option<usize> {
        return Some(self.fields.len());
    }
}
