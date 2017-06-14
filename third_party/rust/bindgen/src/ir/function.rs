//! Intermediate representation for C/C++ functions and methods.

use super::context::{BindgenContext, ItemId};
use super::dot::DotAttributes;
use super::item::Item;
use super::traversal::{EdgeKind, Trace, Tracer};
use super::ty::TypeKind;
use clang;
use clang_sys::CXCallingConv;
use ir::derive::CanDeriveDebug;
use parse::{ClangItemParser, ClangSubItemParser, ParseError, ParseResult};
use std::io;
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

impl DotAttributes for Function {
    fn dot_attributes<W>(&self,
                         _ctx: &BindgenContext,
                         out: &mut W)
                         -> io::Result<()>
        where W: io::Write,
    {
        if let Some(ref mangled) = self.mangled_name {
            try!(writeln!(out,
                          "<tr><td>mangled name</td><td>{}</td></tr>",
                          mangled));
        }

        Ok(())
    }
}

/// An ABI extracted from a clang cursor.
#[derive(Debug, Copy, Clone)]
pub enum Abi {
    /// A known ABI, that rust also understand.
    Known(abi::Abi),
    /// An unknown or invalid ABI.
    Unknown(CXCallingConv),
}

impl Abi {
    /// Returns whether this Abi is known or not.
    fn is_unknown(&self) -> bool {
        match *self {
            Abi::Unknown(..) => true,
            _ => false,
        }
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
    abi: Abi,
}

fn get_abi(cc: CXCallingConv) -> Abi {
    use clang_sys::*;
    Abi::Known(match cc {
        CXCallingConv_Default => abi::Abi::C,
        CXCallingConv_C => abi::Abi::C,
        CXCallingConv_X86StdCall => abi::Abi::Stdcall,
        CXCallingConv_X86FastCall => abi::Abi::Fastcall,
        CXCallingConv_AAPCS => abi::Abi::Aapcs,
        CXCallingConv_X86_64Win64 => abi::Abi::Win64,
        other => return Abi::Unknown(other),
    })
}

// Mac os and Win32 need __ for mangled symbols but rust will automatically
// prepend the extra _.
//
// We need to make sure that we don't include __ because rust will turn into
// ___.
fn mangling_hack_if_needed(ctx: &BindgenContext, symbol: &mut String) {
    // NB: win64 also contains the substring "win32" in the target triple, so
    // we need to actually check for i686...
    if ctx.target().contains("macos") ||
       (ctx.target().contains("i686") && ctx.target().contains("windows")) {
        symbol.remove(0);
    }
}

/// Get the mangled name for the cursor's referent.
pub fn cursor_mangling(ctx: &BindgenContext,
                       cursor: &clang::Cursor)
                       -> Option<String> {
    use clang_sys;
    if !ctx.options().enable_mangling {
        return None;
    }

    // We early return here because libclang may crash in some case
    // if we pass in a variable inside a partial specialized template.
    // See servo/rust-bindgen#67, and servo/rust-bindgen#462.
    if cursor.is_in_non_fully_specialized_template() {
        return None;
    }

    if let Ok(mut manglings) = cursor.cxx_manglings() {
        if let Some(mut m) = manglings.pop() {
            mangling_hack_if_needed(ctx, &mut m);
            return Some(m);
        }
    }

    let mut mangling = cursor.mangling();
    if mangling.is_empty() {
        return None;
    }

    mangling_hack_if_needed(ctx, &mut mangling);

    if cursor.kind() == clang_sys::CXCursor_Destructor {
        // With old (3.8-) libclang versions, and the Itanium ABI, clang returns
        // the "destructor group 0" symbol, which means that it'll try to free
        // memory, which definitely isn't what we want.
        //
        // Explicitly force the destructor group 1 symbol.
        //
        // See http://refspecs.linuxbase.org/cxxabi-1.83.html#mangling-special
        // for the reference, and http://stackoverflow.com/a/6614369/1091587 for
        // a more friendly explanation.
        //
        // We don't need to do this for constructors since clang seems to always
        // have returned the C1 constructor.
        //
        // FIXME(emilio): Can a legit symbol in other ABIs end with this string?
        // I don't think so, but if it can this would become a linker error
        // anyway, not an invalid free at runtime.
        //
        // TODO(emilio, #611): Use cpp_demangle if this becomes nastier with
        // time.
        if mangling.ends_with("D0Ev") {
            let new_len = mangling.len() - 4;
            mangling.truncate(new_len);
            mangling.push_str("D1Ev");
        }
    }

    Some(mangling)
}

impl FunctionSig {
    /// Construct a new function signature.
    pub fn new(return_type: ItemId,
               arguments: Vec<(Option<String>, ItemId)>,
               is_variadic: bool,
               abi: Abi)
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
            CXCursor_ObjCInstanceMethodDecl |
            CXCursor_ObjCClassMethodDecl => {
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
                        let ty = Item::from_ty_or_ref(arg_ty, *arg, None, ctx);
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
                        let ty =
                            Item::from_ty_or_ref(c.cur_type(), c, None, ctx);
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
        let is_destructor = cursor.kind() == CXCursor_Destructor;
        if (is_constructor || is_destructor || is_method) &&
           cursor.lexical_parent() != cursor.semantic_parent() {
            // Only parse constructors once.
            return Err(ParseError::Continue);
        }

        if is_method || is_constructor || is_destructor {
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

        let ty_ret_type = if cursor.kind() == CXCursor_ObjCInstanceMethodDecl ||
                             cursor.kind() == CXCursor_ObjCClassMethodDecl {
            try!(ty.ret_type()
                   .or_else(|| cursor.ret_type())
                   .ok_or(ParseError::Continue))
        } else {
            try!(ty.ret_type().ok_or(ParseError::Continue))
        };
        let ret = Item::from_ty_or_ref(ty_ret_type, cursor, None, ctx);
        let call_conv = ty.call_conv();
        let abi = get_abi(call_conv);

        if abi.is_unknown() {
            warn!("Unknown calling convention: {:?}", call_conv);
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
    pub fn abi(&self) -> Abi {
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
            CXCursor_FunctionDecl |
            CXCursor_Constructor |
            CXCursor_Destructor |
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

        if !context.options().generate_inline_functions &&
            cursor.is_inlined_function() {
            return Err(ParseError::Continue);
        }

        let linkage = cursor.linkage();
        if linkage != CXLinkage_External &&
           linkage != CXLinkage_UniqueExternal {
            return Err(ParseError::Continue);
        }

        // Grab the signature using Item::from_ty.
        let sig =
            try!(Item::from_ty(&cursor.cur_type(), cursor, None, context));

        let mut name = cursor.spelling();
        assert!(!name.is_empty(), "Empty function name?");

        if cursor.kind() == CXCursor_Destructor {
            // Remove the leading `~`. The alternative to this is special-casing
            // code-generation for destructor functions, which seems less than
            // ideal.
            if name.starts_with('~') {
                name.remove(0);
            }

            // Add a suffix to avoid colliding with constructors. This would be
            // technically fine (since we handle duplicated functions/methods),
            // but seems easy enough to handle it here.
            name.push_str("_destructor");
        }

        let mut mangled_name = cursor_mangling(context, &cursor);
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
        tracer.visit_kind(self.return_type(), EdgeKind::FunctionReturn);

        for &(_, ty) in self.argument_types() {
            tracer.visit_kind(ty, EdgeKind::FunctionParameter);
        }
    }
}

// Function pointers follow special rules, see:
//
// https://github.com/servo/rust-bindgen/issues/547,
// https://github.com/rust-lang/rust/issues/38848,
// and https://github.com/rust-lang/rust/issues/40158
//
// Note that copy is always derived, so we don't need to implement it.
impl CanDeriveDebug for FunctionSig {
    type Extra = ();

    fn can_derive_debug(&self, _ctx: &BindgenContext, _: ()) -> bool {
        const RUST_DERIVE_FUNPTR_LIMIT: usize = 12;
        if self.argument_types.len() > RUST_DERIVE_FUNPTR_LIMIT {
            return false;
        }

        match self.abi {
            Abi::Known(abi::Abi::C) |
            Abi::Unknown(..) => true,
            _ => false,
        }
    }
}
