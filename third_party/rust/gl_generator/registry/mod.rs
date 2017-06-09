// Copyright 2015-2016 Brendan Zabarauskas and the gl-rs developers
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

extern crate khronos_api;

use std::borrow::Cow;
use std::collections::{BTreeMap, BTreeSet};
use std::fmt;
use std::hash::{Hash, Hasher};
use std::io;
use std::ops::Add;

use Generator;

mod parse;

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum Api {
    Gl,
    Glx,
    Wgl,
    Egl,
    GlCore,
    Gles1,
    Gles2,
}

impl fmt::Display for Api {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Api::Gl => write!(fmt, "gl"),
            Api::Glx => write!(fmt, "glx"),
            Api::Wgl => write!(fmt, "wgl"),
            Api::Egl => write!(fmt, "egl"),
            Api::GlCore => write!(fmt, "glcore"),
            Api::Gles1 => write!(fmt, "gles1"),
            Api::Gles2 => write!(fmt, "gles2"),
        }
    }
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum Fallbacks {
    All,
    None,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum Profile {
    Core,
    Compatibility,
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct Enum {
    pub ident: String,
    pub value: String,
    pub cast: bool,
    pub alias: Option<String>,
    pub ty: Cow<'static, str>,
}

impl Hash for Enum {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.ident.hash(state);
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct Binding {
    pub ident: String,
    pub ty: Cow<'static, str>,
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct Cmd {
    pub proto: Binding,
    pub params: Vec<Binding>,
    pub alias: Option<String>,
    pub vecequiv: Option<String>,
    pub glx: Option<GlxOpcode>,
}

impl Hash for Cmd {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.proto.ident.hash(state);
    }
}

#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct GlxOpcode {
    pub opcode: String,
    pub name: Option<String>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Registry {
    pub api: Api,
    pub enums: BTreeSet<Enum>,
    pub cmds: BTreeSet<Cmd>,
    pub aliases: BTreeMap<String, Vec<String>>,
}

impl Registry {
    pub fn new<'a, Exts>(api: Api,
                         version: (u8, u8),
                         profile: Profile,
                         fallbacks: Fallbacks,
                         extensions: Exts)
                         -> Registry
        where Exts: AsRef<[&'a str]>
    {
        let (major, minor) = version;
        let extensions = extensions
            .as_ref()
            .iter()
            .map(<&str>::to_string)
            .collect();

        let filter = parse::Filter {
            api: api,
            fallbacks: fallbacks,
            extensions: extensions,
            version: format!("{}.{}", major, minor),
            profile: profile,
        };

        let src = match api {
            Api::Gl | Api::GlCore | Api::Gles1 | Api::Gles2 => khronos_api::GL_XML,
            Api::Glx => khronos_api::GLX_XML,
            Api::Wgl => khronos_api::WGL_XML,
            Api::Egl => khronos_api::EGL_XML,
        };

        parse::from_xml(src, filter)
    }

    pub fn write_bindings<W, G>(&self, generator: G, output: &mut W) -> io::Result<()>
        where G: Generator,
              W: io::Write
    {
        generator.write(&self, output)
    }

    /// Returns a set of all the types used in the supplied registry. This is useful
    /// for working out what conversions are needed for the specific registry.
    pub fn get_tys(&self) -> BTreeSet<&str> {
        let mut tys = BTreeSet::new();
        for def in &self.cmds {
            tys.insert(def.proto.ty.as_ref());
            for param in &def.params {
                tys.insert(param.ty.as_ref());
            }
        }
        tys
    }
}

impl Add for Registry {
    type Output = Registry;

    fn add(mut self, other: Registry) -> Registry {
        self.enums.extend(other.enums);
        self.cmds.extend(other.cmds);
        self.aliases.extend(other.aliases);
        self
    }
}
