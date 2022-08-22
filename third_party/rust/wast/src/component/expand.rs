use crate::component::*;
use crate::core;
use crate::gensym;
use crate::kw;
use crate::token::Id;
use crate::token::{Index, Span};
use std::collections::HashMap;
use std::mem;

/// Performs an AST "expansion" pass over the component fields provided.
///
/// This expansion is intended to desugar the AST from various parsed constructs
/// to bits and bobs amenable for name resolution as well as binary encoding.
/// For example `(import "" (func))` is split into a type definition followed by
/// the import referencing that type definition.
///
/// Most forms of AST expansion happen in this file and afterwards the AST will
/// be handed to the name resolution pass which will convert `Index::Id` to
/// `Index::Num` wherever it's found.
pub fn expand(fields: &mut Vec<ComponentField<'_>>) {
    Expander::default().expand_component_fields(fields)
}

enum AnyType<'a> {
    Core(CoreType<'a>),
    Component(Type<'a>),
}

impl<'a> From<AnyType<'a>> for ComponentTypeDecl<'a> {
    fn from(t: AnyType<'a>) -> Self {
        match t {
            AnyType::Core(t) => Self::CoreType(t),
            AnyType::Component(t) => Self::Type(t),
        }
    }
}

impl<'a> From<AnyType<'a>> for InstanceTypeDecl<'a> {
    fn from(t: AnyType<'a>) -> Self {
        match t {
            AnyType::Core(t) => Self::CoreType(t),
            AnyType::Component(t) => Self::Type(t),
        }
    }
}

impl<'a> From<AnyType<'a>> for ComponentField<'a> {
    fn from(t: AnyType<'a>) -> Self {
        match t {
            AnyType::Core(t) => Self::CoreType(t),
            AnyType::Component(t) => Self::Type(t),
        }
    }
}

#[derive(Default)]
struct Expander<'a> {
    /// Fields, during processing, which should be prepended to the
    /// currently-being-processed field. This should always be empty after
    /// processing is complete.
    types_to_prepend: Vec<AnyType<'a>>,
    component_fields_to_prepend: Vec<ComponentField<'a>>,

    /// Fields that are appended to the end of the module once everything has
    /// finished.
    component_fields_to_append: Vec<ComponentField<'a>>,
}

impl<'a> Expander<'a> {
    fn expand_component_fields(&mut self, fields: &mut Vec<ComponentField<'a>>) {
        let mut cur = 0;
        while cur < fields.len() {
            self.expand_field(&mut fields[cur]);
            let amt = self.types_to_prepend.len() + self.component_fields_to_prepend.len();
            fields.splice(cur..cur, self.component_fields_to_prepend.drain(..));
            fields.splice(cur..cur, self.types_to_prepend.drain(..).map(Into::into));
            cur += 1 + amt;
        }
        fields.append(&mut self.component_fields_to_append);
    }

    fn expand_decls<T>(&mut self, decls: &mut Vec<T>, expand: fn(&mut Self, &mut T))
    where
        T: From<AnyType<'a>>,
    {
        let mut cur = 0;
        while cur < decls.len() {
            expand(self, &mut decls[cur]);
            assert!(self.component_fields_to_prepend.is_empty());
            assert!(self.component_fields_to_append.is_empty());
            let amt = self.types_to_prepend.len();
            decls.splice(cur..cur, self.types_to_prepend.drain(..).map(From::from));
            cur += 1 + amt;
        }
    }

    fn expand_field(&mut self, item: &mut ComponentField<'a>) {
        let expanded = match item {
            ComponentField::CoreModule(m) => self.expand_core_module(m),
            ComponentField::CoreInstance(i) => {
                self.expand_core_instance(i);
                None
            }
            ComponentField::CoreType(t) => {
                self.expand_core_type(t);
                None
            }
            ComponentField::Component(c) => self.expand_nested_component(c),
            ComponentField::Instance(i) => self.expand_instance(i),
            ComponentField::Type(t) => {
                self.expand_type(t);
                None
            }
            ComponentField::CanonicalFunc(f) => {
                self.expand_canonical_func(f);
                None
            }
            ComponentField::CoreFunc(f) => self.expand_core_func(f),
            ComponentField::Func(f) => self.expand_func(f),
            ComponentField::Import(i) => {
                self.expand_item_sig(&mut i.item);
                None
            }
            ComponentField::Start(_)
            | ComponentField::CoreAlias(_)
            | ComponentField::Alias(_)
            | ComponentField::Export(_)
            | ComponentField::Custom(_) => None,
        };

        if let Some(expanded) = expanded {
            *item = expanded;
        }
    }

    fn expand_core_module(&mut self, module: &mut CoreModule<'a>) -> Option<ComponentField<'a>> {
        for name in module.exports.names.drain(..) {
            let id = gensym::fill(module.span, &mut module.id);
            self.component_fields_to_append
                .push(ComponentField::Export(ComponentExport {
                    span: module.span,
                    name,
                    kind: ComponentExportKind::module(module.span, id),
                }));
        }
        match &mut module.kind {
            // inline modules are expanded later during resolution
            CoreModuleKind::Inline { .. } => None,
            CoreModuleKind::Import { import, ty } => {
                let idx = self.expand_core_type_use(ty);
                Some(ComponentField::Import(ComponentImport {
                    span: module.span,
                    name: import.name,
                    item: ItemSig {
                        span: module.span,
                        id: module.id,
                        name: None,
                        kind: ItemSigKind::CoreModule(CoreTypeUse::Ref(idx)),
                    },
                }))
            }
        }
    }

    fn expand_core_instance(&mut self, instance: &mut CoreInstance<'a>) {
        match &mut instance.kind {
            CoreInstanceKind::Instantiate { args, .. } => {
                for arg in args {
                    self.expand_core_instantiation_arg(&mut arg.kind);
                }
            }
            CoreInstanceKind::BundleOfExports { .. } => {}
        }
    }

    fn expand_nested_component(
        &mut self,
        component: &mut NestedComponent<'a>,
    ) -> Option<ComponentField<'a>> {
        for name in component.exports.names.drain(..) {
            let id = gensym::fill(component.span, &mut component.id);
            self.component_fields_to_append
                .push(ComponentField::Export(ComponentExport {
                    span: component.span,
                    name,
                    kind: ComponentExportKind::component(component.span, id),
                }));
        }
        match &mut component.kind {
            NestedComponentKind::Inline(fields) => {
                expand(fields);
                None
            }
            NestedComponentKind::Import { import, ty } => {
                let idx = self.expand_component_type_use(ty);
                Some(ComponentField::Import(ComponentImport {
                    span: component.span,
                    name: import.name,
                    item: ItemSig {
                        span: component.span,
                        id: component.id,
                        name: None,
                        kind: ItemSigKind::Component(ComponentTypeUse::Ref(idx)),
                    },
                }))
            }
        }
    }

    fn expand_instance(&mut self, instance: &mut Instance<'a>) -> Option<ComponentField<'a>> {
        for name in instance.exports.names.drain(..) {
            let id = gensym::fill(instance.span, &mut instance.id);
            self.component_fields_to_append
                .push(ComponentField::Export(ComponentExport {
                    span: instance.span,
                    name,
                    kind: ComponentExportKind::instance(instance.span, id),
                }));
        }
        match &mut instance.kind {
            InstanceKind::Import { import, ty } => {
                let idx = self.expand_component_type_use(ty);
                Some(ComponentField::Import(ComponentImport {
                    span: instance.span,
                    name: import.name,
                    item: ItemSig {
                        span: instance.span,
                        id: instance.id,
                        name: None,
                        kind: ItemSigKind::Instance(ComponentTypeUse::Ref(idx)),
                    },
                }))
            }
            InstanceKind::Instantiate { args, .. } => {
                for arg in args {
                    self.expand_instantiation_arg(&mut arg.kind);
                }
                None
            }
            InstanceKind::BundleOfExports { .. } => None,
        }
    }

    fn expand_canonical_func(&mut self, func: &mut CanonicalFunc<'a>) {
        match &mut func.kind {
            CanonicalFuncKind::Lift { ty, .. } => {
                self.expand_component_type_use(ty);
            }
            CanonicalFuncKind::Lower(_) => {}
        }
    }

    fn expand_core_func(&mut self, func: &mut CoreFunc<'a>) -> Option<ComponentField<'a>> {
        match &mut func.kind {
            CoreFuncKind::Alias(a) => Some(ComponentField::CoreAlias(CoreAlias {
                span: func.span,
                id: func.id,
                name: func.name,
                target: CoreAliasTarget::Export {
                    instance: a.instance,
                    name: a.name,
                    kind: core::ExportKind::Func,
                },
            })),
            CoreFuncKind::Lower(info) => Some(ComponentField::CanonicalFunc(CanonicalFunc {
                span: func.span,
                id: func.id,
                name: func.name,
                kind: CanonicalFuncKind::Lower(mem::take(info)),
            })),
        }
    }

    fn expand_func(&mut self, func: &mut Func<'a>) -> Option<ComponentField<'a>> {
        for name in func.exports.names.drain(..) {
            let id = gensym::fill(func.span, &mut func.id);
            self.component_fields_to_append
                .push(ComponentField::Export(ComponentExport {
                    span: func.span,
                    name,
                    kind: ComponentExportKind::func(func.span, id),
                }));
        }
        match &mut func.kind {
            FuncKind::Import { import, ty } => {
                let idx = self.expand_component_type_use(ty);
                Some(ComponentField::Import(ComponentImport {
                    span: func.span,
                    name: import.name,
                    item: ItemSig {
                        span: func.span,
                        id: func.id,
                        name: None,
                        kind: ItemSigKind::Func(ComponentTypeUse::Ref(idx)),
                    },
                }))
            }
            FuncKind::Lift { ty, info } => {
                let idx = self.expand_component_type_use(ty);
                Some(ComponentField::CanonicalFunc(CanonicalFunc {
                    span: func.span,
                    id: func.id,
                    name: func.name,
                    kind: CanonicalFuncKind::Lift {
                        ty: ComponentTypeUse::Ref(idx),
                        info: mem::take(info),
                    },
                }))
            }
            FuncKind::Alias(a) => Some(ComponentField::Alias(Alias {
                span: func.span,
                id: func.id,
                name: func.name,
                target: AliasTarget::Export {
                    instance: a.instance,
                    name: a.name,
                    kind: ComponentExportAliasKind::Func,
                },
            })),
        }
    }

    fn expand_core_type(&mut self, field: &mut CoreType<'a>) {
        match &mut field.def {
            CoreTypeDef::Def(_) => {}
            CoreTypeDef::Module(m) => self.expand_module_ty(m),
        }

        let id = gensym::fill(field.span, &mut field.id);
        let index = Index::Id(id);
        match &field.def {
            CoreTypeDef::Def(_) => {}
            CoreTypeDef::Module(t) => t.key().insert(self, index),
        }
    }

    fn expand_type(&mut self, field: &mut Type<'a>) {
        match &mut field.def {
            TypeDef::Defined(d) => self.expand_defined_ty(d),
            TypeDef::Func(f) => self.expand_func_ty(f),
            TypeDef::Component(c) => self.expand_component_ty(c),
            TypeDef::Instance(i) => self.expand_instance_ty(i),
        }

        let id = gensym::fill(field.span, &mut field.id);
        let index = Index::Id(id);
        match &field.def {
            TypeDef::Defined(t) => t.key().insert(self, index),
            TypeDef::Func(t) => t.key().insert(self, index),
            TypeDef::Component(t) => t.key().insert(self, index),
            TypeDef::Instance(t) => t.key().insert(self, index),
        }
    }

    fn expand_func_ty(&mut self, ty: &mut ComponentFunctionType<'a>) {
        for param in ty.params.iter_mut() {
            self.expand_component_val_ty(&mut param.ty);
        }
        self.expand_component_val_ty(&mut ty.result);
    }

    fn expand_module_ty(&mut self, ty: &mut ModuleType<'a>) {
        use crate::core::resolve::types::{FuncKey, TypeKey, TypeReference};

        // Note that this is a custom implementation from everything else in
        // this file since this is using core wasm types instead of component
        // types, so a small part of the core wasm expansion process is
        // inlined here to handle the `TypeUse` from core wasm.

        let mut func_type_to_idx = HashMap::new();
        let mut to_prepend = Vec::new();
        let mut i = 0;
        while i < ty.decls.len() {
            match &mut ty.decls[i] {
                ModuleTypeDecl::Type(ty) => match &ty.def {
                    core::TypeDef::Func(f) => {
                        let id = gensym::fill(ty.span, &mut ty.id);
                        func_type_to_idx.insert(f.key(), Index::Id(id));
                    }
                    core::TypeDef::Struct(_) => {}
                    core::TypeDef::Array(_) => {}
                },
                ModuleTypeDecl::Alias(_) => {}
                ModuleTypeDecl::Import(ty) => {
                    expand_sig(&mut ty.item, &mut to_prepend, &mut func_type_to_idx);
                }
                ModuleTypeDecl::Export(_, item) => {
                    expand_sig(item, &mut to_prepend, &mut func_type_to_idx);
                }
            }
            ty.decls.splice(i..i, to_prepend.drain(..));
            i += 1;
        }

        fn expand_sig<'a>(
            item: &mut core::ItemSig<'a>,
            to_prepend: &mut Vec<ModuleTypeDecl<'a>>,
            func_type_to_idx: &mut HashMap<FuncKey<'a>, Index<'a>>,
        ) {
            match &mut item.kind {
                core::ItemKind::Func(t) | core::ItemKind::Tag(core::TagType::Exception(t)) => {
                    // If the index is already filled in then this is skipped
                    if t.index.is_some() {
                        return;
                    }

                    // Otherwise the inline type information is used to
                    // generate a type into this module if necessary. If the
                    // function type already exists we reuse the same key,
                    // otherwise a fresh type definition is created and we use
                    // that one instead.
                    let ty = t.inline.take().unwrap_or_default();
                    let key = ty.key();
                    if let Some(idx) = func_type_to_idx.get(&key) {
                        t.index = Some(*idx);
                        return;
                    }
                    let id = gensym::gen(item.span);
                    to_prepend.push(ModuleTypeDecl::Type(core::Type {
                        span: item.span,
                        id: Some(id),
                        name: None,
                        def: key.to_def(item.span),
                        parent: None,
                    }));
                    let idx = Index::Id(id);
                    t.index = Some(idx);
                }
                core::ItemKind::Global(_)
                | core::ItemKind::Table(_)
                | core::ItemKind::Memory(_) => {}
            }
        }
    }

    fn expand_component_ty(&mut self, ty: &mut ComponentType<'a>) {
        Expander::default().expand_decls(&mut ty.decls, |e, decl| match decl {
            ComponentTypeDecl::CoreType(t) => e.expand_core_type(t),
            ComponentTypeDecl::Type(t) => e.expand_type(t),
            ComponentTypeDecl::Alias(_) => {}
            ComponentTypeDecl::Export(t) => e.expand_item_sig(&mut t.item),
            ComponentTypeDecl::Import(t) => e.expand_item_sig(&mut t.item),
        })
    }

    fn expand_instance_ty(&mut self, ty: &mut InstanceType<'a>) {
        Expander::default().expand_decls(&mut ty.decls, |e, decl| match decl {
            InstanceTypeDecl::CoreType(t) => e.expand_core_type(t),
            InstanceTypeDecl::Type(t) => e.expand_type(t),
            InstanceTypeDecl::Alias(_) => {}
            InstanceTypeDecl::Export(t) => e.expand_item_sig(&mut t.item),
        })
    }

    fn expand_item_sig(&mut self, ext: &mut ItemSig<'a>) {
        match &mut ext.kind {
            ItemSigKind::CoreModule(t) => {
                self.expand_core_type_use(t);
            }
            ItemSigKind::Func(t) => {
                self.expand_component_type_use(t);
            }
            ItemSigKind::Component(t) => {
                self.expand_component_type_use(t);
            }
            ItemSigKind::Instance(t) => {
                self.expand_component_type_use(t);
            }
            ItemSigKind::Value(t) => {
                self.expand_component_val_ty(&mut t.0);
            }
            ItemSigKind::Type(_) => {}
        }
    }

    fn expand_defined_ty(&mut self, ty: &mut ComponentDefinedType<'a>) {
        match ty {
            ComponentDefinedType::Primitive(_)
            | ComponentDefinedType::Flags(_)
            | ComponentDefinedType::Enum(_) => {}
            ComponentDefinedType::Record(r) => {
                for field in r.fields.iter_mut() {
                    self.expand_component_val_ty(&mut field.ty);
                }
            }
            ComponentDefinedType::Variant(v) => {
                for case in v.cases.iter_mut() {
                    self.expand_component_val_ty(&mut case.ty);
                }
            }
            ComponentDefinedType::List(t) => {
                self.expand_component_val_ty(&mut t.element);
            }
            ComponentDefinedType::Tuple(t) => {
                for field in t.fields.iter_mut() {
                    self.expand_component_val_ty(field);
                }
            }
            ComponentDefinedType::Union(u) => {
                for ty in u.types.iter_mut() {
                    self.expand_component_val_ty(ty);
                }
            }
            ComponentDefinedType::Option(t) => {
                self.expand_component_val_ty(&mut t.element);
            }
            ComponentDefinedType::Expected(e) => {
                self.expand_component_val_ty(&mut e.ok);
                self.expand_component_val_ty(&mut e.err);
            }
        }
    }

    fn expand_component_val_ty(&mut self, ty: &mut ComponentValType<'a>) {
        let inline = match ty {
            ComponentValType::Inline(ComponentDefinedType::Primitive(_))
            | ComponentValType::Ref(_) => return,
            ComponentValType::Inline(inline) => {
                self.expand_defined_ty(inline);
                mem::take(inline)
            }
        };
        // If this inline type has already been defined within this context
        // then reuse the previously defined type to avoid injecting too many
        // types into the type index space.
        if let Some(idx) = inline.key().lookup(self) {
            *ty = ComponentValType::Ref(idx);
            return;
        }

        // And if this type isn't already defined we append it to the index
        // space with a fresh and unique name.
        let span = Span::from_offset(0); // FIXME(#613): don't manufacture
        let id = gensym::gen(span);

        self.types_to_prepend.push(inline.into_any_type(span, id));

        let idx = Index::Id(id);
        *ty = ComponentValType::Ref(idx);
    }

    fn expand_core_type_use<T>(
        &mut self,
        item: &mut CoreTypeUse<'a, T>,
    ) -> CoreItemRef<'a, kw::r#type>
    where
        T: TypeReference<'a>,
    {
        let span = Span::from_offset(0); // FIXME(#613): don't manufacture
        let mut inline = match mem::take(item) {
            // If this type-use was already a reference to an existing type
            // then we put it back the way it was and return the corresponding
            // index.
            CoreTypeUse::Ref(idx) => {
                *item = CoreTypeUse::Ref(idx.clone());
                return idx;
            }

            // ... otherwise with an inline type definition we go into
            // processing below.
            CoreTypeUse::Inline(inline) => inline,
        };
        inline.expand(self);

        // If this inline type has already been defined within this context
        // then reuse the previously defined type to avoid injecting too many
        // types into the type index space.
        if let Some(idx) = inline.key().lookup(self) {
            let ret = CoreItemRef {
                idx,
                kind: kw::r#type(span),
                export_name: None,
            };
            *item = CoreTypeUse::Ref(ret.clone());
            return ret;
        }

        // And if this type isn't already defined we append it to the index
        // space with a fresh and unique name.
        let id = gensym::gen(span);

        self.types_to_prepend.push(inline.into_any_type(span, id));

        let idx = Index::Id(id);
        let ret = CoreItemRef {
            idx,
            kind: kw::r#type(span),
            export_name: None,
        };

        *item = CoreTypeUse::Ref(ret.clone());
        ret
    }

    fn expand_component_type_use<T>(
        &mut self,
        item: &mut ComponentTypeUse<'a, T>,
    ) -> ItemRef<'a, kw::r#type>
    where
        T: TypeReference<'a>,
    {
        let span = Span::from_offset(0); // FIXME(#613): don't manufacture
        let mut inline = match mem::take(item) {
            // If this type-use was already a reference to an existing type
            // then we put it back the way it was and return the corresponding
            // index.
            ComponentTypeUse::Ref(idx) => {
                *item = ComponentTypeUse::Ref(idx.clone());
                return idx;
            }

            // ... otherwise with an inline type definition we go into
            // processing below.
            ComponentTypeUse::Inline(inline) => inline,
        };
        inline.expand(self);

        // If this inline type has already been defined within this context
        // then reuse the previously defined type to avoid injecting too many
        // types into the type index space.
        if let Some(idx) = inline.key().lookup(self) {
            let ret = ItemRef {
                idx,
                kind: kw::r#type(span),
                export_names: Vec::new(),
            };
            *item = ComponentTypeUse::Ref(ret.clone());
            return ret;
        }

        // And if this type isn't already defined we append it to the index
        // space with a fresh and unique name.
        let id = gensym::gen(span);

        self.types_to_prepend.push(inline.into_any_type(span, id));

        let idx = Index::Id(id);
        let ret = ItemRef {
            idx,
            kind: kw::r#type(span),
            export_names: Vec::new(),
        };

        *item = ComponentTypeUse::Ref(ret.clone());
        ret
    }

    fn expand_core_instantiation_arg(&mut self, arg: &mut CoreInstantiationArgKind<'a>) {
        let (span, exports) = match arg {
            CoreInstantiationArgKind::Instance(_) => return,
            CoreInstantiationArgKind::BundleOfExports(span, exports) => (*span, mem::take(exports)),
        };
        let id = gensym::gen(span);
        self.component_fields_to_prepend
            .push(ComponentField::CoreInstance(CoreInstance {
                span,
                id: Some(id),
                name: None,
                kind: CoreInstanceKind::BundleOfExports(exports),
            }));
        *arg = CoreInstantiationArgKind::Instance(CoreItemRef {
            kind: kw::instance(span),
            idx: Index::Id(id),
            export_name: None,
        });
    }

    fn expand_instantiation_arg(&mut self, arg: &mut InstantiationArgKind<'a>) {
        let (span, exports) = match arg {
            InstantiationArgKind::Item(_) => return,
            InstantiationArgKind::BundleOfExports(span, exports) => (*span, mem::take(exports)),
        };
        let id = gensym::gen(span);
        self.component_fields_to_prepend
            .push(ComponentField::Instance(Instance {
                span,
                id: Some(id),
                name: None,
                exports: Default::default(),
                kind: InstanceKind::BundleOfExports(exports),
            }));
        *arg = InstantiationArgKind::Item(ComponentExportKind::instance(span, id));
    }
}

trait TypeReference<'a> {
    type Key: TypeKey<'a>;
    fn key(&self) -> Self::Key;
    fn expand(&mut self, cx: &mut Expander<'a>);
    fn into_any_type(self, span: Span, id: Id<'a>) -> AnyType<'a>;
}

impl<'a> TypeReference<'a> for ComponentDefinedType<'a> {
    type Key = Todo; // FIXME(#598): should implement this

    fn key(&self) -> Self::Key {
        Todo
    }

    fn expand(&mut self, cx: &mut Expander<'a>) {
        cx.expand_defined_ty(self)
    }

    fn into_any_type(self, span: Span, id: Id<'a>) -> AnyType<'a> {
        AnyType::Component(Type {
            span,
            id: Some(id),
            name: None,
            exports: Default::default(),
            def: TypeDef::Defined(self),
        })
    }
}

impl<'a> TypeReference<'a> for ComponentType<'a> {
    type Key = Todo; // FIXME(#598): should implement this

    fn key(&self) -> Self::Key {
        Todo
    }

    fn expand(&mut self, cx: &mut Expander<'a>) {
        cx.expand_component_ty(self)
    }

    fn into_any_type(self, span: Span, id: Id<'a>) -> AnyType<'a> {
        AnyType::Component(Type {
            span,
            id: Some(id),
            name: None,
            exports: Default::default(),
            def: TypeDef::Component(self),
        })
    }
}

impl<'a> TypeReference<'a> for ModuleType<'a> {
    type Key = Todo; // FIXME(#598): should implement this

    fn key(&self) -> Self::Key {
        Todo
    }

    fn expand(&mut self, cx: &mut Expander<'a>) {
        cx.expand_module_ty(self)
    }

    fn into_any_type(self, span: Span, id: Id<'a>) -> AnyType<'a> {
        AnyType::Core(CoreType {
            span,
            id: Some(id),
            name: None,
            def: CoreTypeDef::Module(self),
        })
    }
}

impl<'a> TypeReference<'a> for InstanceType<'a> {
    type Key = Todo; // FIXME(#598): should implement this

    fn key(&self) -> Self::Key {
        Todo
    }

    fn expand(&mut self, cx: &mut Expander<'a>) {
        cx.expand_instance_ty(self)
    }

    fn into_any_type(self, span: Span, id: Id<'a>) -> AnyType<'a> {
        AnyType::Component(Type {
            span,
            id: Some(id),
            name: None,
            exports: Default::default(),
            def: TypeDef::Instance(self),
        })
    }
}

impl<'a> TypeReference<'a> for ComponentFunctionType<'a> {
    type Key = Todo; // FIXME(#598): should implement this

    fn key(&self) -> Self::Key {
        Todo
    }

    fn expand(&mut self, cx: &mut Expander<'a>) {
        cx.expand_func_ty(self)
    }

    fn into_any_type(self, span: Span, id: Id<'a>) -> AnyType<'a> {
        AnyType::Component(Type {
            span,
            id: Some(id),
            name: None,
            exports: Default::default(),
            def: TypeDef::Func(self),
        })
    }
}

trait TypeKey<'a> {
    fn lookup(&self, cx: &Expander<'a>) -> Option<Index<'a>>;
    fn insert(&self, cx: &mut Expander<'a>, index: Index<'a>);
}

struct Todo;

impl<'a> TypeKey<'a> for Todo {
    fn lookup(&self, _cx: &Expander<'a>) -> Option<Index<'a>> {
        None
    }

    fn insert(&self, cx: &mut Expander<'a>, index: Index<'a>) {
        drop((cx, index));
    }
}
