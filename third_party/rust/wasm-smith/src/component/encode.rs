use std::borrow::Cow;

use super::*;
use wasm_encoder::{ComponentExportKind, ComponentOuterAliasKind, ExportKind};
use wasmparser::names::KebabStr;

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
            name: (&self.name).into(),
            data: Cow::Borrowed(&self.data),
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
            sec.import(wasm_encoder::ComponentExternName::Kebab(&imp.name), imp.ty);
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
                        ModuleTypeDef::OuterAlias { count, i, kind } => match kind {
                            CoreOuterAliasKind::Type(_) => {
                                enc_mod_ty.alias_outer_core_type(*count, *i);
                            }
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
                let mut f = enc.function();

                f.params(func_ty.params.iter().map(|(name, ty)| (name.as_str(), *ty)));

                if let Some(ty) = func_ty.unnamed_result_ty() {
                    f.result(ty);
                } else {
                    f.results(
                        func_ty.results.iter().map(|(name, ty)| {
                            (name.as_deref().map(KebabStr::as_str).unwrap(), *ty)
                        }),
                    );
                }
            }
            Self::Component(comp_ty) => {
                let mut enc_comp_ty = wasm_encoder::ComponentType::new();
                for def in &comp_ty.defs {
                    match def {
                        ComponentTypeDef::Import(imp) => {
                            enc_comp_ty.import(
                                wasm_encoder::ComponentExternName::Kebab(&imp.name),
                                imp.ty,
                            );
                        }
                        ComponentTypeDef::CoreType(ty) => {
                            ty.encode(enc_comp_ty.core_type());
                        }
                        ComponentTypeDef::Type(ty) => {
                            ty.encode(enc_comp_ty.ty());
                        }
                        ComponentTypeDef::Export { name, url: _, ty } => {
                            enc_comp_ty.export(wasm_encoder::ComponentExternName::Kebab(name), *ty);
                        }
                        ComponentTypeDef::Alias(a) => {
                            enc_comp_ty.alias(translate_alias(a));
                        }
                    }
                }
                enc.component(&enc_comp_ty);
            }
            Self::Instance(inst_ty) => {
                let mut enc_inst_ty = wasm_encoder::InstanceType::new();
                for def in &inst_ty.defs {
                    match def {
                        InstanceTypeDecl::CoreType(ty) => {
                            ty.encode(enc_inst_ty.core_type());
                        }
                        InstanceTypeDecl::Type(ty) => {
                            ty.encode(enc_inst_ty.ty());
                        }
                        InstanceTypeDecl::Export { name, url: _, ty } => {
                            enc_inst_ty.export(wasm_encoder::ComponentExternName::Kebab(name), *ty);
                        }
                        InstanceTypeDecl::Alias(a) => {
                            enc_inst_ty.alias(translate_alias(a));
                        }
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
                enc.record(ty.fields.iter().map(|(name, ty)| (name.as_str(), *ty)));
            }
            Self::Variant(ty) => {
                enc.variant(
                    ty.cases
                        .iter()
                        .map(|(name, ty, refines)| (name.as_str(), *ty, *refines)),
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
            Self::Option(ty) => {
                enc.option(ty.inner_ty);
            }
            Self::Result(ty) => {
                enc.result(ty.ok_ty, ty.err_ty);
            }
        }
    }
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

fn translate_alias(alias: &Alias) -> wasm_encoder::Alias<'_> {
    match alias {
        Alias::InstanceExport {
            instance,
            name,
            kind,
        } => wasm_encoder::Alias::InstanceExport {
            instance: *instance,
            name,
            kind: match kind {
                InstanceExportAliasKind::Module => ComponentExportKind::Module,
                InstanceExportAliasKind::Component => ComponentExportKind::Component,
                InstanceExportAliasKind::Instance => ComponentExportKind::Instance,
                InstanceExportAliasKind::Func => ComponentExportKind::Func,
                InstanceExportAliasKind::Value => ComponentExportKind::Value,
            },
        },
        Alias::CoreInstanceExport {
            instance,
            name,
            kind,
        } => wasm_encoder::Alias::CoreInstanceExport {
            instance: *instance,
            name,
            kind: match kind {
                CoreInstanceExportAliasKind::Func => ExportKind::Func,
                CoreInstanceExportAliasKind::Table => ExportKind::Table,
                CoreInstanceExportAliasKind::Global => ExportKind::Global,
                CoreInstanceExportAliasKind::Memory => ExportKind::Memory,
                CoreInstanceExportAliasKind::Tag => ExportKind::Tag,
            },
        },
        Alias::Outer { count, i, kind } => wasm_encoder::Alias::Outer {
            count: *count,
            index: *i,
            kind: match kind {
                OuterAliasKind::Module => ComponentOuterAliasKind::CoreModule,
                OuterAliasKind::Component => ComponentOuterAliasKind::Component,
                OuterAliasKind::Type(_) => ComponentOuterAliasKind::Type,
                OuterAliasKind::CoreType(_) => ComponentOuterAliasKind::CoreType,
            },
        },
    }
}
