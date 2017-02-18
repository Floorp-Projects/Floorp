//! Intermediate representation for C/C++ functions and methods.

use super::context::{BindgenContext, ItemId};
use super::item::Item;
use super::traversal::{Trace, Tracer};
use super::ty::TypeKind;
use clang;
use clang_sys::CXCallingConv;
use parse::{ClangItemParser, ClangSubItemParser, ParseError, ParseResult};
use syntax::abi;

/// A function declaration, with a signature, arguments, and argument names.
///
/// The argument names vector must be the same length as the ones in the
/// signature.
#[derive(Debug)]
pub struct Function {
    /// The name of this function.
    name: String,

    /// The mangled name, that is, the symbol.
    mangled_name: Option<String>,

    /// The id pointing to the current function signature.
    signature: ItemId,

    /// The doc comment on the function, if any.
    comment: Option<String>,
}

impl Function {
    /// Construct a new function.
    pub fn new(name: String,
               mangled_name: Option<String>,
               sig: ItemId,
               comment: Option<String>)
               -> Self {
        Function {
            name: name,
            mangled_name: mangled_name,
            signature: sig,
            comment: comment,
        }
    }

    /// Get this function's name.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Get this function's name.
    pub fn mangled_name(&self) -> Option<&str> {
        self.mangled_name.as_ref().map(|n| &**n)
    }

    /// Get this function's signature.
    pub fn signature(&self) -> ItemId {
        self.signature
    }
}

/// A function signature.
#[derive(Debug)]
pub struct FunctionSig {
    /// The return type of the function.
    return_type: ItemId,

    /// The type of the arguments, optionally with the name of the argument when
    /// declared.
    argument_types: Vec<(Option<String>, ItemId)>,

    /// Whether this function is variadic.
    is_variadic: bool,

    /// The ABI of this function.
    abi: Option<abi::Abi>,
}

fn get_abi(cc: CXCallingConv) -> Option<abi::Abi> {
    use clang_sys::*;
    match cc {
        CXCallingConv_Default => Some(abi::Abi::C),
        CXCallingConv_C => Some(abi::Abi::C),
        CXCallingConv_X86StdCall => Some(abi::Abi::Stdcall),
        CXCallingConv_X86FastCall => Some(abi::Abi::Fastcall),
        CXCallingConv_AAPCS => Some(abi::Abi::Aapcs),
        CXCallingConv_X86_64Win64 => Some(abi::Abi::Win64),
        CXCallingConv_Invalid => None,
        other => panic!("unsupported calling convention: {:?}", other),
    }
}

/// Get the mangled name for the cursor's referent.
pub fn cursor_mangling(cursor: &clang::Cursor) -> Option<String> {
    // We early return here because libclang may crash in some case
    // if we pass in a variable inside a partial specialized template.
    // See servo/rust-bindgen#67, and servo/rust-bindgen#462.
    if cursor.is_in_non_fully_specialized_template() {
        return None;
    }

    let mut mangling = cursor.mangling();
    if mangling.is_empty() {
        return None;
    }

    // Try to undo backend linkage munging (prepended _, generally)
    if cfg!(target_os = "macos") {
        mangling.remove(0);
    }

    Some(mangling)
}

impl FunctionSig {
    /// Construct a new function signature.
    pub fn new(return_type: ItemId,
               arguments: Vec<(Option<String>, ItemId)>,
               is_variadic: bool,
               abi: Option<abi::Abi>)
               -> Self {
        FunctionSig {
            return_type: return_type,
            argument_types: arguments,
            is_variadic: is_variadic,
            abi: abi,
        }
    }

    /// Construct a new function signature from the given Clang type.
    pub fn from_ty(ty: &clang::Type,
                   cursor: &clang::Cursor,
                   ctx: &mut BindgenContext)
                   -> Result<Self, ParseError> {
        use clang_sys::*;
        debug!("FunctionSig::from_ty {:?} {:?}", ty, cursor);

        // Skip function templates
        if cursor.kind() == CXCursor_FunctionTemplate {
            return Err(ParseError::Continue);
        }

        // Don't parse operatorxx functions in C++
        let spelling = cursor.spelling();
        if spelling.starts_with("operator") {
            return Err(ParseError::Continue);
        }

        let cursor = if cursor.is_valid() {
            *cursor
        } else {
            ty.declaration()
        };

        let mut args: Vec<_> = match cursor.kind() {
            CXCursor_FunctionDecl |
            CXCursor_Constructor |
            CXCursor_CXXMethod |
            CXCursor_ObjCInstanceMethodDecl => {
                // For CXCursor_FunctionDecl, cursor.args() is the reliable way
                // to get parameter names and types.
                cursor.args()
                    .unwrap()
                    .iter()
                    .map(|arg| {
                        let arg_ty = arg.cur_type();
                        let name = arg.spelling();
                        let name =
                            if name.is_empty() { None } else { Some(name) };
                        let ty =
                            Item::from_ty_or_ref(arg_ty, Some(*arg), None, ctx);
                        (name, ty)
                    })
                    .collect()
            }
            _ => {
                // For non-CXCursor_FunctionDecl, visiting the cursor's children
                // is the only reliable way to get parameter names.
                let mut args = vec![];
                cursor.visit(|c| {
                    if c.kind() == CXCursor_ParmDecl {
                        let ty = Item::from_ty_or_ref(c.cur_type(),
                                                      Some(c),
                                                      None,
                                                      ctx);
                        let name = c.spelling();
                        let name =
                            if name.is_empty() { None } else { Some(name) };
                        args.push((name, ty));
                    }
                    CXChildVisit_Continue
                });
                args
            }
        };

        let is_method = cursor.kind() == CXCursor_CXXMethod;
        let is_constructor = cursor.kind() == CXCursor_Constructor;
        if (is_constructor || is_method) &&
           cursor.lexical_parent() != cursor.semantic_parent() {
            // Only parse constructors once.
            return Err(ParseError::Continue);
        }

        if is_method || is_constructor {
            let is_const = is_method && cursor.method_is_const();
            let is_virtual = is_method && cursor.method_is_virtual();
            let is_static = is_method && cursor.method_is_static();
            if !is_static && !is_virtual {
                let class = Item::parse(cursor.semantic_parent(), None, ctx)
                    .expect("Expected to parse the class");
                let ptr =
                    Item::builtin_type(TypeKind::Pointer(class), is_const, ctx);
                args.insert(0, (Some("this".into()), ptr));
            } else if is_virtual {
                let void = Item::builtin_type(TypeKind::Void, false, ctx);
                let ptr =
                    Item::builtin_type(TypeKind::Pointer(void), false, ctx);
                args.insert(0, (Some("this".into()), ptr));
            }
        }

        let ty_ret_type = if cursor.kind() == CXCursor_ObjCInstanceMethodDecl {
            try!(cursor.ret_type().ok_or(ParseError::Continue))
        } else {
            try!(ty.ret_type().ok_or(ParseError::Continue))
        };
        let ret = Item::from_ty_or_ref(ty_ret_type, None, None, ctx);
        let abi = get_abi(ty.call_conv());

        if abi.is_none() {
            assert_eq!(cursor.kind(),
                       CXCursor_ObjCInstanceMethodDecl,
                       "Invalid ABI for function signature")
        }

        Ok(Self::new(ret, args, ty.is_variadic(), abi))
    }

    /// Get this function signature's return type.
    pub fn return_type(&self) -> ItemId {
        self.return_type
    }

    /// Get this function signature's argument (name, type) pairs.
    pub fn argument_types(&self) -> &[(Option<String>, ItemId)] {
        &self.argument_types
    }

    /// Get this function signature's ABI.
    pub fn abi(&self) -> Option<abi::Abi> {
        self.abi
    }

    /// Is this function signature variadic?
    pub fn is_variadic(&self) -> bool {
        // Clang reports some functions as variadic when they *might* be
        // variadic. We do the argument check because rust doesn't codegen well
        // variadic functions without an initial argument.
        self.is_variadic && !self.argument_types.is_empty()
    }
}

impl ClangSubItemParser for Function {
    fn parse(cursor: clang::Cursor,
             context: &mut BindgenContext)
             -> Result<ParseResult<Self>, ParseError> {
        use clang_sys::*;
        match cursor.kind() {
            // FIXME(emilio): Generate destructors properly.
            CXCursor_FunctionDecl |
            CXCursor_Constructor |
            CXCursor_CXXMethod => {}
            _ => return Err(ParseError::Continue),
        };

        debug!("Function::parse({:?}, {:?})", cursor, cursor.cur_type());

        let visibility = cursor.visibility();
        if visibility != CXVisibility_Default {
            return Err(ParseError::Continue);
        }

        if cursor.access_specifier() == CX_CXXPrivate {
            return Err(ParseError::Continue);
        }

        if cursor.is_inlined_function() {
            return Err(ParseError::Continue);
        }

        let linkage = cursor.linkage();
        if linkage != CXLinkage_External &&
           linkage != CXLinkage_UniqueExternal {
            return Err(ParseError::Continue);
        }

        // Grab the signature using Item::from_ty.
        let sig = try!(Item::from_ty(&cursor.cur_type(),
                                     Some(cursor),
                                     None,
                                     context));

        let name = cursor.spelling();
        assert!(!name.is_empty(), "Empty function name?");

        let mut mangled_name = cursor_mangling(&cursor);
        if mangled_name.as_ref() == Some(&name) {
            mangled_name = None;
        }

        let comment = cursor.raw_comment();

        let function = Self::new(name, mangled_name, sig, comment);
        Ok(ParseResult::New(function, Some(cursor)))
    }
}

impl Trace for FunctionSig {
    type Extra = ();

    fn trace<T>(&self, _: &BindgenContext, tracer: &mut T, _: &())
        where T: Tracer,
    {
        tracer.visit(self.return_type());

        for &(_, ty) in self.argument_types() {
            tracer.visit(ty);
        }
    }
}
