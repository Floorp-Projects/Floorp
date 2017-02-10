//! Objective C types

// use clang_sys::CXCursor_ObjCSuperClassRef;

use super::context::BindgenContext;
use clang;
use clang_sys::CXChildVisit_Continue;
use clang_sys::CXCursor_ObjCInstanceMethodDecl;

/// Objective C interface as used in TypeKind
///
/// Also protocols are parsed as this type
#[derive(Debug)]
pub struct ObjCInterface {
    /// The name
    /// like, NSObject
    name: String,

    /// List of the methods defined in this interfae
    methods: Vec<ObjCInstanceMethod>,
}

/// The objective c methods
#[derive(Debug)]
pub struct ObjCInstanceMethod {
    /// The original method selector name
    /// like, dataWithBytes:length:
    name: String,

    /// Method name as converted to rust
    /// like, dataWithBytes_length_
    rust_name: String,
}

impl ObjCInterface {
    fn new(name: &str) -> ObjCInterface {
        ObjCInterface {
            name: name.to_owned(),
            methods: Vec::new(),
        }
    }

    /// The name
    /// like, NSObject
    pub fn name(&self) -> &str {
        self.name.as_ref()
    }

    /// List of the methods defined in this interfae
    pub fn methods(&self) -> &Vec<ObjCInstanceMethod> {
        &self.methods
    }

    /// Parses the Objective C interface from the cursor
    pub fn from_ty(cursor: &clang::Cursor,
                   _ctx: &mut BindgenContext)
                   -> Option<Self> {
        let name = cursor.spelling();
        let mut interface = Self::new(&name);

        cursor.visit(|cursor| {
            match cursor.kind() {
                CXCursor_ObjCInstanceMethodDecl => {
                    let name = cursor.spelling();
                    let method = ObjCInstanceMethod::new(&name);

                    interface.methods.push(method);
                }
                _ => {}
            }
            CXChildVisit_Continue
        });
        Some(interface)
    }
}

impl ObjCInstanceMethod {
    fn new(name: &str) -> ObjCInstanceMethod {
        let split_name: Vec<&str> = name.split(':').collect();

        let rust_name = split_name.join("_");

        ObjCInstanceMethod {
            name: name.to_owned(),
            rust_name: rust_name.to_owned(),
        }
    }

    /// The original method selector name
    /// like, dataWithBytes:length:
    pub fn name(&self) -> &str {
        self.name.as_ref()
    }

    /// Method name as converted to rust
    /// like, dataWithBytes_length_
    pub fn rust_name(&self) -> &str {
        self.rust_name.as_ref()
    }
}
