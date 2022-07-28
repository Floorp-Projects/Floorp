use super::*;

impl Component {
    /// Encode this Wasm component into bytes.
    pub fn to_bytes(&self) -> Vec<u8> {
        self.encoded().finish()
    }

    fn encoded(&self) -> wasm_encoder::Component {
        let mut component = wasm_encoder::Component::new();
        for section in &self.sections {
            section.encode(&mut component);
        }
        component
    }
}

impl Section {
    fn encode(&self, component: &mut wasm_encoder::Component) {
        match self {
            Self::Custom(sec) => sec.encode(component),
            Self::CoreModule(module) => {
                let bytes = module.to_bytes();
                component.section(&wasm_encoder::RawSection {
                    id: wasm_encoder::ComponentSectionId::CoreModule as u8,
                    data: &bytes,
                });
            }
            Self::CoreInstance(_) => todo!(),
            Self::CoreAlias(_) => todo!(),
            Self::CoreType(sec) => sec.encode(component),
            Self::Component(comp) => {
                let bytes = comp.to_bytes();
                component.section(&wasm_encoder::RawSection {
                    id: wasm_encoder::ComponentSectionId::Component as u8,
                    data: &bytes,
                });
            }
            Self::Instance(_) => todo!(),
            Self::Alias(_) => todo!(),
            Self::Type(sec) => sec.encode(component),
            Self::Canonical(sec) => sec.encode(component),
            Self::Start(_) => todo!(),
            Self::Import(sec) => sec.encode(component),
            Self::Export(_) => todo!(),
        }
    }
}

impl CustomSection {
    fn encode(&self, component: &mut wasm_encoder::Component) {
        component.section(&wasm_encoder::CustomSection {
            name: &self.name,
            data: &self.data,
        });
    }
}

impl TypeSection {
    fn encode(&self, component: &mut wasm_encoder::Component) {
        let mut sec = wasm_encoder::ComponentTypeSection::new();
        for ty in &self.types {
            ty.encode(sec.ty());
        }
        component.section(&sec);
    }
}

impl ImportSection {
    fn encode(&self, component: &mut wasm_encoder::Component) {
        let mut sec = wasm_encoder::ComponentImportSection::new();
        for imp in &self.imports {
            sec.import(&imp.name, imp.ty);
        }
        component.section(&sec);
    }
}

impl CanonicalSection {
    fn encode(&self, component: &mut wasm_encoder::Component) {
        let mut sec = wasm_encoder::CanonicalFunctionSection::new();
        for func in &self.funcs {
            match func {
                Func::CanonLift {
                    func_ty,
                    options,
                    core_func_index,
                } => {
                    let options = translate_canon_opt(options);
                    sec.lift(*core_func_index, *func_ty, options);
                }
                Func::CanonLower {
                    options,
                    func_index,
                } => {
                    let options = translate_canon_opt(options);
                    sec.lower(*func_index, options);
                }
            }
        }
        component.section(&sec);
    }
}

impl CoreTypeSection {
    fn encode(&self, component: &mut wasm_encoder::Component) {
        let mut sec = wasm_encoder::CoreTypeSection::new();
        for ty in &self.types {
            ty.encode(sec.ty());
        }
        component.section(&sec);
    }
}

impl CoreType {
    fn encode(&self, enc: wasm_encoder::CoreTypeEncoder<'_>) {
        match self {
            Self::Func(ty) => {
                enc.function(ty.params.iter().copied(), ty.results.iter().copied());
            }
            Self::Module(mod_ty) => {
                let mut enc_mod_ty = wasm_encoder::ModuleType::new();
                for def in &mod_ty.defs {
                    match def {
                        ModuleTypeDef::TypeDef(crate::core::Type::Func(func_ty)) => {
                            enc_mod_ty.ty().function(
                                func_ty.params.iter().copied(),
                                func_ty.results.iter().copied(),
                            );
                        }
                        ModuleTypeDef::Alias(alias) => match alias {
                            CoreAlias::Outer {
                                count,
                                i,
                                kind: CoreOuterAliasKind::Type(_),
                            } => {
                                enc_mod_ty.alias_outer_core_type(*count, *i);
                            }
                            CoreAlias::InstanceExport { .. } => unreachable!(),
                        },
                        ModuleTypeDef::Import(imp) => {
                            enc_mod_ty.import(
                                &imp.module,
                                &imp.field,
                                crate::core::encode::translate_entity_type(&imp.entity_type),
                            );
                        }
                        ModuleTypeDef::Export(name, ty) => {
                            enc_mod_ty.export(name, crate::core::encode::translate_entity_type(ty));
                        }
                    }
                }
                enc.module(&enc_mod_ty);
            }
        }
    }
}

impl Type {
    fn encode(&self, enc: wasm_encoder::ComponentTypeEncoder<'_>) {
        match self {
            Self::Defined(ty) => {
                ty.encode(enc.defined_type());
            }
            Self::Func(func_ty) => {
                enc.function(
                    func_ty.params.iter().map(translate_optional_named_type),
                    func_ty.result,
                );
            }
            Self::Component(comp_ty) => {
                let mut enc_comp_ty = wasm_encoder::ComponentType::new();
                for def in &comp_ty.defs {
                    match def {
                        ComponentTypeDef::Import(imp) => {
                            enc_comp_ty.import(&imp.name, imp.ty);
                        }
                        ComponentTypeDef::CoreType(ty) => {
                            ty.encode(enc_comp_ty.core_type());
                        }
                        ComponentTypeDef::Type(ty) => {
                            ty.encode(enc_comp_ty.ty());
                        }
                        ComponentTypeDef::Export { name, ty } => {
                            enc_comp_ty.export(name, *ty);
                        }
                        ComponentTypeDef::Alias(Alias::Outer {
                            count,
                            i,
                            kind: OuterAliasKind::Type(_),
                        }) => {
                            enc_comp_ty.alias_outer_type(*count, *i);
                        }
                        ComponentTypeDef::Alias(Alias::Outer {
                            count,
                            i,
                            kind: OuterAliasKind::CoreType(_),
                        }) => {
                            enc_comp_ty.alias_outer_core_type(*count, *i);
                        }
                        ComponentTypeDef::Alias(_) => unreachable!(),
                    }
                }
                enc.component(&enc_comp_ty);
            }
            Self::Instance(inst_ty) => {
                let mut enc_inst_ty = wasm_encoder::InstanceType::new();
                for def in &inst_ty.defs {
                    match def {
                        InstanceTypeDef::CoreType(ty) => {
                            ty.encode(enc_inst_ty.core_type());
                        }
                        InstanceTypeDef::Type(ty) => {
                            ty.encode(enc_inst_ty.ty());
                        }
                        InstanceTypeDef::Export { name, ty } => {
                            enc_inst_ty.export(name, *ty);
                        }
                        InstanceTypeDef::Alias(Alias::Outer {
                            count,
                            i,
                            kind: OuterAliasKind::Type(_),
                        }) => {
                            enc_inst_ty.alias_outer_type(*count, *i);
                        }
                        InstanceTypeDef::Alias(Alias::Outer {
                            count,
                            i,
                            kind: OuterAliasKind::CoreType(_),
                        }) => {
                            enc_inst_ty.alias_outer_core_type(*count, *i);
                        }
                        InstanceTypeDef::Alias(_) => unreachable!(),
                    }
                }
                enc.instance(&enc_inst_ty);
            }
        }
    }
}

impl DefinedType {
    fn encode(&self, enc: wasm_encoder::ComponentDefinedTypeEncoder<'_>) {
        match self {
            Self::Primitive(ty) => enc.primitive(*ty),
            Self::Record(ty) => {
                enc.record(ty.fields.iter().map(translate_named_type));
            }
            Self::Variant(ty) => {
                enc.variant(
                    ty.cases
                        .iter()
                        .map(|(ty, refines)| (ty.name.as_str(), ty.ty, *refines)),
                );
            }
            Self::List(ty) => {
                enc.list(ty.elem_ty);
            }
            Self::Tuple(ty) => {
                enc.tuple(ty.fields.iter().copied());
            }
            Self::Flags(ty) => {
                enc.flags(ty.fields.iter().map(|f| f.as_str()));
            }
            Self::Enum(ty) => {
                enc.enum_type(ty.variants.iter().map(|v| v.as_str()));
            }
            Self::Union(ty) => {
                enc.union(ty.variants.iter().copied());
            }
            Self::Option(ty) => {
                enc.option(ty.inner_ty);
            }
            Self::Expected(ty) => {
                enc.expected(ty.ok_ty, ty.err_ty);
            }
        }
    }
}

fn translate_named_type(ty: &NamedType) -> (&str, ComponentValType) {
    (&ty.name, ty.ty)
}

fn translate_optional_named_type(ty: &OptionalNamedType) -> (Option<&str>, ComponentValType) {
    (ty.name.as_deref(), ty.ty)
}

fn translate_canon_opt(options: &[CanonOpt]) -> Vec<wasm_encoder::CanonicalOption> {
    options
        .iter()
        .map(|o| match o {
            CanonOpt::StringUtf8 => wasm_encoder::CanonicalOption::UTF8,
            CanonOpt::StringUtf16 => wasm_encoder::CanonicalOption::UTF16,
            CanonOpt::StringLatin1Utf16 => wasm_encoder::CanonicalOption::CompactUTF16,
            CanonOpt::Memory(idx) => wasm_encoder::CanonicalOption::Memory(*idx),
            CanonOpt::Realloc(idx) => wasm_encoder::CanonicalOption::Realloc(*idx),
            CanonOpt::PostReturn(idx) => wasm_encoder::CanonicalOption::PostReturn(*idx),
        })
        .collect()
}
