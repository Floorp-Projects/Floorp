//! Compound types (unions and structs) in our intermediate representation.

use super::annotations::Annotations;
use super::context::{BindgenContext, ItemId};
use super::derive::{CanDeriveCopy, CanDeriveDebug, CanDeriveDefault};
use super::item::Item;
use super::layout::Layout;
use super::traversal::{EdgeKind, Trace, Tracer};
use super::template::TemplateParameters;
use clang;
use parse::{ClangItemParser, ParseError};
use std::cell::Cell;

/// The kind of compound type.
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum CompKind {
    /// A struct.
    Struct,
    /// A union.
    Union,
}

/// The kind of C++ method.
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum MethodKind {
    /// A constructor. We represent it as method for convenience, to avoid code
    /// duplication.
    Constructor,
    /// A destructor.
    Destructor,
    /// A virtual destructor.
    VirtualDestructor,
    /// A static method.
    Static,
    /// A normal method.
    Normal,
    /// A virtual method.
    Virtual,
}

/// A struct representing a C++ method, either static, normal, or virtual.
#[derive(Debug)]
pub struct Method {
    kind: MethodKind,
    /// The signature of the method. Take into account this is not a `Type`
    /// item, but a `Function` one.
    ///
    /// This is tricky and probably this field should be renamed.
    signature: ItemId,
    is_const: bool,
}

impl Method {
    /// Construct a new `Method`.
    pub fn new(kind: MethodKind, signature: ItemId, is_const: bool) -> Self {
        Method {
            kind: kind,
            signature: signature,
            is_const: is_const,
        }
    }

    /// What kind of method is this?
    pub fn kind(&self) -> MethodKind {
        self.kind
    }

    /// Is this a destructor method?
    pub fn is_destructor(&self) -> bool {
        self.kind == MethodKind::Destructor ||
        self.kind == MethodKind::VirtualDestructor
    }

    /// Is this a constructor?
    pub fn is_constructor(&self) -> bool {
        self.kind == MethodKind::Constructor
    }

    /// Is this a virtual method?
    pub fn is_virtual(&self) -> bool {
        self.kind == MethodKind::Virtual ||
        self.kind == MethodKind::VirtualDestructor
    }

    /// Is this a static method?
    pub fn is_static(&self) -> bool {
        self.kind == MethodKind::Static
    }

    /// Get the `ItemId` for the `Function` signature for this method.
    pub fn signature(&self) -> ItemId {
        self.signature
    }

    /// Is this a const qualified method?
    pub fn is_const(&self) -> bool {
        self.is_const
    }
}

/// A struct representing a C++ field.
#[derive(Clone, Debug)]
pub struct Field {
    /// The name of the field, empty if it's an unnamed bitfield width.
    name: Option<String>,
    /// The inner type.
    ty: ItemId,
    /// The doc comment on the field if any.
    comment: Option<String>,
    /// Annotations for this field, or the default.
    annotations: Annotations,
    /// If this field is a bitfield, and how many bits does it contain if it is.
    bitfield: Option<u32>,
    /// If the C++ field is marked as `mutable`
    mutable: bool,
    /// The offset of the field (in bits)
    offset: Option<usize>,
}

impl Field {
    /// Construct a new `Field`.
    pub fn new(name: Option<String>,
               ty: ItemId,
               comment: Option<String>,
               annotations: Option<Annotations>,
               bitfield: Option<u32>,
               mutable: bool,
               offset: Option<usize>)
               -> Field {
        Field {
            name: name,
            ty: ty,
            comment: comment,
            annotations: annotations.unwrap_or_default(),
            bitfield: bitfield,
            mutable: mutable,
            offset: offset,
        }
    }

    /// Get the name of this field.
    pub fn name(&self) -> Option<&str> {
        self.name.as_ref().map(|n| &**n)
    }

    /// Get the type of this field.
    pub fn ty(&self) -> ItemId {
        self.ty
    }

    /// Get the comment for this field.
    pub fn comment(&self) -> Option<&str> {
        self.comment.as_ref().map(|c| &**c)
    }

    /// If this is a bitfield, how many bits does it need?
    pub fn bitfield(&self) -> Option<u32> {
        self.bitfield
    }

    /// Is this field marked as `mutable`?
    pub fn is_mutable(&self) -> bool {
        self.mutable
    }

    /// Get the annotations for this field.
    pub fn annotations(&self) -> &Annotations {
        &self.annotations
    }

    /// The offset of the field (in bits)
    pub fn offset(&self) -> Option<usize> {
        self.offset
    }
}

impl CanDeriveDebug for Field {
    type Extra = ();

    fn can_derive_debug(&self, ctx: &BindgenContext, _: ()) -> bool {
        self.ty.can_derive_debug(ctx, ())
    }
}

impl CanDeriveDefault for Field {
    type Extra = ();

    fn can_derive_default(&self, ctx: &BindgenContext, _: ()) -> bool {
        self.ty.can_derive_default(ctx, ())
    }
}

impl<'a> CanDeriveCopy<'a> for Field {
    type Extra = ();

    fn can_derive_copy(&self, ctx: &BindgenContext, _: ()) -> bool {
        self.ty.can_derive_copy(ctx, ())
    }

    fn can_derive_copy_in_array(&self, ctx: &BindgenContext, _: ()) -> bool {
        self.ty.can_derive_copy_in_array(ctx, ())
    }
}


/// The kind of inheritance a base class is using.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum BaseKind {
    /// Normal inheritance, like:
    ///
    /// ```cpp
    /// class A : public B {};
    /// ```
    Normal,
    /// Virtual inheritance, like:
    ///
    /// ```cpp
    /// class A: public virtual B {};
    /// ```
    Virtual,
}

/// A base class.
#[derive(Clone, Debug)]
pub struct Base {
    /// The type of this base class.
    pub ty: ItemId,
    /// The kind of inheritance we're doing.
    pub kind: BaseKind,
}

impl Base {
    /// Whether this base class is inheriting virtually.
    pub fn is_virtual(&self) -> bool {
        self.kind == BaseKind::Virtual
    }
}

/// A compound type.
///
/// Either a struct or union, a compound type is built up from the combination
/// of fields which also are associated with their own (potentially compound)
/// type.
#[derive(Debug)]
pub struct CompInfo {
    /// Whether this is a struct or a union.
    kind: CompKind,

    /// The members of this struct or union.
    fields: Vec<Field>,

    /// The abstract template parameters of this class. These are NOT concrete
    /// template arguments, and should always be a
    /// Type(TypeKind::Named(name)). For concrete template arguments, see the
    /// TypeKind::TemplateInstantiation.
    template_params: Vec<ItemId>,

    /// The method declarations inside this class, if in C++ mode.
    methods: Vec<Method>,

    /// The different constructors this struct or class contains.
    constructors: Vec<ItemId>,

    /// The destructor of this type. The bool represents whether this destructor
    /// is virtual.
    destructor: Option<(bool, ItemId)>,

    /// Vector of classes this one inherits from.
    base_members: Vec<Base>,

    /// The inner types that were declared inside this class, in something like:
    ///
    /// class Foo {
    ///     typedef int FooTy;
    ///     struct Bar {
    ///         int baz;
    ///     };
    /// }
    ///
    /// static Foo::Bar const = {3};
    inner_types: Vec<ItemId>,

    /// Set of static constants declared inside this class.
    inner_vars: Vec<ItemId>,

    /// Whether this type should generate an vtable (TODO: Should be able to
    /// look at the virtual methods and ditch this field).
    has_vtable: bool,

    /// Whether this type has destructor.
    has_destructor: bool,

    /// Whether this type has a base type with more than one member.
    ///
    /// TODO: We should be able to compute this.
    has_nonempty_base: bool,

    /// If this type has a template parameter which is not a type (e.g.: a
    /// size_t)
    has_non_type_template_params: bool,

    /// Whether this struct layout is packed.
    packed: bool,

    /// Used to know if we've found an opaque attribute that could cause us to
    /// generate a type with invalid layout. This is explicitly used to avoid us
    /// generating bad alignments when parsing types like max_align_t.
    ///
    /// It's not clear what the behavior should be here, if generating the item
    /// and pray, or behave as an opaque type.
    found_unknown_attr: bool,

    /// Used to detect if we've run in a can_derive_debug cycle while cycling
    /// around the template arguments.
    detect_derive_debug_cycle: Cell<bool>,

    /// Used to detect if we've run in a can_derive_default cycle while cycling
    /// around the template arguments.
    detect_derive_default_cycle: Cell<bool>,

    /// Used to detect if we've run in a has_destructor cycle while cycling
    /// around the template arguments.
    detect_has_destructor_cycle: Cell<bool>,

    /// Used to indicate when a struct has been forward declared. Usually used
    /// in headers so that APIs can't modify them directly.
    is_forward_declaration: bool,
}

impl CompInfo {
    /// Construct a new compound type.
    pub fn new(kind: CompKind) -> Self {
        CompInfo {
            kind: kind,
            fields: vec![],
            template_params: vec![],
            methods: vec![],
            constructors: vec![],
            destructor: None,
            base_members: vec![],
            inner_types: vec![],
            inner_vars: vec![],
            has_vtable: false,
            has_destructor: false,
            has_nonempty_base: false,
            has_non_type_template_params: false,
            packed: false,
            found_unknown_attr: false,
            detect_derive_debug_cycle: Cell::new(false),
            detect_derive_default_cycle: Cell::new(false),
            detect_has_destructor_cycle: Cell::new(false),
            is_forward_declaration: false,
        }
    }

    /// Is this compound type unsized?
    pub fn is_unsized(&self, ctx: &BindgenContext) -> bool {
        !self.has_vtable(ctx) && self.fields.is_empty() &&
        self.base_members.iter().all(|base| {
            ctx.resolve_type(base.ty).canonical_type(ctx).is_unsized(ctx)
        })
    }

    /// Does this compound type have a destructor?
    pub fn has_destructor(&self, ctx: &BindgenContext) -> bool {
        if self.detect_has_destructor_cycle.get() {
            warn!("Cycle detected looking for destructors");
            // Assume no destructor, since we don't have an explicit one.
            return false;
        }

        self.detect_has_destructor_cycle.set(true);

        let has_destructor = self.has_destructor ||
                             match self.kind {
            CompKind::Union => false,
            CompKind::Struct => {
                self.base_members.iter().any(|base| {
                    ctx.resolve_type(base.ty).has_destructor(ctx)
                }) ||
                self.fields.iter().any(|field| {
                    ctx.resolve_type(field.ty)
                        .has_destructor(ctx)
                })
            }
        };

        self.detect_has_destructor_cycle.set(false);

        has_destructor
    }

    /// Compute the layout of this type.
    ///
    /// This is called as a fallback under some circumstances where LLVM doesn't
    /// give us the correct layout.
    ///
    /// If we're a union without known layout, we try to compute it from our
    /// members. This is not ideal, but clang fails to report the size for these
    /// kind of unions, see test/headers/template_union.hpp
    pub fn layout(&self, ctx: &BindgenContext) -> Option<Layout> {
        use std::cmp;
        // We can't do better than clang here, sorry.
        if self.kind == CompKind::Struct {
            return None;
        }

        let mut max_size = 0;
        let mut max_align = 0;
        for field in &self.fields {
            let field_layout = ctx.resolve_type(field.ty)
                .layout(ctx);

            if let Some(layout) = field_layout {
                max_size = cmp::max(max_size, layout.size);
                max_align = cmp::max(max_align, layout.align);
            }
        }

        Some(Layout::new(max_size, max_align))
    }

    /// Get this type's set of fields.
    pub fn fields(&self) -> &[Field] {
        &self.fields
    }

    /// Does this type have any template parameters that aren't types
    /// (e.g. int)?
    pub fn has_non_type_template_params(&self) -> bool {
        self.has_non_type_template_params
    }

    /// Does this type have a virtual table?
    pub fn has_vtable(&self, ctx: &BindgenContext) -> bool {
        self.has_vtable ||
        self.base_members().iter().any(|base| {
            ctx.resolve_type(base.ty)
                .has_vtable(ctx)
        })
    }

    /// Get this type's set of methods.
    pub fn methods(&self) -> &[Method] {
        &self.methods
    }

    /// Get this type's set of constructors.
    pub fn constructors(&self) -> &[ItemId] {
        &self.constructors
    }

    /// Get this type's destructor.
    pub fn destructor(&self) -> Option<(bool, ItemId)> {
        self.destructor
    }

    /// What kind of compound type is this?
    pub fn kind(&self) -> CompKind {
        self.kind
    }

    /// The set of types that this one inherits from.
    pub fn base_members(&self) -> &[Base] {
        &self.base_members
    }

    /// Construct a new compound type from a Clang type.
    pub fn from_ty(potential_id: ItemId,
                   ty: &clang::Type,
                   location: Option<clang::Cursor>,
                   ctx: &mut BindgenContext)
                   -> Result<Self, ParseError> {
        use clang_sys::*;
        assert!(ty.template_args().is_none(),
                "We handle template instantiations elsewhere");

        let mut cursor = ty.declaration();
        let mut kind = Self::kind_from_cursor(&cursor);
        if kind.is_err() {
            if let Some(location) = location {
                kind = Self::kind_from_cursor(&location);
                cursor = location;
            }
        }

        let kind = try!(kind);

        debug!("CompInfo::from_ty({:?}, {:?})", kind, cursor);

        let mut ci = CompInfo::new(kind);
        ci.is_forward_declaration =
            location.map_or(true, |cur| match cur.kind() {
                CXCursor_StructDecl |
                CXCursor_UnionDecl |
                CXCursor_ClassDecl => !cur.is_definition(),
                _ => false,
            });

        let mut maybe_anonymous_struct_field = None;
        cursor.visit(|cur| {
            if cur.kind() != CXCursor_FieldDecl {
                if let Some((ty, clang_ty, offset)) =
                    maybe_anonymous_struct_field.take() {
                    if cur.kind() == CXCursor_TypedefDecl &&
                       cur.typedef_type().unwrap().canonical_type() == clang_ty {
                        // Typedefs of anonymous structs appear later in the ast
                        // than the struct itself, that would otherwise be an
                        // anonymous field. Detect that case here, and do
                        // nothing.
                    } else {
                        let field =
                            Field::new(None, ty, None, None, None, false, offset);
                        ci.fields.push(field);
                    }
                }
            }

            match cur.kind() {
                CXCursor_FieldDecl => {
                    if let Some((ty, clang_ty, offset)) =
                        maybe_anonymous_struct_field.take() {
                        let mut used = false;
                        cur.visit(|child| {
                            if child.cur_type() == clang_ty {
                                used = true;
                            }
                            CXChildVisit_Continue
                        });
                        if !used {
                            let field = Field::new(None,
                                                   ty,
                                                   None,
                                                   None,
                                                   None,
                                                   false,
                                                   offset);
                            ci.fields.push(field);
                        }
                    }

                    let bit_width = cur.bit_width();
                    let field_type = Item::from_ty_or_ref(cur.cur_type(),
                                                          cur,
                                                          Some(potential_id),
                                                          ctx);

                    let comment = cur.raw_comment();
                    let annotations = Annotations::new(&cur);
                    let name = cur.spelling();
                    let is_mutable = cursor.is_mutable_field();
                    let offset = cur.offset_of_field().ok();

                    // Name can be empty if there are bitfields, for example,
                    // see tests/headers/struct_with_bitfields.h
                    assert!(!name.is_empty() || bit_width.is_some(),
                            "Empty field name?");

                    let name = if name.is_empty() { None } else { Some(name) };

                    let field = Field::new(name,
                                           field_type,
                                           comment,
                                           annotations,
                                           bit_width,
                                           is_mutable,
                                           offset);
                    ci.fields.push(field);

                    // No we look for things like attributes and stuff.
                    cur.visit(|cur| {
                        if cur.kind() == CXCursor_UnexposedAttr {
                            ci.found_unknown_attr = true;
                        }
                        CXChildVisit_Continue
                    });

                }
                CXCursor_UnexposedAttr => {
                    ci.found_unknown_attr = true;
                }
                CXCursor_EnumDecl |
                CXCursor_TypeAliasDecl |
                CXCursor_TypeAliasTemplateDecl |
                CXCursor_TypedefDecl |
                CXCursor_StructDecl |
                CXCursor_UnionDecl |
                CXCursor_ClassTemplate |
                CXCursor_ClassDecl => {
                    // We can find non-semantic children here, clang uses a
                    // StructDecl to note incomplete structs that haven't been
                    // forward-declared before, see [1].
                    //
                    // Also, clang seems to scope struct definitions inside
                    // unions, and other named struct definitions inside other
                    // structs to the whole translation unit.
                    //
                    // Let's just assume that if the cursor we've found is a
                    // definition, it's a valid inner type.
                    //
                    // [1]: https://github.com/servo/rust-bindgen/issues/482
                    let is_inner_struct = cur.semantic_parent() == cursor ||
                                          cur.is_definition();
                    if !is_inner_struct {
                        return CXChildVisit_Continue;
                    }

                    let inner = Item::parse(cur, Some(potential_id), ctx)
                        .expect("Inner ClassDecl");

                    ci.inner_types.push(inner);

                    // A declaration of an union or a struct without name could
                    // also be an unnamed field, unfortunately.
                    if cur.spelling().is_empty() &&
                       cur.kind() != CXCursor_EnumDecl {
                        let ty = cur.cur_type();
                        let offset = cur.offset_of_field().ok();
                        maybe_anonymous_struct_field =
                            Some((inner, ty, offset));
                    }
                }
                CXCursor_PackedAttr => {
                    ci.packed = true;
                }
                CXCursor_TemplateTypeParameter => {
                    let param = Item::named_type(None, cur, ctx)
                        .expect("Item::named_type should't fail when pointing \
                                 at a TemplateTypeParameter");
                    ci.template_params.push(param);
                }
                CXCursor_CXXBaseSpecifier => {
                    let is_virtual_base = cur.is_virtual_base();
                    ci.has_vtable |= is_virtual_base;

                    let kind = if is_virtual_base {
                        BaseKind::Virtual
                    } else {
                        BaseKind::Normal
                    };

                    let type_id =
                        Item::from_ty_or_ref(cur.cur_type(), cur, None, ctx);
                    ci.base_members.push(Base {
                        ty: type_id,
                        kind: kind,
                    });
                }
                CXCursor_Constructor |
                CXCursor_Destructor |
                CXCursor_CXXMethod => {
                    let is_virtual = cur.method_is_virtual();
                    let is_static = cur.method_is_static();
                    debug_assert!(!(is_static && is_virtual), "How?");

                    ci.has_destructor |= cur.kind() == CXCursor_Destructor;
                    ci.has_vtable |= is_virtual;

                    // This used to not be here, but then I tried generating
                    // stylo bindings with this (without path filters), and
                    // cried a lot with a method in gfx/Point.h
                    // (ToUnknownPoint), that somehow was causing the same type
                    // to be inserted in the map two times.
                    //
                    // I couldn't make a reduced test case, but anyway...
                    // Methods of template functions not only use to be inlined,
                    // but also instantiated, and we wouldn't be able to call
                    // them, so just bail out.
                    if !ci.template_params.is_empty() {
                        return CXChildVisit_Continue;
                    }

                    // NB: This gets us an owned `Function`, not a
                    // `FunctionSig`.
                    let signature =
                        match Item::parse(cur, Some(potential_id), ctx) {
                            Ok(item) if ctx.resolve_item(item)
                                .kind()
                                .is_function() => item,
                            _ => return CXChildVisit_Continue,
                        };

                    match cur.kind() {
                        CXCursor_Constructor => {
                            ci.constructors.push(signature);
                        }
                        CXCursor_Destructor => {
                            ci.destructor = Some((is_virtual, signature));
                        }
                        CXCursor_CXXMethod => {
                            let is_const = cur.method_is_const();
                            let method_kind = if is_static {
                                MethodKind::Static
                            } else if is_virtual {
                                MethodKind::Virtual
                            } else {
                                MethodKind::Normal
                            };

                            let method =
                                Method::new(method_kind, signature, is_const);

                            ci.methods.push(method);
                        }
                        _ => unreachable!("How can we see this here?"),
                    }
                }
                CXCursor_NonTypeTemplateParameter => {
                    ci.has_non_type_template_params = true;
                }
                CXCursor_VarDecl => {
                    let linkage = cur.linkage();
                    if linkage != CXLinkage_External &&
                       linkage != CXLinkage_UniqueExternal {
                        return CXChildVisit_Continue;
                    }

                    let visibility = cur.visibility();
                    if visibility != CXVisibility_Default {
                        return CXChildVisit_Continue;
                    }

                    if let Ok(item) = Item::parse(cur,
                                                  Some(potential_id),
                                                  ctx) {
                        ci.inner_vars.push(item);
                    }
                }
                // Intentionally not handled
                CXCursor_CXXAccessSpecifier |
                CXCursor_CXXFinalAttr |
                CXCursor_FunctionTemplate |
                CXCursor_ConversionFunction => {}
                _ => {
                    warn!("unhandled comp member `{}` (kind {:?}) in `{}` ({})",
                          cur.spelling(),
                          clang::kind_to_str(cur.kind()),
                          cursor.spelling(),
                          cur.location());
                }
            }
            CXChildVisit_Continue
        });

        if let Some((ty, _, offset)) = maybe_anonymous_struct_field {
            let field =
                Field::new(None, ty, None, None, None, false, offset);
            ci.fields.push(field);
        }

        Ok(ci)
    }

    fn kind_from_cursor(cursor: &clang::Cursor)
                        -> Result<CompKind, ParseError> {
        use clang_sys::*;
        Ok(match cursor.kind() {
            CXCursor_UnionDecl => CompKind::Union,
            CXCursor_ClassDecl |
            CXCursor_StructDecl => CompKind::Struct,
            CXCursor_CXXBaseSpecifier |
            CXCursor_ClassTemplatePartialSpecialization |
            CXCursor_ClassTemplate => {
                match cursor.template_kind() {
                    CXCursor_UnionDecl => CompKind::Union,
                    _ => CompKind::Struct,
                }
            }
            _ => {
                warn!("Unknown kind for comp type: {:?}", cursor);
                return Err(ParseError::Continue);
            }
        })
    }

    /// Get the set of types that were declared within this compound type
    /// (e.g. nested class definitions).
    pub fn inner_types(&self) -> &[ItemId] {
        &self.inner_types
    }

    /// Get the set of static variables declared within this compound type.
    pub fn inner_vars(&self) -> &[ItemId] {
        &self.inner_vars
    }

    /// Have we found a field with an opaque type that could potentially mess up
    /// the layout of this compound type?
    pub fn found_unknown_attr(&self) -> bool {
        self.found_unknown_attr
    }

    /// Is this compound type packed?
    pub fn packed(&self) -> bool {
        self.packed
    }

    /// Returns whether this type needs an explicit vtable because it has
    /// virtual methods and none of its base classes has already a vtable.
    pub fn needs_explicit_vtable(&self, ctx: &BindgenContext) -> bool {
        self.has_vtable(ctx) &&
        !self.base_members.iter().any(|base| {
            // NB: Ideally, we could rely in all these types being `comp`, and
            // life would be beautiful.
            //
            // Unfortunately, given the way we implement --match-pat, and also
            // that you can inherit from templated types, we need to handle
            // other cases here too.
            ctx.resolve_type(base.ty)
                .canonical_type(ctx)
                .as_comp()
                .map_or(false, |ci| ci.has_vtable(ctx))
        })
    }

    /// Returns true if compound type has been forward declared
    pub fn is_forward_declaration(&self) -> bool {
        self.is_forward_declaration
    }
}

impl TemplateParameters for CompInfo {
    fn self_template_params(&self,
                            _ctx: &BindgenContext)
                            -> Option<Vec<ItemId>> {
        if self.template_params.is_empty() {
            None
        } else {
            Some(self.template_params.clone())
        }
    }
}

impl CanDeriveDebug for CompInfo {
    type Extra = Option<Layout>;

    fn can_derive_debug(&self,
                        ctx: &BindgenContext,
                        layout: Option<Layout>)
                        -> bool {
        if self.has_non_type_template_params() {
            return layout.map_or(false, |l| l.opaque().can_derive_debug(ctx, ()));
        }

        // We can reach here recursively via template parameters of a member,
        // for example.
        if self.detect_derive_debug_cycle.get() {
            warn!("Derive debug cycle detected!");
            return true;
        }

        if self.kind == CompKind::Union {
            if ctx.options().unstable_rust {
                return false;
            }

            return layout.unwrap_or_else(Layout::zero)
                .opaque()
                .can_derive_debug(ctx, ());
        }

        self.detect_derive_debug_cycle.set(true);

        let can_derive_debug = {
            self.base_members
                .iter()
                .all(|base| base.ty.can_derive_debug(ctx, ())) &&
            self.fields
                .iter()
                .all(|f| f.can_derive_debug(ctx, ()))
        };

        self.detect_derive_debug_cycle.set(false);

        can_derive_debug
    }
}

impl CanDeriveDefault for CompInfo {
    type Extra = Option<Layout>;

    fn can_derive_default(&self,
                          ctx: &BindgenContext,
                          layout: Option<Layout>)
                          -> bool {
        // We can reach here recursively via template parameters of a member,
        // for example.
        if self.detect_derive_default_cycle.get() {
            warn!("Derive default cycle detected!");
            return true;
        }

        if self.kind == CompKind::Union {
            if ctx.options().unstable_rust {
                return false;
            }

            return layout.unwrap_or_else(Layout::zero)
                .opaque()
                .can_derive_default(ctx, ());
        }

        self.detect_derive_default_cycle.set(true);

        let can_derive_default = !self.has_vtable(ctx) &&
                                 !self.needs_explicit_vtable(ctx) &&
                                 self.base_members
            .iter()
            .all(|base| base.ty.can_derive_default(ctx, ())) &&
                                 self.fields
            .iter()
            .all(|f| f.can_derive_default(ctx, ()));

        self.detect_derive_default_cycle.set(false);

        can_derive_default
    }
}

impl<'a> CanDeriveCopy<'a> for CompInfo {
    type Extra = (&'a Item, Option<Layout>);

    fn can_derive_copy(&self,
                       ctx: &BindgenContext,
                       (item, layout): (&Item, Option<Layout>))
                       -> bool {
        if self.has_non_type_template_params() {
            return layout.map_or(false, |l| l.opaque().can_derive_copy(ctx, ()));
        }

        // NOTE: Take into account that while unions in C and C++ are copied by
        // default, the may have an explicit destructor in C++, so we can't
        // defer this check just for the union case.
        if self.has_destructor(ctx) {
            return false;
        }

        if self.kind == CompKind::Union {
            if !ctx.options().unstable_rust {
                // NOTE: If there's no template parameters we can derive copy
                // unconditionally, since arrays are magical for rustc, and
                // __BindgenUnionField always implements copy.
                return true;
            }

            // https://github.com/rust-lang/rust/issues/36640
            if !self.template_params.is_empty() ||
               item.used_template_params(ctx).is_some() {
                return false;
            }
        }

        self.base_members
            .iter()
            .all(|base| base.ty.can_derive_copy(ctx, ())) &&
        self.fields.iter().all(|field| field.can_derive_copy(ctx, ()))
    }

    fn can_derive_copy_in_array(&self,
                                ctx: &BindgenContext,
                                extra: (&Item, Option<Layout>))
                                -> bool {
        self.can_derive_copy(ctx, extra)
    }
}

impl Trace for CompInfo {
    type Extra = Item;

    fn trace<T>(&self, context: &BindgenContext, tracer: &mut T, item: &Item)
        where T: Tracer,
    {
        let params = item.all_template_params(context).unwrap_or(vec![]);
        for p in params {
            tracer.visit_kind(p, EdgeKind::TemplateParameterDefinition);
        }

        for &ty in self.inner_types() {
            tracer.visit_kind(ty, EdgeKind::InnerType);
        }

        // We unconditionally trace `CompInfo`'s template parameters and inner
        // types for the the usage analysis. However, we don't want to continue
        // tracing anything else, if this type is marked opaque.
        if item.is_opaque(context) {
            return;
        }

        for base in self.base_members() {
            tracer.visit_kind(base.ty, EdgeKind::BaseMember);
        }

        for field in self.fields() {
            tracer.visit_kind(field.ty(), EdgeKind::Field);
        }

        for &var in self.inner_vars() {
            tracer.visit_kind(var, EdgeKind::InnerVar);
        }

        for method in self.methods() {
            tracer.visit_kind(method.signature, EdgeKind::Method);
        }

        for &ctor in self.constructors() {
            tracer.visit_kind(ctor, EdgeKind::Constructor);
        }
    }
}
