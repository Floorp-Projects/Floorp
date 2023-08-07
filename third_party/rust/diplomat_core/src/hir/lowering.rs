use super::{
    Borrow, EnumDef, EnumPath, EnumVariant, IdentBuf, LifetimeEnv, LifetimeLowerer, LookupId,
    MaybeOwn, Method, NonOptional, OpaqueDef, OpaquePath, Optional, OutStructDef, OutStructField,
    OutStructPath, OutType, Param, ParamLifetimeLowerer, ParamSelf, PrimitiveType,
    ReturnLifetimeLowerer, ReturnType, ReturnableStructPath, SelfParamLifetimeLowerer, SelfType,
    Slice, StructDef, StructField, StructPath, SuccessType, Type,
};
use crate::{ast, Env};
use core::fmt;
use strck_ident::IntoCk;

/// An error from lowering the AST to the HIR.
#[derive(Debug)]
pub enum LoweringError {
    /// The purpose of having this is that translating to the HIR has enormous
    /// potential for really detailed error handling and giving suggestions.
    ///
    /// Unfortunately, working out what the error enum should look like to enable
    /// this is really hard. The plan is that once the lowering code is completely
    /// written, we ctrl+F for `"LoweringError::Other"` in the lowering code, and turn every
    /// instance into an specialized enum variant, generalizing where possible
    /// without losing any information.
    Other(String),
}

impl fmt::Display for LoweringError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Self::Other(ref s) => s.fmt(f),
        }
    }
}

/// Lowers an [`ast::Ident`]s into an [`hir::IdentBuf`].
///
/// If there are any errors, they're pushed to `errors` and `None` is returned.
pub fn lower_ident(
    ident: &ast::Ident,
    context: &'static str,
    errors: &mut Vec<LoweringError>,
) -> Option<IdentBuf> {
    match ident.as_str().ck() {
        Ok(name) => Some(name.to_owned()),
        Err(e) => {
            errors.push(LoweringError::Other(format!(
                "Ident `{ident}` from {context} could not be turned into a Rust ident: {e}"
            )));
            None
        }
    }
}

/// Lowers an AST definition.
pub(super) trait TypeLowerer: Sized {
    /// Type of the AST definition that gets lowered.
    type AstDef;

    /// Lowers the AST definition into `Self`.
    ///
    /// If there are any errors, they're pushed to `errors` and `None` is returned.
    fn lower(
        ast_def: &Self::AstDef,
        lookup_id: &LookupId,
        in_path: &ast::Path,
        env: &Env,
        errors: &mut Vec<LoweringError>,
    ) -> Option<Self>;

    /// Lowers multiple items at once
    fn lower_all(
        ast_defs: &[(&ast::Path, &Self::AstDef)],
        lookup_id: &LookupId,
        env: &Env,
        errors: &mut Vec<LoweringError>,
    ) -> Option<Vec<Self>> {
        let mut hir_types = Some(Vec::with_capacity(ast_defs.len()));

        for (in_path, ast_def) in ast_defs {
            let hir_type = Self::lower(ast_def, lookup_id, in_path, env, errors);

            match (hir_type, &mut hir_types) {
                (Some(hir_type), Some(hir_types)) => hir_types.push(hir_type),
                _ => hir_types = None,
            }
        }

        hir_types
    }
}

impl TypeLowerer for EnumDef {
    type AstDef = ast::Enum;

    fn lower(
        ast_enum: &Self::AstDef,
        lookup_id: &LookupId,
        in_path: &ast::Path,
        env: &Env,
        errors: &mut Vec<LoweringError>,
    ) -> Option<Self> {
        let name = lower_ident(&ast_enum.name, "enum name", errors);

        let mut variants = Some(Vec::with_capacity(ast_enum.variants.len()));

        for (ident, discriminant, docs) in ast_enum.variants.iter() {
            let name = lower_ident(ident, "enum variant", errors);

            match (name, &mut variants) {
                (Some(name), Some(variants)) => {
                    variants.push(EnumVariant {
                        docs: docs.clone(),
                        name,
                        discriminant: *discriminant,
                    });
                }
                _ => variants = None,
            }
        }

        let methods = lower_all_methods(&ast_enum.methods[..], lookup_id, in_path, env, errors);

        Some(EnumDef::new(
            ast_enum.docs.clone(),
            name?,
            variants?,
            methods?,
        ))
    }
}

impl TypeLowerer for OpaqueDef {
    type AstDef = ast::OpaqueStruct;

    fn lower(
        ast_opaque: &Self::AstDef,
        lookup_id: &LookupId,
        in_path: &ast::Path,
        env: &Env,
        errors: &mut Vec<LoweringError>,
    ) -> Option<Self> {
        let name = lower_ident(&ast_opaque.name, "opaque name", errors);

        let methods = lower_all_methods(&ast_opaque.methods[..], lookup_id, in_path, env, errors);

        Some(OpaqueDef::new(ast_opaque.docs.clone(), name?, methods?))
    }
}

impl TypeLowerer for StructDef {
    type AstDef = ast::Struct;

    fn lower(
        ast_struct: &Self::AstDef,
        lookup_id: &LookupId,
        in_path: &ast::Path,
        env: &Env,
        errors: &mut Vec<LoweringError>,
    ) -> Option<Self> {
        let name = lower_ident(&ast_struct.name, "struct name", errors);

        let fields = if ast_struct.fields.is_empty() {
            errors.push(LoweringError::Other(format!(
                "struct `{}` is a ZST because it has no fields",
                ast_struct.name
            )));
            None
        } else {
            let mut fields = Some(Vec::with_capacity(ast_struct.fields.len()));

            for (name, ty, docs) in ast_struct.fields.iter() {
                let name = lower_ident(name, "struct field name", errors);
                let ty = lower_type(
                    ty,
                    Some(&mut &ast_struct.lifetimes),
                    lookup_id,
                    in_path,
                    env,
                    errors,
                );

                match (name, ty, &mut fields) {
                    (Some(name), Some(ty), Some(fields)) => fields.push(StructField {
                        docs: docs.clone(),
                        name,
                        ty,
                    }),
                    _ => fields = None,
                }
            }

            fields
        };

        let methods = lower_all_methods(&ast_struct.methods[..], lookup_id, in_path, env, errors);

        Some(StructDef::new(
            ast_struct.docs.clone(),
            name?,
            fields?,
            methods?,
        ))
    }
}

impl TypeLowerer for OutStructDef {
    type AstDef = ast::Struct;

    fn lower(
        ast_out_struct: &Self::AstDef,
        lookup_id: &LookupId,
        in_path: &ast::Path,
        env: &Env,
        errors: &mut Vec<LoweringError>,
    ) -> Option<Self> {
        let name = lower_ident(&ast_out_struct.name, "out-struct name", errors);

        let fields = if ast_out_struct.fields.is_empty() {
            errors.push(LoweringError::Other(format!(
                "struct `{}` is a ZST because it has no fields",
                ast_out_struct.name
            )));
            None
        } else {
            let mut fields = Some(Vec::with_capacity(ast_out_struct.fields.len()));

            for (name, ty, docs) in ast_out_struct.fields.iter() {
                let name = lower_ident(name, "out-struct field name", errors);
                let ty = lower_out_type(
                    ty,
                    Some(&mut &ast_out_struct.lifetimes),
                    lookup_id,
                    in_path,
                    env,
                    errors,
                );

                match (name, ty, &mut fields) {
                    (Some(name), Some(ty), Some(fields)) => fields.push(OutStructField {
                        docs: docs.clone(),
                        name,
                        ty,
                    }),
                    _ => fields = None,
                }
            }

            fields
        };

        let methods =
            lower_all_methods(&ast_out_struct.methods[..], lookup_id, in_path, env, errors);

        Some(OutStructDef::new(
            ast_out_struct.docs.clone(),
            name?,
            fields?,
            methods?,
        ))
    }
}

/// Lowers an [`ast::Method`]s an [`hir::Method`].
///
/// If there are any errors, they're pushed to `errors` and `None` is returned.
fn lower_method(
    method: &ast::Method,
    lookup_id: &LookupId,
    in_path: &ast::Path,
    env: &Env,
    errors: &mut Vec<LoweringError>,
) -> Option<Method> {
    let name = lower_ident(&method.name, "method name", errors);

    let (ast_params, takes_writeable) = match method.params.split_last() {
        Some((last, remaining)) if last.is_writeable() => (remaining, true),
        _ => (&method.params[..], false),
    };

    let self_param_ltl = SelfParamLifetimeLowerer::new(&method.lifetime_env, errors);

    let (param_self, param_ltl) = if let Some(self_param) = method.self_param.as_ref() {
        lower_self_param(
            self_param,
            self_param_ltl,
            lookup_id,
            &method.full_path_name,
            in_path,
            env,
            errors,
        )
        .map(|(param_self, param_ltl)| (Some(Some(param_self)), Some(param_ltl)))
        .unwrap_or((None, None))
    } else {
        (
            Some(None),
            self_param_ltl.map(SelfParamLifetimeLowerer::no_self_ref),
        )
    };

    let (params, return_ltl) =
        lower_many_params(ast_params, param_ltl, lookup_id, in_path, env, errors)
            .map(|(params, return_ltl)| (Some(params), Some(return_ltl)))
            .unwrap_or((None, None));

    let (output, lifetime_env) = lower_return_type(
        method.return_type.as_ref(),
        takes_writeable,
        return_ltl,
        lookup_id,
        in_path,
        env,
        errors,
    )?;

    Some(Method {
        docs: method.docs.clone(),
        name: name?,
        lifetime_env,
        param_self: param_self?,
        params: params?,
        output,
    })
}

/// Lowers many [`ast::Method`]s into a vector of [`hir::Method`]s.
///
/// If there are any errors, they're pushed to `errors` and `None` is returned.
fn lower_all_methods(
    ast_methods: &[ast::Method],
    lookup_id: &LookupId,
    in_path: &ast::Path,
    env: &Env,
    errors: &mut Vec<LoweringError>,
) -> Option<Vec<Method>> {
    let mut methods = Some(Vec::with_capacity(ast_methods.len()));

    for method in ast_methods {
        let method = lower_method(method, lookup_id, in_path, env, errors);
        match (method, &mut methods) {
            (Some(method), Some(methods)) => {
                methods.push(method);
            }
            _ => methods = None,
        }
    }

    methods
}

/// Lowers an [`ast::TypeName`]s into a [`hir::Type`].
///
/// If there are any errors, they're pushed to `errors` and `None` is returned.
fn lower_type<L: LifetimeLowerer>(
    ty: &ast::TypeName,
    ltl: Option<&mut L>,
    lookup_id: &LookupId,
    in_path: &ast::Path,
    env: &Env,
    errors: &mut Vec<LoweringError>,
) -> Option<Type> {
    match ty {
        ast::TypeName::Primitive(prim) => Some(Type::Primitive(PrimitiveType::from_ast(*prim))),
        ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
            match path.resolve(in_path, env) {
                ast::CustomType::Struct(strct) => {
                    if let Some(tcx_id) = lookup_id.resolve_struct(strct) {
                        let lifetimes = ltl?.lower_generics(&path.lifetimes[..], ty.is_self());

                        Some(Type::Struct(StructPath::new(lifetimes, tcx_id)))
                    } else if lookup_id.resolve_out_struct(strct).is_some() {
                        errors.push(LoweringError::Other(format!("found struct in input that is marked with #[diplomat::out]: {ty} in {path}")));
                        None
                    } else {
                        unreachable!("struct `{}` wasn't found in the set of structs or out-structs, this is a bug.", strct.name);
                    }
                }
                ast::CustomType::Opaque(_) => {
                    errors.push(LoweringError::Other(format!(
                        "Opaque passed by value in input: {path}"
                    )));
                    None
                }
                ast::CustomType::Enum(enm) => {
                    let tcx_id = lookup_id
                        .resolve_enum(enm)
                        .expect("can't find enum in lookup map, which contains all enums from env");

                    Some(Type::Enum(EnumPath::new(tcx_id)))
                }
            }
        }
        ast::TypeName::Reference(lifetime, mutability, ref_ty) => match ref_ty.as_ref() {
            ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                match path.resolve(in_path, env) {
                    ast::CustomType::Opaque(opaque) => ltl.map(|ltl| {
                        let borrow = Borrow::new(ltl.lower_lifetime(lifetime), *mutability);
                        let lifetimes = ltl.lower_generics(&path.lifetimes[..], ref_ty.is_self());
                        let tcx_id = lookup_id.resolve_opaque(opaque).expect(
                            "can't find opaque in lookup map, which contains all opaques from env",
                        );

                        Type::Opaque(OpaquePath::new(lifetimes, Optional(false), borrow, tcx_id))
                    }),
                    _ => {
                        errors.push(LoweringError::Other(format!("found &T in input where T is a custom type, but not opaque. T = {ref_ty}")));
                        None
                    }
                }
            }
            _ => {
                errors.push(LoweringError::Other(format!("found &T in input where T isn't a custom type and therefore not opaque. T = {ref_ty}")));
                None
            }
        },
        ast::TypeName::Box(box_ty) => {
            errors.push(match box_ty.as_ref() {
                ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                    match path.resolve(in_path, env) {
                        ast::CustomType::Opaque(_) => LoweringError::Other(format!("found Box<T> in input where T is an opaque, but owned opaques aren't allowed in inputs. try &T instead? T = {path}")),
                        _ => LoweringError::Other(format!("found Box<T> in input where T is a custom type but not opaque. non-opaques can't be behind pointers, and opaques in inputs can't be owned. T = {path}")),
                    }
                }
                _ => LoweringError::Other(format!("found Box<T> in input where T isn't a custom type. T = {box_ty}")),
            });
            None
        }
        ast::TypeName::Option(opt_ty) => {
            match opt_ty.as_ref() {
                ast::TypeName::Reference(lifetime, mutability, ref_ty) => match ref_ty.as_ref() {
                    ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => match path.resolve(in_path, env) {
                        ast::CustomType::Opaque(opaque) => ltl.map(|ltl| {
                            let borrow = Borrow::new(ltl.lower_lifetime(lifetime), *mutability);
                            let lifetimes = ltl.lower_generics(&path.lifetimes, ref_ty.is_self());
                            let tcx_id = lookup_id.resolve_opaque(opaque).expect(
                                    "can't find opaque in lookup map, which contains all opaques from env",
                                );

                            Type::Opaque(OpaquePath::new(
                                lifetimes,
                                Optional(true),
                                borrow,
                                tcx_id,
                            ))
                        }),
                        _ => {
                            errors.push(LoweringError::Other(format!("found Option<&T> in input where T is a custom type, but it's not opaque. T = {ref_ty}")));
                            None
                        }
                    },
                    _ => {
                        errors.push(LoweringError::Other(format!("found Option<&T> in input, but T isn't a custom type and therefore not opaque. T = {ref_ty}")));
                        None
                    }
                },
                ast::TypeName::Box(box_ty) => {
                    // we could see whats in the box here too
                    errors.push(LoweringError::Other(format!("found Option<Box<T>> in input, but box isn't allowed in inputs. T = {box_ty}")));
                    None
                }
                _ => {
                    errors.push(LoweringError::Other(format!("found Option<T> in input, where T isn't a reference but Option<T> in inputs requires that T is a reference to an opaque. T = {opt_ty}")));
                    None
                }
            }
        }
        ast::TypeName::Result(_, _, _) => {
            errors.push(LoweringError::Other(
                "Results can only appear as the top-level return type of methods".into(),
            ));
            None
        }
        ast::TypeName::Writeable => {
            errors.push(LoweringError::Other(
                "Writeables can only appear as the last parameter of a method".into(),
            ));
            None
        }
        ast::TypeName::StrReference(lifetime) => {
            Some(Type::Slice(Slice::Str(ltl?.lower_lifetime(lifetime))))
        }
        ast::TypeName::PrimitiveSlice(lifetime, mutability, prim) => {
            let borrow = Borrow::new(ltl?.lower_lifetime(lifetime), *mutability);
            let prim = PrimitiveType::from_ast(*prim);

            Some(Type::Slice(Slice::Primitive(borrow, prim)))
        }
        ast::TypeName::Unit => {
            errors.push(LoweringError::Other("Unit types can only appear as the return value of a method, or as the Ok/Err variants of a returned result".into()));
            None
        }
    }
}

/// Lowers an [`ast::TypeName`]s into an [`hir::OutType`].
///
/// If there are any errors, they're pushed to `errors` and `None` is returned.
fn lower_out_type<L: LifetimeLowerer>(
    ty: &ast::TypeName,
    ltl: Option<&mut L>,
    lookup_id: &LookupId,
    in_path: &ast::Path,
    env: &Env,
    errors: &mut Vec<LoweringError>,
) -> Option<OutType> {
    match ty {
        ast::TypeName::Primitive(prim) => Some(OutType::Primitive(PrimitiveType::from_ast(*prim))),
        ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
            match path.resolve(in_path, env) {
                ast::CustomType::Struct(strct) => {
                    let lifetimes = ltl?.lower_generics(&path.lifetimes, ty.is_self());

                    if let Some(tcx_id) = lookup_id.resolve_struct(strct) {
                        Some(OutType::Struct(ReturnableStructPath::Struct(
                            StructPath::new(lifetimes, tcx_id),
                        )))
                    } else if let Some(tcx_id) = lookup_id.resolve_out_struct(strct) {
                        Some(OutType::Struct(ReturnableStructPath::OutStruct(
                            OutStructPath::new(lifetimes, tcx_id),
                        )))
                    } else {
                        unreachable!("struct `{}` wasn't found in the set of structs or out-structs, this is a bug.", strct.name);
                    }
                }
                ast::CustomType::Opaque(_) => {
                    errors.push(LoweringError::Other(format!(
                        "Opaque passed by value in input: {path}"
                    )));
                    None
                }
                ast::CustomType::Enum(enm) => {
                    let tcx_id = lookup_id
                        .resolve_enum(enm)
                        .expect("can't find enum in lookup map, which contains all enums from env");

                    Some(OutType::Enum(EnumPath::new(tcx_id)))
                }
            }
        }
        ast::TypeName::Reference(lifetime, mutability, ref_ty) => match ref_ty.as_ref() {
            ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                match path.resolve(in_path, env) {
                    ast::CustomType::Opaque(opaque) => ltl.map(|ltl| {
                        let borrow = Borrow::new(ltl.lower_lifetime(lifetime), *mutability);
                        let lifetimes = ltl.lower_generics(&path.lifetimes, ref_ty.is_self());
                        let tcx_id = lookup_id.resolve_opaque(opaque).expect(
                            "can't find opaque in lookup map, which contains all opaques from env",
                        );

                        OutType::Opaque(OpaquePath::new(
                            lifetimes,
                            Optional(false),
                            MaybeOwn::Borrow(borrow),
                            tcx_id,
                        ))
                    }),
                    _ => {
                        errors.push(LoweringError::Other(format!("found &T in output where T is a custom type, but not opaque. T = {ref_ty}")));
                        None
                    }
                }
            }
            _ => {
                errors.push(LoweringError::Other(format!("found &T in output where T isn't a custom type and therefore not opaque. T = {ref_ty}")));
                None
            }
        },
        ast::TypeName::Box(box_ty) => match box_ty.as_ref() {
            ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                match path.resolve(in_path, env) {
                    ast::CustomType::Opaque(opaque) => ltl.map(|ltl| {
                        let lifetimes = ltl.lower_generics(&path.lifetimes, box_ty.is_self());
                        let tcx_id = lookup_id.resolve_opaque(opaque).expect(
                            "can't find opaque in lookup map, which contains all opaques from env",
                        );

                        OutType::Opaque(OpaquePath::new(
                            lifetimes,
                            Optional(true),
                            MaybeOwn::Own,
                            tcx_id,
                        ))
                    }),
                    _ => {
                        errors.push(LoweringError::Other(format!("found Box<T> in output where T is a custom type but not opaque. non-opaques can't be behind pointers. T = {path}")));
                        None
                    }
                }
            }
            _ => {
                errors.push(LoweringError::Other(format!(
                    "found Box<T> in output where T isn't a custom type. T = {box_ty}"
                )));
                None
            }
        },
        ast::TypeName::Option(opt_ty) => match opt_ty.as_ref() {
            ast::TypeName::Reference(lifetime, mutability, ref_ty) => match ref_ty.as_ref() {
                ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                    match path.resolve(in_path, env) {
                        ast::CustomType::Opaque(opaque) => ltl.map(|ltl| {
                            let borrow = Borrow::new(ltl.lower_lifetime(lifetime), *mutability);
                            let lifetimes = ltl.lower_generics(&path.lifetimes, ref_ty.is_self());
                            let tcx_id = lookup_id.resolve_opaque(opaque).expect(
                                "can't find opaque in lookup map, which contains all opaques from env",
                            );

                            OutType::Opaque(OpaquePath::new(
                                lifetimes,
                                Optional(true),
                                MaybeOwn::Borrow(borrow),
                                tcx_id,
                            ))
                        }),
                        _ => {
                            errors.push(LoweringError::Other(format!("found Option<&T> where T is a custom type, but it's not opaque. T = {ref_ty}")));
                            None
                        }
                    }
                }
                _ => {
                    errors.push(LoweringError::Other(format!("found Option<&T>, but T isn't a custom type and therefore not opaque. T = {ref_ty}")));
                    None
                }
            },
            ast::TypeName::Box(box_ty) => match box_ty.as_ref() {
                ast::TypeName::Named(path) | ast::TypeName::SelfType(path) => {
                    match path.resolve(in_path, env) {
                        ast::CustomType::Opaque(opaque) => {
                            let lifetimes = ltl?.lower_generics(&path.lifetimes, box_ty.is_self());
                            let tcx_id = lookup_id.resolve_opaque(opaque).expect(
                            "can't find opaque in lookup map, which contains all opaques from env",
                        );

                            Some(OutType::Opaque(OpaquePath::new(
                                lifetimes,
                                Optional(true),
                                MaybeOwn::Own,
                                tcx_id,
                            )))
                        }
                        _ => {
                            errors.push(LoweringError::Other(format!("found Option<Box<T>> where T is a custom type, but it's not opaque. T = {box_ty}")));
                            None
                        }
                    }
                }
                _ => {
                    errors.push(LoweringError::Other(format!("found Option<Box<T>>, but T isn't a custom type and therefore not opaque. T = {box_ty}")));
                    None
                }
            },
            _ => {
                errors.push(LoweringError::Other(format!("found Option<T>, where T isn't a reference but Option<T> in inputs requires that T is a reference to an opaque. T = {opt_ty}")));
                None
            }
        },
        ast::TypeName::Result(_, _, _) => {
            errors.push(LoweringError::Other(
                "Results can only appear as the top-level return type of methods".into(),
            ));
            None
        }
        ast::TypeName::Writeable => {
            errors.push(LoweringError::Other(
                "Writeables can only appear as the last parameter of a method".into(),
            ));
            None
        }
        ast::TypeName::StrReference(lifetime) => {
            Some(OutType::Slice(Slice::Str(ltl?.lower_lifetime(lifetime))))
        }
        ast::TypeName::PrimitiveSlice(lifetime, mutability, prim) => {
            let borrow = Borrow::new(ltl?.lower_lifetime(lifetime), *mutability);
            let prim = PrimitiveType::from_ast(*prim);

            Some(OutType::Slice(Slice::Primitive(borrow, prim)))
        }
        ast::TypeName::Unit => {
            errors.push(LoweringError::Other("Unit types can only appear as the return value of a method, or as the Ok/Err variants of a returned result".into()));
            None
        }
    }
}

/// Lowers an [`ast::SelfParam`] into an [`hir::ParamSelf`].
///
/// If there are any errors, they're pushed to `errors` and `None` is returned.
fn lower_self_param<'ast>(
    self_param: &ast::SelfParam,
    self_param_ltl: Option<SelfParamLifetimeLowerer<'ast>>,
    lookup_id: &LookupId,
    method_full_path: &ast::Ident, // for better error msg
    in_path: &ast::Path,
    env: &Env,
    errors: &mut Vec<LoweringError>,
) -> Option<(ParamSelf, ParamLifetimeLowerer<'ast>)> {
    match self_param.path_type.resolve(in_path, env) {
        ast::CustomType::Struct(strct) => {
            if let Some(tcx_id) = lookup_id.resolve_struct(strct) {
                if self_param.reference.is_some() {
                    errors.push(LoweringError::Other(format!("Method `{method_full_path}` takes a reference to a struct as a self parameter, which isn't allowed")));
                    None
                } else {
                    let mut param_ltl = self_param_ltl?.no_self_ref();

                    // Even if we explicitly write out the type of `self` like
                    // `self: Foo<'a>`, the `'a` is still not considered for
                    // elision according to rustc, so is_self=true.
                    let type_lifetimes =
                        param_ltl.lower_generics(&self_param.path_type.lifetimes[..], true);

                    Some((
                        ParamSelf::new(SelfType::Struct(StructPath::new(type_lifetimes, tcx_id))),
                        param_ltl,
                    ))
                }
            } else if lookup_id.resolve_out_struct(strct).is_some() {
                if let Some((lifetime, _)) = &self_param.reference {
                    errors.push(LoweringError::Other(format!("Method `{method_full_path}` takes an out-struct as the self parameter, which isn't allowed. Also, it's behind a reference, `{lifetime}`, but only opaques can be behind references")));
                    None
                } else {
                    errors.push(LoweringError::Other(format!("Method `{method_full_path}` takes an out-struct as the self parameter, which isn't allowed")));
                    None
                }
            } else {
                unreachable!(
                    "struct `{}` wasn't found in the set of structs or out-structs, this is a bug.",
                    strct.name
                );
            }
        }
        ast::CustomType::Opaque(opaque) => {
            let tcx_id = lookup_id.resolve_opaque(opaque).expect("opaque is in env");

            if let Some((lifetime, mutability)) = &self_param.reference {
                let (borrow_lifetime, mut param_ltl) = self_param_ltl?.lower_self_ref(lifetime);
                let borrow = Borrow::new(borrow_lifetime, *mutability);
                let lifetimes = param_ltl.lower_generics(&self_param.path_type.lifetimes, true);

                Some((
                    ParamSelf::new(SelfType::Opaque(OpaquePath::new(
                        lifetimes,
                        NonOptional,
                        borrow,
                        tcx_id,
                    ))),
                    param_ltl,
                ))
            } else {
                errors.push(LoweringError::Other(format!("Method `{method_full_path}` takes an opaque by value as the self parameter, but opaques as inputs must be behind refs")));
                None
            }
        }
        ast::CustomType::Enum(enm) => {
            let tcx_id = lookup_id.resolve_enum(enm).expect("enum is in env");

            Some((
                ParamSelf::new(SelfType::Enum(EnumPath::new(tcx_id))),
                self_param_ltl?.no_self_ref(),
            ))
        }
    }
}

/// Lowers an [`ast::Param`] into an [`hir::Param`].
///
/// If there are any errors, they're pushed to `errors` and `None` is returned.
///
/// Note that this expects that if there was a writeable param at the end in
/// the method, it's not passed into here.
fn lower_param<L: LifetimeLowerer>(
    param: &ast::Param,
    ltl: Option<&mut L>,
    lookup_id: &LookupId,
    in_path: &ast::Path,
    env: &Env,
    errors: &mut Vec<LoweringError>,
) -> Option<Param> {
    let name = lower_ident(&param.name, "param name", errors);
    let ty = lower_type(&param.ty, ltl, lookup_id, in_path, env, errors);

    Some(Param::new(name?, ty?))
}

/// Lowers many [`ast::Param`]s into a vector of [`hir::Param`]s.
///
/// If there are any errors, they're pushed to `errors` and `None` is returned.
///
/// Note that this expects that if there was a writeable param at the end in
/// the method, `ast_params` was sliced to not include it. This happens in
/// `lower_method`, the caller of this function.
fn lower_many_params<'ast>(
    ast_params: &[ast::Param],
    mut param_ltl: Option<ParamLifetimeLowerer<'ast>>,
    lookup_id: &LookupId,
    in_path: &ast::Path,
    env: &Env,
    errors: &mut Vec<LoweringError>,
) -> Option<(Vec<Param>, ReturnLifetimeLowerer<'ast>)> {
    let mut params = Some(Vec::with_capacity(ast_params.len()));

    for param in ast_params {
        let param = lower_param(param, param_ltl.as_mut(), lookup_id, in_path, env, errors);

        match (param, &mut params) {
            (Some(param), Some(params)) => {
                params.push(param);
            }
            _ => params = None,
        }
    }

    Some((params?, param_ltl?.into_return_ltl()))
}

/// Lowers the return type of an [`ast::Method`] into a [`hir::ReturnFallability`].
///
/// If there are any errors, they're pushed to `errors` and `None` is returned.
fn lower_return_type(
    return_type: Option<&ast::TypeName>,
    takes_writeable: bool,
    mut return_ltl: Option<ReturnLifetimeLowerer<'_>>,
    lookup_id: &LookupId,
    in_path: &ast::Path,
    env: &Env,
    errors: &mut Vec<LoweringError>,
) -> Option<(ReturnType, LifetimeEnv)> {
    let writeable_option = if takes_writeable {
        Some(SuccessType::Writeable)
    } else {
        None
    };
    match return_type.unwrap_or(&ast::TypeName::Unit) {
        ast::TypeName::Result(ok_ty, err_ty, _) => {
            let ok_ty = match ok_ty.as_ref() {
                ast::TypeName::Unit => Some(writeable_option),
                ty => lower_out_type(ty, return_ltl.as_mut(), lookup_id, in_path, env, errors)
                    .map(|ty| Some(SuccessType::OutType(ty))),
            };
            let err_ty = match err_ty.as_ref() {
                ast::TypeName::Unit => Some(None),
                ty => lower_out_type(ty, return_ltl.as_mut(), lookup_id, in_path, env, errors)
                    .map(Some),
            };

            match (ok_ty, err_ty) {
                (Some(ok_ty), Some(err_ty)) => Some(ReturnType::Fallible(ok_ty, err_ty)),
                _ => None,
            }
        }
        ast::TypeName::Unit => Some(ReturnType::Infallible(writeable_option)),
        ty => lower_out_type(ty, return_ltl.as_mut(), lookup_id, in_path, env, errors)
            .map(|ty| ReturnType::Infallible(Some(SuccessType::OutType(ty)))),
    }
    .and_then(|return_fallability| Some((return_fallability, return_ltl?.finish())))
}
