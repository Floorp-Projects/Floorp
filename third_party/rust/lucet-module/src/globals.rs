use serde::{Deserialize, Serialize};

/// A WebAssembly global along with its export specification.
///
/// The lifetime parameter exists to support zero-copy deserialization for the `&str` fields at the
/// leaves of the structure. For a variant with owned types at the leaves, see
/// [`OwnedGlobalSpec`](owned/struct.OwnedGlobalSpec.html).
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct GlobalSpec<'a> {
    #[serde(borrow)]
    global: Global<'a>,
    export_names: Vec<&'a str>,
}

impl<'a> GlobalSpec<'a> {
    pub fn new(global: Global<'a>, export_names: Vec<&'a str>) -> Self {
        Self {
            global,
            export_names,
        }
    }

    /// Create a new global definition with an initial value and export names.
    pub fn new_def(init_val: i64, export_names: Vec<&'a str>) -> Self {
        Self::new(Global::Def(GlobalDef::I64(init_val)), export_names)
    }

    /// Create a new global import definition with a module and field name, and export names.
    pub fn new_import(module: &'a str, field: &'a str, export_names: Vec<&'a str>) -> Self {
        Self::new(Global::Import { module, field }, export_names)
    }

    pub fn global(&self) -> &Global<'_> {
        &self.global
    }

    pub fn export_names(&self) -> &[&str] {
        &self.export_names
    }

    pub fn is_internal(&self) -> bool {
        self.export_names.len() == 0
    }
}

/// A WebAssembly global is either defined locally, or is defined in relation to a field of another
/// WebAssembly module.
///
/// The lifetime parameter exists to support zero-copy deserialization for the `&str` fields at the
/// leaves of the structure. For a variant with owned types at the leaves, see
/// [`OwnedGlobal`](owned/struct.OwnedGlobal.html).
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum Global<'a> {
    Def(GlobalDef),
    Import { module: &'a str, field: &'a str },
}

/// Definition for a global in this module (not imported).
#[derive(Debug, Copy, Clone, PartialEq, Serialize, Deserialize)]
pub enum GlobalDef {
    I32(i32),
    I64(i64),
    F32(f32),
    F64(f64),
}

impl GlobalDef {
    pub fn init_val(&self) -> GlobalValue {
        match self {
            GlobalDef::I32(i) => GlobalValue { i_32: *i },
            GlobalDef::I64(i) => GlobalValue { i_64: *i },
            GlobalDef::F32(f) => GlobalValue { f_32: *f },
            GlobalDef::F64(f) => GlobalValue { f_64: *f },
        }
    }
}

#[derive(Copy, Clone)]
pub union GlobalValue {
    pub i_32: i32,
    pub i_64: i64,
    pub f_32: f32,
    pub f_64: f64,
}

impl std::fmt::Debug for GlobalValue {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        // Because GlobalValue is a union of primitives, there won't be anything wrong,
        // representation-wise, with printing the underlying data as an i64, f64, or
        // another primitive. This still may incur UB by doing something like trying to
        // read data from an uninitialized memory, if the union is initialized with a
        // 32-bit value, and then read as a 64-bit value (as this code is about to do).
        //
        // In short, avoid using `<GlobalValue as Debug>`::fmt, please.

        writeln!(f, "GlobalValue {{")?;
        unsafe {
            writeln!(f, "  i_32: {},", self.i_32)?;
            writeln!(f, "  i_64: {},", self.i_64)?;
            writeln!(f, "  f_32: {},", self.f_32)?;
            writeln!(f, "  f_64: {},", self.f_64)?;
        }
        writeln!(f, "}}")
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

/// A variant of [`GlobalSpec`](../struct.GlobalSpec.html) with owned strings throughout.
///
/// This type is useful when directly building up a value to be serialized.
pub struct OwnedGlobalSpec {
    global: OwnedGlobal,
    export_names: Vec<String>,
}

impl OwnedGlobalSpec {
    pub fn new(global: OwnedGlobal, export_names: Vec<String>) -> Self {
        Self {
            global,
            export_names,
        }
    }

    /// Create a new global definition with an initial value and export names.
    pub fn new_def(init_val: i64, export_names: Vec<String>) -> Self {
        Self::new(OwnedGlobal::Def(GlobalDef::I64(init_val)), export_names)
    }

    /// Create a new global import definition with a module and field name, and export names.
    pub fn new_import(module: String, field: String, export_names: Vec<String>) -> Self {
        Self::new(OwnedGlobal::Import { module, field }, export_names)
    }

    /// Create a [`GlobalSpec`](../struct.GlobalSpec.html) backed by the values in this
    /// `OwnedGlobalSpec`.
    pub fn to_ref<'a>(&'a self) -> GlobalSpec<'a> {
        GlobalSpec::new(
            self.global.to_ref(),
            self.export_names.iter().map(|x| x.as_str()).collect(),
        )
    }
}

/// A variant of [`Global`](../struct.Global.html) with owned strings throughout.
///
/// This type is useful when directly building up a value to be serialized.
pub enum OwnedGlobal {
    Def(GlobalDef),
    Import { module: String, field: String },
}

impl OwnedGlobal {
    /// Create a [`Global`](../struct.Global.html) backed by the values in this `OwnedGlobal`.
    pub fn to_ref<'a>(&'a self) -> Global<'a> {
        match self {
            OwnedGlobal::Def(def) => Global::Def(def.clone()),
            OwnedGlobal::Import { module, field } => Global::Import {
                module: module.as_str(),
                field: field.as_str(),
            },
        }
    }
}
