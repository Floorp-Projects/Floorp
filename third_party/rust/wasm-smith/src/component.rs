//! Generation of Wasm
//! [components](https://github.com/WebAssembly/component-model).

// FIXME(#1000): component support in `wasm-smith` is a work in progress.
#![allow(unused_variables, dead_code)]

use crate::{arbitrary_loop, Config, DefaultConfig};
use arbitrary::{Arbitrary, Result, Unstructured};
use std::collections::BTreeMap;
use std::convert::TryFrom;
use std::{
    collections::{HashMap, HashSet},
    marker,
    rc::Rc,
};
use wasm_encoder::{ComponentTypeRef, ComponentValType, PrimitiveValType, TypeBounds, ValType};

mod encode;

/// A pseudo-random WebAssembly [component].
///
/// Construct instances of this type with [the `Arbitrary`
/// trait](https://docs.rs/arbitrary/*/arbitrary/trait.Arbitrary.html).
///
/// [component]: https://github.com/WebAssembly/component-model/blob/ast-and-binary/design/MVP/Explainer.md
///
/// ## Configured Generated Components
///
/// This uses the [`DefaultConfig`][crate::DefaultConfig] configuration. If you
/// want to customize the shape of generated components, define your own
/// configuration type, implement the [`Config`][crate::Config] trait for it,
/// and use [`ConfiguredComponent<YourConfigType>`][crate::ConfiguredComponent]
/// instead of plain `Component`.
#[derive(Debug)]
pub struct Component {
    sections: Vec<Section>,
}

/// A builder to create a component (and possibly a whole tree of nested
/// components).
///
/// Maintains a stack of components we are currently building, as well as
/// metadata about them. The split between `Component` and `ComponentBuilder` is
/// that the builder contains metadata that is purely used when generating
/// components and is unnecessary after we are done generating the structure of
/// the components and only need to encode an already-generated component to
/// bytes.
#[derive(Debug)]
struct ComponentBuilder {
    config: Rc<dyn Config>,

    // The set of core `valtype`s that we are configured to generate.
    core_valtypes: Vec<ValType>,

    // Stack of types scopes that are currently available.
    //
    // There is an entry in this stack for each component, but there can also be
    // additional entries for module/component/instance types, each of which
    // have their own scope.
    //
    // This stack is always non-empty and the last entry is always the current
    // scope.
    //
    // When a particular scope can alias outer types, it can alias from any
    // scope that is older than it (i.e. `types_scope[i]` can alias from
    // `types_scope[j]` when `j <= i`).
    types: Vec<TypesScope>,

    // The set of components we are currently building and their associated
    // metadata.
    components: Vec<ComponentContext>,

    // Whether we are in the final bits of generating this component and we just
    // need to ensure that the minimum number of entities configured have all
    // been generated. This changes the behavior of various
    // `arbitrary_<section>` methods to always fill in their minimums.
    fill_minimums: bool,

    // Our maximums for these entities are applied across the whole component
    // tree, not per-component.
    total_components: usize,
    total_modules: usize,
    total_instances: usize,
    total_values: usize,
}

#[derive(Debug, Clone)]
enum ComponentOrCoreFuncType {
    Component(Rc<FuncType>),
    Core(Rc<crate::core::FuncType>),
}

impl ComponentOrCoreFuncType {
    fn as_core(&self) -> &Rc<crate::core::FuncType> {
        match self {
            ComponentOrCoreFuncType::Core(t) => t,
            ComponentOrCoreFuncType::Component(_) => panic!("not a core func type"),
        }
    }

    fn as_component(&self) -> &Rc<FuncType> {
        match self {
            ComponentOrCoreFuncType::Core(_) => panic!("not a component func type"),
            ComponentOrCoreFuncType::Component(t) => t,
        }
    }
}

#[derive(Debug, Clone)]
enum ComponentOrCoreInstanceType {
    Component(Rc<InstanceType>),
    Core(BTreeMap<String, crate::core::EntityType>),
}

/// Metadata (e.g. contents of various index spaces) we keep track of on a
/// per-component basis.
#[derive(Debug)]
struct ComponentContext {
    // The actual component itself.
    component: Component,

    // The number of imports we have generated thus far.
    num_imports: usize,

    // The set of names of imports we've generated thus far.
    import_names: HashSet<String>,

    // The set of URLs of imports we've generated thus far.
    import_urls: HashSet<String>,

    // This component's function index space.
    funcs: Vec<ComponentOrCoreFuncType>,

    // Which entries in `funcs` are component functions?
    component_funcs: Vec<u32>,

    // Which entries in `component_funcs` are component functions that only use scalar
    // types?
    scalar_component_funcs: Vec<u32>,

    // Which entries in `funcs` are core Wasm functions?
    //
    // Note that a component can't import core functions, so these entries will
    // never point to a `Section::Import`.
    core_funcs: Vec<u32>,

    // This component's component index space.
    //
    // An indirect list of all directly-nested (not transitive) components
    // inside this component.
    //
    // Each entry is of the form `(i, j)` where `component.sections[i]` is
    // guaranteed to be either
    //
    // * a `Section::Component` and we are referencing the component defined in
    //   that section (in this case `j` must also be `0`, since a component
    //   section can only contain a single nested component), or
    //
    // * a `Section::Import` and we are referencing the `j`th import in that
    //   section, which is guaranteed to be a component import.
    components: Vec<(usize, usize)>,

    // This component's module index space.
    //
    // An indirect list of all directly-nested (not transitive) modules
    // inside this component.
    //
    // Each entry is of the form `(i, j)` where `component.sections[i]` is
    // guaranteed to be either
    //
    // * a `Section::Core` and we are referencing the module defined in that
    //   section (in this case `j` must also be `0`, since a core section can
    //   only contain a single nested module), or
    //
    // * a `Section::Import` and we are referencing the `j`th import in that
    //   section, which is guaranteed to be a module import.
    modules: Vec<(usize, usize)>,

    // This component's instance index space.
    instances: Vec<ComponentOrCoreInstanceType>,

    // This component's value index space.
    values: Vec<ComponentValType>,
}

impl ComponentContext {
    fn empty() -> Self {
        ComponentContext {
            component: Component::empty(),
            num_imports: 0,
            import_names: HashSet::default(),
            import_urls: HashSet::default(),
            funcs: vec![],
            component_funcs: vec![],
            scalar_component_funcs: vec![],
            core_funcs: vec![],
            components: vec![],
            modules: vec![],
            instances: vec![],
            values: vec![],
        }
    }

    fn num_modules(&self) -> usize {
        self.modules.len()
    }

    fn num_components(&self) -> usize {
        self.components.len()
    }

    fn num_instances(&self) -> usize {
        self.instances.len()
    }

    fn num_funcs(&self) -> usize {
        self.funcs.len()
    }

    fn num_values(&self) -> usize {
        self.values.len()
    }
}

#[derive(Debug, Default)]
struct TypesScope {
    // All core types in this scope, regardless of kind.
    core_types: Vec<Rc<CoreType>>,

    // The indices of all the entries in `core_types` that are core function types.
    core_func_types: Vec<u32>,

    // The indices of all the entries in `core_types` that are module types.
    module_types: Vec<u32>,

    // All component types in this index space, regardless of kind.
    types: Vec<Rc<Type>>,

    // The indices of all the entries in `types` that are defined value types.
    defined_types: Vec<u32>,

    // The indices of all the entries in `types` that are func types.
    func_types: Vec<u32>,

    // A map from function types to their indices in the types space.
    func_type_to_indices: HashMap<Rc<FuncType>, Vec<u32>>,

    // The indices of all the entries in `types` that are component types.
    component_types: Vec<u32>,

    // The indices of all the entries in `types` that are instance types.
    instance_types: Vec<u32>,
}

impl TypesScope {
    fn push(&mut self, ty: Rc<Type>) -> u32 {
        let ty_idx = u32::try_from(self.types.len()).unwrap();

        let kind_list = match &*ty {
            Type::Defined(_) => &mut self.defined_types,
            Type::Func(func_ty) => {
                self.func_type_to_indices
                    .entry(func_ty.clone())
                    .or_default()
                    .push(ty_idx);
                &mut self.func_types
            }
            Type::Component(_) => &mut self.component_types,
            Type::Instance(_) => &mut self.instance_types,
        };
        kind_list.push(ty_idx);

        self.types.push(ty);
        ty_idx
    }

    fn push_core(&mut self, ty: Rc<CoreType>) -> u32 {
        let ty_idx = u32::try_from(self.core_types.len()).unwrap();

        let kind_list = match &*ty {
            CoreType::Func(_) => &mut self.core_func_types,
            CoreType::Module(_) => &mut self.module_types,
        };
        kind_list.push(ty_idx);

        self.core_types.push(ty);
        ty_idx
    }

    fn get(&self, index: u32) -> &Rc<Type> {
        &self.types[index as usize]
    }

    fn get_core(&self, index: u32) -> &Rc<CoreType> {
        &self.core_types[index as usize]
    }

    fn get_func(&self, index: u32) -> &Rc<FuncType> {
        match &**self.get(index) {
            Type::Func(f) => f,
            _ => panic!("get_func on non-function type"),
        }
    }

    fn can_ref_type(&self) -> bool {
        // All component types and core module types may be referenced
        !self.types.is_empty() || !self.module_types.is_empty()
    }
}

impl<'a> Arbitrary<'a> for Component {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self> {
        Ok(ConfiguredComponent::<DefaultConfig>::arbitrary(u)?.component)
    }
}

/// A pseudo-random generated Wasm component with custom configuration.
///
/// If you don't care about custom configuration, use
/// [`Component`][crate::Component] instead.
///
/// For details on configuring, see the [`Config`][crate::Config] trait.
#[derive(Debug)]
pub struct ConfiguredComponent<C> {
    /// The generated component, controlled by the configuration of `C` in the
    /// `Arbitrary` implementation.
    pub component: Component,
    _marker: marker::PhantomData<C>,
}

impl<'a, C> Arbitrary<'a> for ConfiguredComponent<C>
where
    C: Config + Arbitrary<'a>,
{
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self> {
        let config = C::arbitrary(u)?;
        let component = Component::new(config, u)?;
        Ok(ConfiguredComponent {
            component,
            _marker: marker::PhantomData,
        })
    }
}

#[derive(Default)]
struct EntityCounts {
    globals: usize,
    tables: usize,
    memories: usize,
    tags: usize,
    funcs: usize,
}

impl Component {
    /// Construct a new `Component` using the given configuration.
    pub fn new(config: impl Config, u: &mut Unstructured) -> Result<Self> {
        let mut builder = ComponentBuilder::new(Rc::new(config));
        builder.build(u)
    }

    fn empty() -> Self {
        Component { sections: vec![] }
    }
}

#[must_use]
enum Step {
    Finished(Component),
    StillBuilding,
}

impl Step {
    fn unwrap_still_building(self) {
        match self {
            Step::Finished(_) => panic!(
                "`Step::unwrap_still_building` called on a `Step` that is not `StillBuilding`"
            ),
            Step::StillBuilding => {}
        }
    }
}

impl ComponentBuilder {
    fn new(config: Rc<dyn Config>) -> Self {
        ComponentBuilder {
            config,
            core_valtypes: vec![],
            types: vec![Default::default()],
            components: vec![ComponentContext::empty()],
            fill_minimums: false,
            total_components: 0,
            total_modules: 0,
            total_instances: 0,
            total_values: 0,
        }
    }

    fn build(&mut self, u: &mut Unstructured) -> Result<Component> {
        self.core_valtypes = crate::core::configured_valtypes(&*self.config);

        let mut choices: Vec<fn(&mut ComponentBuilder, &mut Unstructured) -> Result<Step>> = vec![];

        loop {
            choices.clear();
            choices.push(Self::finish_component);

            // Only add any choice other than "finish what we've generated thus
            // far" when there is more arbitrary fuzzer data for us to consume.
            if !u.is_empty() {
                choices.push(Self::arbitrary_custom_section);

                // NB: we add each section as a choice even if we've already
                // generated our maximum number of entities in that section so that
                // we can exercise adding empty sections to the end of the module.
                choices.push(Self::arbitrary_core_type_section);
                choices.push(Self::arbitrary_type_section);
                choices.push(Self::arbitrary_import_section);
                choices.push(Self::arbitrary_canonical_section);

                if self.total_modules < self.config.max_modules() {
                    choices.push(Self::arbitrary_core_module_section);
                }

                if self.components.len() < self.config.max_nesting_depth()
                    && self.total_components < self.config.max_components()
                {
                    choices.push(Self::arbitrary_component_section);
                }

                // FIXME(#1000)
                //
                // choices.push(Self::arbitrary_instance_section);
                // choices.push(Self::arbitrary_export_section);
                // choices.push(Self::arbitrary_start_section);
                // choices.push(Self::arbitrary_alias_section);
            }

            let f = u.choose(&choices)?;
            match f(self, u)? {
                Step::StillBuilding => {}
                Step::Finished(component) => {
                    if self.components.is_empty() {
                        // If we just finished the root component, then return it.
                        return Ok(component);
                    } else {
                        // Otherwise, add it as a nested component in the parent.
                        self.push_section(Section::Component(component));
                    }
                }
            }
        }
    }

    fn finish_component(&mut self, u: &mut Unstructured) -> Result<Step> {
        // Ensure we've generated all of our minimums.
        self.fill_minimums = true;
        {
            if self.current_type_scope().types.len() < self.config.min_types() {
                self.arbitrary_type_section(u)?.unwrap_still_building();
            }
            if self.component().num_imports < self.config.min_imports() {
                self.arbitrary_import_section(u)?.unwrap_still_building();
            }
            if self.component().funcs.len() < self.config.min_funcs() {
                self.arbitrary_canonical_section(u)?.unwrap_still_building();
            }
        }
        self.fill_minimums = false;

        self.types
            .pop()
            .expect("should have a types scope for the component we are finishing");
        Ok(Step::Finished(self.components.pop().unwrap().component))
    }

    fn config(&self) -> &dyn Config {
        &*self.config
    }

    fn component(&self) -> &ComponentContext {
        self.components.last().unwrap()
    }

    fn component_mut(&mut self) -> &mut ComponentContext {
        self.components.last_mut().unwrap()
    }

    fn last_section(&self) -> Option<&Section> {
        self.component().component.sections.last()
    }

    fn last_section_mut(&mut self) -> Option<&mut Section> {
        self.component_mut().component.sections.last_mut()
    }

    fn push_section(&mut self, section: Section) {
        self.component_mut().component.sections.push(section);
    }

    fn ensure_section(
        &mut self,
        mut predicate: impl FnMut(&Section) -> bool,
        mut make_section: impl FnMut() -> Section,
    ) -> &mut Section {
        match self.last_section() {
            Some(sec) if predicate(sec) => {}
            _ => self.push_section(make_section()),
        }
        self.last_section_mut().unwrap()
    }

    fn arbitrary_custom_section(&mut self, u: &mut Unstructured) -> Result<Step> {
        self.push_section(Section::Custom(u.arbitrary()?));
        Ok(Step::StillBuilding)
    }

    fn push_type(&mut self, ty: Rc<Type>) -> u32 {
        match self.ensure_section(
            |s| matches!(s, Section::Type(_)),
            || Section::Type(TypeSection { types: vec![] }),
        ) {
            Section::Type(TypeSection { types }) => {
                types.push(ty.clone());
                self.current_type_scope_mut().push(ty)
            }
            _ => unreachable!(),
        }
    }

    fn push_core_type(&mut self, ty: Rc<CoreType>) -> u32 {
        match self.ensure_section(
            |s| matches!(s, Section::CoreType(_)),
            || Section::CoreType(CoreTypeSection { types: vec![] }),
        ) {
            Section::CoreType(CoreTypeSection { types }) => {
                types.push(ty.clone());
                self.current_type_scope_mut().push_core(ty)
            }
            _ => unreachable!(),
        }
    }

    fn arbitrary_core_type_section(&mut self, u: &mut Unstructured) -> Result<Step> {
        self.push_section(Section::CoreType(CoreTypeSection { types: vec![] }));

        let min = if self.fill_minimums {
            self.config
                .min_types()
                .saturating_sub(self.current_type_scope().types.len())
        } else {
            0
        };

        let max = self.config.max_types() - self.current_type_scope().types.len();

        arbitrary_loop(u, min, max, |u| {
            let mut type_fuel = self.config.max_type_size();
            let ty = self.arbitrary_core_type(u, &mut type_fuel)?;
            self.push_core_type(ty);
            Ok(true)
        })?;

        Ok(Step::StillBuilding)
    }

    fn arbitrary_core_type(
        &self,
        u: &mut Unstructured,
        type_fuel: &mut u32,
    ) -> Result<Rc<CoreType>> {
        *type_fuel = type_fuel.saturating_sub(1);
        if *type_fuel == 0 {
            return Ok(Rc::new(CoreType::Module(Rc::new(ModuleType::default()))));
        }

        let ty = match u.int_in_range::<u8>(0..=1)? {
            0 => CoreType::Func(crate::core::arbitrary_func_type(
                u,
                &self.core_valtypes,
                if self.config.multi_value_enabled() {
                    None
                } else {
                    Some(1)
                },
            )?),
            1 => CoreType::Module(self.arbitrary_module_type(u, type_fuel)?),
            _ => unreachable!(),
        };
        Ok(Rc::new(ty))
    }

    fn arbitrary_type_section(&mut self, u: &mut Unstructured) -> Result<Step> {
        self.push_section(Section::Type(TypeSection { types: vec![] }));

        let min = if self.fill_minimums {
            self.config
                .min_types()
                .saturating_sub(self.current_type_scope().types.len())
        } else {
            0
        };

        let max = self.config.max_types() - self.current_type_scope().types.len();

        arbitrary_loop(u, min, max, |u| {
            let mut type_fuel = self.config.max_type_size();
            let ty = self.arbitrary_type(u, &mut type_fuel)?;
            self.push_type(ty);
            Ok(true)
        })?;

        Ok(Step::StillBuilding)
    }

    fn arbitrary_type_ref<'a>(
        &self,
        u: &mut Unstructured<'a>,
        for_import: bool,
        for_type_def: bool,
    ) -> Result<Option<ComponentTypeRef>> {
        let mut choices: Vec<fn(&Self, &mut Unstructured) -> Result<ComponentTypeRef>> = Vec::new();
        let scope = self.current_type_scope();

        if !scope.module_types.is_empty()
            && (for_type_def || !for_import || self.total_modules < self.config.max_modules())
        {
            choices.push(|me, u| {
                Ok(ComponentTypeRef::Module(
                    *u.choose(&me.current_type_scope().module_types)?,
                ))
            });
        }

        // Types cannot be imported currently
        if !for_import
            && !scope.types.is_empty()
            && (for_type_def || scope.types.len() < self.config.max_types())
        {
            choices.push(|me, u| {
                Ok(ComponentTypeRef::Type(TypeBounds::Eq(u.int_in_range(
                    0..=u32::try_from(me.current_type_scope().types.len() - 1).unwrap(),
                )?)))
            });
        }

        // TODO: wasm-smith needs to ensure that every arbitrary value gets used exactly once.
        //       until that time, don't import values
        // if for_type_def || !for_import || self.total_values < self.config.max_values() {
        //     choices.push(|me, u| Ok(ComponentTypeRef::Value(me.arbitrary_component_val_type(u)?)));
        // }

        if !scope.func_types.is_empty()
            && (for_type_def
                || !for_import
                || self.component().num_funcs() < self.config.max_funcs())
        {
            choices.push(|me, u| {
                Ok(ComponentTypeRef::Func(
                    *u.choose(&me.current_type_scope().func_types)?,
                ))
            });
        }

        if !scope.component_types.is_empty()
            && (for_type_def || !for_import || self.total_components < self.config.max_components())
        {
            choices.push(|me, u| {
                Ok(ComponentTypeRef::Component(
                    *u.choose(&me.current_type_scope().component_types)?,
                ))
            });
        }

        if !scope.instance_types.is_empty()
            && (for_type_def || !for_import || self.total_instances < self.config.max_instances())
        {
            choices.push(|me, u| {
                Ok(ComponentTypeRef::Instance(
                    *u.choose(&me.current_type_scope().instance_types)?,
                ))
            });
        }

        if choices.is_empty() {
            return Ok(None);
        }

        let f = u.choose(&choices)?;
        f(self, u).map(Option::Some)
    }

    fn arbitrary_type(&mut self, u: &mut Unstructured, type_fuel: &mut u32) -> Result<Rc<Type>> {
        *type_fuel = type_fuel.saturating_sub(1);
        if *type_fuel == 0 {
            return Ok(Rc::new(Type::Defined(
                self.arbitrary_defined_type(u, type_fuel)?,
            )));
        }

        let ty = match u.int_in_range::<u8>(0..=3)? {
            0 => Type::Defined(self.arbitrary_defined_type(u, type_fuel)?),
            1 => Type::Func(self.arbitrary_func_type(u, type_fuel)?),
            2 => Type::Component(self.arbitrary_component_type(u, type_fuel)?),
            3 => Type::Instance(self.arbitrary_instance_type(u, type_fuel)?),
            _ => unreachable!(),
        };
        Ok(Rc::new(ty))
    }

    fn arbitrary_module_type(
        &self,
        u: &mut Unstructured,
        type_fuel: &mut u32,
    ) -> Result<Rc<ModuleType>> {
        let mut defs = vec![];
        let mut has_memory = false;
        let mut has_canonical_abi_realloc = false;
        let mut has_canonical_abi_free = false;
        let mut types: Vec<Rc<crate::core::FuncType>> = vec![];
        let mut imports = HashMap::new();
        let mut exports = HashSet::new();
        let mut counts = EntityCounts::default();

        // Special case the canonical ABI functions since certain types can only
        // be passed across the component boundary if they exist and
        // randomly generating them is extremely unlikely.

        // `memory`
        if counts.memories < self.config.max_memories() && u.ratio::<u8>(99, 100)? {
            defs.push(ModuleTypeDef::Export(
                "memory".into(),
                crate::core::EntityType::Memory(self.arbitrary_core_memory_type(u)?),
            ));
            exports.insert("memory".into());
            counts.memories += 1;
            has_memory = true;
        }

        // `canonical_abi_realloc`
        if counts.funcs < self.config.max_funcs()
            && types.len() < self.config.max_types()
            && u.ratio::<u8>(99, 100)?
        {
            let realloc_ty = Rc::new(crate::core::FuncType {
                params: vec![ValType::I32, ValType::I32, ValType::I32, ValType::I32],
                results: vec![ValType::I32],
            });
            let ty_idx = u32::try_from(types.len()).unwrap();
            types.push(realloc_ty.clone());
            defs.push(ModuleTypeDef::TypeDef(crate::core::Type::Func(
                realloc_ty.clone(),
            )));
            defs.push(ModuleTypeDef::Export(
                "canonical_abi_realloc".into(),
                crate::core::EntityType::Func(ty_idx, realloc_ty),
            ));
            exports.insert("canonical_abi_realloc".into());
            counts.funcs += 1;
            has_canonical_abi_realloc = true;
        }

        // `canonical_abi_free`
        if counts.funcs < self.config.max_funcs()
            && types.len() < self.config.max_types()
            && u.ratio::<u8>(99, 100)?
        {
            let free_ty = Rc::new(crate::core::FuncType {
                params: vec![ValType::I32, ValType::I32, ValType::I32],
                results: vec![],
            });
            let ty_idx = u32::try_from(types.len()).unwrap();
            types.push(free_ty.clone());
            defs.push(ModuleTypeDef::TypeDef(crate::core::Type::Func(
                free_ty.clone(),
            )));
            defs.push(ModuleTypeDef::Export(
                "canonical_abi_free".into(),
                crate::core::EntityType::Func(ty_idx, free_ty),
            ));
            exports.insert("canonical_abi_free".into());
            counts.funcs += 1;
            has_canonical_abi_free = true;
        }

        let mut entity_choices: Vec<
            fn(
                &ComponentBuilder,
                &mut Unstructured,
                &mut EntityCounts,
                &[Rc<crate::core::FuncType>],
            ) -> Result<crate::core::EntityType>,
        > = Vec::with_capacity(5);

        arbitrary_loop(u, 0, 100, |u| {
            *type_fuel = type_fuel.saturating_sub(1);
            if *type_fuel == 0 {
                return Ok(false);
            }

            let max_choice = if types.len() < self.config.max_types() {
                // Check if the parent scope has core function types to alias
                if !types.is_empty()
                    || (!self.types.is_empty()
                        && !self.types.last().unwrap().core_func_types.is_empty())
                {
                    // Imports, exports, types, and aliases
                    3
                } else {
                    // Imports, exports, and types
                    2
                }
            } else {
                // Imports and exports
                1
            };

            match u.int_in_range::<u8>(0..=max_choice)? {
                // Import.
                0 => {
                    let module = crate::limited_string(100, u)?;
                    let existing_module_imports = imports.entry(module.clone()).or_default();
                    let field = crate::unique_string(100, existing_module_imports, u)?;
                    let entity_type = match self.arbitrary_core_entity_type(
                        u,
                        &types,
                        &mut entity_choices,
                        &mut counts,
                    )? {
                        None => return Ok(false),
                        Some(x) => x,
                    };
                    defs.push(ModuleTypeDef::Import(crate::core::Import {
                        module,
                        field,
                        entity_type,
                    }));
                }

                // Export.
                1 => {
                    let name = crate::unique_string(100, &mut exports, u)?;
                    let entity_ty = match self.arbitrary_core_entity_type(
                        u,
                        &types,
                        &mut entity_choices,
                        &mut counts,
                    )? {
                        None => return Ok(false),
                        Some(x) => x,
                    };
                    defs.push(ModuleTypeDef::Export(name, entity_ty));
                }

                // Type definition.
                2 => {
                    let ty = crate::core::arbitrary_func_type(
                        u,
                        &self.core_valtypes,
                        if self.config.multi_value_enabled() {
                            None
                        } else {
                            Some(1)
                        },
                    )?;
                    types.push(ty.clone());
                    defs.push(ModuleTypeDef::TypeDef(crate::core::Type::Func(ty)));
                }

                // Alias
                3 => {
                    let (count, index, kind) = self.arbitrary_outer_core_type_alias(u, &types)?;
                    let ty = match &kind {
                        CoreOuterAliasKind::Type(ty) => ty.clone(),
                    };
                    types.push(ty);
                    defs.push(ModuleTypeDef::OuterAlias {
                        count,
                        i: index,
                        kind,
                    });
                }

                _ => unreachable!(),
            }

            Ok(true)
        })?;

        Ok(Rc::new(ModuleType {
            defs,
            has_memory,
            has_canonical_abi_realloc,
            has_canonical_abi_free,
        }))
    }

    fn arbitrary_core_entity_type(
        &self,
        u: &mut Unstructured,
        types: &[Rc<crate::core::FuncType>],
        choices: &mut Vec<
            fn(
                &ComponentBuilder,
                &mut Unstructured,
                &mut EntityCounts,
                &[Rc<crate::core::FuncType>],
            ) -> Result<crate::core::EntityType>,
        >,
        counts: &mut EntityCounts,
    ) -> Result<Option<crate::core::EntityType>> {
        choices.clear();

        if counts.globals < self.config.max_globals() {
            choices.push(|c, u, counts, _types| {
                counts.globals += 1;
                Ok(crate::core::EntityType::Global(
                    c.arbitrary_core_global_type(u)?,
                ))
            });
        }

        if counts.tables < self.config.max_tables() {
            choices.push(|c, u, counts, _types| {
                counts.tables += 1;
                Ok(crate::core::EntityType::Table(
                    c.arbitrary_core_table_type(u)?,
                ))
            });
        }

        if counts.memories < self.config.max_memories() {
            choices.push(|c, u, counts, _types| {
                counts.memories += 1;
                Ok(crate::core::EntityType::Memory(
                    c.arbitrary_core_memory_type(u)?,
                ))
            });
        }

        if types.iter().any(|ty| ty.results.is_empty())
            && self.config.exceptions_enabled()
            && counts.tags < self.config.max_tags()
        {
            choices.push(|c, u, counts, types| {
                counts.tags += 1;
                let tag_func_types = types
                    .iter()
                    .enumerate()
                    .filter(|(_, ty)| ty.results.is_empty())
                    .map(|(i, _)| u32::try_from(i).unwrap())
                    .collect::<Vec<_>>();
                Ok(crate::core::EntityType::Tag(
                    crate::core::arbitrary_tag_type(u, &tag_func_types, |idx| {
                        types[usize::try_from(idx).unwrap()].clone()
                    })?,
                ))
            });
        }

        if !types.is_empty() && counts.funcs < self.config.max_funcs() {
            choices.push(|c, u, counts, types| {
                counts.funcs += 1;
                let ty_idx = u.int_in_range(0..=u32::try_from(types.len() - 1).unwrap())?;
                let ty = types[ty_idx as usize].clone();
                Ok(crate::core::EntityType::Func(ty_idx, ty))
            });
        }

        if choices.is_empty() {
            return Ok(None);
        }

        let f = u.choose(choices)?;
        let ty = f(self, u, counts, types)?;
        Ok(Some(ty))
    }

    fn arbitrary_core_valtype(&self, u: &mut Unstructured) -> Result<ValType> {
        Ok(*u.choose(&self.core_valtypes)?)
    }

    fn arbitrary_core_global_type(&self, u: &mut Unstructured) -> Result<crate::core::GlobalType> {
        Ok(crate::core::GlobalType {
            val_type: self.arbitrary_core_valtype(u)?,
            mutable: u.arbitrary()?,
        })
    }

    fn arbitrary_core_table_type(&self, u: &mut Unstructured) -> Result<crate::core::TableType> {
        crate::core::arbitrary_table_type(u, self.config())
    }

    fn arbitrary_core_memory_type(&self, u: &mut Unstructured) -> Result<crate::core::MemoryType> {
        crate::core::arbitrary_memtype(u, self.config())
    }

    fn with_types_scope<T>(&mut self, f: impl FnOnce(&mut Self) -> Result<T>) -> Result<T> {
        self.types.push(Default::default());
        let result = f(self);
        self.types.pop();
        result
    }

    fn current_type_scope(&self) -> &TypesScope {
        self.types.last().unwrap()
    }

    fn current_type_scope_mut(&mut self) -> &mut TypesScope {
        self.types.last_mut().unwrap()
    }

    fn outer_types_scope(&self, count: u32) -> &TypesScope {
        &self.types[self.types.len() - 1 - usize::try_from(count).unwrap()]
    }

    fn outer_type(&self, count: u32, i: u32) -> &Rc<Type> {
        &self.outer_types_scope(count).types[usize::try_from(i).unwrap()]
    }

    fn arbitrary_component_type(
        &mut self,
        u: &mut Unstructured,
        type_fuel: &mut u32,
    ) -> Result<Rc<ComponentType>> {
        let mut defs = vec![];
        let mut imports = HashSet::new();
        let mut import_urls = HashSet::new();
        let mut exports = HashSet::new();
        let mut export_urls = HashSet::new();

        self.with_types_scope(|me| {
            arbitrary_loop(u, 0, 100, |u| {
                *type_fuel = type_fuel.saturating_sub(1);
                if *type_fuel == 0 {
                    return Ok(false);
                }

                if me.current_type_scope().can_ref_type() && u.int_in_range::<u8>(0..=3)? == 0 {
                    if let Some(ty) = me.arbitrary_type_ref(u, true, true)? {
                        // Imports.
                        let name = crate::unique_kebab_string(100, &mut imports, u)?;
                        let url = if u.arbitrary()? {
                            Some(crate::unique_url(100, &mut import_urls, u)?)
                        } else {
                            None
                        };
                        defs.push(ComponentTypeDef::Import(Import { name, url, ty }));
                        return Ok(true);
                    }

                    // Can't reference an arbitrary type, fallback to another definition.
                }

                // Type definitions, exports, and aliases.
                let def =
                    me.arbitrary_instance_type_def(u, &mut exports, &mut export_urls, type_fuel)?;
                defs.push(def.into());
                Ok(true)
            })
        })?;

        Ok(Rc::new(ComponentType { defs }))
    }

    fn arbitrary_instance_type(
        &mut self,
        u: &mut Unstructured,
        type_fuel: &mut u32,
    ) -> Result<Rc<InstanceType>> {
        let mut defs = vec![];
        let mut exports = HashSet::new();
        let mut export_urls = HashSet::new();

        self.with_types_scope(|me| {
            arbitrary_loop(u, 0, 100, |u| {
                *type_fuel = type_fuel.saturating_sub(1);
                if *type_fuel == 0 {
                    return Ok(false);
                }

                defs.push(me.arbitrary_instance_type_def(
                    u,
                    &mut exports,
                    &mut export_urls,
                    type_fuel,
                )?);
                Ok(true)
            })
        })?;

        Ok(Rc::new(InstanceType { defs }))
    }

    fn arbitrary_instance_type_def(
        &mut self,
        u: &mut Unstructured,
        exports: &mut HashSet<String>,
        export_urls: &mut HashSet<String>,
        type_fuel: &mut u32,
    ) -> Result<InstanceTypeDecl> {
        let mut choices: Vec<
            fn(
                &mut ComponentBuilder,
                &mut HashSet<String>,
                &mut HashSet<String>,
                &mut Unstructured,
                &mut u32,
            ) -> Result<InstanceTypeDecl>,
        > = Vec::with_capacity(3);

        // Export.
        if self.current_type_scope().can_ref_type() {
            choices.push(|me, exports, export_urls, u, _type_fuel| {
                let ty = me.arbitrary_type_ref(u, false, true)?.unwrap();
                if let ComponentTypeRef::Type(TypeBounds::Eq(idx)) = ty {
                    let ty = me.current_type_scope().get(idx).clone();
                    me.current_type_scope_mut().push(ty);
                }
                Ok(InstanceTypeDecl::Export {
                    name: crate::unique_kebab_string(100, exports, u)?,
                    url: if u.arbitrary()? {
                        Some(crate::unique_url(100, export_urls, u)?)
                    } else {
                        None
                    },
                    ty,
                })
            });
        }

        // Outer type alias.
        if self
            .types
            .iter()
            .any(|scope| !scope.types.is_empty() || !scope.core_types.is_empty())
        {
            choices.push(|me, _exports, _export_urls, u, _type_fuel| {
                let alias = me.arbitrary_outer_type_alias(u)?;
                match &alias {
                    Alias::Outer {
                        kind: OuterAliasKind::Type(ty),
                        ..
                    } => me.current_type_scope_mut().push(ty.clone()),
                    Alias::Outer {
                        kind: OuterAliasKind::CoreType(ty),
                        ..
                    } => me.current_type_scope_mut().push_core(ty.clone()),
                    _ => unreachable!(),
                };
                Ok(InstanceTypeDecl::Alias(alias))
            });
        }

        // Core type definition.
        choices.push(|me, _exports, _export_urls, u, type_fuel| {
            let ty = me.arbitrary_core_type(u, type_fuel)?;
            me.current_type_scope_mut().push_core(ty.clone());
            Ok(InstanceTypeDecl::CoreType(ty))
        });

        // Type definition.
        if self.types.len() < self.config.max_nesting_depth() {
            choices.push(|me, _exports, _export_urls, u, type_fuel| {
                let ty = me.arbitrary_type(u, type_fuel)?;
                me.current_type_scope_mut().push(ty.clone());
                Ok(InstanceTypeDecl::Type(ty))
            });
        }

        let f = u.choose(&choices)?;
        f(self, exports, export_urls, u, type_fuel)
    }

    fn arbitrary_outer_core_type_alias(
        &self,
        u: &mut Unstructured,
        local_types: &[Rc<crate::core::FuncType>],
    ) -> Result<(u32, u32, CoreOuterAliasKind)> {
        let enclosing_type_len = if !self.types.is_empty() {
            self.types.last().unwrap().core_func_types.len()
        } else {
            0
        };

        assert!(!local_types.is_empty() || enclosing_type_len > 0);

        let max = enclosing_type_len + local_types.len() - 1;
        let i = u.int_in_range(0..=max)?;
        let (count, index, ty) = if i < enclosing_type_len {
            let enclosing = self.types.last().unwrap();
            let index = enclosing.core_func_types[i];
            (
                1,
                index,
                match enclosing.get_core(index).as_ref() {
                    CoreType::Func(ty) => ty.clone(),
                    CoreType::Module(_) => unreachable!(),
                },
            )
        } else if i - enclosing_type_len < local_types.len() {
            let i = i - enclosing_type_len;
            (0, u32::try_from(i).unwrap(), local_types[i].clone())
        } else {
            unreachable!()
        };

        Ok((count, index, CoreOuterAliasKind::Type(ty)))
    }

    fn arbitrary_outer_type_alias(&self, u: &mut Unstructured) -> Result<Alias> {
        let non_empty_types_scopes: Vec<_> = self
            .types
            .iter()
            .rev()
            .enumerate()
            .filter(|(_, scope)| !scope.types.is_empty() || !scope.core_types.is_empty())
            .collect();
        assert!(
            !non_empty_types_scopes.is_empty(),
            "precondition: there are non-empty types scopes"
        );

        let (count, scope) = u.choose(&non_empty_types_scopes)?;
        let count = u32::try_from(*count).unwrap();
        assert!(!scope.types.is_empty() || !scope.core_types.is_empty());

        let max_type_in_scope = scope.types.len() + scope.core_types.len() - 1;
        let i = u.int_in_range(0..=max_type_in_scope)?;

        let (i, kind) = if i < scope.types.len() {
            let i = u32::try_from(i).unwrap();
            (i, OuterAliasKind::Type(Rc::clone(scope.get(i))))
        } else if i - scope.types.len() < scope.core_types.len() {
            let i = u32::try_from(i - scope.types.len()).unwrap();
            (i, OuterAliasKind::CoreType(Rc::clone(scope.get_core(i))))
        } else {
            unreachable!()
        };

        Ok(Alias::Outer { count, i, kind })
    }

    fn arbitrary_func_type(
        &self,
        u: &mut Unstructured,
        type_fuel: &mut u32,
    ) -> Result<Rc<FuncType>> {
        let mut params = Vec::new();
        let mut results = Vec::new();
        let mut names = HashSet::new();

        // Note: parameters are currently limited to a maximum of 16
        // because any additional parameters will require indirect access
        // via a pointer argument; when this occurs, validation of any
        // lowered function will fail because it will be missing a
        // memory option (not yet implemented).
        //
        // When options are correctly specified on canonical functions,
        // we should increase this maximum to test indirect parameter
        // passing.
        arbitrary_loop(u, 0, 16, |u| {
            *type_fuel = type_fuel.saturating_sub(1);
            if *type_fuel == 0 {
                return Ok(false);
            }

            let name = crate::unique_kebab_string(100, &mut names, u)?;
            let ty = self.arbitrary_component_val_type(u)?;

            params.push((name, ty));

            Ok(true)
        })?;

        names.clear();

        // Likewise, the limit for results is 1 before the memory option is
        // required. When the memory option is implemented, this restriction
        // should be relaxed.
        arbitrary_loop(u, 0, 1, |u| {
            *type_fuel = type_fuel.saturating_sub(1);
            if *type_fuel == 0 {
                return Ok(false);
            }

            // If the result list is empty (i.e. first push), then arbitrarily give
            // the result a name. Otherwise, all of the subsequent items must be named.
            let name = if results.is_empty() {
                // Most of the time we should have a single, unnamed result.
                u.ratio::<u8>(10, 100)?
                    .then(|| crate::unique_kebab_string(100, &mut names, u))
                    .transpose()?
            } else {
                Some(crate::unique_kebab_string(100, &mut names, u)?)
            };

            let ty = self.arbitrary_component_val_type(u)?;

            results.push((name, ty));

            // There can be only one unnamed result.
            if results.len() == 1 && results[0].0.is_none() {
                return Ok(false);
            }

            Ok(true)
        })?;

        Ok(Rc::new(FuncType { params, results }))
    }

    fn arbitrary_component_val_type(&self, u: &mut Unstructured) -> Result<ComponentValType> {
        let max_choices = if self.current_type_scope().defined_types.is_empty() {
            0
        } else {
            1
        };
        match u.int_in_range(0..=max_choices)? {
            0 => Ok(ComponentValType::Primitive(
                self.arbitrary_primitive_val_type(u)?,
            )),
            1 => {
                let index = *u.choose(&self.current_type_scope().defined_types)?;
                let ty = Rc::clone(self.current_type_scope().get(index));
                Ok(ComponentValType::Type(index))
            }
            _ => unreachable!(),
        }
    }

    fn arbitrary_primitive_val_type(&self, u: &mut Unstructured) -> Result<PrimitiveValType> {
        match u.int_in_range(0..=12)? {
            0 => Ok(PrimitiveValType::Bool),
            1 => Ok(PrimitiveValType::S8),
            2 => Ok(PrimitiveValType::U8),
            3 => Ok(PrimitiveValType::S16),
            4 => Ok(PrimitiveValType::U16),
            5 => Ok(PrimitiveValType::S32),
            6 => Ok(PrimitiveValType::U32),
            7 => Ok(PrimitiveValType::S64),
            8 => Ok(PrimitiveValType::U64),
            9 => Ok(PrimitiveValType::Float32),
            10 => Ok(PrimitiveValType::Float64),
            11 => Ok(PrimitiveValType::Char),
            12 => Ok(PrimitiveValType::String),
            _ => unreachable!(),
        }
    }

    fn arbitrary_record_type(
        &self,
        u: &mut Unstructured,
        type_fuel: &mut u32,
    ) -> Result<RecordType> {
        let mut fields = vec![];
        let mut field_names = HashSet::new();
        arbitrary_loop(u, 0, 100, |u| {
            *type_fuel = type_fuel.saturating_sub(1);
            if *type_fuel == 0 {
                return Ok(false);
            }

            let name = crate::unique_kebab_string(100, &mut field_names, u)?;
            let ty = self.arbitrary_component_val_type(u)?;

            fields.push((name, ty));
            Ok(true)
        })?;
        Ok(RecordType { fields })
    }

    fn arbitrary_variant_type(
        &self,
        u: &mut Unstructured,
        type_fuel: &mut u32,
    ) -> Result<VariantType> {
        let mut cases = vec![];
        let mut case_names = HashSet::new();
        arbitrary_loop(u, 1, 100, |u| {
            *type_fuel = type_fuel.saturating_sub(1);
            if *type_fuel == 0 {
                return Ok(false);
            }

            let name = crate::unique_kebab_string(100, &mut case_names, u)?;

            let ty = u
                .arbitrary::<bool>()?
                .then(|| self.arbitrary_component_val_type(u))
                .transpose()?;

            let refines = if !cases.is_empty() && u.arbitrary()? {
                let max_cases = u32::try_from(cases.len() - 1).unwrap();
                Some(u.int_in_range(0..=max_cases)?)
            } else {
                None
            };

            cases.push((name, ty, refines));
            Ok(true)
        })?;

        Ok(VariantType { cases })
    }

    fn arbitrary_list_type(&self, u: &mut Unstructured) -> Result<ListType> {
        Ok(ListType {
            elem_ty: self.arbitrary_component_val_type(u)?,
        })
    }

    fn arbitrary_tuple_type(&self, u: &mut Unstructured, type_fuel: &mut u32) -> Result<TupleType> {
        let mut fields = vec![];
        arbitrary_loop(u, 0, 100, |u| {
            *type_fuel = type_fuel.saturating_sub(1);
            if *type_fuel == 0 {
                return Ok(false);
            }

            fields.push(self.arbitrary_component_val_type(u)?);
            Ok(true)
        })?;
        Ok(TupleType { fields })
    }

    fn arbitrary_flags_type(&self, u: &mut Unstructured, type_fuel: &mut u32) -> Result<FlagsType> {
        let mut fields = vec![];
        let mut field_names = HashSet::new();
        arbitrary_loop(u, 0, 100, |u| {
            *type_fuel = type_fuel.saturating_sub(1);
            if *type_fuel == 0 {
                return Ok(false);
            }

            fields.push(crate::unique_kebab_string(100, &mut field_names, u)?);
            Ok(true)
        })?;
        Ok(FlagsType { fields })
    }

    fn arbitrary_enum_type(&self, u: &mut Unstructured, type_fuel: &mut u32) -> Result<EnumType> {
        let mut variants = vec![];
        let mut variant_names = HashSet::new();
        arbitrary_loop(u, 1, 100, |u| {
            *type_fuel = type_fuel.saturating_sub(1);
            if *type_fuel == 0 {
                return Ok(false);
            }

            variants.push(crate::unique_kebab_string(100, &mut variant_names, u)?);
            Ok(true)
        })?;
        Ok(EnumType { variants })
    }

    fn arbitrary_option_type(&self, u: &mut Unstructured) -> Result<OptionType> {
        Ok(OptionType {
            inner_ty: self.arbitrary_component_val_type(u)?,
        })
    }

    fn arbitrary_result_type(&self, u: &mut Unstructured) -> Result<ResultType> {
        Ok(ResultType {
            ok_ty: u
                .arbitrary::<bool>()?
                .then(|| self.arbitrary_component_val_type(u))
                .transpose()?,
            err_ty: u
                .arbitrary::<bool>()?
                .then(|| self.arbitrary_component_val_type(u))
                .transpose()?,
        })
    }

    fn arbitrary_defined_type(
        &self,
        u: &mut Unstructured,
        type_fuel: &mut u32,
    ) -> Result<DefinedType> {
        match u.int_in_range(0..=8)? {
            0 => Ok(DefinedType::Primitive(
                self.arbitrary_primitive_val_type(u)?,
            )),
            1 => Ok(DefinedType::Record(
                self.arbitrary_record_type(u, type_fuel)?,
            )),
            2 => Ok(DefinedType::Variant(
                self.arbitrary_variant_type(u, type_fuel)?,
            )),
            3 => Ok(DefinedType::List(self.arbitrary_list_type(u)?)),
            4 => Ok(DefinedType::Tuple(self.arbitrary_tuple_type(u, type_fuel)?)),
            5 => Ok(DefinedType::Flags(self.arbitrary_flags_type(u, type_fuel)?)),
            6 => Ok(DefinedType::Enum(self.arbitrary_enum_type(u, type_fuel)?)),
            7 => Ok(DefinedType::Option(self.arbitrary_option_type(u)?)),
            8 => Ok(DefinedType::Result(self.arbitrary_result_type(u)?)),
            _ => unreachable!(),
        }
    }

    fn push_import(&mut self, name: String, url: Option<String>, ty: ComponentTypeRef) {
        let nth = match self.ensure_section(
            |sec| matches!(sec, Section::Import(_)),
            || Section::Import(ImportSection { imports: vec![] }),
        ) {
            Section::Import(sec) => {
                sec.imports.push(Import { name, url, ty });
                sec.imports.len() - 1
            }
            _ => unreachable!(),
        };
        let section_index = self.component().component.sections.len() - 1;

        match ty {
            ComponentTypeRef::Module(_) => {
                self.total_modules += 1;
                self.component_mut().modules.push((section_index, nth));
            }
            ComponentTypeRef::Func(ty_index) => {
                let func_ty = match self.current_type_scope().get(ty_index).as_ref() {
                    Type::Func(ty) => ty.clone(),
                    _ => unreachable!(),
                };

                if func_ty.is_scalar() {
                    let func_index = u32::try_from(self.component().component_funcs.len()).unwrap();
                    self.component_mut().scalar_component_funcs.push(func_index);
                }

                let func_index = u32::try_from(self.component().funcs.len()).unwrap();
                self.component_mut()
                    .funcs
                    .push(ComponentOrCoreFuncType::Component(func_ty));

                self.component_mut().component_funcs.push(func_index);
            }
            ComponentTypeRef::Value(ty) => {
                self.total_values += 1;
                self.component_mut().values.push(ty);
            }
            ComponentTypeRef::Type(TypeBounds::Eq(ty_index)) => {
                let ty = self.current_type_scope().get(ty_index).clone();
                self.current_type_scope_mut().push(ty);
            }
            ComponentTypeRef::Type(TypeBounds::SubResource) => {
                unimplemented!()
            }
            ComponentTypeRef::Instance(ty_index) => {
                let instance_ty = match self.current_type_scope().get(ty_index).as_ref() {
                    Type::Instance(ty) => ty.clone(),
                    _ => unreachable!(),
                };

                self.total_instances += 1;
                self.component_mut()
                    .instances
                    .push(ComponentOrCoreInstanceType::Component(instance_ty));
            }
            ComponentTypeRef::Component(_) => {
                self.total_components += 1;
                self.component_mut().components.push((section_index, nth));
            }
        }
    }

    fn core_function_type(&self, core_func_index: u32) -> &Rc<crate::core::FuncType> {
        self.component().funcs[self.component().core_funcs[core_func_index as usize] as usize]
            .as_core()
    }

    fn component_function_type(&self, func_index: u32) -> &Rc<FuncType> {
        self.component().funcs[self.component().component_funcs[func_index as usize] as usize]
            .as_component()
    }

    fn push_func(&mut self, func: Func) {
        let nth = match self.component_mut().component.sections.last_mut() {
            Some(Section::Canonical(CanonicalSection { funcs })) => funcs.len(),
            _ => {
                self.push_section(Section::Canonical(CanonicalSection { funcs: vec![] }));
                0
            }
        };
        let section_index = self.component().component.sections.len() - 1;

        let func_index = u32::try_from(self.component().funcs.len()).unwrap();

        let ty = match &func {
            Func::CanonLift { func_ty, .. } => {
                let ty = Rc::clone(self.current_type_scope().get_func(*func_ty));
                if ty.is_scalar() {
                    let func_index = u32::try_from(self.component().component_funcs.len()).unwrap();
                    self.component_mut().scalar_component_funcs.push(func_index);
                }
                self.component_mut().component_funcs.push(func_index);
                ComponentOrCoreFuncType::Component(ty)
            }
            Func::CanonLower {
                func_index: comp_func_index,
                ..
            } => {
                let comp_func_ty = self.component_function_type(*comp_func_index);
                let core_func_ty = canonical_abi_for(comp_func_ty);
                self.component_mut().core_funcs.push(func_index);
                ComponentOrCoreFuncType::Core(core_func_ty)
            }
        };

        self.component_mut().funcs.push(ty);

        match self.component_mut().component.sections.last_mut() {
            Some(Section::Canonical(CanonicalSection { funcs })) => funcs.push(func),
            _ => unreachable!(),
        }
    }

    fn arbitrary_import_section(&mut self, u: &mut Unstructured) -> Result<Step> {
        self.push_section(Section::Import(ImportSection { imports: vec![] }));

        let min = if self.fill_minimums {
            self.config
                .min_imports()
                .saturating_sub(self.component().num_imports)
        } else {
            // Allow generating empty sections. We can always fill in the required
            // minimum later.
            0
        };
        let max = self.config.max_imports() - self.component().num_imports;

        crate::arbitrary_loop(u, min, max, |u| {
            match self.arbitrary_type_ref(u, true, false)? {
                Some(ty) => {
                    let name =
                        crate::unique_kebab_string(100, &mut self.component_mut().import_names, u)?;
                    let url = if u.arbitrary()? {
                        Some(crate::unique_url(
                            100,
                            &mut self.component_mut().import_urls,
                            u,
                        )?)
                    } else {
                        None
                    };
                    self.push_import(name, url, ty);
                    Ok(true)
                }
                None => Ok(false),
            }
        })?;

        Ok(Step::StillBuilding)
    }

    fn arbitrary_canonical_section(&mut self, u: &mut Unstructured) -> Result<Step> {
        self.push_section(Section::Canonical(CanonicalSection { funcs: vec![] }));

        let min = if self.fill_minimums {
            self.config
                .min_funcs()
                .saturating_sub(self.component().funcs.len())
        } else {
            // Allow generating empty sections. We can always fill in the
            // required minimum later.
            0
        };
        let max = self.config.max_funcs() - self.component().funcs.len();

        let mut choices: Vec<fn(&mut Unstructured, &mut ComponentBuilder) -> Result<Option<Func>>> =
            Vec::with_capacity(2);

        crate::arbitrary_loop(u, min, max, |u| {
            choices.clear();

            // NB: We only lift/lower scalar component functions.
            //
            // If we generated lifting and lowering of compound value types,
            // the probability of generating a corresponding Wasm module that
            // generates valid instances of the compound value types would
            // be vanishingly tiny (e.g. for `list<string>` we would have to
            // generate a core Wasm module that correctly produces a pointer and
            // length for a memory region that itself is a series of pointers
            // and lengths of valid strings, as well as `canonical_abi_realloc`
            // and `canonical_abi_free` functions that do the right thing).
            //
            // This is a pretty serious limitation of `wasm-smith`'s component
            // types support, but it is one we are intentionally
            // accepting. `wasm-smith` will focus on generating arbitrary
            // component sections, structures, and import/export topologies; not
            // component functions and core Wasm implementations of component
            // functions. In the future, we intend to build a new, distinct test
            // case generator specifically for exercising component functions
            // and the canonical ABI. This new generator won't emit arbitrary
            // component sections, structures, or import/export topologies, and
            // will instead leave that to `wasm-smith`.

            if !self.component().scalar_component_funcs.is_empty() {
                choices.push(|u, c| {
                    let func_index = *u.choose(&c.component().scalar_component_funcs)?;
                    Ok(Some(Func::CanonLower {
                        // Scalar component functions don't use any canonical options.
                        options: vec![],
                        func_index,
                    }))
                });
            }

            if !self.component().core_funcs.is_empty() {
                choices.push(|u, c| {
                    let core_func_index = u.int_in_range(
                        0..=u32::try_from(c.component().core_funcs.len() - 1).unwrap(),
                    )?;
                    let core_func_ty = c.core_function_type(core_func_index);
                    let comp_func_ty = inverse_scalar_canonical_abi_for(u, core_func_ty)?;

                    let func_ty = if let Some(indices) = c
                        .current_type_scope()
                        .func_type_to_indices
                        .get(&comp_func_ty)
                    {
                        // If we've already defined this component function type
                        // one or more times, then choose one of those
                        // definitions arbitrarily.
                        debug_assert!(!indices.is_empty());
                        *u.choose(indices)?
                    } else if c.current_type_scope().types.len() < c.config.max_types() {
                        // If we haven't already defined this component function
                        // type, and we haven't defined the configured maximum
                        // amount of types yet, then just define this type.
                        let ty = Rc::new(Type::Func(Rc::new(comp_func_ty)));
                        c.push_type(ty)
                    } else {
                        // Otherwise, give up on lifting this function.
                        return Ok(None);
                    };

                    Ok(Some(Func::CanonLift {
                        func_ty,
                        // Scalar functions don't use any canonical options.
                        options: vec![],
                        core_func_index,
                    }))
                });
            }

            if choices.is_empty() {
                return Ok(false);
            }

            let f = u.choose(&choices)?;
            if let Some(func) = f(u, self)? {
                self.push_func(func);
            }

            Ok(true)
        })?;

        Ok(Step::StillBuilding)
    }

    fn arbitrary_core_module_section(&mut self, u: &mut Unstructured) -> Result<Step> {
        let config: Rc<dyn Config> = Rc::clone(&self.config);
        let module = crate::core::Module::new_internal(
            config,
            u,
            crate::core::DuplicateImportsBehavior::Disallowed,
        )?;
        self.push_section(Section::CoreModule(module));
        self.total_modules += 1;
        Ok(Step::StillBuilding)
    }

    fn arbitrary_component_section(&mut self, u: &mut Unstructured) -> Result<Step> {
        self.types.push(TypesScope::default());
        self.components.push(ComponentContext::empty());
        self.total_components += 1;
        Ok(Step::StillBuilding)
    }

    fn arbitrary_instance_section(&mut self, u: &mut Unstructured) -> Result<()> {
        todo!()
    }

    fn arbitrary_export_section(&mut self, u: &mut Unstructured) -> Result<()> {
        todo!()
    }

    fn arbitrary_start_section(&mut self, u: &mut Unstructured) -> Result<()> {
        todo!()
    }

    fn arbitrary_alias_section(&mut self, u: &mut Unstructured) -> Result<()> {
        todo!()
    }
}

fn canonical_abi_for(func_ty: &FuncType) -> Rc<crate::core::FuncType> {
    let to_core_ty = |ty| match ty {
        ComponentValType::Primitive(prim_ty) => match prim_ty {
            PrimitiveValType::Char
            | PrimitiveValType::Bool
            | PrimitiveValType::S8
            | PrimitiveValType::U8
            | PrimitiveValType::S16
            | PrimitiveValType::U16
            | PrimitiveValType::S32
            | PrimitiveValType::U32 => ValType::I32,
            PrimitiveValType::S64 | PrimitiveValType::U64 => ValType::I64,
            PrimitiveValType::Float32 => ValType::F32,
            PrimitiveValType::Float64 => ValType::F64,
            PrimitiveValType::String => {
                unimplemented!("non-scalar types are not supported yet")
            }
        },
        ComponentValType::Type(_) => unimplemented!("non-scalar types are not supported yet"),
    };

    Rc::new(crate::core::FuncType {
        params: func_ty
            .params
            .iter()
            .map(|(_, ty)| to_core_ty(*ty))
            .collect(),
        results: func_ty
            .results
            .iter()
            .map(|(_, ty)| to_core_ty(*ty))
            .collect(),
    })
}

fn inverse_scalar_canonical_abi_for(
    u: &mut Unstructured,
    core_func_ty: &crate::core::FuncType,
) -> Result<FuncType> {
    let from_core_ty = |u: &mut Unstructured, core_ty| match core_ty {
        ValType::I32 => u
            .choose(&[
                ComponentValType::Primitive(PrimitiveValType::Char),
                ComponentValType::Primitive(PrimitiveValType::Bool),
                ComponentValType::Primitive(PrimitiveValType::S8),
                ComponentValType::Primitive(PrimitiveValType::U8),
                ComponentValType::Primitive(PrimitiveValType::S16),
                ComponentValType::Primitive(PrimitiveValType::U16),
                ComponentValType::Primitive(PrimitiveValType::S32),
                ComponentValType::Primitive(PrimitiveValType::U32),
            ])
            .cloned(),
        ValType::I64 => u
            .choose(&[
                ComponentValType::Primitive(PrimitiveValType::S64),
                ComponentValType::Primitive(PrimitiveValType::U64),
            ])
            .cloned(),
        ValType::F32 => Ok(ComponentValType::Primitive(PrimitiveValType::Float32)),
        ValType::F64 => Ok(ComponentValType::Primitive(PrimitiveValType::Float64)),
        ValType::V128 | ValType::Ref(_) => {
            unreachable!("not used in canonical ABI")
        }
    };

    let mut names = HashSet::default();
    let mut params = vec![];

    for core_ty in &core_func_ty.params {
        params.push((
            crate::unique_kebab_string(100, &mut names, u)?,
            from_core_ty(u, *core_ty)?,
        ));
    }

    names.clear();

    let results = match core_func_ty.results.len() {
        0 => Vec::new(),
        1 => vec![(
            if u.arbitrary()? {
                Some(crate::unique_kebab_string(100, &mut names, u)?)
            } else {
                None
            },
            from_core_ty(u, core_func_ty.results[0])?,
        )],
        _ => unimplemented!("non-scalar types are not supported yet"),
    };

    Ok(FuncType { params, results })
}

#[derive(Debug)]
enum Section {
    Custom(CustomSection),
    CoreModule(crate::Module),
    CoreInstance(CoreInstanceSection),
    CoreType(CoreTypeSection),
    Component(Component),
    Instance(InstanceSection),
    Alias(AliasSection),
    Type(TypeSection),
    Canonical(CanonicalSection),
    Start(StartSection),
    Import(ImportSection),
    Export(ExportSection),
}

#[derive(Debug)]
struct CustomSection {
    name: String,
    data: Vec<u8>,
}

impl<'a> Arbitrary<'a> for CustomSection {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self> {
        let name = crate::limited_string(1_000, u)?;
        let data = u.arbitrary()?;
        Ok(CustomSection { name, data })
    }
}

#[derive(Debug)]
struct TypeSection {
    types: Vec<Rc<Type>>,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
enum CoreType {
    Func(Rc<crate::core::FuncType>),
    Module(Rc<ModuleType>),
}

#[derive(Clone, Debug, PartialEq, Eq, Hash, Default)]
struct ModuleType {
    defs: Vec<ModuleTypeDef>,
    has_memory: bool,
    has_canonical_abi_realloc: bool,
    has_canonical_abi_free: bool,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
enum ModuleTypeDef {
    TypeDef(crate::core::Type),
    Import(crate::core::Import),
    OuterAlias {
        count: u32,
        i: u32,
        kind: CoreOuterAliasKind,
    },
    Export(String, crate::core::EntityType),
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
enum Type {
    Defined(DefinedType),
    Func(Rc<FuncType>),
    Component(Rc<ComponentType>),
    Instance(Rc<InstanceType>),
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
enum CoreInstanceExportAliasKind {
    Func,
    Table,
    Memory,
    Global,
    Tag,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
enum CoreOuterAliasKind {
    Type(Rc<crate::core::FuncType>),
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
enum Alias {
    InstanceExport {
        instance: u32,
        name: String,
        kind: InstanceExportAliasKind,
    },
    CoreInstanceExport {
        instance: u32,
        name: String,
        kind: CoreInstanceExportAliasKind,
    },
    Outer {
        count: u32,
        i: u32,
        kind: OuterAliasKind,
    },
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
enum InstanceExportAliasKind {
    Module,
    Component,
    Instance,
    Func,
    Value,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
enum OuterAliasKind {
    Module,
    Component,
    CoreType(Rc<CoreType>),
    Type(Rc<Type>),
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
struct ComponentType {
    defs: Vec<ComponentTypeDef>,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
enum ComponentTypeDef {
    CoreType(Rc<CoreType>),
    Type(Rc<Type>),
    Alias(Alias),
    Import(Import),
    Export {
        name: String,
        url: Option<String>,
        ty: ComponentTypeRef,
    },
}

impl From<InstanceTypeDecl> for ComponentTypeDef {
    fn from(def: InstanceTypeDecl) -> Self {
        match def {
            InstanceTypeDecl::CoreType(t) => Self::CoreType(t),
            InstanceTypeDecl::Type(t) => Self::Type(t),
            InstanceTypeDecl::Export { name, url, ty } => Self::Export { name, url, ty },
            InstanceTypeDecl::Alias(a) => Self::Alias(a),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
struct InstanceType {
    defs: Vec<InstanceTypeDecl>,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
enum InstanceTypeDecl {
    CoreType(Rc<CoreType>),
    Type(Rc<Type>),
    Alias(Alias),
    Export {
        name: String,
        url: Option<String>,
        ty: ComponentTypeRef,
    },
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
struct FuncType {
    params: Vec<(String, ComponentValType)>,
    results: Vec<(Option<String>, ComponentValType)>,
}

impl FuncType {
    fn unnamed_result_ty(&self) -> Option<ComponentValType> {
        if self.results.len() == 1 {
            let (name, ty) = &self.results[0];
            if name.is_none() {
                return Some(*ty);
            }
        }
        None
    }

    fn is_scalar(&self) -> bool {
        self.params.iter().all(|(_, ty)| is_scalar(ty))
            && self.results.len() == 1
            && is_scalar(&self.results[0].1)
    }
}

fn is_scalar(ty: &ComponentValType) -> bool {
    match ty {
        ComponentValType::Primitive(prim) => match prim {
            PrimitiveValType::Bool
            | PrimitiveValType::S8
            | PrimitiveValType::U8
            | PrimitiveValType::S16
            | PrimitiveValType::U16
            | PrimitiveValType::S32
            | PrimitiveValType::U32
            | PrimitiveValType::S64
            | PrimitiveValType::U64
            | PrimitiveValType::Float32
            | PrimitiveValType::Float64
            | PrimitiveValType::Char => true,
            PrimitiveValType::String => false,
        },
        ComponentValType::Type(_) => false,
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
enum DefinedType {
    Primitive(PrimitiveValType),
    Record(RecordType),
    Variant(VariantType),
    List(ListType),
    Tuple(TupleType),
    Flags(FlagsType),
    Enum(EnumType),
    Option(OptionType),
    Result(ResultType),
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
struct RecordType {
    fields: Vec<(String, ComponentValType)>,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
struct VariantType {
    cases: Vec<(String, Option<ComponentValType>, Option<u32>)>,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
struct ListType {
    elem_ty: ComponentValType,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
struct TupleType {
    fields: Vec<ComponentValType>,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
struct FlagsType {
    fields: Vec<String>,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
struct EnumType {
    variants: Vec<String>,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
struct OptionType {
    inner_ty: ComponentValType,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
struct ResultType {
    ok_ty: Option<ComponentValType>,
    err_ty: Option<ComponentValType>,
}

#[derive(Debug)]
struct ImportSection {
    imports: Vec<Import>,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
struct Import {
    name: String,
    url: Option<String>,
    ty: ComponentTypeRef,
}

#[derive(Debug)]
struct CanonicalSection {
    funcs: Vec<Func>,
}

#[derive(Debug)]
enum Func {
    CanonLift {
        func_ty: u32,
        options: Vec<CanonOpt>,
        core_func_index: u32,
    },
    CanonLower {
        options: Vec<CanonOpt>,
        func_index: u32,
    },
}

#[derive(Debug)]
enum CanonOpt {
    StringUtf8,
    StringUtf16,
    StringLatin1Utf16,
    Memory(u32),
    Realloc(u32),
    PostReturn(u32),
}

#[derive(Debug)]
struct InstanceSection {}

#[derive(Debug)]
struct ExportSection {}

#[derive(Debug)]
struct StartSection {}

#[derive(Debug)]
struct AliasSection {}

#[derive(Debug)]
struct CoreInstanceSection {}

#[derive(Debug)]
struct CoreTypeSection {
    types: Vec<Rc<CoreType>>,
}
