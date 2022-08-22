//! State relating to validating a WebAssembly component.

use super::{
    check_max, combine_type_sizes,
    core::Module,
    types::{
        ComponentFuncType, ComponentInstanceType, ComponentInstanceTypeKind, ComponentType,
        ComponentValType, EntityType, InstanceType, ModuleType, RecordType, Type, TypeId, TypeList,
        VariantCase,
    },
};
use crate::{
    limits::*,
    types::{
        ComponentDefinedType, ComponentEntityType, InstanceTypeKind, LoweringInfo, TupleType,
        UnionType, VariantType,
    },
    BinaryReaderError, CanonicalOption, ComponentExternalKind, ComponentOuterAliasKind,
    ComponentTypeRef, ExternalKind, FuncType, GlobalType, InstantiationArgKind, MemoryType,
    PrimitiveValType, Result, TableType, TypeBounds, ValType, WasmFeatures,
};
use indexmap::{IndexMap, IndexSet};
use std::mem;

pub(crate) struct ComponentState {
    // Core index spaces
    pub core_types: Vec<TypeId>,
    pub core_modules: Vec<TypeId>,
    pub core_instances: Vec<TypeId>,
    pub core_funcs: Vec<TypeId>,
    pub core_memories: Vec<MemoryType>,
    pub core_tables: Vec<TableType>,
    pub core_globals: Vec<GlobalType>,
    pub core_tags: Vec<TypeId>,

    // Component index spaces
    pub types: Vec<TypeId>,
    pub funcs: Vec<TypeId>,
    pub values: Vec<(ComponentValType, bool)>,
    pub instances: Vec<TypeId>,
    pub components: Vec<TypeId>,

    pub imports: IndexMap<String, ComponentEntityType>,
    pub exports: IndexMap<String, ComponentEntityType>,
    has_start: bool,
    type_size: usize,
}

impl ComponentState {
    pub fn type_count(&self) -> usize {
        self.core_types.len() + self.types.len()
    }

    pub fn instance_count(&self) -> usize {
        self.core_instances.len() + self.instances.len()
    }

    pub fn function_count(&self) -> usize {
        self.core_funcs.len() + self.funcs.len()
    }

    pub fn add_core_type(
        components: &mut [Self],
        ty: crate::CoreType,
        features: &WasmFeatures,
        types: &mut TypeList,
        offset: usize,
        check_limit: bool,
    ) -> Result<()> {
        let ty = match ty {
            crate::CoreType::Func(ty) => Type::Func(ty),
            crate::CoreType::Module(decls) => Type::Module(Self::create_module_type(
                components,
                decls.into_vec(),
                features,
                types,
                offset,
            )?),
        };

        let current = components.last_mut().unwrap();

        if check_limit {
            check_max(current.type_count(), 1, MAX_WASM_TYPES, "types", offset)?;
        }

        current.core_types.push(TypeId {
            type_size: ty.type_size(),
            index: types.len(),
        });
        types.push(ty);

        Ok(())
    }

    pub fn add_core_module(
        &mut self,
        module: &Module,
        types: &mut TypeList,
        offset: usize,
    ) -> Result<()> {
        let imports = module.imports_for_module_type(offset)?;

        // We have to clone the module's imports and exports here
        // because we cannot take the data out of the `MaybeOwned`
        // as it might be shared with a function validator.
        let ty = Type::Module(ModuleType {
            type_size: module.type_size,
            imports,
            exports: module.exports.clone(),
        });

        self.core_modules.push(TypeId {
            type_size: ty.type_size(),
            index: types.len(),
        });

        types.push(ty);

        Ok(())
    }

    pub fn add_core_instance(
        &mut self,
        instance: crate::Instance,
        types: &mut TypeList,
        offset: usize,
    ) -> Result<()> {
        let instance = match instance {
            crate::Instance::Instantiate { module_index, args } => {
                self.instantiate_module(module_index, args.into_vec(), types, offset)?
            }
            crate::Instance::FromExports(exports) => {
                self.instantiate_core_exports(exports.into_vec(), types, offset)?
            }
        };

        self.core_instances.push(instance);

        Ok(())
    }

    pub fn add_core_alias(
        components: &mut [Self],
        alias: crate::Alias,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        match alias {
            crate::Alias::InstanceExport {
                instance_index,
                kind,
                name,
            } => components.last_mut().unwrap().alias_core_instance_export(
                instance_index,
                kind,
                name,
                types,
                offset,
            ),
            crate::Alias::Outer { kind, count, index } => match kind {
                crate::OuterAliasKind::Type => {
                    Self::alias_core_type(components, count, index, offset)
                }
            },
        }
    }

    pub fn add_type(
        components: &mut Vec<Self>,
        ty: crate::ComponentType,
        features: &WasmFeatures,
        types: &mut TypeList,
        offset: usize,
        check_limit: bool,
    ) -> Result<()> {
        assert!(!components.is_empty());
        let ty = match ty {
            crate::ComponentType::Defined(ty) => Type::Defined(
                components
                    .last_mut()
                    .unwrap()
                    .create_defined_type(ty, types, offset)?,
            ),
            crate::ComponentType::Func(ty) => Type::ComponentFunc(
                components
                    .last_mut()
                    .unwrap()
                    .create_function_type(ty, types, offset)?,
            ),
            crate::ComponentType::Component(decls) => Type::Component(Self::create_component_type(
                components,
                decls.into_vec(),
                features,
                types,
                offset,
            )?),
            crate::ComponentType::Instance(decls) => Type::ComponentInstance(
                Self::create_instance_type(components, decls.into_vec(), features, types, offset)?,
            ),
        };

        let current = components.last_mut().unwrap();
        if check_limit {
            check_max(current.type_count(), 1, MAX_WASM_TYPES, "types", offset)?;
        }

        current.types.push(TypeId {
            type_size: ty.type_size(),
            index: types.len(),
        });
        types.push(ty);

        Ok(())
    }

    pub fn add_import(
        &mut self,
        import: crate::ComponentImport,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        let entity = self.check_type_ref(&import.ty, types, offset)?;

        let (len, max, desc) = match entity {
            ComponentEntityType::Module(id) => {
                self.core_modules.push(id);
                (self.core_modules.len(), MAX_WASM_MODULES, "modules")
            }
            ComponentEntityType::Component(id) => {
                self.components.push(id);
                (self.components.len(), MAX_WASM_COMPONENTS, "components")
            }
            ComponentEntityType::Instance(id) => {
                self.instances.push(id);
                (self.instance_count(), MAX_WASM_INSTANCES, "instances")
            }
            ComponentEntityType::Func(id) => {
                self.funcs.push(id);
                (self.function_count(), MAX_WASM_FUNCTIONS, "functions")
            }
            ComponentEntityType::Value(ty) => {
                self.values.push((ty, false));
                (self.values.len(), MAX_WASM_VALUES, "values")
            }
            ComponentEntityType::Type(id) => {
                self.types.push(id);
                (self.types.len(), MAX_WASM_TYPES, "types")
            }
        };

        check_max(len, 0, max, desc, offset)?;

        self.type_size = combine_type_sizes(self.type_size, entity.type_size(), offset)?;

        if self
            .imports
            .insert(import.name.to_string(), entity)
            .is_some()
        {
            return Err(BinaryReaderError::new(
                format!("duplicate import name `{}` already defined", import.name),
                offset,
            ));
        }

        Ok(())
    }

    pub fn add_export(
        &mut self,
        name: &str,
        ty: ComponentEntityType,
        offset: usize,
        check_limit: bool,
    ) -> Result<()> {
        if check_limit {
            check_max(self.exports.len(), 1, MAX_WASM_EXPORTS, "exports", offset)?;
        }

        self.type_size = combine_type_sizes(self.type_size, ty.type_size(), offset)?;

        if self.exports.insert(name.to_string(), ty).is_some() {
            return Err(BinaryReaderError::new(
                format!("duplicate export name `{}` already defined", name),
                offset,
            ));
        }

        Ok(())
    }

    pub fn lift_function(
        &mut self,
        core_func_index: u32,
        type_index: u32,
        options: Vec<CanonicalOption>,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        let ty = self.function_type_at(type_index, types, offset)?;
        let core_ty = types[self.core_function_at(core_func_index, offset)?]
            .as_func_type()
            .unwrap();

        // Lifting a function is for an export, so match the expected canonical ABI
        // export signature
        let info = ty.lower(types, false);
        self.check_options(Some(core_ty), &info, &options, types, offset)?;

        if core_ty.params.as_ref() != info.params.as_slice() {
            return Err(BinaryReaderError::new(
                format!(
                    "lowered parameter types `{:?}` do not match parameter types `{:?}` of core function {core_func_index}",
                    info.params.as_slice(),
                    core_ty.params
                ),
                offset,
            ));
        }

        if core_ty.returns.as_ref() != info.results.as_slice() {
            return Err(BinaryReaderError::new(
                format!(
                    "lowered result types `{:?}` do not match result types `{:?}` of core function {core_func_index}",
                    info.results.as_slice(),
                    core_ty.returns
                ),
                offset,
            ));
        }

        self.funcs.push(self.types[type_index as usize]);

        Ok(())
    }

    pub fn lower_function(
        &mut self,
        func_index: u32,
        options: Vec<CanonicalOption>,
        types: &mut TypeList,
        offset: usize,
    ) -> Result<()> {
        let ty = types[self.function_at(func_index, offset)?]
            .as_component_func_type()
            .unwrap();

        // Lowering a function is for an import, so use a function type that matches
        // the expected canonical ABI import signature.
        let info = ty.lower(types, true);

        self.check_options(None, &info, &options, types, offset)?;

        let lowered_ty = Type::Func(info.into_func_type());

        self.core_funcs.push(TypeId {
            type_size: lowered_ty.type_size(),
            index: types.len(),
        });

        types.push(lowered_ty);

        Ok(())
    }

    pub fn add_component(&mut self, component: &mut Self, types: &mut TypeList) {
        let ty = Type::Component(ComponentType {
            type_size: component.type_size,
            imports: mem::take(&mut component.imports),
            exports: mem::take(&mut component.exports),
        });

        self.components.push(TypeId {
            type_size: ty.type_size(),
            index: types.len(),
        });

        types.push(ty);
    }

    pub fn add_instance(
        &mut self,
        instance: crate::ComponentInstance,
        types: &mut TypeList,
        offset: usize,
    ) -> Result<()> {
        let instance = match instance {
            crate::ComponentInstance::Instantiate {
                component_index,
                args,
            } => self.instantiate_component(component_index, args.into_vec(), types, offset)?,
            crate::ComponentInstance::FromExports(exports) => {
                self.instantiate_exports(exports.into_vec(), types, offset)?
            }
        };

        self.instances.push(instance);

        Ok(())
    }

    pub fn add_alias(
        components: &mut [Self],
        alias: crate::ComponentAlias,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        match alias {
            crate::ComponentAlias::InstanceExport {
                instance_index,
                kind,
                name,
            } => components.last_mut().unwrap().alias_instance_export(
                instance_index,
                kind,
                name,
                types,
                offset,
            ),
            crate::ComponentAlias::Outer { kind, count, index } => match kind {
                ComponentOuterAliasKind::CoreModule => {
                    Self::alias_module(components, count, index, offset)
                }
                ComponentOuterAliasKind::CoreType => {
                    Self::alias_core_type(components, count, index, offset)
                }
                ComponentOuterAliasKind::Type => Self::alias_type(components, count, index, offset),
                ComponentOuterAliasKind::Component => {
                    Self::alias_component(components, count, index, offset)
                }
            },
        }
    }

    pub fn add_start(
        &mut self,
        func_index: u32,
        args: &[u32],
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        if self.has_start {
            return Err(BinaryReaderError::new(
                "component cannot have more than one start function",
                offset,
            ));
        }

        let ft = types[self.function_at(func_index, offset)?]
            .as_component_func_type()
            .unwrap();

        if ft.params.len() != args.len() {
            return Err(BinaryReaderError::new(
                format!(
                    "component start function requires {} arguments but was given {}",
                    ft.params.len(),
                    args.len()
                ),
                offset,
            ));
        }

        for (i, ((_, ty), arg)) in ft.params.iter().zip(args).enumerate() {
            // Ensure the value's type is a subtype of the parameter type
            if !self
                .value_at(*arg, offset)?
                .internal_is_subtype_of(ty, types)
            {
                return Err(BinaryReaderError::new(
                    format!(
                        "value type mismatch for component start function argument {}",
                        i
                    ),
                    offset,
                ));
            }
        }

        match ft.result {
            ComponentValType::Primitive(PrimitiveValType::Unit) => {}
            ty => {
                self.values.push((ty, false));
            }
        }

        self.has_start = true;

        Ok(())
    }

    fn check_options(
        &self,
        core_ty: Option<&FuncType>,
        info: &LoweringInfo,
        options: &[CanonicalOption],
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        fn display(option: CanonicalOption) -> &'static str {
            match option {
                CanonicalOption::UTF8 => "utf8",
                CanonicalOption::UTF16 => "utf16",
                CanonicalOption::CompactUTF16 => "latin1-utf16",
                CanonicalOption::Memory(_) => "memory",
                CanonicalOption::Realloc(_) => "realloc",
                CanonicalOption::PostReturn(_) => "post-return",
            }
        }

        let mut encoding = None;
        let mut memory = None;
        let mut realloc = None;
        let mut post_return = None;

        for option in options {
            match option {
                CanonicalOption::UTF8 | CanonicalOption::UTF16 | CanonicalOption::CompactUTF16 => {
                    match encoding {
                        Some(existing) => {
                            return Err(BinaryReaderError::new(
                                format!(
                                    "canonical encoding option `{}` conflicts with option `{}`",
                                    display(existing),
                                    display(*option),
                                ),
                                offset,
                            ))
                        }
                        None => encoding = Some(*option),
                    }
                }
                CanonicalOption::Memory(idx) => {
                    memory = match memory {
                        None => {
                            self.memory_at(*idx, offset)?;
                            Some(*idx)
                        }
                        Some(_) => {
                            return Err(BinaryReaderError::new(
                                "canonical option `memory` is specified more than once",
                                offset,
                            ))
                        }
                    }
                }
                CanonicalOption::Realloc(idx) => {
                    realloc = match realloc {
                        None => {
                            let ty = types[self.core_function_at(*idx, offset)?]
                                .as_func_type()
                                .unwrap();
                            if ty.params.as_ref()
                                != [ValType::I32, ValType::I32, ValType::I32, ValType::I32]
                                || ty.returns.as_ref() != [ValType::I32]
                            {
                                return Err(BinaryReaderError::new(
                                    "canonical option `realloc` uses a core function with an incorrect signature",
                                    offset,
                                ));
                            }
                            Some(*idx)
                        }
                        Some(_) => {
                            return Err(BinaryReaderError::new(
                                "canonical option `realloc` is specified more than once",
                                offset,
                            ))
                        }
                    }
                }
                CanonicalOption::PostReturn(idx) => {
                    post_return = match post_return {
                        None => {
                            let core_ty = core_ty.ok_or_else(|| {
                                BinaryReaderError::new(
                                    "canonical option `post-return` cannot be specified for lowerings",
                                    offset,
                                )
                            })?;

                            let ty = types[self.core_function_at(*idx, offset)?]
                                .as_func_type()
                                .unwrap();

                            if ty.params != core_ty.returns || !ty.returns.is_empty() {
                                return Err(BinaryReaderError::new(
                                    "canonical option `post-return` uses a core function with an incorrect signature",
                                    offset,
                                ));
                            }
                            Some(*idx)
                        }
                        Some(_) => {
                            return Err(BinaryReaderError::new(
                                "canonical option `post-return` is specified more than once",
                                offset,
                            ))
                        }
                    }
                }
            }
        }

        if info.requires_memory && memory.is_none() {
            return Err(BinaryReaderError::new(
                "canonical option `memory` is required",
                offset,
            ));
        }

        if info.requires_realloc && realloc.is_none() {
            return Err(BinaryReaderError::new(
                "canonical option `realloc` is required",
                offset,
            ));
        }

        Ok(())
    }

    pub fn check_type_ref(
        &self,
        ty: &ComponentTypeRef,
        types: &TypeList,
        offset: usize,
    ) -> Result<ComponentEntityType> {
        Ok(match ty {
            ComponentTypeRef::Module(index) => {
                let id = self.type_at(*index, true, offset)?;
                types[id].as_module_type().ok_or_else(|| {
                    BinaryReaderError::new(
                        format!("core type index {} is not a module type", index),
                        offset,
                    )
                })?;
                ComponentEntityType::Module(id)
            }
            ComponentTypeRef::Func(index) => {
                let id = self.type_at(*index, false, offset)?;
                types[id].as_component_func_type().ok_or_else(|| {
                    BinaryReaderError::new(
                        format!("type index {} is not a function type", index),
                        offset,
                    )
                })?;
                ComponentEntityType::Func(id)
            }
            ComponentTypeRef::Value(ty) => {
                let ty = match ty {
                    crate::ComponentValType::Primitive(ty) => ComponentValType::Primitive(*ty),
                    crate::ComponentValType::Type(index) => {
                        ComponentValType::Type(self.defined_type_at(*index, types, offset)?)
                    }
                };
                ComponentEntityType::Value(ty)
            }
            ComponentTypeRef::Type(TypeBounds::Eq, index) => {
                ComponentEntityType::Type(self.type_at(*index, false, offset)?)
            }
            ComponentTypeRef::Instance(index) => {
                let id = self.type_at(*index, false, offset)?;
                types[id].as_component_instance_type().ok_or_else(|| {
                    BinaryReaderError::new(
                        format!("type index {} is not an instance type", index),
                        offset,
                    )
                })?;
                ComponentEntityType::Instance(id)
            }
            ComponentTypeRef::Component(index) => {
                let id = self.type_at(*index, false, offset)?;
                types[id].as_component_type().ok_or_else(|| {
                    BinaryReaderError::new(
                        format!("type index {} is not a component type", index),
                        offset,
                    )
                })?;
                ComponentEntityType::Component(id)
            }
        })
    }

    pub fn export_to_entity_type(
        &mut self,
        export: &crate::ComponentExport,
        offset: usize,
    ) -> Result<ComponentEntityType> {
        Ok(match export.kind {
            ComponentExternalKind::Module => {
                ComponentEntityType::Module(self.module_at(export.index, offset)?)
            }
            ComponentExternalKind::Func => {
                ComponentEntityType::Func(self.function_at(export.index, offset)?)
            }
            ComponentExternalKind::Value => {
                ComponentEntityType::Value(*self.value_at(export.index, offset)?)
            }
            ComponentExternalKind::Type => {
                ComponentEntityType::Type(self.type_at(export.index, false, offset)?)
            }
            ComponentExternalKind::Instance => {
                ComponentEntityType::Instance(self.instance_at(export.index, offset)?)
            }
            ComponentExternalKind::Component => {
                ComponentEntityType::Component(self.component_at(export.index, offset)?)
            }
        })
    }

    fn create_module_type(
        components: &[Self],
        decls: Vec<crate::ModuleTypeDeclaration>,
        features: &WasmFeatures,
        types: &mut TypeList,
        offset: usize,
    ) -> Result<ModuleType> {
        let mut state = Module::default();

        for decl in decls {
            match decl {
                crate::ModuleTypeDeclaration::Type(ty) => {
                    state.add_type(ty, features, types, offset, true)?;
                }
                crate::ModuleTypeDeclaration::Export { name, ty } => {
                    let ty = state.check_type_ref(&ty, features, types, offset)?;
                    state.add_export(name, ty, features, offset, true)?;
                }
                crate::ModuleTypeDeclaration::Alias(alias) => match alias {
                    crate::Alias::InstanceExport { .. } => {
                        return Err(BinaryReaderError::new(
                            "only outer type aliases are allowed in module type declarations",
                            offset,
                        ))
                    }
                    crate::Alias::Outer { kind, count, index } => {
                        if count > 1 {
                            return Err(BinaryReaderError::new(
                                    "outer type aliases in module type declarations are limited to a maximum count of 1",
                                    offset,
                                ));
                        }
                        match kind {
                            crate::OuterAliasKind::Type => {
                                let ty = if count == 0 {
                                    // Local alias, check the local module state
                                    state.type_at(index, offset)?
                                } else {
                                    // Otherwise, check the enclosing component state
                                    let component =
                                        Self::check_alias_count(components, count - 1, offset)?;
                                    component.type_at(index, true, offset)?
                                };

                                check_max(state.types.len(), 1, MAX_WASM_TYPES, "types", offset)?;

                                state.types.push(ty);
                            }
                        }
                    }
                },
                crate::ModuleTypeDeclaration::Import(import) => {
                    state.add_import(import, features, types, offset)?;
                }
            }
        }

        let imports = state.imports_for_module_type(offset)?;

        Ok(ModuleType {
            type_size: state.type_size,
            imports,
            exports: state.exports,
        })
    }

    fn create_component_type(
        components: &mut Vec<Self>,
        decls: Vec<crate::ComponentTypeDeclaration>,
        features: &WasmFeatures,
        types: &mut TypeList,
        offset: usize,
    ) -> Result<ComponentType> {
        components.push(ComponentState::default());

        for decl in decls {
            match decl {
                crate::ComponentTypeDeclaration::CoreType(ty) => {
                    Self::add_core_type(components, ty, features, types, offset, true)?;
                }
                crate::ComponentTypeDeclaration::Type(ty) => {
                    Self::add_type(components, ty, features, types, offset, true)?;
                }
                crate::ComponentTypeDeclaration::Export { name, ty } => {
                    let current = components.last_mut().unwrap();
                    let ty = current.check_type_ref(&ty, types, offset)?;
                    current.add_export(name, ty, offset, true)?;
                }
                crate::ComponentTypeDeclaration::Import(import) => {
                    components
                        .last_mut()
                        .unwrap()
                        .add_import(import, types, offset)?;
                }
                crate::ComponentTypeDeclaration::Alias(alias) => {
                    match alias {
                        crate::ComponentAlias::Outer { kind, count, index }
                            if kind == ComponentOuterAliasKind::CoreType
                                || kind == ComponentOuterAliasKind::Type =>
                        {
                            match kind {
                                ComponentOuterAliasKind::CoreType => {
                                    Self::alias_core_type(components, count, index, offset)?
                                }
                                ComponentOuterAliasKind::Type => {
                                    Self::alias_type(components, count, index, offset)?
                                }
                                _ => unreachable!(),
                            }
                        }
                        _ => return Err(BinaryReaderError::new(
                            "only outer type aliases are allowed in component type declarations",
                            offset,
                        )),
                    }
                }
            };
        }

        let state = components.pop().unwrap();

        Ok(ComponentType {
            type_size: state.type_size,
            imports: state.imports,
            exports: state.exports,
        })
    }

    fn create_instance_type(
        components: &mut Vec<Self>,
        decls: Vec<crate::InstanceTypeDeclaration>,
        features: &WasmFeatures,
        types: &mut TypeList,
        offset: usize,
    ) -> Result<ComponentInstanceType> {
        components.push(ComponentState::default());

        for decl in decls {
            match decl {
                crate::InstanceTypeDeclaration::CoreType(ty) => {
                    Self::add_core_type(components, ty, features, types, offset, true)?;
                }
                crate::InstanceTypeDeclaration::Type(ty) => {
                    Self::add_type(components, ty, features, types, offset, true)?;
                }
                crate::InstanceTypeDeclaration::Export { name, ty } => {
                    let current = components.last_mut().unwrap();
                    let ty = current.check_type_ref(&ty, types, offset)?;
                    current.add_export(name, ty, offset, true)?;
                }
                crate::InstanceTypeDeclaration::Alias(alias) => match alias {
                    crate::ComponentAlias::Outer { kind, count, index }
                        if kind == ComponentOuterAliasKind::CoreType
                            || kind == ComponentOuterAliasKind::Type =>
                    {
                        match kind {
                            ComponentOuterAliasKind::CoreType => {
                                Self::alias_core_type(components, count, index, offset)?
                            }
                            ComponentOuterAliasKind::Type => {
                                Self::alias_type(components, count, index, offset)?
                            }
                            _ => unreachable!(),
                        }
                    }
                    _ => {
                        return Err(BinaryReaderError::new(
                            "only outer type aliases are allowed in instance type declarations",
                            offset,
                        ))
                    }
                },
            };
        }

        let state = components.pop().unwrap();

        Ok(ComponentInstanceType {
            type_size: state.type_size,
            kind: ComponentInstanceTypeKind::Defined(state.exports),
        })
    }

    fn create_function_type(
        &self,
        ty: crate::ComponentFuncType,
        types: &TypeList,
        offset: usize,
    ) -> Result<ComponentFuncType> {
        let mut type_size = 1;

        let params = ty
            .params
            .iter()
            .map(|(name, ty)| {
                if let Some(name) = name {
                    Self::check_name(name, "function parameter", offset)?;
                }
                let ty = self.create_component_val_type(*ty, types, offset)?;
                type_size = combine_type_sizes(type_size, ty.type_size(), offset)?;
                Ok((name.map(ToOwned::to_owned), ty))
            })
            .collect::<Result<_>>()?;

        let result = self.create_component_val_type(ty.result, types, offset)?;
        type_size = combine_type_sizes(type_size, result.type_size(), offset)?;

        Ok(ComponentFuncType {
            type_size,
            params,
            result,
        })
    }

    fn check_name(name: &str, desc: &str, offset: usize) -> Result<()> {
        if name.is_empty() {
            return Err(BinaryReaderError::new(
                format!("{} name cannot be empty", desc),
                offset,
            ));
        }

        Ok(())
    }

    fn instantiate_module(
        &self,
        module_index: u32,
        module_args: Vec<crate::InstantiationArg>,
        types: &mut TypeList,
        offset: usize,
    ) -> Result<TypeId> {
        fn insert_arg<'a>(
            name: &'a str,
            arg: &'a InstanceType,
            args: &mut IndexMap<&'a str, &'a InstanceType>,
            offset: usize,
        ) -> Result<()> {
            if args.insert(name, arg).is_some() {
                return Err(BinaryReaderError::new(
                    format!("duplicate module instantiation argument named `{}`", name),
                    offset,
                ));
            }

            Ok(())
        }

        let module_type_id = self.module_at(module_index, offset)?;
        let mut args = IndexMap::new();

        // Populate the arguments
        for module_arg in module_args {
            match module_arg.kind {
                InstantiationArgKind::Instance => {
                    let instance_type = types[self.core_instance_at(module_arg.index, offset)?]
                        .as_instance_type()
                        .unwrap();
                    insert_arg(module_arg.name, instance_type, &mut args, offset)?;
                }
            }
        }

        // Validate the arguments
        let module_type = types[module_type_id].as_module_type().unwrap();
        for ((module, name), expected) in module_type.imports.iter() {
            let instance = args.get(module.as_str()).ok_or_else(|| {
                BinaryReaderError::new(
                    format!("missing module instantiation argument named `{}`", module),
                    offset,
                )
            })?;

            let arg = instance.exports(types).get(name.as_str()).ok_or_else(|| {
                BinaryReaderError::new(
                    format!(
                        "module instantiation argument `{}` does not export an item named `{}`",
                        module, name,
                    ),
                    offset,
                )
            })?;

            match (arg, expected) {
                (EntityType::Func(_), EntityType::Func(_)) |
                (EntityType::Table(_), EntityType::Table(_)) |
                (EntityType::Memory(_), EntityType::Memory(_)) |
                (EntityType::Global(_), EntityType::Global(_)) |
                (EntityType::Tag(_), EntityType::Tag(_)) => {},
                _ => {
                    return Err(BinaryReaderError::new(
                        format!(
                            "module instantiation argument `{}` exports an item named `{}` but it is not a {}",
                            module,
                            name,
                            expected.desc()
                        ),
                        offset,
                    ))
                }
            }

            if !arg.internal_is_subtype_of(expected, types) {
                return Err(BinaryReaderError::new(
                    format!(
                        "{} type mismatch for export `{}` of module instantiation argument `{}`",
                        expected.desc(),
                        name,
                        module,
                    ),
                    offset,
                ));
            }
        }

        let ty = Type::Instance(InstanceType {
            type_size: module_type
                .exports
                .iter()
                .fold(1, |acc, (_, ty)| acc + ty.type_size()),
            kind: InstanceTypeKind::Instantiated(module_type_id),
        });

        let id = TypeId {
            type_size: ty.type_size(),
            index: types.len(),
        };

        types.push(ty);

        Ok(id)
    }

    fn instantiate_component(
        &mut self,
        component_index: u32,
        component_args: Vec<crate::ComponentInstantiationArg>,
        types: &mut TypeList,
        offset: usize,
    ) -> Result<TypeId> {
        fn insert_arg<'a>(
            name: &'a str,
            arg: ComponentEntityType,
            args: &mut IndexMap<&'a str, ComponentEntityType>,
            offset: usize,
        ) -> Result<()> {
            if args.insert(name, arg).is_some() {
                return Err(BinaryReaderError::new(
                    format!(
                        "duplicate component instantiation argument named `{}`",
                        name
                    ),
                    offset,
                ));
            }

            Ok(())
        }

        let component_type_id = self.component_at(component_index, offset)?;
        let mut args = IndexMap::new();

        // Populate the arguments
        for component_arg in component_args {
            match component_arg.kind {
                ComponentExternalKind::Module => {
                    insert_arg(
                        component_arg.name,
                        ComponentEntityType::Module(self.module_at(component_arg.index, offset)?),
                        &mut args,
                        offset,
                    )?;
                }
                ComponentExternalKind::Component => {
                    insert_arg(
                        component_arg.name,
                        ComponentEntityType::Component(
                            self.component_at(component_arg.index, offset)?,
                        ),
                        &mut args,
                        offset,
                    )?;
                }
                ComponentExternalKind::Instance => {
                    insert_arg(
                        component_arg.name,
                        ComponentEntityType::Instance(
                            self.instance_at(component_arg.index, offset)?,
                        ),
                        &mut args,
                        offset,
                    )?;
                }
                ComponentExternalKind::Func => {
                    insert_arg(
                        component_arg.name,
                        ComponentEntityType::Func(self.function_at(component_arg.index, offset)?),
                        &mut args,
                        offset,
                    )?;
                }
                ComponentExternalKind::Value => {
                    insert_arg(
                        component_arg.name,
                        ComponentEntityType::Value(*self.value_at(component_arg.index, offset)?),
                        &mut args,
                        offset,
                    )?;
                }
                ComponentExternalKind::Type => {
                    // Type arguments are ignored
                }
            }
        }

        // Validate the arguments
        let component_type = types[component_type_id].as_component_type().unwrap();
        for (name, expected) in component_type.imports.iter() {
            match args.get(name.as_str()) {
                Some(arg) => {
                    match (arg, expected) {
                        (ComponentEntityType::Module(_), ComponentEntityType::Module(_))
                        | (ComponentEntityType::Component(_), ComponentEntityType::Component(_))
                        | (ComponentEntityType::Instance(_), ComponentEntityType::Instance(_))
                        | (ComponentEntityType::Func(_), ComponentEntityType::Func(_))
                        | (ComponentEntityType::Value(_), ComponentEntityType::Value(_)) => {}
                        _ => {
                            return Err(BinaryReaderError::new(
                                format!(
                                "expected component instantiation argument `{}` to be of type `{}`",
                                name,
                                expected.desc()
                            ),
                                offset,
                            ))
                        }
                    };

                    if !arg.internal_is_subtype_of(expected, types) {
                        return Err(BinaryReaderError::new(
                            format!(
                                "{} type mismatch for component instantiation argument `{}`",
                                expected.desc(),
                                name
                            ),
                            offset,
                        ));
                    }
                }
                None => {
                    return Err(BinaryReaderError::new(
                        format!("missing component instantiation argument named `{}`", name),
                        offset,
                    ))
                }
            }
        }

        let ty = Type::ComponentInstance(ComponentInstanceType {
            type_size: component_type
                .exports
                .iter()
                .fold(1, |acc, (_, ty)| acc + ty.type_size()),
            kind: ComponentInstanceTypeKind::Instantiated(component_type_id),
        });

        let id = TypeId {
            type_size: ty.type_size(),
            index: types.len(),
        };

        types.push(ty);

        Ok(id)
    }

    fn instantiate_exports(
        &mut self,
        exports: Vec<crate::ComponentExport>,
        types: &mut TypeList,
        offset: usize,
    ) -> Result<TypeId> {
        fn insert_export(
            name: &str,
            export: ComponentEntityType,
            exports: &mut IndexMap<String, ComponentEntityType>,
            type_size: &mut usize,
            offset: usize,
        ) -> Result<()> {
            *type_size = combine_type_sizes(*type_size, export.type_size(), offset)?;

            if exports.insert(name.to_string(), export).is_some() {
                return Err(BinaryReaderError::new(
                    format!(
                        "duplicate instantiation export name `{}` already defined",
                        name
                    ),
                    offset,
                ));
            }

            Ok(())
        }

        let mut type_size = 1;
        let mut inst_exports = IndexMap::new();
        for export in exports {
            match export.kind {
                ComponentExternalKind::Module => {
                    insert_export(
                        export.name,
                        ComponentEntityType::Module(self.module_at(export.index, offset)?),
                        &mut inst_exports,
                        &mut type_size,
                        offset,
                    )?;
                }
                ComponentExternalKind::Component => {
                    insert_export(
                        export.name,
                        ComponentEntityType::Component(self.component_at(export.index, offset)?),
                        &mut inst_exports,
                        &mut type_size,
                        offset,
                    )?;
                }
                ComponentExternalKind::Instance => {
                    insert_export(
                        export.name,
                        ComponentEntityType::Instance(self.instance_at(export.index, offset)?),
                        &mut inst_exports,
                        &mut type_size,
                        offset,
                    )?;
                }
                ComponentExternalKind::Func => {
                    insert_export(
                        export.name,
                        ComponentEntityType::Func(self.function_at(export.index, offset)?),
                        &mut inst_exports,
                        &mut type_size,
                        offset,
                    )?;
                }
                ComponentExternalKind::Value => {
                    insert_export(
                        export.name,
                        ComponentEntityType::Value(*self.value_at(export.index, offset)?),
                        &mut inst_exports,
                        &mut type_size,
                        offset,
                    )?;
                }
                ComponentExternalKind::Type => {
                    insert_export(
                        export.name,
                        ComponentEntityType::Type(self.type_at(export.index, false, offset)?),
                        &mut inst_exports,
                        &mut type_size,
                        offset,
                    )?;
                }
            }
        }

        let ty = Type::ComponentInstance(ComponentInstanceType {
            type_size,
            kind: ComponentInstanceTypeKind::Exports(inst_exports),
        });

        let id = TypeId {
            type_size: ty.type_size(),
            index: types.len(),
        };

        types.push(ty);

        Ok(id)
    }

    fn instantiate_core_exports(
        &mut self,
        exports: Vec<crate::Export>,
        types: &mut TypeList,
        offset: usize,
    ) -> Result<TypeId> {
        fn insert_export(
            name: &str,
            export: EntityType,
            exports: &mut IndexMap<String, EntityType>,
            type_size: &mut usize,
            offset: usize,
        ) -> Result<()> {
            *type_size = combine_type_sizes(*type_size, export.type_size(), offset)?;

            if exports.insert(name.to_string(), export).is_some() {
                return Err(BinaryReaderError::new(
                    format!(
                        "duplicate instantiation export name `{}` already defined",
                        name
                    ),
                    offset,
                ));
            }

            Ok(())
        }

        let mut type_size = 1;
        let mut inst_exports = IndexMap::new();
        for export in exports {
            match export.kind {
                ExternalKind::Func => {
                    insert_export(
                        export.name,
                        EntityType::Func(self.core_function_at(export.index, offset)?),
                        &mut inst_exports,
                        &mut type_size,
                        offset,
                    )?;
                }
                ExternalKind::Table => insert_export(
                    export.name,
                    EntityType::Table(*self.table_at(export.index, offset)?),
                    &mut inst_exports,
                    &mut type_size,
                    offset,
                )?,
                ExternalKind::Memory => insert_export(
                    export.name,
                    EntityType::Memory(*self.memory_at(export.index, offset)?),
                    &mut inst_exports,
                    &mut type_size,
                    offset,
                )?,
                ExternalKind::Global => {
                    insert_export(
                        export.name,
                        EntityType::Global(*self.global_at(export.index, offset)?),
                        &mut inst_exports,
                        &mut type_size,
                        offset,
                    )?;
                }
                ExternalKind::Tag => insert_export(
                    export.name,
                    EntityType::Tag(self.core_function_at(export.index, offset)?),
                    &mut inst_exports,
                    &mut type_size,
                    offset,
                )?,
            }
        }

        let ty = Type::Instance(InstanceType {
            type_size,
            kind: InstanceTypeKind::Exports(inst_exports),
        });

        let id = TypeId {
            type_size: ty.type_size(),
            index: types.len(),
        };

        types.push(ty);

        Ok(id)
    }

    fn alias_core_instance_export(
        &mut self,
        instance_index: u32,
        kind: ExternalKind,
        name: &str,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        macro_rules! push_module_export {
            ($expected:path, $collection:ident, $ty:literal) => {{
                match self.core_instance_export(instance_index, name, types, offset)? {
                    $expected(ty) => {
                        self.$collection.push(*ty);
                        Ok(())
                    }
                    _ => {
                        return Err(BinaryReaderError::new(
                            format!(
                                "export `{}` for core instance {} is not a {}",
                                name, instance_index, $ty
                            ),
                            offset,
                        ))
                    }
                }
            }};
        }

        match kind {
            ExternalKind::Func => {
                check_max(
                    self.function_count(),
                    1,
                    MAX_WASM_FUNCTIONS,
                    "functions",
                    offset,
                )?;
                push_module_export!(EntityType::Func, core_funcs, "function")
            }
            ExternalKind::Table => {
                check_max(self.core_tables.len(), 1, MAX_WASM_TABLES, "tables", offset)?;
                push_module_export!(EntityType::Table, core_tables, "table")
            }
            ExternalKind::Memory => {
                check_max(
                    self.core_memories.len(),
                    1,
                    MAX_WASM_MEMORIES,
                    "memories",
                    offset,
                )?;
                push_module_export!(EntityType::Memory, core_memories, "memory")
            }
            ExternalKind::Global => {
                check_max(
                    self.core_globals.len(),
                    1,
                    MAX_WASM_GLOBALS,
                    "globals",
                    offset,
                )?;
                push_module_export!(EntityType::Global, core_globals, "global")
            }
            ExternalKind::Tag => {
                check_max(self.core_tags.len(), 1, MAX_WASM_TAGS, "tags", offset)?;
                push_module_export!(EntityType::Tag, core_tags, "tag")
            }
        }
    }

    fn alias_instance_export(
        &mut self,
        instance_index: u32,
        kind: ComponentExternalKind,
        name: &str,
        types: &TypeList,
        offset: usize,
    ) -> Result<()> {
        macro_rules! push_component_export {
            ($expected:path, $collection:ident, $ty:literal) => {{
                match self.instance_export(instance_index, name, types, offset)? {
                    $expected(ty) => {
                        self.$collection.push(*ty);
                        Ok(())
                    }
                    _ => {
                        return Err(BinaryReaderError::new(
                            format!(
                                "export `{}` for instance {} is not a {}",
                                name, instance_index, $ty
                            ),
                            offset,
                        ))
                    }
                }
            }};
        }

        match kind {
            ComponentExternalKind::Module => {
                check_max(
                    self.core_modules.len(),
                    1,
                    MAX_WASM_MODULES,
                    "modules",
                    offset,
                )?;
                push_component_export!(ComponentEntityType::Module, core_modules, "module")
            }
            ComponentExternalKind::Component => {
                check_max(
                    self.components.len(),
                    1,
                    MAX_WASM_COMPONENTS,
                    "components",
                    offset,
                )?;
                push_component_export!(ComponentEntityType::Component, components, "component")
            }
            ComponentExternalKind::Instance => {
                check_max(
                    self.instance_count(),
                    1,
                    MAX_WASM_INSTANCES,
                    "instances",
                    offset,
                )?;
                push_component_export!(ComponentEntityType::Instance, instances, "instance")
            }
            ComponentExternalKind::Func => {
                check_max(
                    self.function_count(),
                    1,
                    MAX_WASM_FUNCTIONS,
                    "functions",
                    offset,
                )?;
                push_component_export!(ComponentEntityType::Func, funcs, "function")
            }
            ComponentExternalKind::Value => {
                check_max(self.values.len(), 1, MAX_WASM_VALUES, "values", offset)?;
                match self.instance_export(instance_index, name, types, offset)? {
                    ComponentEntityType::Value(ty) => {
                        self.values.push((*ty, false));
                        Ok(())
                    }
                    _ => {
                        return Err(BinaryReaderError::new(
                            format!(
                                "export `{}` for instance {} is not a value",
                                name, instance_index
                            ),
                            offset,
                        ))
                    }
                }
            }
            ComponentExternalKind::Type => {
                check_max(self.type_count(), 1, MAX_WASM_TYPES, "types", offset)?;
                push_component_export!(ComponentEntityType::Type, types, "type")
            }
        }
    }

    fn alias_module(components: &mut [Self], count: u32, index: u32, offset: usize) -> Result<()> {
        let component = Self::check_alias_count(components, count, offset)?;
        let ty = component.module_at(index, offset)?;

        let current = components.last_mut().unwrap();
        check_max(
            current.core_modules.len(),
            1,
            MAX_WASM_MODULES,
            "modules",
            offset,
        )?;

        current.core_modules.push(ty);
        Ok(())
    }

    fn alias_component(
        components: &mut [Self],
        count: u32,
        index: u32,
        offset: usize,
    ) -> Result<()> {
        let component = Self::check_alias_count(components, count, offset)?;
        let ty = component.component_at(index, offset)?;

        let current = components.last_mut().unwrap();
        check_max(
            current.components.len(),
            1,
            MAX_WASM_COMPONENTS,
            "components",
            offset,
        )?;

        current.components.push(ty);
        Ok(())
    }

    fn alias_core_type(
        components: &mut [Self],
        count: u32,
        index: u32,
        offset: usize,
    ) -> Result<()> {
        let component = Self::check_alias_count(components, count, offset)?;
        let ty = component.type_at(index, true, offset)?;

        let current = components.last_mut().unwrap();
        check_max(current.type_count(), 1, MAX_WASM_TYPES, "types", offset)?;

        current.core_types.push(ty);
        Ok(())
    }

    fn alias_type(components: &mut [Self], count: u32, index: u32, offset: usize) -> Result<()> {
        let component = Self::check_alias_count(components, count, offset)?;
        let ty = component.type_at(index, false, offset)?;

        let current = components.last_mut().unwrap();
        check_max(current.type_count(), 1, MAX_WASM_TYPES, "types", offset)?;

        current.types.push(ty);
        Ok(())
    }

    fn check_alias_count(components: &[Self], count: u32, offset: usize) -> Result<&Self> {
        let count = count as usize;
        if count >= components.len() {
            return Err(BinaryReaderError::new(
                format!("invalid outer alias count of {}", count),
                offset,
            ));
        }

        Ok(&components[components.len() - count - 1])
    }

    fn create_defined_type(
        &self,
        ty: crate::ComponentDefinedType,
        types: &TypeList,
        offset: usize,
    ) -> Result<ComponentDefinedType> {
        match ty {
            crate::ComponentDefinedType::Primitive(ty) => Ok(ComponentDefinedType::Primitive(ty)),
            crate::ComponentDefinedType::Record(fields) => {
                self.create_record_type(fields.as_ref(), types, offset)
            }
            crate::ComponentDefinedType::Variant(cases) => {
                self.create_variant_type(cases.as_ref(), types, offset)
            }
            crate::ComponentDefinedType::List(ty) => Ok(ComponentDefinedType::List(
                self.create_component_val_type(ty, types, offset)?,
            )),
            crate::ComponentDefinedType::Tuple(tys) => {
                self.create_tuple_type(tys.as_ref(), types, offset)
            }
            crate::ComponentDefinedType::Flags(names) => {
                self.create_flags_type(names.as_ref(), offset)
            }
            crate::ComponentDefinedType::Enum(cases) => {
                self.create_enum_type(cases.as_ref(), offset)
            }
            crate::ComponentDefinedType::Union(tys) => {
                self.create_union_type(tys.as_ref(), types, offset)
            }
            crate::ComponentDefinedType::Option(ty) => Ok(ComponentDefinedType::Option(
                self.create_component_val_type(ty, types, offset)?,
            )),
            crate::ComponentDefinedType::Expected { ok, error } => {
                Ok(ComponentDefinedType::Expected(
                    self.create_component_val_type(ok, types, offset)?,
                    self.create_component_val_type(error, types, offset)?,
                ))
            }
        }
    }

    fn create_record_type(
        &self,
        fields: &[(&str, crate::ComponentValType)],
        types: &TypeList,
        offset: usize,
    ) -> Result<ComponentDefinedType> {
        let mut type_size = 1;
        let mut field_map = IndexMap::with_capacity(fields.len());

        for (name, ty) in fields {
            Self::check_name(name, "record field", offset)?;
            let ty = self.create_component_val_type(*ty, types, offset)?;
            type_size = combine_type_sizes(type_size, ty.type_size(), offset)?;

            if field_map.insert(name.to_string(), ty).is_some() {
                return Err(BinaryReaderError::new(
                    format!("duplicate field named `{}` in record type", name),
                    offset,
                ));
            }
        }

        Ok(ComponentDefinedType::Record(RecordType {
            type_size,
            fields: field_map,
        }))
    }

    fn create_variant_type(
        &self,
        cases: &[crate::VariantCase],
        types: &TypeList,
        offset: usize,
    ) -> Result<ComponentDefinedType> {
        let mut type_size = 1;
        let mut case_map = IndexMap::with_capacity(cases.len());

        if cases.is_empty() {
            return Err(BinaryReaderError::new(
                "variant type must have at least one case",
                offset,
            ));
        }

        for (i, case) in cases.iter().enumerate() {
            Self::check_name(case.name, "variant case", offset)?;
            if let Some(refines) = case.refines {
                if refines >= i as u32 {
                    return Err(BinaryReaderError::new(
                        "variant case can only refine a previously defined case",
                        offset,
                    ));
                }
            }
            let ty = self.create_component_val_type(case.ty, types, offset)?;
            type_size = combine_type_sizes(type_size, ty.type_size(), offset)?;

            if case_map
                .insert(
                    case.name.to_string(),
                    VariantCase {
                        ty,
                        refines: case.refines.map(|i| cases[i as usize].name.to_string()),
                    },
                )
                .is_some()
            {
                return Err(BinaryReaderError::new(
                    format!("duplicate case named `{}` in variant type", case.name),
                    offset,
                ));
            }
        }

        Ok(ComponentDefinedType::Variant(VariantType {
            type_size,
            cases: case_map,
        }))
    }

    fn create_tuple_type(
        &self,
        tys: &[crate::ComponentValType],
        types: &TypeList,
        offset: usize,
    ) -> Result<ComponentDefinedType> {
        let mut type_size = 1;
        let types = tys
            .iter()
            .map(|ty| {
                let ty = self.create_component_val_type(*ty, types, offset)?;
                type_size = combine_type_sizes(type_size, ty.type_size(), offset)?;
                Ok(ty)
            })
            .collect::<Result<_>>()?;

        Ok(ComponentDefinedType::Tuple(TupleType { type_size, types }))
    }

    fn create_flags_type(&self, names: &[&str], offset: usize) -> Result<ComponentDefinedType> {
        let mut names_set = IndexSet::with_capacity(names.len());

        for name in names {
            Self::check_name(name, "flag", offset)?;
            if !names_set.insert(name.to_string()) {
                return Err(BinaryReaderError::new(
                    format!("duplicate flag named `{}`", name),
                    offset,
                ));
            }
        }

        Ok(ComponentDefinedType::Flags(names_set))
    }

    fn create_enum_type(&self, cases: &[&str], offset: usize) -> Result<ComponentDefinedType> {
        if cases.len() > u32::max_value() as usize {
            return Err(BinaryReaderError::new(
                "enumeration type cannot be represented with a 32-bit discriminant value",
                offset,
            ));
        }

        let mut tags = IndexSet::with_capacity(cases.len());

        for tag in cases {
            Self::check_name(tag, "enum tag", offset)?;
            if !tags.insert(tag.to_string()) {
                return Err(BinaryReaderError::new(
                    format!("duplicate enum tag named `{}`", tag),
                    offset,
                ));
            }
        }

        Ok(ComponentDefinedType::Enum(tags))
    }

    fn create_union_type(
        &self,
        tys: &[crate::ComponentValType],
        types: &TypeList,
        offset: usize,
    ) -> Result<ComponentDefinedType> {
        let mut type_size = 1;
        let types = tys
            .iter()
            .map(|ty| {
                let ty = self.create_component_val_type(*ty, types, offset)?;
                type_size = combine_type_sizes(type_size, ty.type_size(), offset)?;
                Ok(ty)
            })
            .collect::<Result<_>>()?;

        Ok(ComponentDefinedType::Union(UnionType { type_size, types }))
    }

    fn create_component_val_type(
        &self,
        ty: crate::ComponentValType,
        types: &TypeList,
        offset: usize,
    ) -> Result<ComponentValType> {
        Ok(match ty {
            crate::ComponentValType::Primitive(pt) => ComponentValType::Primitive(pt),
            crate::ComponentValType::Type(idx) => {
                ComponentValType::Type(self.defined_type_at(idx, types, offset)?)
            }
        })
    }

    pub fn type_at(&self, idx: u32, core: bool, offset: usize) -> Result<TypeId> {
        let types = if core { &self.core_types } else { &self.types };
        types.get(idx as usize).copied().ok_or_else(|| {
            BinaryReaderError::new(
                format!("unknown type {}: type index out of bounds", idx),
                offset,
            )
        })
    }

    fn function_type_at<'a>(
        &self,
        idx: u32,
        types: &'a TypeList,
        offset: usize,
    ) -> Result<&'a ComponentFuncType> {
        types[self.type_at(idx, false, offset)?]
            .as_component_func_type()
            .ok_or_else(|| {
                BinaryReaderError::new(format!("type index {} is not a function type", idx), offset)
            })
    }

    fn function_at(&self, idx: u32, offset: usize) -> Result<TypeId> {
        self.funcs.get(idx as usize).copied().ok_or_else(|| {
            BinaryReaderError::new(
                format!("unknown function {}: function index out of bounds", idx),
                offset,
            )
        })
    }

    fn component_at(&self, idx: u32, offset: usize) -> Result<TypeId> {
        self.components.get(idx as usize).copied().ok_or_else(|| {
            BinaryReaderError::new(
                format!("unknown component {}: component index out of bounds", idx),
                offset,
            )
        })
    }

    fn instance_at(&self, idx: u32, offset: usize) -> Result<TypeId> {
        self.instances.get(idx as usize).copied().ok_or_else(|| {
            BinaryReaderError::new(
                format!("unknown instance {}: instance index out of bounds", idx),
                offset,
            )
        })
    }

    fn instance_export<'a>(
        &self,
        instance_index: u32,
        name: &str,
        types: &'a TypeList,
        offset: usize,
    ) -> Result<&'a ComponentEntityType> {
        match types[self.instance_at(instance_index, offset)?]
            .as_component_instance_type()
            .unwrap()
            .exports(types)
            .get(name)
        {
            Some(export) => Ok(export),
            None => {
                return Err(BinaryReaderError::new(
                    format!("instance {} has no export named `{}`", instance_index, name),
                    offset,
                ))
            }
        }
    }

    fn value_at(&mut self, idx: u32, offset: usize) -> Result<&ComponentValType> {
        match self.values.get_mut(idx as usize) {
            Some((ty, used)) if !*used => {
                *used = true;
                Ok(ty)
            }
            Some(_) => Err(BinaryReaderError::new(
                format!("value {} cannot be used more than once", idx),
                offset,
            )),
            None => Err(BinaryReaderError::new(
                format!("unknown value {}: value index out of bounds", idx),
                offset,
            )),
        }
    }

    fn defined_type_at(&self, idx: u32, types: &TypeList, offset: usize) -> Result<TypeId> {
        let id = self.type_at(idx, false, offset)?;
        match &types[id] {
            Type::Defined(_) => Ok(id),
            _ => Err(BinaryReaderError::new(
                format!("type index {} is not a defined type", id.index),
                offset,
            )),
        }
    }

    fn core_function_at(&self, idx: u32, offset: usize) -> Result<TypeId> {
        match self.core_funcs.get(idx as usize) {
            Some(id) => Ok(*id),
            None => Err(BinaryReaderError::new(
                format!(
                    "unknown core function {}: function index out of bounds",
                    idx
                ),
                offset,
            )),
        }
    }

    fn module_at(&self, idx: u32, offset: usize) -> Result<TypeId> {
        match self.core_modules.get(idx as usize) {
            Some(id) => Ok(*id),
            None => Err(BinaryReaderError::new(
                format!("unknown module {}: module index out of bounds", idx),
                offset,
            )),
        }
    }

    fn core_instance_at(&self, idx: u32, offset: usize) -> Result<TypeId> {
        match self.core_instances.get(idx as usize) {
            Some(id) => Ok(*id),
            None => Err(BinaryReaderError::new(
                format!(
                    "unknown core instance {}: instance index out of bounds",
                    idx
                ),
                offset,
            )),
        }
    }

    fn core_instance_export<'a>(
        &self,
        instance_index: u32,
        name: &str,
        types: &'a TypeList,
        offset: usize,
    ) -> Result<&'a EntityType> {
        match types[self.core_instance_at(instance_index, offset)?]
            .as_instance_type()
            .unwrap()
            .exports(types)
            .get(name)
        {
            Some(export) => Ok(export),
            None => {
                return Err(BinaryReaderError::new(
                    format!(
                        "core instance {} has no export named `{}`",
                        instance_index, name
                    ),
                    offset,
                ))
            }
        }
    }

    fn global_at(&self, idx: u32, offset: usize) -> Result<&GlobalType> {
        match self.core_globals.get(idx as usize) {
            Some(t) => Ok(t),
            None => Err(BinaryReaderError::new(
                format!("unknown global {}: global index out of bounds", idx,),
                offset,
            )),
        }
    }

    fn table_at(&self, idx: u32, offset: usize) -> Result<&TableType> {
        match self.core_tables.get(idx as usize) {
            Some(t) => Ok(t),
            None => Err(BinaryReaderError::new(
                format!("unknown table {}: table index out of bounds", idx),
                offset,
            )),
        }
    }

    fn memory_at(&self, idx: u32, offset: usize) -> Result<&MemoryType> {
        match self.core_memories.get(idx as usize) {
            Some(t) => Ok(t),
            None => Err(BinaryReaderError::new(
                format!("unknown memory {}: memory index out of bounds", idx,),
                offset,
            )),
        }
    }
}

impl Default for ComponentState {
    fn default() -> Self {
        Self {
            core_types: Default::default(),
            core_modules: Default::default(),
            core_instances: Default::default(),
            core_funcs: Default::default(),
            core_memories: Default::default(),
            core_tables: Default::default(),
            core_globals: Default::default(),
            core_tags: Default::default(),
            types: Default::default(),
            funcs: Default::default(),
            values: Default::default(),
            instances: Default::default(),
            components: Default::default(),
            imports: Default::default(),
            exports: Default::default(),
            has_start: Default::default(),
            type_size: 1,
        }
    }
}
