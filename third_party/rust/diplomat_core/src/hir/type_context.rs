//! Store all the types contained in the HIR.

use super::{EnumDef, LoweringError, OpaqueDef, OutStructDef, StructDef, TypeDef, TypeLowerer};
#[allow(unused_imports)] // use in docs links
use crate::hir;
use crate::{ast, Env};
use smallvec::SmallVec;
use std::collections::HashMap;
use std::ops::Index;

/// A context type owning all types exposed to Diplomat.
#[derive(Debug)]
pub struct TypeContext {
    out_structs: Vec<OutStructDef>,
    structs: Vec<StructDef>,
    opaques: Vec<OpaqueDef>,
    enums: Vec<EnumDef>,
}

/// Key used to index into a [`TypeContext`] representing a struct.
#[derive(Copy, Clone, Debug)]
pub struct StructId(usize);

/// Key used to index into a [`TypeContext`] representing an out struct.
#[derive(Copy, Clone, Debug)]
pub struct OutStructId(usize);

/// Key used to index into a [`TypeContext`] representing a opaque.
#[derive(Copy, Clone, Debug)]
pub struct OpaqueId(usize);

/// Key used to index into a [`TypeContext`] representing an enum.
#[derive(Copy, Clone, Debug)]
pub struct EnumId(usize);

#[derive(Copy, Clone, Debug)]
pub enum TypeId {
    Struct(StructId),
    OutStruct(OutStructId),
    Opaque(OpaqueId),
    Enum(EnumId),
}

impl TypeContext {
    pub fn all_types<'tcx>(&'tcx self) -> impl Iterator<Item = (TypeId, TypeDef<'tcx>)> {
        self.structs
            .iter()
            .enumerate()
            .map(|(i, ty)| (TypeId::Struct(StructId(i)), TypeDef::Struct(ty)))
            .chain(
                self.out_structs
                    .iter()
                    .enumerate()
                    .map(|(i, ty)| (TypeId::OutStruct(OutStructId(i)), TypeDef::OutStruct(ty))),
            )
            .chain(
                self.opaques
                    .iter()
                    .enumerate()
                    .map(|(i, ty)| (TypeId::Opaque(OpaqueId(i)), TypeDef::Opaque(ty))),
            )
            .chain(
                self.enums
                    .iter()
                    .enumerate()
                    .map(|(i, ty)| (TypeId::Enum(EnumId(i)), TypeDef::Enum(ty))),
            )
    }

    pub fn out_structs(&self) -> &[OutStructDef] {
        &self.out_structs
    }

    pub fn structs(&self) -> &[StructDef] {
        &self.structs
    }

    pub fn opaques(&self) -> &[OpaqueDef] {
        &self.opaques
    }

    pub fn enums(&self) -> &[EnumDef] {
        &self.enums
    }

    pub fn resolve_type<'tcx>(&'tcx self, id: TypeId) -> TypeDef<'tcx> {
        match id {
            TypeId::Struct(i) => TypeDef::Struct(self.resolve_struct(i)),
            TypeId::OutStruct(i) => TypeDef::OutStruct(self.resolve_out_struct(i)),
            TypeId::Opaque(i) => TypeDef::Opaque(self.resolve_opaque(i)),
            TypeId::Enum(i) => TypeDef::Enum(self.resolve_enum(i)),
        }
    }

    pub(crate) fn resolve_out_struct(&self, id: OutStructId) -> &OutStructDef {
        self.out_structs.index(id.0)
    }

    pub(crate) fn resolve_struct(&self, id: StructId) -> &StructDef {
        self.structs.index(id.0)
    }

    pub(crate) fn resolve_opaque(&self, id: OpaqueId) -> &OpaqueDef {
        self.opaques.index(id.0)
    }

    pub(crate) fn resolve_enum(&self, id: EnumId) -> &EnumDef {
        self.enums.index(id.0)
    }

    /// Lower the AST to the HIR while simultaneously performing validation.
    pub fn from_ast(env: &Env) -> Result<Self, Vec<LoweringError>> {
        let mut ast_out_structs = SmallVec::<[_; 16]>::new();
        let mut ast_structs = SmallVec::<[_; 16]>::new();
        let mut ast_opaques = SmallVec::<[_; 16]>::new();
        let mut ast_enums = SmallVec::<[_; 16]>::new();

        let mut errors = Vec::with_capacity(0);

        for (path, _, sym) in env.iter_items() {
            if let ast::ModSymbol::CustomType(custom_type) = sym {
                match custom_type {
                    ast::CustomType::Struct(strct) => {
                        if strct.output_only {
                            ast_out_structs.push((path, strct));
                        } else {
                            ast_structs.push((path, strct));
                        }
                    }
                    ast::CustomType::Opaque(opaque) => ast_opaques.push((path, opaque)),
                    ast::CustomType::Enum(enm) => ast_enums.push((path, enm)),
                }
            }
        }

        let lookup_id = LookupId::new(
            &ast_out_structs[..],
            &ast_structs[..],
            &ast_opaques[..],
            &ast_enums[..],
        );

        let out_structs =
            OutStructDef::lower_all(&ast_out_structs[..], &lookup_id, env, &mut errors);
        let structs = StructDef::lower_all(&ast_structs[..], &lookup_id, env, &mut errors);
        let opaques = OpaqueDef::lower_all(&ast_opaques[..], &lookup_id, env, &mut errors);
        let enums = EnumDef::lower_all(&ast_enums[..], &lookup_id, env, &mut errors);

        match (out_structs, structs, opaques, enums) {
            (Some(out_structs), Some(structs), Some(opaques), Some(enums)) => {
                assert!(
                    errors.is_empty(),
                    "All lowering succeeded but still found error messages: {errors:?}"
                );
                Ok(Self {
                    out_structs,
                    structs,
                    opaques,
                    enums,
                })
            }
            _ => {
                assert!(!errors.is_empty(), "Lowering failed without error messages");
                Err(errors)
            }
        }
    }
}

/// Struct that just wraps the mapping from AST custom types to their IDs that
/// will show up in the final [`TypeContext`].
///
/// The entire point of this type is to reduce the number of arguments in helper
/// functions which need to look up IDs for structs. It does nothing fancy and
/// is only ever used when constructing a [`TypeContext`].
pub(super) struct LookupId<'ast> {
    out_struct_map: HashMap<&'ast ast::Struct, OutStructId>,
    struct_map: HashMap<&'ast ast::Struct, StructId>,
    opaque_map: HashMap<&'ast ast::OpaqueStruct, OpaqueId>,
    enum_map: HashMap<&'ast ast::Enum, EnumId>,
}

impl<'ast> LookupId<'ast> {
    /// Returns a new [`LookupId`].
    fn new(
        out_structs: &[(&ast::Path, &'ast ast::Struct)],
        structs: &[(&ast::Path, &'ast ast::Struct)],
        opaques: &[(&ast::Path, &'ast ast::OpaqueStruct)],
        enums: &[(&ast::Path, &'ast ast::Enum)],
    ) -> Self {
        Self {
            out_struct_map: out_structs
                .iter()
                .enumerate()
                .map(|(index, (_, strct))| (*strct, OutStructId(index)))
                .collect(),
            struct_map: structs
                .iter()
                .enumerate()
                .map(|(index, (_, strct))| (*strct, StructId(index)))
                .collect(),
            opaque_map: opaques
                .iter()
                .enumerate()
                .map(|(index, (_, opaque))| (*opaque, OpaqueId(index)))
                .collect(),
            enum_map: enums
                .iter()
                .enumerate()
                .map(|(index, (_, enm))| (*enm, EnumId(index)))
                .collect(),
        }
    }

    pub(super) fn resolve_out_struct(&self, strct: &ast::Struct) -> Option<OutStructId> {
        self.out_struct_map.get(strct).copied()
    }

    pub(super) fn resolve_struct(&self, strct: &ast::Struct) -> Option<StructId> {
        self.struct_map.get(strct).copied()
    }

    pub(super) fn resolve_opaque(&self, opaque: &ast::OpaqueStruct) -> Option<OpaqueId> {
        self.opaque_map.get(opaque).copied()
    }

    pub(super) fn resolve_enum(&self, enm: &ast::Enum) -> Option<EnumId> {
        self.enum_map.get(enm).copied()
    }
}

impl From<StructId> for TypeId {
    fn from(x: StructId) -> Self {
        TypeId::Struct(x)
    }
}

impl From<OutStructId> for TypeId {
    fn from(x: OutStructId) -> Self {
        TypeId::OutStruct(x)
    }
}

impl From<OpaqueId> for TypeId {
    fn from(x: OpaqueId) -> Self {
        TypeId::Opaque(x)
    }
}

impl From<EnumId> for TypeId {
    fn from(x: EnumId) -> Self {
        TypeId::Enum(x)
    }
}
