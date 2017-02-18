//! Everything related to types in our intermediate representation.

use super::comp::CompInfo;
use super::context::{BindgenContext, ItemId};
use super::derive::{CanDeriveCopy, CanDeriveDebug, CanDeriveDefault};
use super::enum_ty::Enum;
use super::function::FunctionSig;
use super::int::IntKind;
use super::item::Item;
use super::layout::Layout;
use super::objc::ObjCInterface;
use super::traversal::{Trace, Tracer};
use clang::{self, Cursor};
use parse::{ClangItemParser, ParseError, ParseResult};
use std::mem;

/// Template declaration related methods.
pub trait TemplateDeclaration {
    /// Get the set of `ItemId`s that make up this template declaration's free
    /// template parameters.
    ///
    /// Note that these might *not* all be named types: C++ allows
    /// constant-value template parameters. Of course, Rust does not allow
    /// generic parameters to be anything but types, so we must treat them as
    /// opaque, and avoid instantiating them.
    fn template_params(&self, ctx: &BindgenContext) -> Option<Vec<ItemId>>;

    /// Get the number of free template parameters this template declaration
    /// has.
    ///
    /// Implementations *may* return `Some` from this method when
    /// `template_params` returns `None`. This is useful when we only have
    /// partial information about the template declaration, such as when we are
    /// in the middle of parsing it.
    fn num_template_params(&self, ctx: &BindgenContext) -> Option<usize> {
        self.template_params(ctx).map(|params| params.len())
    }
}

/// The base representation of a type in bindgen.
///
/// A type has an optional name, which if present cannot be empty, a `layout`
/// (size, alignment and packedness) if known, a `Kind`, which determines which
/// kind of type it is, and whether the type is const.
#[derive(Debug)]
pub struct Type {
    /// The name of the type, or None if it was an unnamed struct or union.
    name: Option<String>,
    /// The layout of the type, if known.
    layout: Option<Layout>,
    /// The inner kind of the type
    kind: TypeKind,
    /// Whether this type is const-qualified.
    is_const: bool,
}

/// The maximum number of items in an array for which Rust implements common
/// traits, and so if we have a type containing an array with more than this
/// many items, we won't be able to derive common traits on that type.
///
/// We need type-level integers yesterday :'(
pub const RUST_DERIVE_IN_ARRAY_LIMIT: usize = 32;

impl Type {
    /// Get the underlying `CompInfo` for this type, or `None` if this is some
    /// other kind of type.
    pub fn as_comp(&self) -> Option<&CompInfo> {
        match self.kind {
            TypeKind::Comp(ref ci) => Some(ci),
            _ => None,
        }
    }

    /// Construct a new `Type`.
    pub fn new(name: Option<String>,
               layout: Option<Layout>,
               kind: TypeKind,
               is_const: bool)
               -> Self {
        Type {
            name: name,
            layout: layout,
            kind: kind,
            is_const: is_const,
        }
    }

    /// Which kind of type is this?
    pub fn kind(&self) -> &TypeKind {
        &self.kind
    }

    /// Get a mutable reference to this type's kind.
    pub fn kind_mut(&mut self) -> &mut TypeKind {
        &mut self.kind
    }

    /// Get this type's name.
    pub fn name(&self) -> Option<&str> {
        self.name.as_ref().map(|name| &**name)
    }

    /// Is this a compound type?
    pub fn is_comp(&self) -> bool {
        match self.kind {
            TypeKind::Comp(..) => true,
            _ => false,
        }
    }

    /// Is this a named type?
    pub fn is_named(&self) -> bool {
        match self.kind {
            TypeKind::Named => true,
            _ => false,
        }
    }

    /// Is this a template alias type?
    pub fn is_template_alias(&self) -> bool {
        match self.kind {
            TypeKind::TemplateAlias(..) => true,
            _ => false,
        }
    }

    /// Is this a function type?
    pub fn is_function(&self) -> bool {
        match self.kind {
            TypeKind::Function(..) => true,
            _ => false,
        }
    }

    /// Is this an enum type?
    pub fn is_enum(&self) -> bool {
        match self.kind {
            TypeKind::Enum(..) => true,
            _ => false,
        }
    }

    /// Is this either a builtin or named type?
    pub fn is_builtin_or_named(&self) -> bool {
        match self.kind {
            TypeKind::Void |
            TypeKind::NullPtr |
            TypeKind::Function(..) |
            TypeKind::Array(..) |
            TypeKind::Reference(..) |
            TypeKind::Pointer(..) |
            TypeKind::BlockPointer |
            TypeKind::Int(..) |
            TypeKind::Float(..) |
            TypeKind::Named => true,
            _ => false,
        }
    }

    /// Creates a new named type, with name `name`.
    pub fn named(name: String) -> Self {
        assert!(!name.is_empty());
        Self::new(Some(name), None, TypeKind::Named, false)
    }

    /// Is this a floating point type?
    pub fn is_float(&self) -> bool {
        match self.kind {
            TypeKind::Float(..) => true,
            _ => false,
        }
    }

    /// Is this a boolean type?
    pub fn is_bool(&self) -> bool {
        match self.kind {
            TypeKind::Int(IntKind::Bool) => true,
            _ => false,
        }
    }

    /// Is this an integer type?
    pub fn is_integer(&self) -> bool {
        match self.kind {
            TypeKind::Int(..) => true,
            _ => false,
        }
    }

    /// Is this a `const` qualified type?
    pub fn is_const(&self) -> bool {
        self.is_const
    }

    /// Is this a reference to another type?
    pub fn is_type_ref(&self) -> bool {
        match self.kind {
            TypeKind::ResolvedTypeRef(_) |
            TypeKind::UnresolvedTypeRef(_, _, _) => true,
            _ => false,
        }
    }

    /// Is this a incomplete array type?
    pub fn is_incomplete_array(&self, ctx: &BindgenContext) -> Option<ItemId> {
        match self.kind {
            TypeKind::Array(item, len) => {
                if len == 0 { Some(item) } else { None }
            }
            TypeKind::ResolvedTypeRef(inner) => {
                ctx.resolve_type(inner).is_incomplete_array(ctx)
            }
            _ => None,
        }
    }

    /// What is the layout of this type?
    pub fn layout(&self, ctx: &BindgenContext) -> Option<Layout> {
        use std::mem;

        self.layout.or_else(|| {
            match self.kind {
                TypeKind::Comp(ref ci) => ci.layout(ctx),
                // FIXME(emilio): This is a hack for anonymous union templates.
                // Use the actual pointer size!
                TypeKind::Pointer(..) |
                TypeKind::BlockPointer => {
                    Some(Layout::new(mem::size_of::<*mut ()>(),
                                     mem::align_of::<*mut ()>()))
                }
                TypeKind::ResolvedTypeRef(inner) => {
                    ctx.resolve_type(inner).layout(ctx)
                }
                _ => None,
            }
        })
    }

    /// Whether this type has a vtable.
    pub fn has_vtable(&self, ctx: &BindgenContext) -> bool {
        // FIXME: Can we do something about template parameters? Huh...
        match self.kind {
            TypeKind::TemplateInstantiation(t, _) |
            TypeKind::TemplateAlias(t, _) |
            TypeKind::Alias(t) |
            TypeKind::ResolvedTypeRef(t) => ctx.resolve_type(t).has_vtable(ctx),
            TypeKind::Comp(ref info) => info.has_vtable(ctx),
            _ => false,
        }

    }

    /// Returns whether this type has a destructor.
    pub fn has_destructor(&self, ctx: &BindgenContext) -> bool {
        match self.kind {
            TypeKind::TemplateInstantiation(t, _) |
            TypeKind::TemplateAlias(t, _) |
            TypeKind::Alias(t) |
            TypeKind::ResolvedTypeRef(t) => {
                ctx.resolve_type(t).has_destructor(ctx)
            }
            TypeKind::Comp(ref info) => info.has_destructor(ctx),
            _ => false,
        }
    }

    /// See the comment in `Item::signature_contains_named_type`.
    pub fn signature_contains_named_type(&self,
                                         ctx: &BindgenContext,
                                         ty: &Type)
                                         -> bool {
        let name = match *ty.kind() {
            TypeKind::Named => ty.name(),
            ref other @ _ => unreachable!("Not a named type: {:?}", other),
        };

        match self.kind {
            TypeKind::Named => self.name() == name,
            TypeKind::ResolvedTypeRef(t) |
            TypeKind::Array(t, _) |
            TypeKind::Pointer(t) |
            TypeKind::Alias(t) => {
                ctx.resolve_type(t)
                    .signature_contains_named_type(ctx, ty)
            }
            TypeKind::Function(ref sig) => {
                sig.argument_types().iter().any(|&(_, arg)| {
                    ctx.resolve_type(arg)
                        .signature_contains_named_type(ctx, ty)
                }) ||
                ctx.resolve_type(sig.return_type())
                    .signature_contains_named_type(ctx, ty)
            }
            TypeKind::TemplateAlias(_, ref template_args) |
            TypeKind::TemplateInstantiation(_, ref template_args) => {
                template_args.iter().any(|arg| {
                    ctx.resolve_type(*arg)
                        .signature_contains_named_type(ctx, ty)
                })
            }
            TypeKind::Comp(ref ci) => ci.signature_contains_named_type(ctx, ty),
            _ => false,
        }
    }

    /// Whether this named type is an invalid C++ identifier. This is done to
    /// avoid generating invalid code with some cases we can't handle, see:
    ///
    /// tests/headers/381-decltype-alias.hpp
    pub fn is_invalid_named_type(&self) -> bool {
        match self.kind {
            TypeKind::Named => {
                let name = self.name().expect("Unnamed named type?");
                !Self::is_valid_identifier(&name)
            }
            _ => false,
        }
    }

    /// Checks whether the name looks like an identifier,
    /// i.e. is alphanumeric (including '_') and does not start with a digit.
    pub fn is_valid_identifier(name: &str) -> bool {
        let mut chars = name.chars();
        let first_valid = chars.next().map(|c| c.is_alphabetic() || c == '_').unwrap_or(false);

        first_valid && chars.all(|c| c.is_alphanumeric() || c == '_')
    }

    /// See safe_canonical_type.
    pub fn canonical_type<'tr>(&'tr self,
                               ctx: &'tr BindgenContext)
                               -> &'tr Type {
        self.safe_canonical_type(ctx)
            .expect("Should have been resolved after parsing!")
    }

    /// Returns the canonical type of this type, that is, the "inner type".
    ///
    /// For example, for a `typedef`, the canonical type would be the
    /// `typedef`ed type, for a template specialization, would be the template
    /// its specializing, and so on. Return None if the type is unresolved.
    pub fn safe_canonical_type<'tr>(&'tr self,
                                    ctx: &'tr BindgenContext)
                                    -> Option<&'tr Type> {
        match self.kind {
            TypeKind::Named |
            TypeKind::Array(..) |
            TypeKind::Comp(..) |
            TypeKind::Int(..) |
            TypeKind::Float(..) |
            TypeKind::Complex(..) |
            TypeKind::Function(..) |
            TypeKind::Enum(..) |
            TypeKind::Reference(..) |
            TypeKind::Void |
            TypeKind::NullPtr |
            TypeKind::BlockPointer |
            TypeKind::Pointer(..) |
            TypeKind::ObjCInterface(..) => Some(self),

            TypeKind::ResolvedTypeRef(inner) |
            TypeKind::Alias(inner) |
            TypeKind::TemplateAlias(inner, _) |
            TypeKind::TemplateInstantiation(inner, _) => {
                ctx.resolve_type(inner).safe_canonical_type(ctx)
            }

            TypeKind::UnresolvedTypeRef(..) => None,
        }
    }

    /// There are some types we don't want to stop at when finding an opaque
    /// item, so we can arrive to the proper item that needs to be generated.
    pub fn should_be_traced_unconditionally(&self) -> bool {
        match self.kind {
            TypeKind::Function(..) |
            TypeKind::Pointer(..) |
            TypeKind::Array(..) |
            TypeKind::Reference(..) |
            TypeKind::TemplateInstantiation(..) |
            TypeKind::ResolvedTypeRef(..) => true,
            _ => false,
        }
    }
}

#[test]
fn is_invalid_named_type_valid() {
    let ty = Type::new(Some("foo".into()), None, TypeKind::Named, false);
    assert!(!ty.is_invalid_named_type())
}

#[test]
fn is_invalid_named_type_valid_underscore_and_numbers() {
    let ty =
        Type::new(Some("_foo123456789_".into()), None, TypeKind::Named, false);
    assert!(!ty.is_invalid_named_type())
}

#[test]
fn is_invalid_named_type_valid_unnamed_kind() {
    let ty = Type::new(Some("foo".into()), None, TypeKind::Void, false);
    assert!(!ty.is_invalid_named_type())
}

#[test]
fn is_invalid_named_type_invalid_start() {
    let ty = Type::new(Some("1foo".into()), None, TypeKind::Named, false);
    assert!(ty.is_invalid_named_type())
}

#[test]
fn is_invalid_named_type_invalid_remaing() {
    let ty = Type::new(Some("foo-".into()), None, TypeKind::Named, false);
    assert!(ty.is_invalid_named_type())
}

#[test]
#[should_panic]
fn is_invalid_named_type_unnamed() {
    let ty = Type::new(None, None, TypeKind::Named, false);
    assert!(ty.is_invalid_named_type())
}

#[test]
fn is_invalid_named_type_empty_name() {
    let ty = Type::new(Some("".into()), None, TypeKind::Named, false);
    assert!(ty.is_invalid_named_type())
}


impl TemplateDeclaration for Type {
    fn template_params(&self, ctx: &BindgenContext) -> Option<Vec<ItemId>> {
        self.kind.template_params(ctx)
    }
}

impl TemplateDeclaration for TypeKind {
    fn template_params(&self, ctx: &BindgenContext) -> Option<Vec<ItemId>> {
        match *self {
            TypeKind::ResolvedTypeRef(id) => {
                ctx.resolve_type(id).template_params(ctx)
            }
            TypeKind::Comp(ref comp) => comp.template_params(ctx),
            TypeKind::TemplateAlias(_, ref args) => Some(args.clone()),

            TypeKind::TemplateInstantiation(..) |
            TypeKind::Void |
            TypeKind::NullPtr |
            TypeKind::Int(_) |
            TypeKind::Float(_) |
            TypeKind::Complex(_) |
            TypeKind::Array(..) |
            TypeKind::Function(_) |
            TypeKind::Enum(_) |
            TypeKind::Pointer(_) |
            TypeKind::BlockPointer |
            TypeKind::Reference(_) |
            TypeKind::UnresolvedTypeRef(..) |
            TypeKind::Named |
            TypeKind::Alias(_) |
            TypeKind::ObjCInterface(_) => None,
        }
    }
}

impl CanDeriveDebug for Type {
    type Extra = ();

    fn can_derive_debug(&self, ctx: &BindgenContext, _: ()) -> bool {
        match self.kind {
            TypeKind::Array(t, len) => {
                len <= RUST_DERIVE_IN_ARRAY_LIMIT && t.can_derive_debug(ctx, ())
            }
            TypeKind::ResolvedTypeRef(t) |
            TypeKind::TemplateAlias(t, _) |
            TypeKind::Alias(t) => t.can_derive_debug(ctx, ()),
            TypeKind::Comp(ref info) => {
                info.can_derive_debug(ctx, self.layout(ctx))
            }
            _ => true,
        }
    }
}

impl CanDeriveDefault for Type {
    type Extra = ();

    fn can_derive_default(&self, ctx: &BindgenContext, _: ()) -> bool {
        match self.kind {
            TypeKind::Array(t, len) => {
                len <= RUST_DERIVE_IN_ARRAY_LIMIT &&
                t.can_derive_default(ctx, ())
            }
            TypeKind::ResolvedTypeRef(t) |
            TypeKind::TemplateAlias(t, _) |
            TypeKind::Alias(t) => t.can_derive_default(ctx, ()),
            TypeKind::Comp(ref info) => {
                info.can_derive_default(ctx, self.layout(ctx))
            }
            TypeKind::Void |
            TypeKind::Named |
            TypeKind::TemplateInstantiation(..) |
            TypeKind::Reference(..) |
            TypeKind::NullPtr |
            TypeKind::Pointer(..) |
            TypeKind::BlockPointer |
            TypeKind::ObjCInterface(..) |
            TypeKind::Enum(..) => false,
            TypeKind::Function(..) |
            TypeKind::Int(..) |
            TypeKind::Float(..) |
            TypeKind::Complex(..) => true,
            TypeKind::UnresolvedTypeRef(..) => unreachable!(),
        }
    }
}

impl<'a> CanDeriveCopy<'a> for Type {
    type Extra = &'a Item;

    fn can_derive_copy(&self, ctx: &BindgenContext, item: &Item) -> bool {
        match self.kind {
            TypeKind::Array(t, len) => {
                len <= RUST_DERIVE_IN_ARRAY_LIMIT &&
                t.can_derive_copy_in_array(ctx, ())
            }
            TypeKind::ResolvedTypeRef(t) |
            TypeKind::TemplateAlias(t, _) |
            TypeKind::TemplateInstantiation(t, _) |
            TypeKind::Alias(t) => t.can_derive_copy(ctx, ()),
            TypeKind::Comp(ref info) => {
                info.can_derive_copy(ctx, (item, self.layout(ctx)))
            }
            _ => true,
        }
    }

    fn can_derive_copy_in_array(&self,
                                ctx: &BindgenContext,
                                item: &Item)
                                -> bool {
        match self.kind {
            TypeKind::ResolvedTypeRef(t) |
            TypeKind::TemplateAlias(t, _) |
            TypeKind::Alias(t) |
            TypeKind::Array(t, _) => t.can_derive_copy_in_array(ctx, ()),
            TypeKind::Named => false,
            _ => self.can_derive_copy(ctx, item),
        }
    }
}

/// The kind of float this type represents.
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum FloatKind {
    /// A `float`.
    Float,
    /// A `double`.
    Double,
    /// A `long double`.
    LongDouble,
    /// A `__float128`.
    Float128,
}

impl FloatKind {
    /// If this type has a known size, return it (in bytes).
    pub fn known_size(&self) -> usize {
        match *self {
            FloatKind::Float => mem::size_of::<f32>(),
            FloatKind::Double | FloatKind::LongDouble => mem::size_of::<f64>(),
            FloatKind::Float128 => mem::size_of::<f64>() * 2,
        }
    }
}

/// The different kinds of types that we can parse.
#[derive(Debug)]
pub enum TypeKind {
    /// The void type.
    Void,

    /// The `nullptr_t` type.
    NullPtr,

    /// A compound type, that is, a class, struct, or union.
    Comp(CompInfo),

    /// An integer type, of a given kind. `bool` and `char` are also considered
    /// integers.
    Int(IntKind),

    /// A floating point type.
    Float(FloatKind),

    /// A complex floating point type.
    Complex(FloatKind),

    /// A type alias, with a name, that points to another type.
    Alias(ItemId),

    /// A templated alias, pointing to an inner type, just as `Alias`, but with
    /// template parameters.
    TemplateAlias(ItemId, Vec<ItemId>),

    /// An array of a type and a lenght.
    Array(ItemId, usize),

    /// A function type, with a given signature.
    Function(FunctionSig),

    /// An `enum` type.
    Enum(Enum),

    /// A pointer to a type. The bool field represents whether it's const or
    /// not.
    Pointer(ItemId),

    /// A pointer to an Apple block.
    BlockPointer,

    /// A reference to a type, as in: int& foo().
    Reference(ItemId),

    /// An instantiation of an abstract template declaration (first tuple
    /// member) with a set of concrete template arguments (second tuple member).
    TemplateInstantiation(ItemId, Vec<ItemId>),

    /// A reference to a yet-to-resolve type. This stores the clang cursor
    /// itself, and postpones its resolution.
    ///
    /// These are gone in a phase after parsing where these are mapped to
    /// already known types, and are converted to ResolvedTypeRef.
    ///
    /// see tests/headers/typeref.hpp to see somewhere where this is a problem.
    UnresolvedTypeRef(clang::Type,
                      Option<clang::Cursor>,
                      /* parent_id */
                      Option<ItemId>),

    /// An indirection to another type.
    ///
    /// These are generated after we resolve a forward declaration, or when we
    /// replace one type with another.
    ResolvedTypeRef(ItemId),

    /// A named type, that is, a template parameter.
    Named,

    /// Objective C interface. Always referenced through a pointer
    ObjCInterface(ObjCInterface),
}

impl Type {
    /// Whether this type is unsized, that is, has no members. This is used to
    /// derive whether we should generate a dummy `_address` field for structs,
    /// to comply to the C and C++ layouts, that specify that every type needs
    /// to be addressable.
    pub fn is_unsized(&self, ctx: &BindgenContext) -> bool {
        debug_assert!(ctx.in_codegen_phase(), "Not yet");

        match self.kind {
            TypeKind::Void => true,
            TypeKind::Comp(ref ci) => ci.is_unsized(ctx),
            TypeKind::Array(inner, size) => {
                size == 0 || ctx.resolve_type(inner).is_unsized(ctx)
            }
            TypeKind::ResolvedTypeRef(inner) |
            TypeKind::Alias(inner) |
            TypeKind::TemplateAlias(inner, _) |
            TypeKind::TemplateInstantiation(inner, _) => {
                ctx.resolve_type(inner).is_unsized(ctx)
            }
            TypeKind::Named |
            TypeKind::Int(..) |
            TypeKind::Float(..) |
            TypeKind::Complex(..) |
            TypeKind::Function(..) |
            TypeKind::Enum(..) |
            TypeKind::Reference(..) |
            TypeKind::NullPtr |
            TypeKind::BlockPointer |
            TypeKind::Pointer(..) => false,

            TypeKind::ObjCInterface(..) => true, // dunno?

            TypeKind::UnresolvedTypeRef(..) => {
                unreachable!("Should have been resolved after parsing!");
            }
        }
    }

    /// This is another of the nasty methods. This one is the one that takes
    /// care of the core logic of converting a clang type to a `Type`.
    ///
    /// It's sort of nasty and full of special-casing, but hopefully the
    /// comments in every special case justify why they're there.
    pub fn from_clang_ty(potential_id: ItemId,
                         ty: &clang::Type,
                         location: Option<Cursor>,
                         parent_id: Option<ItemId>,
                         ctx: &mut BindgenContext)
                         -> Result<ParseResult<Self>, ParseError> {
        use clang_sys::*;
        {
            let already_resolved =
                ctx.builtin_or_resolved_ty(potential_id,
                                           parent_id,
                                           ty,
                                           location);
            if let Some(ty) = already_resolved {
                debug!("{:?} already resolved: {:?}", ty, location);
                return Ok(ParseResult::AlreadyResolved(ty));
            }
        }

        let layout = ty.fallible_layout().ok();
        let cursor = ty.declaration();
        let mut name = cursor.spelling();

        debug!("from_clang_ty: {:?}, ty: {:?}, loc: {:?}",
               potential_id,
               ty,
               location);
        debug!("currently_parsed_types: {:?}", ctx.currently_parsed_types());

        let canonical_ty = ty.canonical_type();

        // Parse objc protocols as if they were interfaces
        let mut ty_kind = ty.kind();
        if let Some(loc) = location {
            if loc.kind() == CXCursor_ObjCProtocolDecl {
                ty_kind = CXType_ObjCInterface;
            }
        }

        let kind = match ty_kind {
            CXType_Unexposed if *ty != canonical_ty &&
                                canonical_ty.kind() != CXType_Invalid &&
                                // Sometime clang desugars some types more than
                                // what we need, specially with function
                                // pointers.
                                //
                                // We should also try the solution of inverting
                                // those checks instead of doing this, that is,
                                // something like:
                                //
                                // CXType_Unexposed if ty.ret_type().is_some()
                                //   => { ... }
                                //
                                // etc.
                                !canonical_ty.spelling().contains("type-parameter") => {
                debug!("Looking for canonical type: {:?}", canonical_ty);
                return Self::from_clang_ty(potential_id,
                                           &canonical_ty,
                                           location,
                                           parent_id,
                                           ctx);
            }
            CXType_Unexposed | CXType_Invalid => {
                // For some reason Clang doesn't give us any hint in some
                // situations where we should generate a function pointer (see
                // tests/headers/func_ptr_in_struct.h), so we do a guess here
                // trying to see if it has a valid return type.
                if ty.ret_type().is_some() {
                    let signature = try!(FunctionSig::from_ty(ty,
                                                  &location.unwrap_or(cursor),
                                                  ctx));
                    TypeKind::Function(signature)
                    // Same here, with template specialisations we can safely
                    // assume this is a Comp(..)
                } else if ty.is_fully_specialized_template() {
                    debug!("Template specialization: {:?}, {:?} {:?}",
                           ty,
                           location,
                           canonical_ty);
                    let complex =
                        CompInfo::from_ty(potential_id, ty, location, ctx)
                            .expect("C'mon");
                    TypeKind::Comp(complex)
                } else if let Some(location) = location {
                    match location.kind() {
                        CXCursor_ClassTemplatePartialSpecialization |
                        CXCursor_CXXBaseSpecifier |
                        CXCursor_ClassTemplate => {
                            if location.kind() == CXCursor_CXXBaseSpecifier {
                                // In the case we're parsing a base specifier
                                // inside an unexposed or invalid type, it means
                                // that we're parsing one of two things:
                                //
                                //  * A template parameter.
                                //  * A complex class that isn't exposed.
                                //
                                // This means, unfortunately, that there's no
                                // good way to differentiate between them.
                                //
                                // Probably we could try to look at the
                                // declaration and complicate more this logic,
                                // but we'll keep it simple... if it's a valid
                                // C++ identifier, we'll consider it as a
                                // template parameter.
                                //
                                // This is because:
                                //
                                //  * We expect every other base that is a
                                //    proper identifier (that is, a simple
                                //    struct/union declaration), to be exposed,
                                //    so this path can't be reached in that
                                //    case.
                                //
                                //  * Quite conveniently, complex base
                                //    specifiers preserve their full names (that
                                //    is: Foo<T> instead of Foo). We can take
                                //    advantage of this.
                                //
                                // If we find some edge case where this doesn't
                                // work (which I guess is unlikely, see the
                                // different test cases[1][2][3][4]), we'd need
                                // to find more creative ways of differentiating
                                // these two cases.
                                //
                                // [1]: inherit_named.hpp
                                // [2]: forward-inherit-struct-with-fields.hpp
                                // [3]: forward-inherit-struct.hpp
                                // [4]: inherit-namespaced.hpp
                                if location.spelling()
                                    .chars()
                                    .all(|c| c.is_alphanumeric() || c == '_') {
                                    return Err(ParseError::Recurse);
                                }
                            } else {
                                name = location.spelling();
                            }
                            let complex = CompInfo::from_ty(potential_id,
                                                            ty,
                                                            Some(location),
                                                            ctx)
                                .expect("C'mon");
                            TypeKind::Comp(complex)
                        }
                        CXCursor_TypeAliasTemplateDecl => {
                            debug!("TypeAliasTemplateDecl");

                            // We need to manually unwind this one.
                            let mut inner = Err(ParseError::Continue);
                            let mut args = vec![];

                            location.visit(|cur| {
                                match cur.kind() {
                                    CXCursor_TypeAliasDecl => {
                                        let current = cur.cur_type();

                                        debug_assert!(current.kind() ==
                                                      CXType_Typedef);

                                        name = current.spelling();

                                        let inner_ty = cur.typedef_type()
                                            .expect("Not valid Type?");
                                        inner =
                                            Item::from_ty(&inner_ty,
                                                          Some(cur),
                                                          Some(potential_id),
                                                          ctx);
                                    }
                                    CXCursor_TemplateTypeParameter => {
                                        // See the comment in src/ir/comp.rs
                                        // about the same situation.
                                        if cur.spelling().is_empty() {
                                            return CXChildVisit_Continue;
                                        }

                                        let param =
                                            Item::named_type(cur.spelling(),
                                                             potential_id,
                                                             ctx);
                                        args.push(param);
                                    }
                                    _ => {}
                                }
                                CXChildVisit_Continue
                            });

                            let inner_type = match inner {
                                Ok(inner) => inner,
                                Err(..) => {
                                    error!("Failed to parse template alias \
                                           {:?}",
                                           location);
                                    return Err(ParseError::Continue);
                                }
                            };

                            TypeKind::TemplateAlias(inner_type, args)
                        }
                        CXCursor_TemplateRef => {
                            let referenced = location.referenced().unwrap();
                            let referenced_ty = referenced.cur_type();

                            debug!("TemplateRef: location = {:?}; referenced = \
                                    {:?}; referenced_ty = {:?}",
                                   location,
                                   referenced,
                                   referenced_ty);

                            return Self::from_clang_ty(potential_id,
                                                       &referenced_ty,
                                                       Some(referenced),
                                                       parent_id,
                                                       ctx);
                        }
                        CXCursor_TypeRef => {
                            let referenced = location.referenced().unwrap();
                            let referenced_ty = referenced.cur_type();
                            let declaration = referenced_ty.declaration();

                            debug!("TypeRef: location = {:?}; referenced = \
                                    {:?}; referenced_ty = {:?}",
                                   location,
                                   referenced,
                                   referenced_ty);

                            let item =
                                Item::from_ty_or_ref_with_id(potential_id,
                                                             referenced_ty,
                                                             Some(declaration),
                                                             parent_id,
                                                             ctx);
                            return Ok(ParseResult::AlreadyResolved(item));
                        }
                        CXCursor_NamespaceRef => {
                            return Err(ParseError::Continue);
                        }
                        _ => {
                            if ty.kind() == CXType_Unexposed {
                                warn!("Unexposed type {:?}, recursing inside, \
                                      loc: {:?}",
                                      ty,
                                      location);
                                return Err(ParseError::Recurse);
                            }

                            // If the type name is empty we're probably
                            // over-recursing to find a template parameter name
                            // or something like that, so just don't be too
                            // noisy with it since it causes confusion, see for
                            // example the discussion in:
                            //
                            // https://github.com/jamesmunns/teensy3-rs/issues/9
                            if !ty.spelling().is_empty() {
                                warn!("invalid type {:?}", ty);
                            } else {
                                warn!("invalid type {:?}", ty);
                            }
                            return Err(ParseError::Continue);
                        }
                    }
                } else {
                    // TODO: Don't duplicate this!
                    if ty.kind() == CXType_Unexposed {
                        warn!("Unexposed type {:?}, recursing inside", ty);
                        return Err(ParseError::Recurse);
                    }

                    if !ty.spelling().is_empty() {
                        warn!("invalid type {:?}", ty);
                    } else {
                        warn!("invalid type {:?}", ty);
                    }
                    return Err(ParseError::Continue);
                }
            }
            CXType_Auto => {
                if canonical_ty == *ty {
                    debug!("Couldn't find deduced type: {:?}", ty);
                    return Err(ParseError::Continue);
                }

                return Self::from_clang_ty(potential_id,
                                           &canonical_ty,
                                           location,
                                           parent_id,
                                           ctx);
            }
            // NOTE: We don't resolve pointers eagerly because the pointee type
            // might not have been parsed, and if it contains templates or
            // something else we might get confused, see the comment inside
            // TypeRef.
            //
            // We might need to, though, if the context is already in the
            // process of resolving them.
            CXType_ObjCObjectPointer |
            CXType_MemberPointer |
            CXType_Pointer => {
                let inner = Item::from_ty_or_ref(ty.pointee_type().unwrap(),
                                                 location,
                                                 None,
                                                 ctx);
                TypeKind::Pointer(inner)
            }
            CXType_BlockPointer => TypeKind::BlockPointer,
            // XXX: RValueReference is most likely wrong, but I don't think we
            // can even add bindings for that, so huh.
            CXType_RValueReference |
            CXType_LValueReference => {
                let inner = Item::from_ty_or_ref(ty.pointee_type().unwrap(),
                                                 location,
                                                 None,
                                                 ctx);
                TypeKind::Reference(inner)
            }
            // XXX DependentSizedArray is wrong
            CXType_VariableArray |
            CXType_DependentSizedArray => {
                let inner = Item::from_ty(ty.elem_type().as_ref().unwrap(),
                                          location,
                                          None,
                                          ctx)
                    .expect("Not able to resolve array element?");
                TypeKind::Pointer(inner)
            }
            CXType_IncompleteArray => {
                let inner = Item::from_ty(ty.elem_type().as_ref().unwrap(),
                                          location,
                                          None,
                                          ctx)
                    .expect("Not able to resolve array element?");
                TypeKind::Array(inner, 0)
            }
            CXType_FunctionNoProto |
            CXType_FunctionProto => {
                let signature = try!(FunctionSig::from_ty(ty,
                                              &location.unwrap_or(cursor),
                                              ctx));
                TypeKind::Function(signature)
            }
            CXType_Typedef => {
                let inner = cursor.typedef_type().expect("Not valid Type?");
                let inner = Item::from_ty_or_ref(inner, location, None, ctx);
                TypeKind::Alias(inner)
            }
            CXType_Enum => {
                let enum_ = Enum::from_ty(ty, ctx).expect("Not an enum?");

                if name.is_empty() {
                    let pretty_name = ty.spelling();
                    if Self::is_valid_identifier(&pretty_name) {
                        name = pretty_name;
                    }
                }

                TypeKind::Enum(enum_)
            }
            CXType_Record => {
                let complex =
                    CompInfo::from_ty(potential_id, ty, location, ctx)
                        .expect("Not a complex type?");

                if name.is_empty() {
                    // The pretty-printed name may contain typedefed name,
                    // but may also be "struct (anonymous at .h:1)"
                    let pretty_name = ty.spelling();
                    if Self::is_valid_identifier(&pretty_name) {
                        name = pretty_name;
                    }
                }

                TypeKind::Comp(complex)
            }
            // FIXME: We stub vectors as arrays since in 99% of the cases the
            // layout is going to be correct, and there's no way we can generate
            // vector types properly in Rust for now.
            //
            // That being said, that should be fixed eventually.
            CXType_Vector |
            CXType_ConstantArray => {
                let inner = Item::from_ty(ty.elem_type().as_ref().unwrap(),
                                          location,
                                          None,
                                          ctx)
                    .expect("Not able to resolve array element?");
                TypeKind::Array(inner, ty.num_elements().unwrap())
            }
            CXType_Elaborated => {
                return Self::from_clang_ty(potential_id,
                                           &ty.named(),
                                           location,
                                           parent_id,
                                           ctx);
            }
            CXType_ObjCInterface => {
                let interface = ObjCInterface::from_ty(&location.unwrap(), ctx)
                    .expect("Not a valid objc interface?");
                TypeKind::ObjCInterface(interface)
            }
            _ => {
                error!("unsupported type: kind = {:?}; ty = {:?}; at {:?}",
                       ty.kind(),
                       ty,
                       location);
                return Err(ParseError::Continue);
            }
        };

        let name = if name.is_empty() { None } else { Some(name) };
        let is_const = ty.is_const();

        let ty = Type::new(name, layout, kind, is_const);
        // TODO: maybe declaration.canonical()?
        Ok(ParseResult::New(ty, Some(cursor.canonical())))
    }
}

impl Trace for Type {
    type Extra = Item;

    fn trace<T>(&self, context: &BindgenContext, tracer: &mut T, item: &Item)
        where T: Tracer,
    {
        match *self.kind() {
            TypeKind::Pointer(inner) |
            TypeKind::Reference(inner) |
            TypeKind::Array(inner, _) |
            TypeKind::Alias(inner) |
            TypeKind::ResolvedTypeRef(inner) => {
                tracer.visit(inner);
            }

            TypeKind::TemplateAlias(inner, ref template_args) |
            TypeKind::TemplateInstantiation(inner, ref template_args) => {
                tracer.visit(inner);
                for &item in template_args {
                    tracer.visit(item);
                }
            }
            TypeKind::Comp(ref ci) => ci.trace(context, tracer, item),
            TypeKind::Function(ref sig) => sig.trace(context, tracer, &()),
            TypeKind::Enum(ref en) => {
                if let Some(repr) = en.repr() {
                    tracer.visit(repr);
                }
            }
            TypeKind::UnresolvedTypeRef(_, _, Some(id)) => {
                tracer.visit(id);
            }

            TypeKind::ObjCInterface(_) => {
                // TODO:
            }

            // None of these variants have edges to other items and types.
            TypeKind::UnresolvedTypeRef(_, _, None) |
            TypeKind::Named |
            TypeKind::Void |
            TypeKind::NullPtr |
            TypeKind::Int(_) |
            TypeKind::Float(_) |
            TypeKind::Complex(_) |
            TypeKind::BlockPointer => {}
        }
    }
}
