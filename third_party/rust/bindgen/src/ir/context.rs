//! Common context that is passed around during parsing and codegen.

use super::analysis::{CannotDeriveCopy, CannotDeriveDebug, CannotDeriveDefault,
                      CannotDeriveHash, CannotDerivePartialEqOrPartialOrd,
                      HasTypeParameterInArray, HasVtableAnalysis,
                      HasVtableResult, HasDestructorAnalysis,
                      UsedTemplateParameters, HasFloat, SizednessAnalysis,
                      SizednessResult, analyze};
use super::derive::{CanDeriveCopy, CanDeriveDebug, CanDeriveDefault,
                    CanDeriveHash, CanDerivePartialOrd, CanDeriveOrd,
                    CanDerivePartialEq, CanDeriveEq, CanDerive};
use super::int::IntKind;
use super::item::{IsOpaque, Item, ItemAncestors, ItemCanonicalPath, ItemSet};
use super::item_kind::ItemKind;
use super::module::{Module, ModuleKind};
use super::template::{TemplateInstantiation, TemplateParameters};
use super::traversal::{self, Edge, ItemTraversal};
use super::ty::{FloatKind, Type, TypeKind};
use super::function::Function;
use super::super::time::Timer;
use BindgenOptions;
use callbacks::ParseCallbacks;
use cexpr;
use clang::{self, Cursor};
use clang_sys;
use parse::ClangItemParser;
use proc_macro2::{Term, Span};
use std::borrow::Cow;
use std::cell::Cell;
use std::collections::{HashMap, HashSet, hash_map};
use std::collections::btree_map::{self, BTreeMap};
use std::iter::IntoIterator;
use std::mem;

/// An identifier for some kind of IR item.
#[derive(Debug, Copy, Clone, Eq, PartialOrd, Ord, Hash)]
pub struct ItemId(usize);

macro_rules! item_id_newtype {
    (
        $( #[$attr:meta] )*
        pub struct $name:ident(ItemId)
        where
            $( #[$checked_attr:meta] )*
            checked = $checked:ident with $check_method:ident,
            $( #[$expected_attr:meta] )*
            expected = $expected:ident,
            $( #[$unchecked_attr:meta] )*
            unchecked = $unchecked:ident;
    ) => {
        $( #[$attr] )*
        #[derive(Debug, Copy, Clone, Eq, PartialOrd, Ord, Hash)]
        pub struct $name(ItemId);

        impl $name {
            /// Create an `ItemResolver` from this id.
            pub fn into_resolver(self) -> ItemResolver {
                let id: ItemId = self.into();
                id.into()
            }
        }

        impl<T> ::std::cmp::PartialEq<T> for $name
        where
            T: Copy + Into<ItemId>
        {
            fn eq(&self, rhs: &T) -> bool {
                let rhs: ItemId = (*rhs).into();
                self.0 == rhs
            }
        }

        impl From<$name> for ItemId {
            fn from(id: $name) -> ItemId {
                id.0
            }
        }

        impl<'a> From<&'a $name> for ItemId {
            fn from(id: &'a $name) -> ItemId {
                id.0
            }
        }

        impl ItemId {
            $( #[$checked_attr] )*
            pub fn $checked(&self, ctx: &BindgenContext) -> Option<$name> {
                if ctx.resolve_item(*self).kind().$check_method() {
                    Some($name(*self))
                } else {
                    None
                }
            }

            $( #[$expected_attr] )*
            pub fn $expected(&self, ctx: &BindgenContext) -> $name {
                self.$checked(ctx)
                    .expect(concat!(
                        stringify!($expected),
                        " called with ItemId that points to the wrong ItemKind"
                    ))
            }

            $( #[$unchecked_attr] )*
            pub fn $unchecked(&self) -> $name {
                $name(*self)
            }
        }
    }
}

item_id_newtype! {
    /// An identifier for an `Item` whose `ItemKind` is known to be
    /// `ItemKind::Type`.
    pub struct TypeId(ItemId)
    where
        /// Convert this `ItemId` into a `TypeId` if its associated item is a type,
        /// otherwise return `None`.
        checked = as_type_id with is_type,

        /// Convert this `ItemId` into a `TypeId`.
        ///
        /// If this `ItemId` does not point to a type, then panic.
        expected = expect_type_id,

        /// Convert this `ItemId` into a `TypeId` without actually checking whether
        /// this id actually points to a `Type`.
        unchecked = as_type_id_unchecked;
}

item_id_newtype! {
    /// An identifier for an `Item` whose `ItemKind` is known to be
    /// `ItemKind::Module`.
    pub struct ModuleId(ItemId)
    where
        /// Convert this `ItemId` into a `ModuleId` if its associated item is a
        /// module, otherwise return `None`.
        checked = as_module_id with is_module,

        /// Convert this `ItemId` into a `ModuleId`.
        ///
        /// If this `ItemId` does not point to a module, then panic.
        expected = expect_module_id,

        /// Convert this `ItemId` into a `ModuleId` without actually checking
        /// whether this id actually points to a `Module`.
        unchecked = as_module_id_unchecked;
}

item_id_newtype! {
    /// An identifier for an `Item` whose `ItemKind` is known to be
    /// `ItemKind::Var`.
    pub struct VarId(ItemId)
    where
        /// Convert this `ItemId` into a `VarId` if its associated item is a var,
        /// otherwise return `None`.
        checked = as_var_id with is_var,

        /// Convert this `ItemId` into a `VarId`.
        ///
        /// If this `ItemId` does not point to a var, then panic.
        expected = expect_var_id,

        /// Convert this `ItemId` into a `VarId` without actually checking whether
        /// this id actually points to a `Var`.
        unchecked = as_var_id_unchecked;
}

item_id_newtype! {
    /// An identifier for an `Item` whose `ItemKind` is known to be
    /// `ItemKind::Function`.
    pub struct FunctionId(ItemId)
    where
        /// Convert this `ItemId` into a `FunctionId` if its associated item is a function,
        /// otherwise return `None`.
        checked = as_function_id with is_function,

        /// Convert this `ItemId` into a `FunctionId`.
        ///
        /// If this `ItemId` does not point to a function, then panic.
        expected = expect_function_id,

        /// Convert this `ItemId` into a `FunctionId` without actually checking whether
        /// this id actually points to a `Function`.
        unchecked = as_function_id_unchecked;
}

impl From<ItemId> for usize {
    fn from(id: ItemId) -> usize {
        id.0
    }
}

impl ItemId {
    /// Get a numeric representation of this id.
    pub fn as_usize(&self) -> usize {
        (*self).into()
    }
}

impl<T> ::std::cmp::PartialEq<T> for ItemId
where
    T: Copy + Into<ItemId>
{
    fn eq(&self, rhs: &T) -> bool {
        let rhs: ItemId = (*rhs).into();
        self.0 == rhs.0
    }
}

impl<T> CanDeriveDebug for T
where
    T: Copy + Into<ItemId>
{
    fn can_derive_debug(&self, ctx: &BindgenContext) -> bool {
        ctx.options().derive_debug && ctx.lookup_can_derive_debug(*self)
    }
}

impl<T> CanDeriveDefault for T
where
    T: Copy + Into<ItemId>
{
    fn can_derive_default(&self, ctx: &BindgenContext) -> bool {
        ctx.options().derive_default && ctx.lookup_can_derive_default(*self)
    }
}

impl<'a, T> CanDeriveCopy<'a> for T
where
    T: Copy + Into<ItemId>
{
    fn can_derive_copy(&self, ctx: &BindgenContext) -> bool {
        ctx.options().derive_copy && ctx.lookup_can_derive_copy(*self)
    }
}

impl<T> CanDeriveHash for T
where
    T: Copy + Into<ItemId>
{
    fn can_derive_hash(&self, ctx: &BindgenContext) -> bool {
        ctx.options().derive_hash && ctx.lookup_can_derive_hash(*self)
    }
}

impl<T> CanDerivePartialOrd for T
where
    T: Copy + Into<ItemId>
{
    fn can_derive_partialord(&self, ctx: &BindgenContext) -> bool {
        ctx.options().derive_partialord &&
            ctx.lookup_can_derive_partialeq_or_partialord(*self) == CanDerive::Yes
    }
}

impl<T> CanDerivePartialEq for T
where
    T: Copy + Into<ItemId>
{
    fn can_derive_partialeq(&self, ctx: &BindgenContext) -> bool {
        ctx.options().derive_partialeq &&
            ctx.lookup_can_derive_partialeq_or_partialord(*self) == CanDerive::Yes
    }
}

impl<T> CanDeriveEq for T
where
    T: Copy + Into<ItemId>
{
    fn can_derive_eq(&self, ctx: &BindgenContext) -> bool {
        ctx.options().derive_eq &&
            ctx.lookup_can_derive_partialeq_or_partialord(*self) == CanDerive::Yes &&
            !ctx.lookup_has_float(*self)
    }
}

impl<T> CanDeriveOrd for T
where
    T: Copy + Into<ItemId>
{
    fn can_derive_ord(&self, ctx: &BindgenContext) -> bool {
        ctx.options().derive_ord &&
            ctx.lookup_can_derive_partialeq_or_partialord(*self) == CanDerive::Yes &&
            !ctx.lookup_has_float(*self)
    }
}

/// A key used to index a resolved type, so we only process it once.
///
/// This is almost always a USR string (an unique identifier generated by
/// clang), but it can also be the canonical declaration if the type is unnamed,
/// in which case clang may generate the same USR for multiple nested unnamed
/// types.
#[derive(Eq, PartialEq, Hash, Debug)]
enum TypeKey {
    USR(String),
    Declaration(Cursor),
}

/// A context used during parsing and generation of structs.
#[derive(Debug)]
pub struct BindgenContext {
    /// The map of all the items parsed so far.
    ///
    /// It's a BTreeMap because we want the keys to be sorted to have consistent
    /// output.
    items: BTreeMap<ItemId, Item>,

    /// The next item id to use during this bindings regeneration.
    next_item_id: ItemId,

    /// Clang USR to type map. This is needed to be able to associate types with
    /// item ids during parsing.
    types: HashMap<TypeKey, TypeId>,

    /// Maps from a cursor to the item id of the named template type parameter
    /// for that cursor.
    type_params: HashMap<clang::Cursor, TypeId>,

    /// A cursor to module map. Similar reason than above.
    modules: HashMap<Cursor, ModuleId>,

    /// The root module, this is guaranteed to be an item of kind Module.
    root_module: ModuleId,

    /// Current module being traversed.
    current_module: ModuleId,

    /// A HashMap keyed on a type definition, and whose value is the parent id
    /// of the declaration.
    ///
    /// This is used to handle the cases where the semantic and the lexical
    /// parents of the cursor differ, like when a nested class is defined
    /// outside of the parent class.
    semantic_parents: HashMap<clang::Cursor, ItemId>,

    /// A stack with the current type declarations and types we're parsing. This
    /// is needed to avoid infinite recursion when parsing a type like:
    ///
    /// struct c { struct c* next; };
    ///
    /// This means effectively, that a type has a potential ID before knowing if
    /// it's a correct type. But that's not important in practice.
    ///
    /// We could also use the `types` HashMap, but my intention with it is that
    /// only valid types and declarations end up there, and this could
    /// potentially break that assumption.
    currently_parsed_types: Vec<PartialType>,

    /// A HashSet with all the already parsed macro names. This is done to avoid
    /// hard errors while parsing duplicated macros, as well to allow macro
    /// expression parsing.
    parsed_macros: HashMap<Vec<u8>, cexpr::expr::EvalResult>,

    /// The active replacements collected from replaces="xxx" annotations.
    replacements: HashMap<Vec<String>, ItemId>,

    collected_typerefs: bool,

    in_codegen: bool,

    /// The clang index for parsing.
    index: clang::Index,

    /// The translation unit for parsing.
    translation_unit: clang::TranslationUnit,

    /// Target information that can be useful for some stuff.
    target_info: Option<clang::TargetInfo>,

    /// The options given by the user via cli or other medium.
    options: BindgenOptions,

    /// Whether a bindgen complex was generated
    generated_bindgen_complex: Cell<bool>,

    /// The set of `ItemId`s that are whitelisted. This the very first thing
    /// computed after parsing our IR, and before running any of our analyses.
    whitelisted: Option<ItemSet>,

    /// The set of `ItemId`s that are whitelisted for code generation _and_ that
    /// we should generate accounting for the codegen options.
    ///
    /// It's computed right after computing the whitelisted items.
    codegen_items: Option<ItemSet>,

    /// Map from an item's id to the set of template parameter items that it
    /// uses. See `ir::named` for more details. Always `Some` during the codegen
    /// phase.
    used_template_parameters: Option<HashMap<ItemId, ItemSet>>,

    /// The set of `TypeKind::Comp` items found during parsing that need their
    /// bitfield allocation units computed. Drained in `compute_bitfield_units`.
    need_bitfield_allocation: Vec<ItemId>,

    /// The set of (`ItemId`s of) types that can't derive debug.
    ///
    /// This is populated when we enter codegen by `compute_cannot_derive_debug`
    /// and is always `None` before that and `Some` after.
    cannot_derive_debug: Option<HashSet<ItemId>>,

    /// The set of (`ItemId`s of) types that can't derive default.
    ///
    /// This is populated when we enter codegen by `compute_cannot_derive_default`
    /// and is always `None` before that and `Some` after.
    cannot_derive_default: Option<HashSet<ItemId>>,

    /// The set of (`ItemId`s of) types that can't derive copy.
    ///
    /// This is populated when we enter codegen by `compute_cannot_derive_copy`
    /// and is always `None` before that and `Some` after.
    cannot_derive_copy: Option<HashSet<ItemId>>,

    /// The set of (`ItemId`s of) types that can't derive copy in array.
    ///
    /// This is populated when we enter codegen by `compute_cannot_derive_copy`
    /// and is always `None` before that and `Some` after.
    cannot_derive_copy_in_array: Option<HashSet<ItemId>>,

    /// The set of (`ItemId`s of) types that can't derive hash.
    ///
    /// This is populated when we enter codegen by `compute_can_derive_hash`
    /// and is always `None` before that and `Some` after.
    cannot_derive_hash: Option<HashSet<ItemId>>,

    /// The map why specified `ItemId`s of) types that can't derive hash.
    ///
    /// This is populated when we enter codegen by
    /// `compute_cannot_derive_partialord_partialeq_or_eq` and is always `None`
    /// before that and `Some` after.
    cannot_derive_partialeq_or_partialord: Option<HashMap<ItemId, CanDerive>>,

    /// The sizedness of types.
    ///
    /// This is populated by `compute_sizedness` and is always `None` before
    /// that function is invoked and `Some` afterwards.
    sizedness: Option<HashMap<TypeId, SizednessResult>>,

    /// The set of (`ItemId's of`) types that has vtable.
    ///
    /// Populated when we enter codegen by `compute_has_vtable`; always `None`
    /// before that and `Some` after.
    have_vtable: Option<HashMap<ItemId, HasVtableResult>>,

    /// The set of (`ItemId's of`) types that has destructor.
    ///
    /// Populated when we enter codegen by `compute_has_destructor`; always `None`
    /// before that and `Some` after.
    have_destructor: Option<HashSet<ItemId>>,

    /// The set of (`ItemId's of`) types that has array.
    ///
    /// Populated when we enter codegen by `compute_has_type_param_in_array`; always `None`
    /// before that and `Some` after.
    has_type_param_in_array: Option<HashSet<ItemId>>,

    /// The set of (`ItemId's of`) types that has float.
    ///
    /// Populated when we enter codegen by `compute_has_float`; always `None`
    /// before that and `Some` after.
    has_float: Option<HashSet<ItemId>>,
}

/// A traversal of whitelisted items.
struct WhitelistedItemsTraversal<'ctx> {
    ctx: &'ctx BindgenContext,
    traversal: ItemTraversal<
        'ctx,
        ItemSet,
        Vec<ItemId>,
        for<'a> fn(&'a BindgenContext, Edge) -> bool,
    >,
}

impl<'ctx> Iterator for WhitelistedItemsTraversal<'ctx> {
    type Item = ItemId;

    fn next(&mut self) -> Option<ItemId> {
        loop {
            let id = self.traversal.next()?;

            if self.ctx.resolve_item(id).is_blacklisted(self.ctx) {
                continue
            }

            return Some(id);
        }
    }
}

impl<'ctx> WhitelistedItemsTraversal<'ctx> {
    /// Construct a new whitelisted items traversal.
    pub fn new<R>(
        ctx: &'ctx BindgenContext,
        roots: R,
        predicate: for<'a> fn(&'a BindgenContext, Edge) -> bool,
    ) -> Self
    where
        R: IntoIterator<Item = ItemId>,
    {
        WhitelistedItemsTraversal {
            ctx,
            traversal: ItemTraversal::new(ctx, roots, predicate),
        }
    }
}

const HOST_TARGET: &'static str =
    include_str!(concat!(env!("OUT_DIR"), "/host-target.txt"));

/// Returns the effective target, and whether it was explicitly specified on the
/// clang flags.
fn find_effective_target(clang_args: &[String]) -> (String, bool) {
    use std::env;

    for opt in clang_args {
        if opt.starts_with("--target=") {
            let mut split = opt.split('=');
            split.next();
            return (split.next().unwrap().to_owned(), true);
        }
    }

    // If we're running from a build script, try to find the cargo target.
    if let Ok(t) = env::var("TARGET") {
        return (t, false)
    }

    (HOST_TARGET.to_owned(), false)
}

impl BindgenContext {
    /// Construct the context for the given `options`.
    pub(crate) fn new(options: BindgenOptions) -> Self {
        use clang_sys;

        // TODO(emilio): Use the CXTargetInfo here when available.
        //
        // see: https://reviews.llvm.org/D32389
        let (effective_target, explicit_target) =
            find_effective_target(&options.clang_args);

        let index = clang::Index::new(false, true);

        let parse_options =
            clang_sys::CXTranslationUnit_DetailedPreprocessingRecord;

        let translation_unit = {
            let clang_args = if explicit_target {
                Cow::Borrowed(&options.clang_args)
            } else {
                let mut args = Vec::with_capacity(options.clang_args.len() + 1);
                args.push(format!("--target={}", effective_target));
                args.extend_from_slice(&options.clang_args);
                Cow::Owned(args)
            };

            clang::TranslationUnit::parse(
                &index,
                "",
                &clang_args,
                &options.input_unsaved_files,
                parse_options,
            ).expect("TranslationUnit::parse failed")
        };

        let target_info = clang::TargetInfo::new(&translation_unit);

        #[cfg(debug_assertions)]
        {
            if let Some(ref ti) = target_info {
                if effective_target == HOST_TARGET {
                    assert_eq!(ti.pointer_width / 8, mem::size_of::<*mut ()>());
                }
            }
        }

        let root_module = Self::build_root_module(ItemId(0));
        let root_module_id = root_module.id().as_module_id_unchecked();

        let mut me = BindgenContext {
            items: Default::default(),
            types: Default::default(),
            type_params: Default::default(),
            modules: Default::default(),
            next_item_id: ItemId(1),
            root_module: root_module_id,
            current_module: root_module_id,
            semantic_parents: Default::default(),
            currently_parsed_types: vec![],
            parsed_macros: Default::default(),
            replacements: Default::default(),
            collected_typerefs: false,
            in_codegen: false,
            index,
            translation_unit,
            target_info,
            options,
            generated_bindgen_complex: Cell::new(false),
            whitelisted: None,
            codegen_items: None,
            used_template_parameters: None,
            need_bitfield_allocation: Default::default(),
            cannot_derive_debug: None,
            cannot_derive_default: None,
            cannot_derive_copy: None,
            cannot_derive_copy_in_array: None,
            cannot_derive_hash: None,
            cannot_derive_partialeq_or_partialord: None,
            sizedness: None,
            have_vtable: None,
            have_destructor: None,
            has_type_param_in_array: None,
            has_float: None,
        };

        me.add_item(root_module, None, None);

        me
    }

    /// Creates a timer for the current bindgen phase. If time_phases is `true`,
    /// the timer will print to stderr when it is dropped, otherwise it will do
    /// nothing.
    pub fn timer<'a>(&self, name: &'a str) -> Timer<'a> {
        Timer::new(name).with_output(self.options.time_phases)
    }

    /// Returns the pointer width to use for the target for the current
    /// translation.
    pub fn target_pointer_size(&self) -> usize {
        if let Some(ref ti) = self.target_info {
            return ti.pointer_width / 8;
        }
        mem::size_of::<*mut ()>()
    }

    /// Get the stack of partially parsed types that we are in the middle of
    /// parsing.
    pub fn currently_parsed_types(&self) -> &[PartialType] {
        &self.currently_parsed_types[..]
    }

    /// Begin parsing the given partial type, and push it onto the
    /// `currently_parsed_types` stack so that we won't infinite recurse if we
    /// run into a reference to it while parsing it.
    pub fn begin_parsing(&mut self, partial_ty: PartialType) {
        self.currently_parsed_types.push(partial_ty);
    }

    /// Finish parsing the current partial type, pop it off the
    /// `currently_parsed_types` stack, and return it.
    pub fn finish_parsing(&mut self) -> PartialType {
        self.currently_parsed_types.pop().expect(
            "should have been parsing a type, if we finished parsing a type",
        )
    }

    /// Get the user-provided callbacks by reference, if any.
    pub fn parse_callbacks(&self) -> Option<&ParseCallbacks> {
        self.options().parse_callbacks.as_ref().map(|t| &**t)
    }

    /// Define a new item.
    ///
    /// This inserts it into the internal items set, and its type into the
    /// internal types set.
    pub fn add_item(
        &mut self,
        item: Item,
        declaration: Option<Cursor>,
        location: Option<Cursor>,
    ) {
        debug!(
            "BindgenContext::add_item({:?}, declaration: {:?}, loc: {:?}",
            item,
            declaration,
            location
        );
        debug_assert!(
            declaration.is_some() || !item.kind().is_type() ||
                item.kind().expect_type().is_builtin_or_type_param() ||
                item.kind().expect_type().is_opaque(self, &item),
            "Adding a type without declaration?"
        );

        let id = item.id();
        let is_type = item.kind().is_type();
        let is_unnamed = is_type && item.expect_type().name().is_none();
        let is_template_instantiation = is_type &&
            item.expect_type().is_template_instantiation();

        if item.id() != self.root_module {
            self.add_item_to_module(&item);
        }

        if is_type && item.expect_type().is_comp() {
            self.need_bitfield_allocation.push(id);
        }

        let old_item = self.items.insert(id, item);
        assert!(
            old_item.is_none(),
            "should not have already associated an item with the given id"
        );

        // Unnamed items can have an USR, but they can't be referenced from
        // other sites explicitly and the USR can match if the unnamed items are
        // nested, so don't bother tracking them.
        if is_type && !is_template_instantiation && declaration.is_some() {
            let mut declaration = declaration.unwrap();
            if !declaration.is_valid() {
                if let Some(location) = location {
                    if location.is_template_like() {
                        declaration = location;
                    }
                }
            }
            declaration = declaration.canonical();
            if !declaration.is_valid() {
                // This could happen, for example, with types like `int*` or
                // similar.
                //
                // Fortunately, we don't care about those types being
                // duplicated, so we can just ignore them.
                debug!(
                    "Invalid declaration {:?} found for type {:?}",
                    declaration,
                    self.items.get(&id).unwrap().kind().expect_type()
                );
                return;
            }

            let key = if is_unnamed {
                TypeKey::Declaration(declaration)
            } else if let Some(usr) = declaration.usr() {
                TypeKey::USR(usr)
            } else {
                warn!(
                    "Valid declaration with no USR: {:?}, {:?}",
                    declaration,
                    location
                );
                TypeKey::Declaration(declaration)
            };

            let old = self.types.insert(key, id.as_type_id_unchecked());
            debug_assert_eq!(old, None);
        }
    }

    /// Ensure that every item (other than the root module) is in a module's
    /// children list. This is to make sure that every whitelisted item get's
    /// codegen'd, even if its parent is not whitelisted. See issue #769 for
    /// details.
    fn add_item_to_module(&mut self, item: &Item) {
        assert!(item.id() != self.root_module);
        assert!(!self.items.contains_key(&item.id()));

        if let Some(parent) = self.items.get_mut(&item.parent_id()) {
            if let Some(module) = parent.as_module_mut() {
                debug!(
                    "add_item_to_module: adding {:?} as child of parent module {:?}",
                    item.id(),
                    item.parent_id()
                );

                module.children_mut().insert(item.id());
                return;
            }
        }

        debug!(
            "add_item_to_module: adding {:?} as child of current module {:?}",
            item.id(),
            self.current_module
        );

        self.items
            .get_mut(&self.current_module.into())
            .expect("Should always have an item for self.current_module")
            .as_module_mut()
            .expect("self.current_module should always be a module")
            .children_mut()
            .insert(item.id());
    }

    /// Add a new named template type parameter to this context's item set.
    pub fn add_type_param(&mut self, item: Item, definition: clang::Cursor) {
        debug!(
            "BindgenContext::add_type_param: item = {:?}; definition = {:?}",
            item,
            definition
        );

        assert!(
            item.expect_type().is_type_param(),
            "Should directly be a named type, not a resolved reference or anything"
        );
        assert_eq!(
            definition.kind(),
            clang_sys::CXCursor_TemplateTypeParameter
        );

        self.add_item_to_module(&item);

        let id = item.id();
        let old_item = self.items.insert(id, item);
        assert!(
            old_item.is_none(),
            "should not have already associated an item with the given id"
        );

        let old_named_ty = self.type_params.insert(definition, id.as_type_id_unchecked());
        assert!(
            old_named_ty.is_none(),
            "should not have already associated a named type with this id"
        );
    }

    /// Get the named type defined at the given cursor location, if we've
    /// already added one.
    pub fn get_type_param(&self, definition: &clang::Cursor) -> Option<TypeId> {
        assert_eq!(
            definition.kind(),
            clang_sys::CXCursor_TemplateTypeParameter
        );
        self.type_params.get(definition).cloned()
    }

    // TODO: Move all this syntax crap to other part of the code.

    /// Mangles a name so it doesn't conflict with any keyword.
    pub fn rust_mangle<'a>(&self, name: &'a str) -> Cow<'a, str> {
        if name.contains("@") ||
            name.contains("?") ||
            name.contains("$") ||
            match name {
                "abstract" |
 	            "alignof" |
 	            "as" |
 	            "become" |
 	            "box" |
                "break" |
 	            "const" |
 	            "continue" |
 	            "crate" |
 	            "do" |
                "else" |
 	            "enum" |
 	            "extern" |
 	            "false" |
 	            "final" |
                "fn" |
 	            "for" |
 	            "if" |
 	            "impl" |
 	            "in" |
                "let" |
 	            "loop" |
 	            "macro" |
 	            "match" |
 	            "mod" |
                "move" |
 	            "mut" |
 	            "offsetof" |
 	            "override" |
 	            "priv" |
                "proc" |
 	            "pub" |
 	            "pure" |
 	            "ref" |
 	            "return" |
                "Self" |
 	            "self" |
 	            "sizeof" |
 	            "static" |
 	            "struct" |
                "super" |
 	            "trait" |
 	            "true" |
 	            "type" |
 	            "typeof" |
                "unsafe" |
 	            "unsized" |
 	            "use" |
 	            "virtual" |
 	            "where" |
                "while" |
 	            "yield" |
                "bool" |
                "_" => true,
                _ => false,
            }
        {
            let mut s = name.to_owned();
            s = s.replace("@", "_");
            s = s.replace("?", "_");
            s = s.replace("$", "_");
            s.push_str("_");
            return Cow::Owned(s);
        }
        Cow::Borrowed(name)
    }

    /// Returns a mangled name as a rust identifier.
    pub fn rust_ident<S>(&self, name: S) -> Term
    where
        S: AsRef<str>
    {
        self.rust_ident_raw(self.rust_mangle(name.as_ref()))
    }

    /// Returns a mangled name as a rust identifier.
    pub fn rust_ident_raw<T>(&self, name: T) -> Term
    where
        T: AsRef<str>
    {
        Term::new(name.as_ref(), Span::call_site())
    }

    /// Iterate over all items that have been defined.
    pub fn items<'a>(&'a self) -> btree_map::Iter<'a, ItemId, Item> {
        self.items.iter()
    }

    /// Have we collected all unresolved type references yet?
    pub fn collected_typerefs(&self) -> bool {
        self.collected_typerefs
    }

    /// Gather all the unresolved type references.
    fn collect_typerefs(
        &mut self,
    ) -> Vec<(ItemId, clang::Type, clang::Cursor, Option<ItemId>)> {
        debug_assert!(!self.collected_typerefs);
        self.collected_typerefs = true;
        let mut typerefs = vec![];
        for (id, ref mut item) in &mut self.items {
            let kind = item.kind();
            let ty = match kind.as_type() {
                Some(ty) => ty,
                None => continue,
            };

            match *ty.kind() {
                TypeKind::UnresolvedTypeRef(ref ty, loc, parent_id) => {
                    typerefs.push((*id, ty.clone(), loc, parent_id));
                }
                _ => {}
            };
        }
        typerefs
    }

    /// Collect all of our unresolved type references and resolve them.
    fn resolve_typerefs(&mut self) {
        let typerefs = self.collect_typerefs();

        for (id, ty, loc, parent_id) in typerefs {
            let _resolved = {
                let resolved = Item::from_ty(&ty, loc, parent_id, self)
                    .unwrap_or_else(|_| {
                        warn!("Could not resolve type reference, falling back \
                               to opaque blob");
                        Item::new_opaque_type(self.next_item_id(), &ty, self)
                    });

                let item = self.items.get_mut(&id).unwrap();
                *item.kind_mut().as_type_mut().unwrap().kind_mut() =
                    TypeKind::ResolvedTypeRef(resolved);
                resolved
            };

            // Something in the STL is trolling me. I don't need this assertion
            // right now, but worth investigating properly once this lands.
            //
            // debug_assert!(self.items.get(&resolved).is_some(), "How?");
            //
            // if let Some(parent_id) = parent_id {
            //     assert_eq!(self.items[&resolved].parent_id(), parent_id);
            // }
        }
    }

    /// Temporarily loan `Item` with the given `ItemId`. This provides means to
    /// mutably borrow `Item` while having a reference to `BindgenContext`.
    ///
    /// `Item` with the given `ItemId` is removed from the context, given
    /// closure is executed and then `Item` is placed back.
    ///
    /// # Panics
    ///
    /// Panics if attempt to resolve given `ItemId` inside the given
    /// closure is made.
    fn with_loaned_item<F, T>(&mut self, id: ItemId, f: F) -> T
    where
        F: (FnOnce(&BindgenContext, &mut Item) -> T)
    {
        let mut item = self.items.remove(&id).unwrap();

        let result = f(self, &mut item);

        let existing = self.items.insert(id, item);
        assert!(existing.is_none());

        result
    }

    /// Compute the bitfield allocation units for all `TypeKind::Comp` items we
    /// parsed.
    fn compute_bitfield_units(&mut self) {
        assert!(self.collected_typerefs());

        let need_bitfield_allocation =
            mem::replace(&mut self.need_bitfield_allocation, vec![]);
        for id in need_bitfield_allocation {
            self.with_loaned_item(id, |ctx, item| {
                item.kind_mut()
                    .as_type_mut()
                    .unwrap()
                    .as_comp_mut()
                    .unwrap()
                    .compute_bitfield_units(ctx);
            });
        }
    }

    /// Assign a new generated name for each anonymous field.
    fn deanonymize_fields(&mut self) {
        let _t = self.timer("deanonymize_fields");

        let comp_item_ids: Vec<ItemId> = self.items
            .iter()
            .filter_map(|(id, item)| {
                if item.kind().as_type()?.is_comp() {
                    return Some(id);
                }
                None
            })
            .cloned()
            .collect();

        for id in comp_item_ids {
            self.with_loaned_item(id, |ctx, item| {
                item.kind_mut()
                    .as_type_mut()
                    .unwrap()
                    .as_comp_mut()
                    .unwrap()
                    .deanonymize_fields(ctx);
            });
        }
    }

    /// Iterate over all items and replace any item that has been named in a
    /// `replaces="SomeType"` annotation with the replacement type.
    fn process_replacements(&mut self) {
        let _t = self.timer("process_replacements");
        if self.replacements.is_empty() {
            debug!("No replacements to process");
            return;
        }

        // FIXME: This is linear, but the replaces="xxx" annotation was already
        // there, and for better or worse it's useful, sigh...
        //
        // We leverage the ResolvedTypeRef thing, though, which is cool :P.

        let mut replacements = vec![];

        for (id, item) in self.items.iter() {
            if item.annotations().use_instead_of().is_some() {
                continue;
            }

            // Calls to `canonical_name` are expensive, so eagerly filter out
            // items that cannot be replaced.
            let ty = match item.kind().as_type() {
                Some(ty) => ty,
                None => continue,
            };

            match *ty.kind() {
                TypeKind::Comp(..) |
                TypeKind::TemplateAlias(..) |
                TypeKind::Enum(..) |
                TypeKind::Alias(..) => {}
                _ => continue,
            }

            let path = item.canonical_path(self);
            let replacement = self.replacements.get(&path[1..]);

            if let Some(replacement) = replacement {
                if replacement != id {
                    // We set this just after parsing the annotation. It's
                    // very unlikely, but this can happen.
                    if self.items.get(replacement).is_some() {
                        replacements.push((id.expect_type_id(self), replacement.expect_type_id(self)));
                    }
                }
            }
        }

        for (id, replacement_id) in replacements {
            debug!("Replacing {:?} with {:?}", id, replacement_id);

            let new_parent = {
                let item = self.items.get_mut(&id.into()).unwrap();
                *item.kind_mut().as_type_mut().unwrap().kind_mut() =
                    TypeKind::ResolvedTypeRef(replacement_id);
                item.parent_id()
            };

            // Relocate the replacement item from where it was declared, to
            // where the thing it is replacing was declared.
            //
            // First, we'll make sure that its parent id is correct.

            let old_parent = self.resolve_item(replacement_id).parent_id();
            if new_parent == old_parent {
                // Same parent and therefore also same containing
                // module. Nothing to do here.
                continue;
            }

            self.items
                .get_mut(&replacement_id.into())
                .unwrap()
                .set_parent_for_replacement(new_parent);

            // Second, make sure that it is in the correct module's children
            // set.

            let old_module = {
                let immut_self = &*self;
                old_parent
                    .ancestors(immut_self)
                    .chain(Some(immut_self.root_module.into()))
                    .find(|id| {
                        let item = immut_self.resolve_item(*id);
                        item.as_module().map_or(false, |m| {
                            m.children().contains(&replacement_id.into())
                        })
                    })
            };
            let old_module = old_module.expect(
                "Every replacement item should be in a module",
            );

            let new_module = {
                let immut_self = &*self;
                new_parent.ancestors(immut_self).find(|id| {
                    immut_self.resolve_item(*id).is_module()
                })
            };
            let new_module = new_module.unwrap_or(self.root_module.into());

            if new_module == old_module {
                // Already in the correct module.
                continue;
            }

            self.items
                .get_mut(&old_module)
                .unwrap()
                .as_module_mut()
                .unwrap()
                .children_mut()
                .remove(&replacement_id.into());

            self.items
                .get_mut(&new_module)
                .unwrap()
                .as_module_mut()
                .unwrap()
                .children_mut()
                .insert(replacement_id.into());
        }
    }

    /// Enter the code generation phase, invoke the given callback `cb`, and
    /// leave the code generation phase.
    pub(crate) fn gen<F, Out>(mut self, cb: F) -> (Out, BindgenOptions)
    where
        F: FnOnce(&Self) -> Out,
    {
        self.in_codegen = true;

        self.resolve_typerefs();
        self.compute_bitfield_units();
        self.process_replacements();

        self.deanonymize_fields();

        self.assert_no_dangling_references();

        // Compute the whitelisted set after processing replacements and
        // resolving type refs, as those are the final mutations of the IR
        // graph, and their completion means that the IR graph is now frozen.
        self.compute_whitelisted_and_codegen_items();

        // Make sure to do this after processing replacements, since that messes
        // with the parentage and module children, and we want to assert that it
        // messes with them correctly.
        self.assert_every_item_in_a_module();

        self.compute_has_vtable();
        self.compute_sizedness();
        self.compute_has_destructor();
        self.find_used_template_parameters();
        self.compute_cannot_derive_debug();
        self.compute_cannot_derive_default();
        self.compute_cannot_derive_copy();
        self.compute_has_type_param_in_array();
        self.compute_has_float();
        self.compute_cannot_derive_hash();
        self.compute_cannot_derive_partialord_partialeq_or_eq();

        let ret = cb(&self);
        (ret, self.options)
    }

    /// When the `testing_only_extra_assertions` feature is enabled, this
    /// function walks the IR graph and asserts that we do not have any edges
    /// referencing an ItemId for which we do not have an associated IR item.
    fn assert_no_dangling_references(&self) {
        if cfg!(feature = "testing_only_extra_assertions") {
            for _ in self.assert_no_dangling_item_traversal() {
                // The iterator's next method does the asserting for us.
            }
        }
    }

    fn assert_no_dangling_item_traversal(
        &self,
    ) -> traversal::AssertNoDanglingItemsTraversal {
        assert!(self.in_codegen_phase());
        assert!(self.current_module == self.root_module);

        let roots = self.items().map(|(&id, _)| id);
        traversal::AssertNoDanglingItemsTraversal::new(
            self,
            roots,
            traversal::all_edges,
        )
    }

    /// When the `testing_only_extra_assertions` feature is enabled, walk over
    /// every item and ensure that it is in the children set of one of its
    /// module ancestors.
    fn assert_every_item_in_a_module(&self) {
        if cfg!(feature = "testing_only_extra_assertions") {
            assert!(self.in_codegen_phase());
            assert!(self.current_module == self.root_module);

            for (&id, _item) in self.items() {
                if id == self.root_module {
                    continue;
                }

                assert!(
                    {
                        let id = id.into_resolver()
                            .through_type_refs()
                            .through_type_aliases()
                            .resolve(self)
                            .id();
                        id.ancestors(self).chain(Some(self.root_module.into())).any(
                            |ancestor| {
                                debug!(
                                    "Checking if {:?} is a child of {:?}",
                                    id,
                                    ancestor
                                );
                                self.resolve_item(ancestor).as_module().map_or(
                                    false,
                                    |m| {
                                        m.children().contains(&id)
                                    },
                                )
                            },
                        )
                    },
                    "{:?} should be in some ancestor module's children set",
                    id
                );
            }
        }
    }

    /// Compute for every type whether it is sized or not, and whether it is
    /// sized or not as a base class.
    fn compute_sizedness(&mut self) {
        let _t = self.timer("compute_sizedness");
        assert!(self.sizedness.is_none());
        self.sizedness = Some(analyze::<SizednessAnalysis>(self));
    }

    /// Look up whether the type with the given id is sized or not.
    pub fn lookup_sizedness(&self, id: TypeId) -> SizednessResult {
        assert!(
            self.in_codegen_phase(),
            "We only compute sizedness after we've entered codegen"
        );

        self.sizedness
            .as_ref()
            .unwrap()
            .get(&id)
            .cloned()
            .unwrap_or(SizednessResult::ZeroSized)
    }

    /// Compute whether the type has vtable.
    fn compute_has_vtable(&mut self) {
        let _t = self.timer("compute_has_vtable");
        assert!(self.have_vtable.is_none());
        self.have_vtable = Some(analyze::<HasVtableAnalysis>(self));
    }

    /// Look up whether the item with `id` has vtable or not.
    pub fn lookup_has_vtable(&self, id: TypeId) -> HasVtableResult {
        assert!(
            self.in_codegen_phase(),
            "We only compute vtables when we enter codegen"
        );

        // Look up the computed value for whether the item with `id` has a
        // vtable or not.
        self.have_vtable
            .as_ref()
            .unwrap()
            .get(&id.into())
            .cloned()
            .unwrap_or(HasVtableResult::No)
    }

    /// Compute whether the type has a destructor.
    fn compute_has_destructor(&mut self) {
        let _t = self.timer("compute_has_destructor");
        assert!(self.have_destructor.is_none());
        self.have_destructor = Some(analyze::<HasDestructorAnalysis>(self));
    }

    /// Look up whether the item with `id` has a destructor.
    pub fn lookup_has_destructor(&self, id: TypeId) -> bool {
        assert!(
            self.in_codegen_phase(),
            "We only compute destructors when we enter codegen"
        );

        self.have_destructor.as_ref().unwrap().contains(&id.into())
    }

    fn find_used_template_parameters(&mut self) {
        let _t = self.timer("find_used_template_parameters");
        if self.options.whitelist_recursively {
            let used_params = analyze::<UsedTemplateParameters>(self);
            self.used_template_parameters = Some(used_params);
        } else {
            // If you aren't recursively whitelisting, then we can't really make
            // any sense of template parameter usage, and you're on your own.
            let mut used_params = HashMap::new();
            for &id in self.whitelisted_items() {
                used_params.entry(id).or_insert(
                    id.self_template_params(self).into_iter().map(|p| p.into()).collect()
                );
            }
            self.used_template_parameters = Some(used_params);
        }
    }

    /// Return `true` if `item` uses the given `template_param`, `false`
    /// otherwise.
    ///
    /// This method may only be called during the codegen phase, because the
    /// template usage information is only computed as we enter the codegen
    /// phase.
    ///
    /// If the item is blacklisted, then we say that it always uses the template
    /// parameter. This is a little subtle. The template parameter usage
    /// analysis only considers whitelisted items, and if any blacklisted item
    /// shows up in the generated bindings, it is the user's responsibility to
    /// manually provide a definition for them. To give them the most
    /// flexibility when doing that, we assume that they use every template
    /// parameter and always pass template arguments through in instantiations.
    pub fn uses_template_parameter(
        &self,
        item: ItemId,
        template_param: TypeId,
    ) -> bool {
        assert!(
            self.in_codegen_phase(),
            "We only compute template parameter usage as we enter codegen"
        );

        if self.resolve_item(item).is_blacklisted(self) {
            return true;
        }

        let template_param = template_param
            .into_resolver()
            .through_type_refs()
            .through_type_aliases()
            .resolve(self)
            .id();

        self.used_template_parameters
            .as_ref()
            .expect("should have found template parameter usage if we're in codegen")
            .get(&item)
            .map_or(false, |items_used_params| items_used_params.contains(&template_param))
    }

    /// Return `true` if `item` uses any unbound, generic template parameters,
    /// `false` otherwise.
    ///
    /// Has the same restrictions that `uses_template_parameter` has.
    pub fn uses_any_template_parameters(&self, item: ItemId) -> bool {
        assert!(
            self.in_codegen_phase(),
            "We only compute template parameter usage as we enter codegen"
        );

        self.used_template_parameters
            .as_ref()
            .expect(
                "should have template parameter usage info in codegen phase",
            )
            .get(&item)
            .map_or(false, |used| !used.is_empty())
    }

    // This deserves a comment. Builtin types don't get a valid declaration, so
    // we can't add it to the cursor->type map.
    //
    // That being said, they're not generated anyway, and are few, so the
    // duplication and special-casing is fine.
    //
    // If at some point we care about the memory here, probably a map TypeKind
    // -> builtin type ItemId would be the best to improve that.
    fn add_builtin_item(&mut self, item: Item) {
        debug!("add_builtin_item: item = {:?}", item);
        debug_assert!(item.kind().is_type());
        self.add_item_to_module(&item);
        let id = item.id();
        let old_item = self.items.insert(id, item);
        assert!(old_item.is_none(), "Inserted type twice?");
    }

    fn build_root_module(id: ItemId) -> Item {
        let module = Module::new(Some("root".into()), ModuleKind::Normal);
        Item::new(id, None, None, id, ItemKind::Module(module))
    }

    /// Get the root module.
    pub fn root_module(&self) -> ModuleId {
        self.root_module
    }

    /// Resolve a type with the given id.
    ///
    /// Panics if there is no item for the given `TypeId` or if the resolved
    /// item is not a `Type`.
    pub fn resolve_type(&self, type_id: TypeId) -> &Type {
        self.resolve_item(type_id).kind().expect_type()
    }

    /// Resolve a function with the given id.
    ///
    /// Panics if there is no item for the given `FunctionId` or if the resolved
    /// item is not a `Function`.
    pub fn resolve_func(&self, func_id: FunctionId) -> &Function {
        self.resolve_item(func_id).kind().expect_function()
    }

    /// Resolve the given `ItemId` as a type, or `None` if there is no item with
    /// the given id.
    ///
    /// Panics if the id resolves to an item that is not a type.
    pub fn safe_resolve_type(&self, type_id: TypeId) -> Option<&Type> {
        self.items.get(&type_id.into()).map(|t| t.kind().expect_type())
    }

    /// Resolve the given `ItemId` into an `Item`, or `None` if no such item
    /// exists.
    pub fn resolve_item_fallible<Id: Into<ItemId>>(&self, id: Id) -> Option<&Item> {
        self.items.get(&id.into())
    }

    /// Resolve the given `ItemId` into an `Item`.
    ///
    /// Panics if the given id does not resolve to any item.
    pub fn resolve_item<Id: Into<ItemId>>(&self, item_id: Id) -> &Item {
        let item_id = item_id.into();
        match self.items.get(&item_id) {
            Some(item) => item,
            None => panic!("Not an item: {:?}", item_id),
        }
    }

    /// Get the current module.
    pub fn current_module(&self) -> ModuleId {
        self.current_module
    }

    /// Add a semantic parent for a given type definition.
    ///
    /// We do this from the type declaration, in order to be able to find the
    /// correct type definition afterwards.
    ///
    /// TODO(emilio): We could consider doing this only when
    /// declaration.lexical_parent() != definition.lexical_parent(), but it's
    /// not sure it's worth it.
    pub fn add_semantic_parent(
        &mut self,
        definition: clang::Cursor,
        parent_id: ItemId,
    ) {
        self.semantic_parents.insert(definition, parent_id);
    }

    /// Returns a known semantic parent for a given definition.
    pub fn known_semantic_parent(
        &self,
        definition: clang::Cursor
    ) -> Option<ItemId> {
        self.semantic_parents.get(&definition).cloned()
    }


    /// Given a cursor pointing to the location of a template instantiation,
    /// return a tuple of the form `(declaration_cursor, declaration_id,
    /// num_expected_template_args)`.
    ///
    /// Note that `declaration_id` is not guaranteed to be in the context's item
    /// set! It is possible that it is a partial type that we are still in the
    /// middle of parsing.
    fn get_declaration_info_for_template_instantiation(
        &self,
        instantiation: &Cursor,
    ) -> Option<(Cursor, ItemId, usize)> {
        instantiation
            .cur_type()
            .canonical_declaration(Some(instantiation))
            .and_then(|canon_decl| {
                self.get_resolved_type(&canon_decl).and_then(
                    |template_decl_id| {
                        let num_params = template_decl_id.num_self_template_params(self);
                        if num_params == 0 {
                            None
                        } else {
                            Some((
                                *canon_decl.cursor(),
                                template_decl_id.into(),
                                num_params,
                            ))
                        }
                    },
                )
            })
            .or_else(|| {
                // If we haven't already parsed the declaration of
                // the template being instantiated, then it *must*
                // be on the stack of types we are currently
                // parsing. If it wasn't then clang would have
                // already errored out before we started
                // constructing our IR because you can't instantiate
                // a template until it is fully defined.
                instantiation
                    .referenced()
                    .and_then(|referenced| {
                        self.currently_parsed_types()
                            .iter()
                            .find(|partial_ty| *partial_ty.decl() == referenced)
                            .cloned()
                    })
                    .and_then(|template_decl| {
                        let num_template_params = template_decl.num_self_template_params(self);
                        if num_template_params == 0 {
                            None
                        } else {
                            Some((
                                *template_decl.decl(),
                                template_decl.id(),
                                num_template_params,
                            ))
                        }
                    })
            })
    }

    /// Parse a template instantiation, eg `Foo<int>`.
    ///
    /// This is surprisingly difficult to do with libclang, due to the fact that
    /// it doesn't provide explicit template argument information, except for
    /// function template declarations(!?!??!).
    ///
    /// The only way to do this is manually inspecting the AST and looking for
    /// TypeRefs and TemplateRefs inside. This, unfortunately, doesn't work for
    /// more complex cases, see the comment on the assertion below.
    ///
    /// To add insult to injury, the AST itself has structure that doesn't make
    /// sense. Sometimes `Foo<Bar<int>>` has an AST with nesting like you might
    /// expect: `(Foo (Bar (int)))`. Other times, the AST we get is completely
    /// flat: `(Foo Bar int)`.
    ///
    /// To see an example of what this method handles:
    ///
    /// ```c++
    /// template<typename T>
    /// class Incomplete {
    ///   T p;
    /// };
    ///
    /// template<typename U>
    /// class Foo {
    ///   Incomplete<U> bar;
    /// };
    /// ```
    ///
    /// Finally, template instantiations are always children of the current
    /// module. They use their template's definition for their name, so the
    /// parent is only useful for ensuring that their layout tests get
    /// codegen'd.
    fn instantiate_template(
        &mut self,
        with_id: ItemId,
        template: TypeId,
        ty: &clang::Type,
        location: clang::Cursor,
    ) -> Option<TypeId> {
        use clang_sys;

        let num_expected_args = self.resolve_type(template).num_self_template_params(self);
        if num_expected_args == 0 {
            warn!(
                "Tried to instantiate a template for which we could not \
                   determine any template parameters"
            );
            return None;
        }

        let mut args = vec![];
        let mut found_const_arg = false;
        let mut children = location.collect_children();

        if children.iter().all(|c| !c.has_children()) {
            // This is insanity... If clang isn't giving us a properly nested
            // AST for which template arguments belong to which template we are
            // instantiating, we'll need to construct it ourselves. However,
            // there is an extra `NamespaceRef, NamespaceRef, ..., TemplateRef`
            // representing a reference to the outermost template declaration
            // that we need to filter out of the children. We need to do this
            // filtering because we already know which template declaration is
            // being specialized via the `location`'s type, and if we do not
            // filter it out, we'll add an extra layer of template instantiation
            // on accident.
            let idx = children.iter().position(|c| {
                c.kind() == clang_sys::CXCursor_TemplateRef
            });
            if let Some(idx) = idx {
                if children.iter().take(idx).all(|c| {
                    c.kind() == clang_sys::CXCursor_NamespaceRef
                })
                {
                    children = children.into_iter().skip(idx + 1).collect();
                }
            }
        }

        for child in children.iter().rev() {
            match child.kind() {
                clang_sys::CXCursor_TypeRef |
                clang_sys::CXCursor_TypedefDecl |
                clang_sys::CXCursor_TypeAliasDecl => {
                    // The `with_id` id will potentially end up unused if we give up
                    // on this type (for example, because it has const value
                    // template args), so if we pass `with_id` as the parent, it is
                    // potentially a dangling reference. Instead, use the canonical
                    // template declaration as the parent. It is already parsed and
                    // has a known-resolvable `ItemId`.
                    let ty = Item::from_ty_or_ref(
                        child.cur_type(),
                        *child,
                        Some(template.into()),
                        self,
                    );
                    args.push(ty);
                }
                clang_sys::CXCursor_TemplateRef => {
                    let (template_decl_cursor, template_decl_id, num_expected_template_args) =
                        self.get_declaration_info_for_template_instantiation(child)?;

                    if num_expected_template_args == 0 ||
                        child.has_at_least_num_children(
                            num_expected_template_args,
                        )
                    {
                        // Do a happy little parse. See comment in the TypeRef
                        // match arm about parent IDs.
                        let ty = Item::from_ty_or_ref(
                            child.cur_type(),
                            *child,
                            Some(template.into()),
                            self,
                        );
                        args.push(ty);
                    } else {
                        // This is the case mentioned in the doc comment where
                        // clang gives us a flattened AST and we have to
                        // reconstruct which template arguments go to which
                        // instantiation :(
                        let args_len = args.len();
                        if args_len < num_expected_template_args {
                            warn!(
                                "Found a template instantiation without \
                                   enough template arguments"
                            );
                            return None;
                        }

                        let mut sub_args: Vec<_> = args
                            .drain(args_len - num_expected_template_args..)
                            .collect();
                        sub_args.reverse();

                        let sub_name = Some(template_decl_cursor.spelling());
                        let sub_inst = TemplateInstantiation::new(
                            // This isn't guaranteed to be a type that we've
                            // already finished parsing yet.
                            template_decl_id.as_type_id_unchecked(),
                            sub_args,
                        );
                        let sub_kind =
                            TypeKind::TemplateInstantiation(sub_inst);
                        let sub_ty = Type::new(
                            sub_name,
                            template_decl_cursor
                                .cur_type()
                                .fallible_layout()
                                .ok(),
                            sub_kind,
                            false,
                        );
                        let sub_id = self.next_item_id();
                        let sub_item = Item::new(
                            sub_id,
                            None,
                            None,
                            self.current_module.into(),
                            ItemKind::Type(sub_ty),
                        );

                        // Bypass all the validations in add_item explicitly.
                        debug!(
                            "instantiate_template: inserting nested \
                                instantiation item: {:?}",
                            sub_item
                        );
                        self.add_item_to_module(&sub_item);
                        debug_assert!(sub_id == sub_item.id());
                        self.items.insert(sub_id, sub_item);
                        args.push(sub_id.as_type_id_unchecked());
                    }
                }
                _ => {
                    warn!(
                        "Found template arg cursor we can't handle: {:?}",
                        child
                    );
                    found_const_arg = true;
                }
            }
        }

        if found_const_arg {
            // This is a dependently typed template instantiation. That is, an
            // instantiation of a template with one or more const values as
            // template arguments, rather than only types as template
            // arguments. For example, `Foo<true, 5>` versus `Bar<bool, int>`.
            // We can't handle these instantiations, so just punt in this
            // situation...
            warn!(
                "Found template instantiated with a const value; \
                   bindgen can't handle this kind of template instantiation!"
            );
            return None;
        }

        if args.len() != num_expected_args {
            warn!(
                "Found a template with an unexpected number of template \
                   arguments"
            );
            return None;
        }

        args.reverse();
        let type_kind = TypeKind::TemplateInstantiation(
            TemplateInstantiation::new(template, args),
        );
        let name = ty.spelling();
        let name = if name.is_empty() { None } else { Some(name) };
        let ty = Type::new(
            name,
            ty.fallible_layout().ok(),
            type_kind,
            ty.is_const(),
        );
        let item = Item::new(
            with_id,
            None,
            None,
            self.current_module.into(),
            ItemKind::Type(ty),
        );

        // Bypass all the validations in add_item explicitly.
        debug!("instantiate_template: inserting item: {:?}", item);
        self.add_item_to_module(&item);
        debug_assert!(with_id == item.id());
        self.items.insert(with_id, item);
        Some(with_id.as_type_id_unchecked())
    }

    /// If we have already resolved the type for the given type declaration,
    /// return its `ItemId`. Otherwise, return `None`.
    pub fn get_resolved_type(
        &self,
        decl: &clang::CanonicalTypeDeclaration,
    ) -> Option<TypeId> {
        self.types
            .get(&TypeKey::Declaration(*decl.cursor()))
            .or_else(|| {
                decl.cursor().usr().and_then(
                    |usr| self.types.get(&TypeKey::USR(usr)),
                )
            })
            .cloned()
    }

    /// Looks up for an already resolved type, either because it's builtin, or
    /// because we already have it in the map.
    pub fn builtin_or_resolved_ty(
        &mut self,
        with_id: ItemId,
        parent_id: Option<ItemId>,
        ty: &clang::Type,
        location: Option<clang::Cursor>,
    ) -> Option<TypeId> {
        use clang_sys::{CXCursor_TypeAliasTemplateDecl, CXCursor_TypeRef};
        debug!(
            "builtin_or_resolved_ty: {:?}, {:?}, {:?}",
            ty,
            location,
            parent_id
        );

        if let Some(decl) = ty.canonical_declaration(location.as_ref()) {
            if let Some(id) = self.get_resolved_type(&decl) {
                debug!(
                    "Already resolved ty {:?}, {:?}, {:?} {:?}",
                    id,
                    decl,
                    ty,
                    location
                );
                // If the declaration already exists, then either:
                //
                //   * the declaration is a template declaration of some sort,
                //     and we are looking at an instantiation or specialization
                //     of it, or
                //   * we have already parsed and resolved this type, and
                //     there's nothing left to do.
                if decl.cursor().is_template_like() &&
                    *ty != decl.cursor().cur_type() &&
                    location.is_some()
                {
                    let location = location.unwrap();

                    // For specialized type aliases, there's no way to get the
                    // template parameters as of this writing (for a struct
                    // specialization we wouldn't be in this branch anyway).
                    //
                    // Explicitly return `None` if there aren't any
                    // unspecialized parameters (contains any `TypeRef`) so we
                    // resolve the canonical type if there is one and it's
                    // exposed.
                    //
                    // This is _tricky_, I know :(
                    if decl.cursor().kind() == CXCursor_TypeAliasTemplateDecl &&
                        !location.contains_cursor(CXCursor_TypeRef) &&
                        ty.canonical_type().is_valid_and_exposed()
                    {
                        return None;
                    }

                    return self.instantiate_template(with_id, id, ty, location)
                        .or_else(|| Some(id));
                }

                return Some(self.build_ty_wrapper(with_id, id, parent_id, ty));
            }
        }

        debug!("Not resolved, maybe builtin?");
        self.build_builtin_ty(ty)
    }

    /// Make a new item that is a resolved type reference to the `wrapped_id`.
    ///
    /// This is unfortunately a lot of bloat, but is needed to properly track
    /// constness et al.
    ///
    /// We should probably make the constness tracking separate, so it doesn't
    /// bloat that much, but hey, we already bloat the heck out of builtin
    /// types.
    pub fn build_ty_wrapper(
        &mut self,
        with_id: ItemId,
        wrapped_id: TypeId,
        parent_id: Option<ItemId>,
        ty: &clang::Type,
    ) -> TypeId {
        self.build_wrapper(
            with_id,
            wrapped_id,
            parent_id,
            ty,
            ty.is_const(),
        )
    }

    /// A wrapper over a type that adds a const qualifier explicitly.
    ///
    /// Needed to handle const methods in C++, wrapping the type .
    pub fn build_const_wrapper(
        &mut self,
        with_id: ItemId,
        wrapped_id: TypeId,
        parent_id: Option<ItemId>,
        ty: &clang::Type,
    ) -> TypeId {
        self.build_wrapper(
            with_id,
            wrapped_id,
            parent_id,
            ty,
            /* is_const = */ true,
        )
    }

    fn build_wrapper(
        &mut self,
        with_id: ItemId,
        wrapped_id: TypeId,
        parent_id: Option<ItemId>,
        ty: &clang::Type,
        is_const: bool,
    ) -> TypeId {
        let spelling = ty.spelling();
        let layout = ty.fallible_layout().ok();
        let type_kind = TypeKind::ResolvedTypeRef(wrapped_id);
        let ty = Type::new(Some(spelling), layout, type_kind, is_const);
        let item = Item::new(
            with_id,
            None,
            None,
            parent_id.unwrap_or(self.current_module.into()),
            ItemKind::Type(ty),
        );
        self.add_builtin_item(item);
        with_id.as_type_id_unchecked()
    }

    /// Returns the next item id to be used for an item.
    pub fn next_item_id(&mut self) -> ItemId {
        let ret = self.next_item_id;
        self.next_item_id = ItemId(self.next_item_id.0 + 1);
        ret
    }

    fn build_builtin_ty(&mut self, ty: &clang::Type) -> Option<TypeId> {
        use clang_sys::*;
        let type_kind = match ty.kind() {
            CXType_NullPtr => TypeKind::NullPtr,
            CXType_Void => TypeKind::Void,
            CXType_Bool => TypeKind::Int(IntKind::Bool),
            CXType_Int => TypeKind::Int(IntKind::Int),
            CXType_UInt => TypeKind::Int(IntKind::UInt),
            CXType_Char_S => TypeKind::Int(IntKind::Char {
                is_signed: true,
            }),
            CXType_Char_U => TypeKind::Int(IntKind::Char {
                is_signed: false,
            }),
            CXType_SChar => TypeKind::Int(IntKind::SChar),
            CXType_UChar => TypeKind::Int(IntKind::UChar),
            CXType_Short => TypeKind::Int(IntKind::Short),
            CXType_UShort => TypeKind::Int(IntKind::UShort),
            CXType_WChar | CXType_Char16 => TypeKind::Int(IntKind::U16),
            CXType_Char32 => TypeKind::Int(IntKind::U32),
            CXType_Long => TypeKind::Int(IntKind::Long),
            CXType_ULong => TypeKind::Int(IntKind::ULong),
            CXType_LongLong => TypeKind::Int(IntKind::LongLong),
            CXType_ULongLong => TypeKind::Int(IntKind::ULongLong),
            CXType_Int128 => TypeKind::Int(IntKind::I128),
            CXType_UInt128 => TypeKind::Int(IntKind::U128),
            CXType_Float => TypeKind::Float(FloatKind::Float),
            CXType_Double => TypeKind::Float(FloatKind::Double),
            CXType_LongDouble => TypeKind::Float(FloatKind::LongDouble),
            CXType_Float128 => TypeKind::Float(FloatKind::Float128),
            CXType_Complex => {
                let float_type =
                    ty.elem_type().expect("Not able to resolve complex type?");
                let float_kind = match float_type.kind() {
                    CXType_Float => FloatKind::Float,
                    CXType_Double => FloatKind::Double,
                    CXType_LongDouble => FloatKind::LongDouble,
                    CXType_Float128 => FloatKind::Float128,
                    _ => {
                        panic!(
                            "Non floating-type complex? {:?}, {:?}",
                            ty,
                            float_type,
                        )
                    },
                };
                TypeKind::Complex(float_kind)
            }
            _ => return None,
        };

        let spelling = ty.spelling();
        let is_const = ty.is_const();
        let layout = ty.fallible_layout().ok();
        let ty = Type::new(Some(spelling), layout, type_kind, is_const);
        let id = self.next_item_id();
        let item =
            Item::new(id, None, None, self.root_module.into(), ItemKind::Type(ty));
        self.add_builtin_item(item);
        Some(id.as_type_id_unchecked())
    }

    /// Get the current Clang translation unit that is being processed.
    pub fn translation_unit(&self) -> &clang::TranslationUnit {
        &self.translation_unit
    }

    /// Have we parsed the macro named `macro_name` already?
    pub fn parsed_macro(&self, macro_name: &[u8]) -> bool {
        self.parsed_macros.contains_key(macro_name)
    }

    /// Get the currently parsed macros.
    pub fn parsed_macros(&self) -> &HashMap<Vec<u8>, cexpr::expr::EvalResult> {
        debug_assert!(!self.in_codegen_phase());
        &self.parsed_macros
    }

    /// Mark the macro named `macro_name` as parsed.
    pub fn note_parsed_macro(
        &mut self,
        id: Vec<u8>,
        value: cexpr::expr::EvalResult,
    ) {
        self.parsed_macros.insert(id, value);
    }

    /// Are we in the codegen phase?
    pub fn in_codegen_phase(&self) -> bool {
        self.in_codegen
    }

    /// Mark the type with the given `name` as replaced by the type with id
    /// `potential_ty`.
    ///
    /// Replacement types are declared using the `replaces="xxx"` annotation,
    /// and implies that the original type is hidden.
    pub fn replace(&mut self, name: &[String], potential_ty: ItemId) {
        match self.replacements.entry(name.into()) {
            hash_map::Entry::Vacant(entry) => {
                debug!(
                    "Defining replacement for {:?} as {:?}",
                    name,
                    potential_ty
                );
                entry.insert(potential_ty);
            }
            hash_map::Entry::Occupied(occupied) => {
                warn!(
                    "Replacement for {:?} already defined as {:?}; \
                       ignoring duplicate replacement definition as {:?}",
                    name,
                    occupied.get(),
                    potential_ty
                );
            }
        }
    }

    /// Is the item with the given `name` blacklisted? Or is the item with the given
    /// `name` and `id` replaced by another type, and effectively blacklisted?
    pub fn blacklisted_by_name<Id: Into<ItemId>>(&self, path: &[String], id: Id) -> bool {
        let id = id.into();
        debug_assert!(
            self.in_codegen_phase(),
            "You're not supposed to call this yet"
        );
        self.options.blacklisted_types.matches(&path[1..].join("::")) ||
            self.is_replaced_type(path, id)
    }

    /// Has the item with the given `name` and `id` been replaced by another
    /// type?
    fn is_replaced_type<Id: Into<ItemId>>(&self, path: &[String], id: Id) -> bool {
        let id = id.into();
        match self.replacements.get(path) {
            Some(replaced_by) if *replaced_by != id => true,
            _ => false,
        }
    }

    /// Is the type with the given `name` marked as opaque?
    pub fn opaque_by_name(&self, path: &[String]) -> bool {
        debug_assert!(
            self.in_codegen_phase(),
            "You're not supposed to call this yet"
        );
        self.options.opaque_types.matches(&path[1..].join("::"))
    }

    /// Get the options used to configure this bindgen context.
    pub(crate) fn options(&self) -> &BindgenOptions {
        &self.options
    }

    /// Tokenizes a namespace cursor in order to get the name and kind of the
    /// namespace.
    fn tokenize_namespace(
        &self,
        cursor: &clang::Cursor,
    ) -> (Option<String>, ModuleKind) {
        assert_eq!(
            cursor.kind(),
            ::clang_sys::CXCursor_Namespace,
            "Be a nice person"
        );
        let tokens = match cursor.tokens() {
            Some(tokens) => tokens,
            None => return (None, ModuleKind::Normal),
        };

        let mut iter = tokens.iter();
        let mut kind = ModuleKind::Normal;
        let mut found_namespace_keyword = false;
        let mut module_name = None;

        while let Some(token) = iter.next() {
            match &*token.spelling {
                "inline" => {
                    assert!(!found_namespace_keyword);
                    assert!(kind != ModuleKind::Inline);
                    kind = ModuleKind::Inline;
                }
                // The double colon allows us to handle nested namespaces like
                // namespace foo::bar { }
                //
                // libclang still gives us two namespace cursors, which is cool,
                // but the tokenization of the second begins with the double
                // colon. That's ok, so we only need to handle the weird
                // tokenization here.
                //
                // Fortunately enough, inline nested namespace specifiers aren't
                // a thing, and are invalid C++ :)
                "namespace" | "::" => {
                    found_namespace_keyword = true;
                }
                "{" => {
                    assert!(found_namespace_keyword);
                    break;
                }
                name if found_namespace_keyword => {
                    module_name = Some(name.to_owned());
                    break;
                }
                _ => {
                    panic!(
                        "Unknown token while processing namespace: {:?}",
                        token
                    );
                }
            }
        }

        (module_name, kind)
    }

    /// Given a CXCursor_Namespace cursor, return the item id of the
    /// corresponding module, or create one on the fly.
    pub fn module(&mut self, cursor: clang::Cursor) -> ModuleId {
        use clang_sys::*;
        assert_eq!(cursor.kind(), CXCursor_Namespace, "Be a nice person");
        let cursor = cursor.canonical();
        if let Some(id) = self.modules.get(&cursor) {
            return *id;
        }

        let (module_name, kind) = self.tokenize_namespace(&cursor);

        let module_id = self.next_item_id();
        let module = Module::new(module_name, kind);
        let module = Item::new(
            module_id,
            None,
            None,
            self.current_module.into(),
            ItemKind::Module(module),
        );

        let module_id = module.id().as_module_id_unchecked();
        self.modules.insert(cursor, module_id);

        self.add_item(module, None, None);

        module_id
    }

    /// Start traversing the module with the given `module_id`, invoke the
    /// callback `cb`, and then return to traversing the original module.
    pub fn with_module<F>(&mut self, module_id: ModuleId, cb: F)
    where
        F: FnOnce(&mut Self),
    {
        debug_assert!(self.resolve_item(module_id).kind().is_module(), "Wat");

        let previous_id = self.current_module;
        self.current_module = module_id;

        cb(self);

        self.current_module = previous_id;
    }

    /// Iterate over all (explicitly or transitively) whitelisted items.
    ///
    /// If no items are explicitly whitelisted, then all items are considered
    /// whitelisted.
    pub fn whitelisted_items(&self) -> &ItemSet {
        assert!(self.in_codegen_phase());
        assert!(self.current_module == self.root_module);

        self.whitelisted.as_ref().unwrap()
    }

    /// Get a reference to the set of items we should generate.
    pub fn codegen_items(&self) -> &ItemSet {
        assert!(self.in_codegen_phase());
        assert!(self.current_module == self.root_module);
        self.codegen_items.as_ref().unwrap()
    }

    /// Compute the whitelisted items set and populate `self.whitelisted`.
    fn compute_whitelisted_and_codegen_items(&mut self) {
        assert!(self.in_codegen_phase());
        assert!(self.current_module == self.root_module);
        assert!(self.whitelisted.is_none());
        let _t = self.timer("compute_whitelisted_and_codegen_items");

        let roots = {
            let mut roots = self.items()
                // Only consider roots that are enabled for codegen.
                .filter(|&(_, item)| item.is_enabled_for_codegen(self))
                .filter(|&(_, item)| {
                    // If nothing is explicitly whitelisted, then everything is fair
                    // game.
                    if self.options().whitelisted_types.is_empty() &&
                        self.options().whitelisted_functions.is_empty() &&
                        self.options().whitelisted_vars.is_empty() {
                            return true;
                        }

                    // If this is a type that explicitly replaces another, we assume
                    // you know what you're doing.
                    if item.annotations().use_instead_of().is_some() {
                        return true;
                    }

                    let name = item.canonical_path(self)[1..].join("::");
                    debug!("whitelisted_items: testing {:?}", name);
                    match *item.kind() {
                        ItemKind::Module(..) => true,
                        ItemKind::Function(_) => {
                            self.options().whitelisted_functions.matches(&name)
                        }
                        ItemKind::Var(_) => {
                            self.options().whitelisted_vars.matches(&name)
                        }
                        ItemKind::Type(ref ty) => {
                            if self.options().whitelisted_types.matches(&name) {
                                return true;
                            }

                            let parent = self.resolve_item(item.parent_id());
                            if parent.is_module() {
                                let mut prefix_path = parent.canonical_path(self);

                                // Unnamed top-level enums are special and we
                                // whitelist them via the `whitelisted_vars` filter,
                                // since they're effectively top-level constants,
                                // and there's no way for them to be referenced
                                // consistently.
                                if let TypeKind::Enum(ref enum_) = *ty.kind() {
                                    if ty.name().is_none() &&
                                        enum_.variants().iter().any(|variant| {
                                            prefix_path.push(variant.name().into());
                                            let name = prefix_path[1..].join("::");
                                            prefix_path.pop().unwrap();
                                            self.options()
                                                .whitelisted_vars
                                                .matches(&name)
                                        }) {
                                            return true;
                                        }
                                }
                            }

                            false
                        }
                    }
                })
                .map(|(&id, _)| id)
                .collect::<Vec<_>>();

            // The reversal preserves the expected ordering of traversal,
            // resulting in more stable-ish bindgen-generated names for
            // anonymous types (like unions).
            roots.reverse();
            roots
        };

        let whitelisted_items_predicate =
            if self.options().whitelist_recursively {
                traversal::all_edges
            } else {
                // Only follow InnerType edges from the whitelisted roots.
                // Such inner types (e.g. anonymous structs/unions) are
                // always emitted by codegen, and they need to be whitelisted
                // to make sure they are processed by e.g. the derive analysis.
                traversal::only_inner_type_edges
            };

        let whitelisted = WhitelistedItemsTraversal::new(
            self,
            roots.clone(),
            whitelisted_items_predicate,
        ).collect::<ItemSet>();

        let codegen_items = if self.options().whitelist_recursively {
            WhitelistedItemsTraversal::new(
                self,
                roots.clone(),
                traversal::codegen_edges,
            ).collect::<ItemSet>()
        } else {
            whitelisted.clone()
        };

        self.whitelisted = Some(whitelisted);
        self.codegen_items = Some(codegen_items);
    }

    /// Convenient method for getting the prefix to use for most traits in
    /// codegen depending on the `use_core` option.
    pub fn trait_prefix(&self) -> Term {
        if self.options().use_core {
            self.rust_ident_raw("core")
        } else {
            self.rust_ident_raw("std")
        }
    }

    /// Call if a bindgen complex is generated
    pub fn generated_bindgen_complex(&self) {
        self.generated_bindgen_complex.set(true)
    }

    /// Whether we need to generate the bindgen complex type
    pub fn need_bindgen_complex_type(&self) -> bool {
        self.generated_bindgen_complex.get()
    }

    /// Compute whether we can derive debug.
    fn compute_cannot_derive_debug(&mut self) {
        let _t = self.timer("compute_cannot_derive_debug");
        assert!(self.cannot_derive_debug.is_none());
        if self.options.derive_debug {
            self.cannot_derive_debug = Some(analyze::<CannotDeriveDebug>(self));
        }
    }

    /// Look up whether the item with `id` can
    /// derive debug or not.
    pub fn lookup_can_derive_debug<Id: Into<ItemId>>(&self, id: Id) -> bool {
        let id = id.into();
        assert!(
            self.in_codegen_phase(),
            "We only compute can_derive_debug when we enter codegen"
        );

        // Look up the computed value for whether the item with `id` can
        // derive debug or not.
        !self.cannot_derive_debug.as_ref().unwrap().contains(&id)
    }

    /// Compute whether we can derive default.
    fn compute_cannot_derive_default(&mut self) {
        let _t = self.timer("compute_cannot_derive_default");
        assert!(self.cannot_derive_default.is_none());
        if self.options.derive_default {
            self.cannot_derive_default =
                Some(analyze::<CannotDeriveDefault>(self));
        }
    }

    /// Look up whether the item with `id` can
    /// derive default or not.
    pub fn lookup_can_derive_default<Id: Into<ItemId>>(&self, id: Id) -> bool {
        let id = id.into();
        assert!(
            self.in_codegen_phase(),
            "We only compute can_derive_default when we enter codegen"
        );

        // Look up the computed value for whether the item with `id` can
        // derive default or not.
        !self.cannot_derive_default.as_ref().unwrap().contains(&id)
    }

    /// Compute whether we can derive copy.
    fn compute_cannot_derive_copy(&mut self) {
        let _t = self.timer("compute_cannot_derive_copy");
        assert!(self.cannot_derive_copy.is_none());
        self.cannot_derive_copy = Some(analyze::<CannotDeriveCopy>(self));
    }

    /// Compute whether we can derive hash.
    fn compute_cannot_derive_hash(&mut self) {
        let _t = self.timer("compute_cannot_derive_hash");
        assert!(self.cannot_derive_hash.is_none());
        if self.options.derive_hash {
            self.cannot_derive_hash = Some(analyze::<CannotDeriveHash>(self));
        }
    }

    /// Look up whether the item with `id` can
    /// derive hash or not.
    pub fn lookup_can_derive_hash<Id: Into<ItemId>>(&self, id: Id) -> bool {
        let id = id.into();
        assert!(
            self.in_codegen_phase(),
            "We only compute can_derive_debug when we enter codegen"
        );

        // Look up the computed value for whether the item with `id` can
        // derive hash or not.
        !self.cannot_derive_hash.as_ref().unwrap().contains(&id)
    }

    /// Compute whether we can derive PartialOrd, PartialEq or Eq.
    fn compute_cannot_derive_partialord_partialeq_or_eq(&mut self) {
        let _t = self.timer("compute_cannot_derive_partialord_partialeq_or_eq");
        assert!(self.cannot_derive_partialeq_or_partialord.is_none());
        if self.options.derive_partialord || self.options.derive_partialeq || self.options.derive_eq {
            self.cannot_derive_partialeq_or_partialord = Some(analyze::<CannotDerivePartialEqOrPartialOrd>(self));
        }
    }

    /// Look up whether the item with `id` can derive `Partial{Eq,Ord}`.
    pub fn lookup_can_derive_partialeq_or_partialord<Id: Into<ItemId>>(&self, id: Id) -> CanDerive {
        let id = id.into();
        assert!(
            self.in_codegen_phase(),
            "We only compute can_derive_partialeq_or_partialord when we enter codegen"
        );

        // Look up the computed value for whether the item with `id` can
        // derive partialeq or not.
        self.cannot_derive_partialeq_or_partialord.as_ref()
            .unwrap()
            .get(&id)
            .cloned()
            .unwrap_or(CanDerive::Yes)
    }

    /// Look up whether the item with `id` can derive `Copy` or not.
    pub fn lookup_can_derive_copy<Id: Into<ItemId>>(&self, id: Id) -> bool {
        assert!(
            self.in_codegen_phase(),
            "We only compute can_derive_debug when we enter codegen"
        );

        // Look up the computed value for whether the item with `id` can
        // derive `Copy` or not.
        let id = id.into();

        !self.lookup_has_type_param_in_array(id) &&
            !self.cannot_derive_copy.as_ref().unwrap().contains(&id)
    }

    /// Compute whether the type has type parameter in array.
    fn compute_has_type_param_in_array(&mut self) {
        let _t = self.timer("compute_has_type_param_in_array");
        assert!(self.has_type_param_in_array.is_none());
        self.has_type_param_in_array =
            Some(analyze::<HasTypeParameterInArray>(self));
    }

    /// Look up whether the item with `id` has type parameter in array or not.
    pub fn lookup_has_type_param_in_array<Id: Into<ItemId>>(&self, id: Id) -> bool {
        assert!(
            self.in_codegen_phase(),
            "We only compute has array when we enter codegen"
        );

        // Look up the computed value for whether the item with `id` has
        // type parameter in array or not.
        self.has_type_param_in_array.as_ref().unwrap().contains(&id.into())
    }

    /// Compute whether the type has float.
    fn compute_has_float(&mut self) {
        let _t = self.timer("compute_has_float");
        assert!(self.has_float.is_none());
        if self.options.derive_eq || self.options.derive_ord {
            self.has_float = Some(analyze::<HasFloat>(self));
        }
    }

    /// Look up whether the item with `id` has array or not.
    pub fn lookup_has_float<Id: Into<ItemId>>(&self, id: Id) -> bool {
        assert!(self.in_codegen_phase(),
                "We only compute has float when we enter codegen");

        // Look up the computed value for whether the item with `id` has
        // float or not.
        self.has_float.as_ref().unwrap().contains(&id.into())
    }

    /// Check if `--no-partialeq` flag is enabled for this item.
    pub fn no_partialeq_by_name(&self, item: &Item) -> bool {
        let name = item.canonical_path(self)[1..].join("::");
        self.options().no_partialeq_types.matches(&name)
    }

    /// Check if `--no-copy` flag is enabled for this item.
    pub fn no_copy_by_name(&self, item: &Item) -> bool {
        let name = item.canonical_path(self)[1..].join("::");
        self.options().no_copy_types.matches(&name)
    }

    /// Check if `--no-hash` flag is enabled for this item.
    pub fn no_hash_by_name(&self, item: &Item) -> bool {
        let name = item.canonical_path(self)[1..].join("::");
        self.options().no_hash_types.matches(&name)
    }
}

/// A builder struct for configuring item resolution options.
#[derive(Debug, Copy, Clone)]
pub struct ItemResolver {
    id: ItemId,
    through_type_refs: bool,
    through_type_aliases: bool,
}

impl ItemId {
    /// Create an `ItemResolver` from this item id.
    pub fn into_resolver(self) -> ItemResolver {
        self.into()
    }
}

impl<T> From<T> for ItemResolver
where
    T: Into<ItemId>
{
    fn from(id: T) -> ItemResolver {
        ItemResolver::new(id)
    }
}

impl ItemResolver {
    /// Construct a new `ItemResolver` from the given id.
    pub fn new<Id: Into<ItemId>>(id: Id) -> ItemResolver {
        let id = id.into();
        ItemResolver {
            id: id,
            through_type_refs: false,
            through_type_aliases: false,
        }
    }

    /// Keep resolving through `Type::TypeRef` items.
    pub fn through_type_refs(mut self) -> ItemResolver {
        self.through_type_refs = true;
        self
    }

    /// Keep resolving through `Type::Alias` items.
    pub fn through_type_aliases(mut self) -> ItemResolver {
        self.through_type_aliases = true;
        self
    }

    /// Finish configuring and perform the actual item resolution.
    pub fn resolve(self, ctx: &BindgenContext) -> &Item {
        assert!(ctx.collected_typerefs());

        let mut id = self.id;
        loop {
            let item = ctx.resolve_item(id);
            let ty_kind = item.as_type().map(|t| t.kind());
            match ty_kind {
                Some(&TypeKind::ResolvedTypeRef(next_id))
                    if self.through_type_refs => {
                    id = next_id.into();
                }
                // We intentionally ignore template aliases here, as they are
                // more complicated, and don't represent a simple renaming of
                // some type.
                Some(&TypeKind::Alias(next_id))
                    if self.through_type_aliases => {
                    id = next_id.into();
                }
                _ => return item,
            }
        }
    }
}

/// A type that we are in the middle of parsing.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct PartialType {
    decl: Cursor,
    // Just an ItemId, and not a TypeId, because we haven't finished this type
    // yet, so there's still time for things to go wrong.
    id: ItemId,
}

impl PartialType {
    /// Construct a new `PartialType`.
    pub fn new(decl: Cursor, id: ItemId) -> PartialType {
        // assert!(decl == decl.canonical());
        PartialType {
            decl: decl,
            id: id,
        }
    }

    /// The cursor pointing to this partial type's declaration location.
    pub fn decl(&self) -> &Cursor {
        &self.decl
    }

    /// The item ID allocated for this type. This is *NOT* a key for an entry in
    /// the context's item set yet!
    pub fn id(&self) -> ItemId {
        self.id
    }
}

impl TemplateParameters for PartialType {
    fn self_template_params(
        &self,
        _ctx: &BindgenContext,
    ) -> Vec<TypeId> {
        // Maybe at some point we will eagerly parse named types, but for now we
        // don't and this information is unavailable.
        vec![]
    }

    fn num_self_template_params(&self, _ctx: &BindgenContext) -> usize {
        // Wouldn't it be nice if libclang would reliably give us this
        // information‽
        match self.decl().kind() {
            clang_sys::CXCursor_ClassTemplate |
            clang_sys::CXCursor_FunctionTemplate |
            clang_sys::CXCursor_TypeAliasTemplateDecl => {
                let mut num_params = 0;
                self.decl().visit(|c| {
                    match c.kind() {
                        clang_sys::CXCursor_TemplateTypeParameter |
                        clang_sys::CXCursor_TemplateTemplateParameter |
                        clang_sys::CXCursor_NonTypeTemplateParameter => {
                            num_params += 1;
                        }
                        _ => {}
                    };
                    clang_sys::CXChildVisit_Continue
                });
                num_params
            }
            _ => 0,
        }
    }
}
