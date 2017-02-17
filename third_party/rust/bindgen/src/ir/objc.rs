//! Objective C types

use super::context::BindgenContext;
use super::function::FunctionSig;
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

    signature: FunctionSig,
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
                   ctx: &mut BindgenContext)
                   -> Option<Self> {
        let name = cursor.spelling();
        let mut interface = Self::new(&name);

        cursor.visit(|cursor| {
            match cursor.kind() {
                CXCursor_ObjCInstanceMethodDecl => {
                    let name = cursor.spelling();
                    let signature =
                        FunctionSig::from_ty(&cursor.cur_type(), &cursor, ctx)
                            .expect("Invalid function sig");
                    let method = ObjCInstanceMethod::new(&name, signature);

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
    fn new(name: &str, signature: FunctionSig) -> ObjCInstanceMethod {
        let split_name: Vec<&str> = name.split(':').collect();

        let rust_name = split_name.join("_");

        ObjCInstanceMethod {
            name: name.to_owned(),
            rust_name: rust_name.to_owned(),
            signature: signature,
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

    /// Returns the methods signature as FunctionSig
    pub fn signature(&self) -> &FunctionSig {
        &self.signature
    }

    /// Formats the method call
    pub fn format_method_call(&self, args: &[String]) -> String {
        let split_name: Vec<&str> =
            self.name.split(':').filter(|p| !p.is_empty()).collect();

        // No arguments
        if args.len() == 0 && split_name.len() == 1 {
            return split_name[0].to_string();
        }

        // Check right amount of arguments
        if args.len() != split_name.len() {
            panic!("Incorrect method name or arguments for objc method, {:?} vs {:?}",
                   args,
                   split_name);
        }

        split_name.iter()
            .zip(args.iter())
            .map(|parts| format!("{}:{} ", parts.0, parts.1))
            .collect::<Vec<_>>()
            .join("")
    }
}
