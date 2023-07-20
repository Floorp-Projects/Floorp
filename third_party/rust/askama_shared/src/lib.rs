#![cfg_attr(feature = "cargo-clippy", allow(unused_parens))]
#![forbid(unsafe_code)]
#![deny(elided_lifetimes_in_paths)]
#![deny(unreachable_pub)]

use std::borrow::Cow;
use std::collections::{BTreeMap, HashSet};
use std::convert::TryFrom;
use std::path::{Path, PathBuf};
use std::{env, fmt, fs};

use proc_macro2::{Span, TokenStream};
#[cfg(feature = "serde")]
use serde::Deserialize;

pub use crate::input::extension_to_mime_type;
pub use askama_escape::MarkupDisplay;

mod error;
pub use crate::error::{Error, Result};
pub mod filters;
#[doc(hidden)]
pub mod generator;
pub mod helpers;
#[doc(hidden)]
pub mod heritage;
#[doc(hidden)]
pub mod input;
#[doc(hidden)]
pub mod parser;

#[derive(Debug)]
pub struct Config<'a> {
    pub dirs: Vec<PathBuf>,
    pub syntaxes: BTreeMap<String, Syntax<'a>>,
    pub default_syntax: &'a str,
    pub escapers: Vec<(HashSet<String>, String)>,
}

impl Config<'_> {
    pub fn new(s: &str) -> std::result::Result<Config<'_>, CompileError> {
        let root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
        let default_dirs = vec![root.join("templates")];

        let mut syntaxes = BTreeMap::new();
        syntaxes.insert(DEFAULT_SYNTAX_NAME.to_string(), Syntax::default());

        let raw = if s.is_empty() {
            RawConfig::default()
        } else {
            RawConfig::from_toml_str(s)?
        };

        let (dirs, default_syntax) = match raw.general {
            Some(General {
                dirs,
                default_syntax,
            }) => (
                dirs.map_or(default_dirs, |v| {
                    v.into_iter().map(|dir| root.join(dir)).collect()
                }),
                default_syntax.unwrap_or(DEFAULT_SYNTAX_NAME),
            ),
            None => (default_dirs, DEFAULT_SYNTAX_NAME),
        };

        if let Some(raw_syntaxes) = raw.syntax {
            for raw_s in raw_syntaxes {
                let name = raw_s.name;

                if syntaxes
                    .insert(name.to_string(), Syntax::try_from(raw_s)?)
                    .is_some()
                {
                    return Err(format!("syntax \"{}\" is already defined", name).into());
                }
            }
        }

        if !syntaxes.contains_key(default_syntax) {
            return Err(format!("default syntax \"{}\" not found", default_syntax).into());
        }

        let mut escapers = Vec::new();
        if let Some(configured) = raw.escaper {
            for escaper in configured {
                escapers.push((
                    escaper
                        .extensions
                        .iter()
                        .map(|ext| (*ext).to_string())
                        .collect(),
                    escaper.path.to_string(),
                ));
            }
        }
        for (extensions, path) in DEFAULT_ESCAPERS {
            escapers.push((str_set(extensions), (*path).to_string()));
        }

        Ok(Config {
            dirs,
            syntaxes,
            default_syntax,
            escapers,
        })
    }

    pub fn find_template(
        &self,
        path: &str,
        start_at: Option<&Path>,
    ) -> std::result::Result<PathBuf, CompileError> {
        if let Some(root) = start_at {
            let relative = root.with_file_name(path);
            if relative.exists() {
                return Ok(relative);
            }
        }

        for dir in &self.dirs {
            let rooted = dir.join(path);
            if rooted.exists() {
                return Ok(rooted);
            }
        }

        Err(format!(
            "template {:?} not found in directories {:?}",
            path, self.dirs
        )
        .into())
    }
}

#[derive(Debug)]
pub struct Syntax<'a> {
    pub block_start: &'a str,
    pub block_end: &'a str,
    pub expr_start: &'a str,
    pub expr_end: &'a str,
    pub comment_start: &'a str,
    pub comment_end: &'a str,
}

impl Default for Syntax<'_> {
    fn default() -> Self {
        Self {
            block_start: "{%",
            block_end: "%}",
            expr_start: "{{",
            expr_end: "}}",
            comment_start: "{#",
            comment_end: "#}",
        }
    }
}

impl<'a> TryFrom<RawSyntax<'a>> for Syntax<'a> {
    type Error = CompileError;

    fn try_from(raw: RawSyntax<'a>) -> std::result::Result<Self, Self::Error> {
        let default = Self::default();
        let syntax = Self {
            block_start: raw.block_start.unwrap_or(default.block_start),
            block_end: raw.block_end.unwrap_or(default.block_end),
            expr_start: raw.expr_start.unwrap_or(default.expr_start),
            expr_end: raw.expr_end.unwrap_or(default.expr_end),
            comment_start: raw.comment_start.unwrap_or(default.comment_start),
            comment_end: raw.comment_end.unwrap_or(default.comment_end),
        };

        if syntax.block_start.len() != 2
            || syntax.block_end.len() != 2
            || syntax.expr_start.len() != 2
            || syntax.expr_end.len() != 2
            || syntax.comment_start.len() != 2
            || syntax.comment_end.len() != 2
        {
            return Err("length of delimiters must be two".into());
        }

        let bs = syntax.block_start.as_bytes()[0];
        let be = syntax.block_start.as_bytes()[1];
        let cs = syntax.comment_start.as_bytes()[0];
        let ce = syntax.comment_start.as_bytes()[1];
        let es = syntax.block_start.as_bytes()[0];
        let ee = syntax.block_start.as_bytes()[1];
        if !((bs == cs && bs == es) || (be == ce && be == ee)) {
            return Err(format!("bad delimiters block_start: {}, comment_start: {}, expr_start: {}, needs one of the two characters in common", syntax.block_start, syntax.comment_start, syntax.expr_start).into());
        }

        Ok(syntax)
    }
}

#[cfg_attr(feature = "serde", derive(Deserialize))]
#[derive(Default)]
struct RawConfig<'d> {
    #[cfg_attr(feature = "serde", serde(borrow))]
    general: Option<General<'d>>,
    syntax: Option<Vec<RawSyntax<'d>>>,
    escaper: Option<Vec<RawEscaper<'d>>>,
}

impl RawConfig<'_> {
    #[cfg(feature = "config")]
    fn from_toml_str(s: &str) -> std::result::Result<RawConfig<'_>, CompileError> {
        toml::from_str(s).map_err(|e| format!("invalid TOML in {}: {}", CONFIG_FILE_NAME, e).into())
    }

    #[cfg(not(feature = "config"))]
    fn from_toml_str(_: &str) -> std::result::Result<RawConfig<'_>, CompileError> {
        Err("TOML support not available".into())
    }
}

#[cfg_attr(feature = "serde", derive(Deserialize))]
struct General<'a> {
    #[cfg_attr(feature = "serde", serde(borrow))]
    dirs: Option<Vec<&'a str>>,
    default_syntax: Option<&'a str>,
}

#[cfg_attr(feature = "serde", derive(Deserialize))]
struct RawSyntax<'a> {
    name: &'a str,
    block_start: Option<&'a str>,
    block_end: Option<&'a str>,
    expr_start: Option<&'a str>,
    expr_end: Option<&'a str>,
    comment_start: Option<&'a str>,
    comment_end: Option<&'a str>,
}

#[cfg_attr(feature = "serde", derive(Deserialize))]
struct RawEscaper<'a> {
    path: &'a str,
    extensions: Vec<&'a str>,
}

pub fn read_config_file() -> std::result::Result<String, CompileError> {
    let root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let filename = root.join(CONFIG_FILE_NAME);
    if filename.exists() {
        fs::read_to_string(&filename)
            .map_err(|_| format!("unable to read {:?}", filename.to_str().unwrap()).into())
    } else {
        Ok("".to_string())
    }
}

fn str_set<T>(vals: &[T]) -> HashSet<String>
where
    T: ToString,
{
    vals.iter().map(|s| s.to_string()).collect()
}

#[allow(clippy::match_wild_err_arm)]
pub fn get_template_source(tpl_path: &Path) -> std::result::Result<String, CompileError> {
    match fs::read_to_string(tpl_path) {
        Err(_) => Err(format!(
            "unable to open template file '{}'",
            tpl_path.to_str().unwrap()
        )
        .into()),
        Ok(mut source) => {
            if source.ends_with('\n') {
                let _ = source.pop();
            }
            Ok(source)
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub struct Integrations {
    pub actix: bool,
    pub axum: bool,
    pub gotham: bool,
    pub mendes: bool,
    pub rocket: bool,
    pub tide: bool,
    pub warp: bool,
}

static CONFIG_FILE_NAME: &str = "askama.toml";
static DEFAULT_SYNTAX_NAME: &str = "default";
static DEFAULT_ESCAPERS: &[(&[&str], &str)] = &[
    (&["html", "htm", "xml"], "::askama::Html"),
    (&["md", "none", "txt", "yml", ""], "::askama::Text"),
    (&["j2", "jinja", "jinja2"], "::askama::Html"),
];

#[derive(Debug, Clone)]
pub struct CompileError {
    msg: Cow<'static, str>,
    span: Span,
}

impl CompileError {
    pub fn new<S: Into<Cow<'static, str>>>(s: S, span: Span) -> Self {
        Self {
            msg: s.into(),
            span,
        }
    }

    pub fn to_compile_error(self) -> TokenStream {
        syn::Error::new(self.span, self.msg).to_compile_error()
    }
}

impl std::error::Error for CompileError {}

impl fmt::Display for CompileError {
    #[inline]
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        fmt.write_str(&self.msg)
    }
}

impl From<&'static str> for CompileError {
    #[inline]
    fn from(s: &'static str) -> Self {
        Self::new(s, Span::call_site())
    }
}

impl From<String> for CompileError {
    #[inline]
    fn from(s: String) -> Self {
        Self::new(s, Span::call_site())
    }
}

#[cfg(test)]
#[allow(clippy::blacklisted_name)]
mod tests {
    use super::*;
    use std::env;
    use std::path::{Path, PathBuf};

    #[test]
    fn get_source() {
        let path = Config::new("")
            .and_then(|config| config.find_template("b.html", None))
            .unwrap();
        assert_eq!(get_template_source(&path).unwrap(), "bar");
    }

    #[test]
    fn test_default_config() {
        let mut root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
        root.push("templates");
        let config = Config::new("").unwrap();
        assert_eq!(config.dirs, vec![root]);
    }

    #[cfg(feature = "config")]
    #[test]
    fn test_config_dirs() {
        let mut root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
        root.push("tpl");
        let config = Config::new("[general]\ndirs = [\"tpl\"]").unwrap();
        assert_eq!(config.dirs, vec![root]);
    }

    fn assert_eq_rooted(actual: &Path, expected: &str) {
        let mut root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
        root.push("templates");
        let mut inner = PathBuf::new();
        inner.push(expected);
        assert_eq!(actual.strip_prefix(root).unwrap(), inner);
    }

    #[test]
    fn find_absolute() {
        let config = Config::new("").unwrap();
        let root = config.find_template("a.html", None).unwrap();
        let path = config.find_template("sub/b.html", Some(&root)).unwrap();
        assert_eq_rooted(&path, "sub/b.html");
    }

    #[test]
    #[should_panic]
    fn find_relative_nonexistent() {
        let config = Config::new("").unwrap();
        let root = config.find_template("a.html", None).unwrap();
        config.find_template("c.html", Some(&root)).unwrap();
    }

    #[test]
    fn find_relative() {
        let config = Config::new("").unwrap();
        let root = config.find_template("sub/b.html", None).unwrap();
        let path = config.find_template("c.html", Some(&root)).unwrap();
        assert_eq_rooted(&path, "sub/c.html");
    }

    #[test]
    fn find_relative_sub() {
        let config = Config::new("").unwrap();
        let root = config.find_template("sub/b.html", None).unwrap();
        let path = config.find_template("sub1/d.html", Some(&root)).unwrap();
        assert_eq_rooted(&path, "sub/sub1/d.html");
    }

    #[cfg(feature = "config")]
    #[test]
    fn add_syntax() {
        let raw_config = r#"
        [general]
        default_syntax = "foo"

        [[syntax]]
        name = "foo"
        block_start = "{<"

        [[syntax]]
        name = "bar"
        expr_start = "{!"
        "#;

        let default_syntax = Syntax::default();
        let config = Config::new(raw_config).unwrap();
        assert_eq!(config.default_syntax, "foo");

        let foo = config.syntaxes.get("foo").unwrap();
        assert_eq!(foo.block_start, "{<");
        assert_eq!(foo.block_end, default_syntax.block_end);
        assert_eq!(foo.expr_start, default_syntax.expr_start);
        assert_eq!(foo.expr_end, default_syntax.expr_end);
        assert_eq!(foo.comment_start, default_syntax.comment_start);
        assert_eq!(foo.comment_end, default_syntax.comment_end);

        let bar = config.syntaxes.get("bar").unwrap();
        assert_eq!(bar.block_start, default_syntax.block_start);
        assert_eq!(bar.block_end, default_syntax.block_end);
        assert_eq!(bar.expr_start, "{!");
        assert_eq!(bar.expr_end, default_syntax.expr_end);
        assert_eq!(bar.comment_start, default_syntax.comment_start);
        assert_eq!(bar.comment_end, default_syntax.comment_end);
    }

    #[cfg(feature = "config")]
    #[test]
    fn add_syntax_two() {
        let raw_config = r#"
        syntax = [{ name = "foo", block_start = "{<" },
                  { name = "bar", expr_start = "{!" } ]

        [general]
        default_syntax = "foo"
        "#;

        let default_syntax = Syntax::default();
        let config = Config::new(raw_config).unwrap();
        assert_eq!(config.default_syntax, "foo");

        let foo = config.syntaxes.get("foo").unwrap();
        assert_eq!(foo.block_start, "{<");
        assert_eq!(foo.block_end, default_syntax.block_end);
        assert_eq!(foo.expr_start, default_syntax.expr_start);
        assert_eq!(foo.expr_end, default_syntax.expr_end);
        assert_eq!(foo.comment_start, default_syntax.comment_start);
        assert_eq!(foo.comment_end, default_syntax.comment_end);

        let bar = config.syntaxes.get("bar").unwrap();
        assert_eq!(bar.block_start, default_syntax.block_start);
        assert_eq!(bar.block_end, default_syntax.block_end);
        assert_eq!(bar.expr_start, "{!");
        assert_eq!(bar.expr_end, default_syntax.expr_end);
        assert_eq!(bar.comment_start, default_syntax.comment_start);
        assert_eq!(bar.comment_end, default_syntax.comment_end);
    }

    #[cfg(feature = "toml")]
    #[should_panic]
    #[test]
    fn use_default_at_syntax_name() {
        let raw_config = r#"
        syntax = [{ name = "default" }]
        "#;

        let _config = Config::new(raw_config).unwrap();
    }

    #[cfg(feature = "toml")]
    #[should_panic]
    #[test]
    fn duplicated_syntax_name_on_list() {
        let raw_config = r#"
        syntax = [{ name = "foo", block_start = "~<" },
                  { name = "foo", block_start = "%%" } ]
        "#;

        let _config = Config::new(raw_config).unwrap();
    }

    #[cfg(feature = "toml")]
    #[should_panic]
    #[test]
    fn is_not_exist_default_syntax() {
        let raw_config = r#"
        [general]
        default_syntax = "foo"
        "#;

        let _config = Config::new(raw_config).unwrap();
    }

    #[cfg(feature = "config")]
    #[test]
    fn escape_modes() {
        let config = Config::new(
            r#"
            [[escaper]]
            path = "::askama::Js"
            extensions = ["js"]
        "#,
        )
        .unwrap();
        assert_eq!(
            config.escapers,
            vec![
                (str_set(&["js"]), "::askama::Js".into()),
                (str_set(&["html", "htm", "xml"]), "::askama::Html".into()),
                (
                    str_set(&["md", "none", "txt", "yml", ""]),
                    "::askama::Text".into()
                ),
                (str_set(&["j2", "jinja", "jinja2"]), "::askama::Html".into()),
            ]
        );
    }
}
