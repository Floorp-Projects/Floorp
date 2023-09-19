use crate::component::*;
use crate::core::{self, ValType};
use crate::kw;
use crate::names::Namespace;
use crate::token::Span;
use crate::token::{Id, Index};
use crate::Error;

/// Resolve the fields of a component and everything nested within it, changing
/// `Index::Id` to `Index::Num` and expanding alias syntax sugar.
pub fn resolve(component: &mut Component<'_>) -> Result<(), Error> {
    let fields = match &mut component.kind {
        ComponentKind::Text(fields) => fields,
        ComponentKind::Binary(_) => return Ok(()),
    };
    let mut resolver = Resolver::default();
    resolver.fields(component.id, fields)
}

impl<'a> From<Alias<'a>> for ComponentField<'a> {
    fn from(a: Alias<'a>) -> Self {
        Self::Alias(a)
    }
}

impl<'a> From<Alias<'a>> for ModuleTypeDecl<'a> {
    fn from(a: Alias<'a>) -> Self {
        Self::Alias(a)
    }
}

impl<'a> From<Alias<'a>> for ComponentTypeDecl<'a> {
    fn from(a: Alias<'a>) -> Self {
        Self::Alias(a)
    }
}

impl<'a> From<Alias<'a>> for InstanceTypeDecl<'a> {
    fn from(a: Alias<'a>) -> Self {
        Self::Alias(a)
    }
}

#[derive(Default)]
struct Resolver<'a> {
    stack: Vec<ComponentState<'a>>,

    // When a name refers to a definition in an outer scope, we'll need to
    // insert an outer alias before it. This collects the aliases to be
    // inserted during resolution.
    aliases_to_insert: Vec<Alias<'a>>,
}

/// Context structure used to perform name resolution.
#[derive(Default)]
struct ComponentState<'a> {
    id: Option<Id<'a>>,

    // Namespaces within each component. Note that each namespace carries
    // with it information about the signature of the item in that namespace.
    // The signature is later used to synthesize the type of a component and
    // inject type annotations if necessary.
    core_funcs: Namespace<'a>,
    core_globals: Namespace<'a>,
    core_tables: Namespace<'a>,
    core_memories: Namespace<'a>,
    core_types: Namespace<'a>,
    core_tags: Namespace<'a>,
    core_instances: Namespace<'a>,
    core_modules: Namespace<'a>,

    funcs: Namespace<'a>,
    types: Namespace<'a>,
    instances: Namespace<'a>,
    components: Namespace<'a>,
    values: Namespace<'a>,
}

impl<'a> ComponentState<'a> {
    fn new(id: Option<Id<'a>>) -> ComponentState<'a> {
        ComponentState {
            id,
            ..ComponentState::default()
        }
    }

    fn register_item_sig(&mut self, sig: &ItemSig<'a>) -> Result<u32, Error> {
        match &sig.kind {
            ItemSigKind::CoreModule(_) => self.core_modules.register(sig.id, "core module"),
            ItemSigKind::Func(_) => self.funcs.register(sig.id, "func"),
            ItemSigKind::Component(_) => self.components.register(sig.id, "component"),
            ItemSigKind::Instance(_) => self.instances.register(sig.id, "instance"),
            ItemSigKind::Value(_) => self.values.register(sig.id, "value"),
            ItemSigKind::Type(_) => self.types.register(sig.id, "type"),
        }
    }
}

impl<'a> Resolver<'a> {
    fn current(&mut self) -> &mut ComponentState<'a> {
        self.stack
            .last_mut()
            .expect("should have at least one component state")
    }

    fn fields(
        &mut self,
        id: Option<Id<'a>>,
        fields: &mut Vec<ComponentField<'a>>,
    ) -> Result<(), Error> {
        self.stack.push(ComponentState::new(id));
        self.resolve_prepending_aliases(fields, Resolver::field, ComponentState::register)?;
        self.stack.pop();
        Ok(())
    }

    fn resolve_prepending_aliases<T>(
        &mut self,
        fields: &mut Vec<T>,
        resolve: fn(&mut Self, &mut T) -> Result<(), Error>,
        register: fn(&mut ComponentState<'a>, &T) -> Result<(), Error>,
    ) -> Result<(), Error>
    where
        T: From<Alias<'a>>,
    {
        assert!(self.aliases_to_insert.is_empty());

        // Iterate through the fields of the component. We use an index
        // instead of an iterator because we'll be inserting aliases
        // as we go.
        let mut i = 0;
        while i < fields.len() {
            // Resolve names within the field.
            resolve(self, &mut fields[i])?;

            // Name resolution may have emitted some aliases. Insert them before
            // the current definition.
            let amt = self.aliases_to_insert.len();
            fields.splice(i..i, self.aliases_to_insert.drain(..).map(T::from));
            i += amt;

            // Definitions can't refer to themselves or to definitions that appear
            // later in the format. Now that we're done resolving this field,
            // assign it an index for later definitions to refer to.
            register(self.current(), &fields[i])?;

            i += 1;
        }

        Ok(())
    }

    fn field(&mut self, field: &mut ComponentField<'a>) -> Result<(), Error> {
        match field {
            ComponentField::CoreModule(m) => self.core_module(m),
            ComponentField::CoreInstance(i) => self.core_instance(i),
            ComponentField::CoreType(t) => self.core_ty(t),
            ComponentField::Component(c) => self.component(c),
            ComponentField::Instance(i) => self.instance(i),
            ComponentField::Alias(a) => self.alias(a, false),
            ComponentField::Type(t) => self.ty(t),
            ComponentField::CanonicalFunc(f) => self.canonical_func(f),
            ComponentField::CoreFunc(_) => unreachable!("should be expanded already"),
            ComponentField::Func(_) => unreachable!("should be expanded already"),
            ComponentField::Start(s) => self.start(s),
            ComponentField::Import(i) => self.item_sig(&mut i.item),
            ComponentField::Export(e) => {
                if let Some(ty) = &mut e.ty {
                    self.item_sig(&mut ty.0)?;
                }
                self.export(&mut e.kind)
            }
            ComponentField::Custom(_) | ComponentField::Producers(_) => Ok(()),
        }
    }

    fn core_module(&mut self, module: &mut CoreModule) -> Result<(), Error> {
        match &mut module.kind {
            CoreModuleKind::Inline { fields } => {
                crate::core::resolve::resolve(fields)?;
            }

            CoreModuleKind::Import { .. } => {
                unreachable!("should be expanded already")
            }
        }

        Ok(())
    }

    fn component(&mut self, component: &mut NestedComponent<'a>) -> Result<(), Error> {
        match &mut component.kind {
            NestedComponentKind::Import { .. } => unreachable!("should be expanded already"),
            NestedComponentKind::Inline(fields) => self.fields(component.id, fields),
        }
    }

    fn core_instance(&mut self, instance: &mut CoreInstance<'a>) -> Result<(), Error> {
        match &mut instance.kind {
            CoreInstanceKind::Instantiate { module, args } => {
                self.component_item_ref(module)?;
                for arg in args {
                    match &mut arg.kind {
                        CoreInstantiationArgKind::Instance(i) => {
                            self.core_item_ref(i)?;
                        }
                        CoreInstantiationArgKind::BundleOfExports(..) => {
                            unreachable!("should be expanded already");
                        }
                    }
                }
            }
            CoreInstanceKind::BundleOfExports(exports) => {
                for export in exports {
                    self.core_item_ref(&mut export.item)?;
                }
            }
        }
        Ok(())
    }

    fn instance(&mut self, instance: &mut Instance<'a>) -> Result<(), Error> {
        match &mut instance.kind {
            InstanceKind::Instantiate { component, args } => {
                self.component_item_ref(component)?;
                for arg in args {
                    match &mut arg.kind {
                        InstantiationArgKind::Item(e) => {
                            self.export(e)?;
                        }
                        InstantiationArgKind::BundleOfExports(..) => {
                            unreachable!("should be expanded already")
                        }
                    }
                }
            }
            InstanceKind::BundleOfExports(exports) => {
                for export in exports {
                    self.export(&mut export.kind)?;
                }
            }
            InstanceKind::Import { .. } => {
                unreachable!("should be expanded already")
            }
        }
        Ok(())
    }

    fn item_sig(&mut self, item: &mut ItemSig<'a>) -> Result<(), Error> {
        match &mut item.kind {
            // Here we must be explicit otherwise the module type reference will
            // be assumed to be in the component type namespace
            ItemSigKind::CoreModule(t) => self.core_type_use(t),
            ItemSigKind::Func(t) => self.component_type_use(t),
            ItemSigKind::Component(t) => self.component_type_use(t),
            ItemSigKind::Instance(t) => self.component_type_use(t),
            ItemSigKind::Value(t) => self.component_val_type(&mut t.0),
            ItemSigKind::Type(b) => match b {
                TypeBounds::Eq(i) => self.resolve_ns(i, Ns::Type),
                TypeBounds::SubResource => Ok(()),
            },
        }
    }

    fn export(&mut self, kind: &mut ComponentExportKind<'a>) -> Result<(), Error> {
        match kind {
            // Here we do *not* have to be explicit as the item ref is to a core module
            ComponentExportKind::CoreModule(r) => self.component_item_ref(r),
            ComponentExportKind::Func(r) => self.component_item_ref(r),
            ComponentExportKind::Value(r) => self.component_item_ref(r),
            ComponentExportKind::Type(r) => self.component_item_ref(r),
            ComponentExportKind::Component(r) => self.component_item_ref(r),
            ComponentExportKind::Instance(r) => self.component_item_ref(r),
        }
    }

    fn start(&mut self, start: &mut Start<'a>) -> Result<(), Error> {
        self.resolve_ns(&mut start.func, Ns::Func)?;
        for arg in start.args.iter_mut() {
            self.component_item_ref(arg)?;
        }
        Ok(())
    }

    fn outer_alias<T: Into<Ns>>(
        &mut self,
        outer: &mut Index<'a>,
        index: &mut Index<'a>,
        kind: T,
        span: Span,
        enclosing_only: bool,
    ) -> Result<(), Error> {
        // Short-circuit when both indices are already resolved as this
        // helps to write tests for invalid modules where wasmparser should
        // be the one returning the error.
        if let Index::Num(..) = outer {
            if let Index::Num(..) = index {
                return Ok(());
            }
        }

        // Resolve `outer`, and compute the depth at which to look up
        // `index`.
        let depth = match outer {
            Index::Id(id) => {
                let mut depth = 0;
                for resolver in self.stack.iter().rev() {
                    if resolver.id == Some(*id) {
                        break;
                    }
                    depth += 1;
                }
                if depth as usize == self.stack.len() {
                    return Err(Error::new(
                        span,
                        format!("outer component `{}` not found", id.name()),
                    ));
                }
                depth
            }
            Index::Num(n, _span) => *n,
        };

        if depth as usize >= self.stack.len() {
            return Err(Error::new(
                span,
                format!("outer count of `{}` is too large", depth),
            ));
        }

        if enclosing_only && depth > 1 {
            return Err(Error::new(
                span,
                "only the local or enclosing scope can be aliased".to_string(),
            ));
        }

        *outer = Index::Num(depth, span);

        // Resolve `index` within the computed scope depth.
        let computed = self.stack.len() - 1 - depth as usize;
        self.stack[computed].resolve(kind.into(), index)?;

        Ok(())
    }

    fn alias(&mut self, alias: &mut Alias<'a>, enclosing_only: bool) -> Result<(), Error> {
        match &mut alias.target {
            AliasTarget::Export {
                instance,
                name: _,
                kind: _,
            } => self.resolve_ns(instance, Ns::Instance),
            AliasTarget::CoreExport {
                instance,
                name: _,
                kind: _,
            } => self.resolve_ns(instance, Ns::CoreInstance),
            AliasTarget::Outer { outer, index, kind } => {
                self.outer_alias(outer, index, *kind, alias.span, enclosing_only)
            }
        }
    }

    fn canonical_func(&mut self, func: &mut CanonicalFunc<'a>) -> Result<(), Error> {
        let opts = match &mut func.kind {
            CanonicalFuncKind::Lift { ty, info } => {
                self.component_type_use(ty)?;
                self.core_item_ref(&mut info.func)?;
                &mut info.opts
            }
            CanonicalFuncKind::Lower(info) => {
                self.component_item_ref(&mut info.func)?;
                &mut info.opts
            }
            CanonicalFuncKind::ResourceNew(info) => return self.resolve_ns(&mut info.ty, Ns::Type),
            CanonicalFuncKind::ResourceRep(info) => return self.resolve_ns(&mut info.ty, Ns::Type),
            CanonicalFuncKind::ResourceDrop(info) => {
                return self.resolve_ns(&mut info.ty, Ns::Type)
            }
        };

        for opt in opts {
            match opt {
                CanonOpt::StringUtf8 | CanonOpt::StringUtf16 | CanonOpt::StringLatin1Utf16 => {}
                CanonOpt::Memory(r) => self.core_item_ref(r)?,
                CanonOpt::Realloc(r) | CanonOpt::PostReturn(r) => self.core_item_ref(r)?,
            }
        }

        Ok(())
    }

    fn core_type_use<T>(&mut self, ty: &mut CoreTypeUse<'a, T>) -> Result<(), Error> {
        let item = match ty {
            CoreTypeUse::Ref(r) => r,
            CoreTypeUse::Inline(_) => {
                unreachable!("inline type-use should be expanded by now")
            }
        };
        self.core_item_ref(item)
    }

    fn component_type_use<T>(&mut self, ty: &mut ComponentTypeUse<'a, T>) -> Result<(), Error> {
        let item = match ty {
            ComponentTypeUse::Ref(r) => r,
            ComponentTypeUse::Inline(_) => {
                unreachable!("inline type-use should be expanded by now")
            }
        };
        self.component_item_ref(item)
    }

    fn defined_type(&mut self, ty: &mut ComponentDefinedType<'a>) -> Result<(), Error> {
        match ty {
            ComponentDefinedType::Primitive(_) => {}
            ComponentDefinedType::Flags(_) => {}
            ComponentDefinedType::Enum(_) => {}
            ComponentDefinedType::Record(r) => {
                for field in r.fields.iter_mut() {
                    self.component_val_type(&mut field.ty)?;
                }
            }
            ComponentDefinedType::Variant(v) => {
                // Namespace for case identifier resolution
                let mut ns = Namespace::default();
                for case in v.cases.iter_mut() {
                    let index = ns.register(case.id, "variant case")?;

                    if let Some(ty) = &mut case.ty {
                        self.component_val_type(ty)?;
                    }

                    if let Some(refines) = &mut case.refines {
                        if let Refinement::Index(span, idx) = refines {
                            let resolved = ns.resolve(idx, "variant case")?;
                            if resolved == index {
                                return Err(Error::new(
                                    *span,
                                    "variant case cannot refine itself".to_string(),
                                ));
                            }

                            *refines = Refinement::Resolved(resolved);
                        }
                    }
                }
            }
            ComponentDefinedType::List(l) => {
                self.component_val_type(&mut l.element)?;
            }
            ComponentDefinedType::Tuple(t) => {
                for field in t.fields.iter_mut() {
                    self.component_val_type(field)?;
                }
            }
            ComponentDefinedType::Option(o) => {
                self.component_val_type(&mut o.element)?;
            }
            ComponentDefinedType::Result(r) => {
                if let Some(ty) = &mut r.ok {
                    self.component_val_type(ty)?;
                }

                if let Some(ty) = &mut r.err {
                    self.component_val_type(ty)?;
                }
            }
            ComponentDefinedType::Own(t) | ComponentDefinedType::Borrow(t) => {
                self.resolve_ns(t, Ns::Type)?;
            }
        }
        Ok(())
    }

    fn component_val_type(&mut self, ty: &mut ComponentValType<'a>) -> Result<(), Error> {
        match ty {
            ComponentValType::Ref(idx) => self.resolve_ns(idx, Ns::Type),
            ComponentValType::Inline(ComponentDefinedType::Primitive(_)) => Ok(()),
            ComponentValType::Inline(_) => unreachable!("should be expanded by now"),
        }
    }

    fn core_ty(&mut self, field: &mut CoreType<'a>) -> Result<(), Error> {
        match &mut field.def {
            CoreTypeDef::Def(_) => {}
            CoreTypeDef::Module(t) => {
                self.stack.push(ComponentState::new(field.id));
                self.module_type(t)?;
                self.stack.pop();
            }
        }
        Ok(())
    }

    fn ty(&mut self, field: &mut Type<'a>) -> Result<(), Error> {
        match &mut field.def {
            TypeDef::Defined(t) => {
                self.defined_type(t)?;
            }
            TypeDef::Func(f) => {
                for param in f.params.iter_mut() {
                    self.component_val_type(&mut param.ty)?;
                }

                for result in f.results.iter_mut() {
                    self.component_val_type(&mut result.ty)?;
                }
            }
            TypeDef::Component(c) => {
                self.stack.push(ComponentState::new(field.id));
                self.component_type(c)?;
                self.stack.pop();
            }
            TypeDef::Instance(i) => {
                self.stack.push(ComponentState::new(field.id));
                self.instance_type(i)?;
                self.stack.pop();
            }
            TypeDef::Resource(r) => {
                match &mut r.rep {
                    ValType::I32 | ValType::I64 | ValType::F32 | ValType::F64 | ValType::V128 => {}
                    ValType::Ref(r) => match &mut r.heap {
                        core::HeapType::Func
                        | core::HeapType::Extern
                        | core::HeapType::Any
                        | core::HeapType::Eq
                        | core::HeapType::Array
                        | core::HeapType::I31
                        | core::HeapType::Struct
                        | core::HeapType::None
                        | core::HeapType::NoFunc
                        | core::HeapType::NoExtern => {}
                        core::HeapType::Index(id) => {
                            self.resolve_ns(id, Ns::Type)?;
                        }
                    },
                }
                if let Some(dtor) = &mut r.dtor {
                    self.core_item_ref(dtor)?;
                }
            }
        }
        Ok(())
    }

    fn component_type(&mut self, c: &mut ComponentType<'a>) -> Result<(), Error> {
        self.resolve_prepending_aliases(
            &mut c.decls,
            |resolver, decl| match decl {
                ComponentTypeDecl::Alias(alias) => resolver.alias(alias, false),
                ComponentTypeDecl::CoreType(ty) => resolver.core_ty(ty),
                ComponentTypeDecl::Type(ty) => resolver.ty(ty),
                ComponentTypeDecl::Import(import) => resolver.item_sig(&mut import.item),
                ComponentTypeDecl::Export(export) => resolver.item_sig(&mut export.item),
            },
            |state, decl| {
                match decl {
                    ComponentTypeDecl::Alias(alias) => {
                        state.register_alias(alias)?;
                    }
                    ComponentTypeDecl::CoreType(ty) => {
                        state.core_types.register(ty.id, "core type")?;
                    }
                    ComponentTypeDecl::Type(ty) => {
                        state.types.register(ty.id, "type")?;
                    }
                    ComponentTypeDecl::Export(e) => {
                        state.register_item_sig(&e.item)?;
                    }
                    ComponentTypeDecl::Import(i) => {
                        state.register_item_sig(&i.item)?;
                    }
                }
                Ok(())
            },
        )
    }

    fn instance_type(&mut self, c: &mut InstanceType<'a>) -> Result<(), Error> {
        self.resolve_prepending_aliases(
            &mut c.decls,
            |resolver, decl| match decl {
                InstanceTypeDecl::Alias(alias) => resolver.alias(alias, false),
                InstanceTypeDecl::CoreType(ty) => resolver.core_ty(ty),
                InstanceTypeDecl::Type(ty) => resolver.ty(ty),
                InstanceTypeDecl::Export(export) => resolver.item_sig(&mut export.item),
            },
            |state, decl| {
                match decl {
                    InstanceTypeDecl::Alias(alias) => {
                        state.register_alias(alias)?;
                    }
                    InstanceTypeDecl::CoreType(ty) => {
                        state.core_types.register(ty.id, "core type")?;
                    }
                    InstanceTypeDecl::Type(ty) => {
                        state.types.register(ty.id, "type")?;
                    }
                    InstanceTypeDecl::Export(export) => {
                        state.register_item_sig(&export.item)?;
                    }
                }
                Ok(())
            },
        )
    }

    fn core_item_ref<K>(&mut self, item: &mut CoreItemRef<'a, K>) -> Result<(), Error>
    where
        K: CoreItem + Copy,
    {
        // Check for not being an instance export reference
        if item.export_name.is_none() {
            self.resolve_ns(&mut item.idx, item.kind.ns())?;
            return Ok(());
        }

        // This is a reference to a core instance export
        let mut index = item.idx;
        self.resolve_ns(&mut index, Ns::CoreInstance)?;

        // Record an alias to reference the export
        let span = item.idx.span();
        let alias = Alias {
            span,
            id: None,
            name: None,
            target: AliasTarget::CoreExport {
                instance: index,
                name: item.export_name.unwrap(),
                kind: item.kind.ns().into(),
            },
        };

        index = Index::Num(self.current().register_alias(&alias)?, span);
        self.aliases_to_insert.push(alias);

        item.idx = index;
        item.export_name = None;

        Ok(())
    }

    fn component_item_ref<K>(&mut self, item: &mut ItemRef<'a, K>) -> Result<(), Error>
    where
        K: ComponentItem + Copy,
    {
        // Check for not being an instance export reference
        if item.export_names.is_empty() {
            self.resolve_ns(&mut item.idx, item.kind.ns())?;
            return Ok(());
        }

        // This is a reference to an instance export
        let mut index = item.idx;
        self.resolve_ns(&mut index, Ns::Instance)?;

        let span = item.idx.span();
        for (pos, export_name) in item.export_names.iter().enumerate() {
            // Record an alias to reference the export
            let alias = Alias {
                span,
                id: None,
                name: None,
                target: AliasTarget::Export {
                    instance: index,
                    name: export_name,
                    kind: if pos == item.export_names.len() - 1 {
                        item.kind.ns().into()
                    } else {
                        ComponentExportAliasKind::Instance
                    },
                },
            };

            index = Index::Num(self.current().register_alias(&alias)?, span);
            self.aliases_to_insert.push(alias);
        }

        item.idx = index;
        item.export_names = Vec::new();

        Ok(())
    }

    fn resolve_ns(&mut self, idx: &mut Index<'a>, ns: Ns) -> Result<(), Error> {
        // Perform resolution on a local clone walking up the stack of components
        // that we have. Note that a local clone is used since we don't want to use
        // the parent's resolved index if a parent matches, instead we want to use
        // the index of the alias that we will automatically insert.
        let mut idx_clone = *idx;
        for (depth, resolver) in self.stack.iter_mut().rev().enumerate() {
            let depth = depth as u32;
            let found = match resolver.resolve(ns, &mut idx_clone) {
                Ok(idx) => idx,
                // Try the next parent
                Err(_) => continue,
            };

            // If this is the current component then no extra alias is necessary, so
            // return success.
            if depth == 0 {
                *idx = idx_clone;
                return Ok(());
            }
            let id = match idx {
                Index::Id(id) => *id,
                Index::Num(..) => unreachable!(),
            };

            // When resolution succeeds in a parent then an outer alias is
            // automatically inserted here in this component.
            let span = idx.span();
            let alias = Alias {
                span,
                id: Some(id),
                name: None,
                target: AliasTarget::Outer {
                    outer: Index::Num(depth, span),
                    index: Index::Num(found, span),
                    kind: ns.into(),
                },
            };
            let local_index = self.current().register_alias(&alias)?;
            self.aliases_to_insert.push(alias);
            *idx = Index::Num(local_index, span);
            return Ok(());
        }

        // If resolution in any parent failed then simply return the error from our
        // local namespace
        self.current().resolve(ns, idx)?;
        unreachable!()
    }

    fn module_type(&mut self, ty: &mut ModuleType<'a>) -> Result<(), Error> {
        return self.resolve_prepending_aliases(
            &mut ty.decls,
            |resolver, decl| match decl {
                ModuleTypeDecl::Alias(alias) => resolver.alias(alias, true),
                ModuleTypeDecl::Type(_) => Ok(()),
                ModuleTypeDecl::Import(import) => resolve_item_sig(resolver, &mut import.item),
                ModuleTypeDecl::Export(_, item) => resolve_item_sig(resolver, item),
            },
            |state, decl| {
                match decl {
                    ModuleTypeDecl::Alias(alias) => {
                        state.register_alias(alias)?;
                    }
                    ModuleTypeDecl::Type(ty) => {
                        state.core_types.register(ty.id, "type")?;
                    }
                    // Only the type namespace is populated within the module type
                    // namespace so these are ignored here.
                    ModuleTypeDecl::Import(_) | ModuleTypeDecl::Export(..) => {}
                }
                Ok(())
            },
        );

        fn resolve_item_sig<'a>(
            resolver: &Resolver<'a>,
            sig: &mut core::ItemSig<'a>,
        ) -> Result<(), Error> {
            match &mut sig.kind {
                core::ItemKind::Func(ty) | core::ItemKind::Tag(core::TagType::Exception(ty)) => {
                    let idx = ty.index.as_mut().expect("index should be filled in");
                    resolver
                        .stack
                        .last()
                        .unwrap()
                        .core_types
                        .resolve(idx, "type")?;
                }
                core::ItemKind::Memory(_)
                | core::ItemKind::Global(_)
                | core::ItemKind::Table(_) => {}
            }
            Ok(())
        }
    }
}

impl<'a> ComponentState<'a> {
    fn resolve(&mut self, ns: Ns, idx: &mut Index<'a>) -> Result<u32, Error> {
        match ns {
            Ns::CoreFunc => self.core_funcs.resolve(idx, "core func"),
            Ns::CoreGlobal => self.core_globals.resolve(idx, "core global"),
            Ns::CoreTable => self.core_tables.resolve(idx, "core table"),
            Ns::CoreMemory => self.core_memories.resolve(idx, "core memory"),
            Ns::CoreType => self.core_types.resolve(idx, "core type"),
            Ns::CoreTag => self.core_tags.resolve(idx, "core tag"),
            Ns::CoreInstance => self.core_instances.resolve(idx, "core instance"),
            Ns::CoreModule => self.core_modules.resolve(idx, "core module"),
            Ns::Func => self.funcs.resolve(idx, "func"),
            Ns::Type => self.types.resolve(idx, "type"),
            Ns::Instance => self.instances.resolve(idx, "instance"),
            Ns::Component => self.components.resolve(idx, "component"),
            Ns::Value => self.values.resolve(idx, "value"),
        }
    }

    /// Assign an index to the given field.
    fn register(&mut self, item: &ComponentField<'a>) -> Result<(), Error> {
        match item {
            ComponentField::CoreModule(m) => self.core_modules.register(m.id, "core module")?,
            ComponentField::CoreInstance(i) => {
                self.core_instances.register(i.id, "core instance")?
            }
            ComponentField::CoreType(t) => self.core_types.register(t.id, "core type")?,
            ComponentField::Component(c) => self.components.register(c.id, "component")?,
            ComponentField::Instance(i) => self.instances.register(i.id, "instance")?,
            ComponentField::Alias(a) => self.register_alias(a)?,
            ComponentField::Type(t) => self.types.register(t.id, "type")?,
            ComponentField::CanonicalFunc(f) => match &f.kind {
                CanonicalFuncKind::Lift { .. } => self.funcs.register(f.id, "func")?,
                CanonicalFuncKind::Lower(_)
                | CanonicalFuncKind::ResourceNew(_)
                | CanonicalFuncKind::ResourceRep(_)
                | CanonicalFuncKind::ResourceDrop(_) => {
                    self.core_funcs.register(f.id, "core func")?
                }
            },
            ComponentField::CoreFunc(_) | ComponentField::Func(_) => {
                unreachable!("should be expanded already")
            }
            ComponentField::Start(s) => {
                for r in &s.results {
                    self.values.register(*r, "value")?;
                }
                return Ok(());
            }
            ComponentField::Import(i) => self.register_item_sig(&i.item)?,
            ComponentField::Export(e) => match &e.kind {
                ComponentExportKind::CoreModule(_) => {
                    self.core_modules.register(e.id, "core module")?
                }
                ComponentExportKind::Func(_) => self.funcs.register(e.id, "func")?,
                ComponentExportKind::Instance(_) => self.instances.register(e.id, "instance")?,
                ComponentExportKind::Value(_) => self.values.register(e.id, "value")?,
                ComponentExportKind::Component(_) => self.components.register(e.id, "component")?,
                ComponentExportKind::Type(_) => self.types.register(e.id, "type")?,
            },
            ComponentField::Custom(_) | ComponentField::Producers(_) => return Ok(()),
        };

        Ok(())
    }

    fn register_alias(&mut self, alias: &Alias<'a>) -> Result<u32, Error> {
        match alias.target {
            AliasTarget::Export { kind, .. } => match kind {
                ComponentExportAliasKind::CoreModule => {
                    self.core_modules.register(alias.id, "core module")
                }
                ComponentExportAliasKind::Func => self.funcs.register(alias.id, "func"),
                ComponentExportAliasKind::Value => self.values.register(alias.id, "value"),
                ComponentExportAliasKind::Type => self.types.register(alias.id, "type"),
                ComponentExportAliasKind::Component => {
                    self.components.register(alias.id, "component")
                }
                ComponentExportAliasKind::Instance => self.instances.register(alias.id, "instance"),
            },
            AliasTarget::CoreExport { kind, .. } => match kind {
                core::ExportKind::Func => self.core_funcs.register(alias.id, "core func"),
                core::ExportKind::Table => self.core_tables.register(alias.id, "core table"),
                core::ExportKind::Memory => self.core_memories.register(alias.id, "core memory"),
                core::ExportKind::Global => self.core_globals.register(alias.id, "core global"),
                core::ExportKind::Tag => self.core_tags.register(alias.id, "core tag"),
            },
            AliasTarget::Outer { kind, .. } => match kind {
                ComponentOuterAliasKind::CoreModule => {
                    self.core_modules.register(alias.id, "core module")
                }
                ComponentOuterAliasKind::CoreType => {
                    self.core_types.register(alias.id, "core type")
                }
                ComponentOuterAliasKind::Type => self.types.register(alias.id, "type"),
                ComponentOuterAliasKind::Component => {
                    self.components.register(alias.id, "component")
                }
            },
        }
    }
}

#[derive(PartialEq, Eq, Hash, Copy, Clone, Debug)]
enum Ns {
    CoreFunc,
    CoreGlobal,
    CoreTable,
    CoreMemory,
    CoreType,
    CoreTag,
    CoreInstance,
    CoreModule,
    Func,
    Type,
    Instance,
    Component,
    Value,
}

trait ComponentItem {
    fn ns(&self) -> Ns;
}

trait CoreItem {
    fn ns(&self) -> Ns;
}

macro_rules! component_item {
    ($kw:path, $kind:ident) => {
        impl ComponentItem for $kw {
            fn ns(&self) -> Ns {
                Ns::$kind
            }
        }
    };
}

macro_rules! core_item {
    ($kw:path, $kind:ident) => {
        impl CoreItem for $kw {
            fn ns(&self) -> Ns {
                Ns::$kind
            }
        }
    };
}

component_item!(kw::func, Func);
component_item!(kw::r#type, Type);
component_item!(kw::r#instance, Instance);
component_item!(kw::component, Component);
component_item!(kw::value, Value);
component_item!(kw::module, CoreModule);

core_item!(kw::func, CoreFunc);
core_item!(kw::memory, CoreMemory);
core_item!(kw::r#type, CoreType);
core_item!(kw::r#instance, CoreInstance);

impl From<Ns> for ComponentExportAliasKind {
    fn from(ns: Ns) -> Self {
        match ns {
            Ns::CoreModule => Self::CoreModule,
            Ns::Func => Self::Func,
            Ns::Type => Self::Type,
            Ns::Instance => Self::Instance,
            Ns::Component => Self::Component,
            Ns::Value => Self::Value,
            _ => unreachable!("not a component exportable namespace"),
        }
    }
}

impl From<Ns> for ComponentOuterAliasKind {
    fn from(ns: Ns) -> Self {
        match ns {
            Ns::CoreModule => Self::CoreModule,
            Ns::CoreType => Self::CoreType,
            Ns::Type => Self::Type,
            Ns::Component => Self::Component,
            _ => unreachable!("not an outer alias namespace"),
        }
    }
}

impl From<Ns> for core::ExportKind {
    fn from(ns: Ns) -> Self {
        match ns {
            Ns::CoreFunc => Self::Func,
            Ns::CoreTable => Self::Table,
            Ns::CoreGlobal => Self::Global,
            Ns::CoreMemory => Self::Memory,
            Ns::CoreTag => Self::Tag,
            _ => unreachable!("not a core exportable namespace"),
        }
    }
}

impl From<ComponentOuterAliasKind> for Ns {
    fn from(kind: ComponentOuterAliasKind) -> Self {
        match kind {
            ComponentOuterAliasKind::CoreModule => Self::CoreModule,
            ComponentOuterAliasKind::CoreType => Self::CoreType,
            ComponentOuterAliasKind::Type => Self::Type,
            ComponentOuterAliasKind::Component => Self::Component,
        }
    }
}

impl CoreItem for core::ExportKind {
    fn ns(&self) -> Ns {
        match self {
            Self::Func => Ns::CoreFunc,
            Self::Table => Ns::CoreTable,
            Self::Global => Ns::CoreGlobal,
            Self::Memory => Ns::CoreMemory,
            Self::Tag => Ns::CoreTag,
        }
    }
}
