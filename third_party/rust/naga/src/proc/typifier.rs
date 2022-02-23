use crate::arena::{Arena, Handle, UniqueArena};

use thiserror::Error;

/// The result of computing an expression's type.
///
/// This is the (Rust) type returned by [`ResolveContext::resolve`] to represent
/// the (Naga) type it ascribes to some expression.
///
/// You might expect such a function to simply return a `Handle<Type>`. However,
/// we want type resolution to be a read-only process, and that would limit the
/// possible results to types already present in the expression's associated
/// `UniqueArena<Type>`. Naga IR does have certain expressions whose types are
/// not certain to be present.
///
/// So instead, type resolution returns a `TypeResolution` enum: either a
/// [`Handle`], referencing some type in the arena, or a [`Value`], holding a
/// free-floating [`TypeInner`]. This extends the range to cover anything that
/// can be represented with a `TypeInner` referring to the existing arena.
///
/// What sorts of expressions can have types not available in the arena?
///
/// -   An [`Access`] or [`AccessIndex`] expression applied to a [`Vector`] or
///     [`Matrix`] must have a [`Scalar`] or [`Vector`] type. But since `Vector`
///     and `Matrix` represent their element and column types implicitly, not
///     via a handle, there may not be a suitable type in the expression's
///     associated arena. Instead, resolving such an expression returns a
///     `TypeResolution::Value(TypeInner::X { ... })`, where `X` is `Scalar` or
///     `Vector`.
///
/// -   Similarly, the type of an [`Access`] or [`AccessIndex`] expression
///     applied to a *pointer to* a vector or matrix must produce a *pointer to*
///     a scalar or vector type. These cannot be represented with a
///     [`TypeInner::Pointer`], since the `Pointer`'s `base` must point into the
///     arena, and as before, we cannot assume that a suitable scalar or vector
///     type is there. So we take things one step further and provide
///     [`TypeInner::ValuePointer`], specifically for the case of pointers to
///     scalars or vectors. This type fits in a `TypeInner` and is exactly
///     equivalent to a `Pointer` to a `Vector` or `Scalar`.
///
/// So, for example, the type of an `Access` expression applied to a value of type:
///
/// ```ignore
/// TypeInner::Matrix { columns, rows, width }
/// ```
///
/// might be:
///
/// ```ignore
/// TypeResolution::Value(TypeInner::Vector {
///     size: rows,
///     kind: ScalarKind::Float,
///     width,
/// })
/// ```
///
/// and the type of an access to a pointer of storage class `class` to such a
/// matrix might be:
///
/// ```ignore
/// TypeResolution::Value(TypeInner::ValuePointer {
///     size: Some(rows),
///     kind: ScalarKind::Float,
///     width,
///     class
/// })
/// ```
///
/// [`Handle`]: TypeResolution::Handle
/// [`Value`]: TypeResolution::Value
///
/// [`Access`]: crate::Expression::Access
/// [`AccessIndex`]: crate::Expression::AccessIndex
///
/// [`TypeInner`]: crate::TypeInner
/// [`Matrix`]: crate::TypeInner::Matrix
/// [`Pointer`]: crate::TypeInner::Pointer
/// [`Scalar`]: crate::TypeInner::Scalar
/// [`ValuePointer`]: crate::TypeInner::ValuePointer
/// [`Vector`]: crate::TypeInner::Vector
///
/// [`TypeInner::Pointer`]: crate::TypeInner::Pointer
/// [`TypeInner::ValuePointer`]: crate::TypeInner::ValuePointer
#[derive(Debug, PartialEq)]
#[cfg_attr(feature = "serialize", derive(serde::Serialize))]
#[cfg_attr(feature = "deserialize", derive(serde::Deserialize))]
pub enum TypeResolution {
    /// A type stored in the associated arena.
    Handle(Handle<crate::Type>),

    /// A free-floating [`TypeInner`], representing a type that may not be
    /// available in the associated arena. However, the `TypeInner` itself may
    /// contain `Handle<Type>` values referring to types from the arena.
    ///
    /// [`TypeInner`]: crate::TypeInner
    Value(crate::TypeInner),
}

impl TypeResolution {
    pub fn handle(&self) -> Option<Handle<crate::Type>> {
        match *self {
            Self::Handle(handle) => Some(handle),
            Self::Value(_) => None,
        }
    }

    pub fn inner_with<'a>(&'a self, arena: &'a UniqueArena<crate::Type>) -> &'a crate::TypeInner {
        match *self {
            Self::Handle(handle) => &arena[handle].inner,
            Self::Value(ref inner) => inner,
        }
    }
}

// Clone is only implemented for numeric variants of `TypeInner`.
impl Clone for TypeResolution {
    fn clone(&self) -> Self {
        use crate::TypeInner as Ti;
        match *self {
            Self::Handle(handle) => Self::Handle(handle),
            Self::Value(ref v) => Self::Value(match *v {
                Ti::Scalar { kind, width } => Ti::Scalar { kind, width },
                Ti::Vector { size, kind, width } => Ti::Vector { size, kind, width },
                Ti::Matrix {
                    rows,
                    columns,
                    width,
                } => Ti::Matrix {
                    rows,
                    columns,
                    width,
                },
                Ti::Pointer { base, class } => Ti::Pointer { base, class },
                Ti::ValuePointer {
                    size,
                    kind,
                    width,
                    class,
                } => Ti::ValuePointer {
                    size,
                    kind,
                    width,
                    class,
                },
                _ => unreachable!("Unexpected clone type: {:?}", v),
            }),
        }
    }
}

impl crate::ConstantInner {
    pub fn resolve_type(&self) -> TypeResolution {
        match *self {
            Self::Scalar { width, ref value } => TypeResolution::Value(crate::TypeInner::Scalar {
                kind: value.scalar_kind(),
                width,
            }),
            Self::Composite { ty, components: _ } => TypeResolution::Handle(ty),
        }
    }
}

#[derive(Clone, Debug, Error, PartialEq)]
pub enum ResolveError {
    #[error("Index {index} is out of bounds for expression {expr:?}")]
    OutOfBoundsIndex {
        expr: Handle<crate::Expression>,
        index: u32,
    },
    #[error("Invalid access into expression {expr:?}, indexed: {indexed}")]
    InvalidAccess {
        expr: Handle<crate::Expression>,
        indexed: bool,
    },
    #[error("Invalid sub-access into type {ty:?}, indexed: {indexed}")]
    InvalidSubAccess {
        ty: Handle<crate::Type>,
        indexed: bool,
    },
    #[error("Invalid scalar {0:?}")]
    InvalidScalar(Handle<crate::Expression>),
    #[error("Invalid vector {0:?}")]
    InvalidVector(Handle<crate::Expression>),
    #[error("Invalid pointer {0:?}")]
    InvalidPointer(Handle<crate::Expression>),
    #[error("Invalid image {0:?}")]
    InvalidImage(Handle<crate::Expression>),
    #[error("Function {name} not defined")]
    FunctionNotDefined { name: String },
    #[error("Function without return type")]
    FunctionReturnsVoid,
    #[error("Type is not found in the given immutable arena")]
    TypeNotFound,
    #[error("Incompatible operands: {0}")]
    IncompatibleOperands(String),
}

pub struct ResolveContext<'a> {
    pub constants: &'a Arena<crate::Constant>,
    pub types: &'a UniqueArena<crate::Type>,
    pub global_vars: &'a Arena<crate::GlobalVariable>,
    pub local_vars: &'a Arena<crate::LocalVariable>,
    pub functions: &'a Arena<crate::Function>,
    pub arguments: &'a [crate::FunctionArgument],
}

impl<'a> ResolveContext<'a> {
    /// Determine the type of `expr`.
    ///
    /// The `past` argument must be a closure that can resolve the types of any
    /// expressions that `expr` refers to. These can be gathered by caching the
    /// results of prior calls to `resolve`, perhaps as done by the
    /// [`front::Typifier`] utility type.
    ///
    /// Type resolution is a read-only process: this method takes `self` by
    /// shared reference. However, this means that we cannot add anything to
    /// `self.types` that we might need to describe `expr`. To work around this,
    /// this method returns a [`TypeResolution`], rather than simply returning a
    /// `Handle<Type>`; see the documentation for [`TypeResolution`] for
    /// details.
    ///
    /// [`front::Typifier`]: crate::front::Typifier
    pub fn resolve(
        &self,
        expr: &crate::Expression,
        past: impl Fn(Handle<crate::Expression>) -> &'a TypeResolution,
    ) -> Result<TypeResolution, ResolveError> {
        use crate::TypeInner as Ti;
        let types = self.types;
        Ok(match *expr {
            crate::Expression::Access { base, .. } => match *past(base).inner_with(types) {
                // Arrays and matrices can only be indexed dynamically behind a
                // pointer, but that's a validation error, not a type error, so
                // go ahead provide a type here.
                Ti::Array { base, .. } => TypeResolution::Handle(base),
                Ti::Matrix { rows, width, .. } => TypeResolution::Value(Ti::Vector {
                    size: rows,
                    kind: crate::ScalarKind::Float,
                    width,
                }),
                Ti::Vector {
                    size: _,
                    kind,
                    width,
                } => TypeResolution::Value(Ti::Scalar { kind, width }),
                Ti::ValuePointer {
                    size: Some(_),
                    kind,
                    width,
                    class,
                } => TypeResolution::Value(Ti::ValuePointer {
                    size: None,
                    kind,
                    width,
                    class,
                }),
                Ti::Pointer { base, class } => {
                    TypeResolution::Value(match types[base].inner {
                        Ti::Array { base, .. } => Ti::Pointer { base, class },
                        Ti::Vector {
                            size: _,
                            kind,
                            width,
                        } => Ti::ValuePointer {
                            size: None,
                            kind,
                            width,
                            class,
                        },
                        // Matrices are only dynamically indexed behind a pointer
                        Ti::Matrix {
                            columns: _,
                            rows,
                            width,
                        } => Ti::ValuePointer {
                            kind: crate::ScalarKind::Float,
                            size: Some(rows),
                            width,
                            class,
                        },
                        ref other => {
                            log::error!("Access sub-type {:?}", other);
                            return Err(ResolveError::InvalidSubAccess {
                                ty: base,
                                indexed: false,
                            });
                        }
                    })
                }
                ref other => {
                    log::error!("Access type {:?}", other);
                    return Err(ResolveError::InvalidAccess {
                        expr: base,
                        indexed: false,
                    });
                }
            },
            crate::Expression::AccessIndex { base, index } => match *past(base).inner_with(types) {
                Ti::Vector { size, kind, width } => {
                    if index >= size as u32 {
                        return Err(ResolveError::OutOfBoundsIndex { expr: base, index });
                    }
                    TypeResolution::Value(Ti::Scalar { kind, width })
                }
                Ti::Matrix {
                    columns,
                    rows,
                    width,
                } => {
                    if index >= columns as u32 {
                        return Err(ResolveError::OutOfBoundsIndex { expr: base, index });
                    }
                    TypeResolution::Value(crate::TypeInner::Vector {
                        size: rows,
                        kind: crate::ScalarKind::Float,
                        width,
                    })
                }
                Ti::Array { base, .. } => TypeResolution::Handle(base),
                Ti::Struct { ref members, .. } => {
                    let member = members
                        .get(index as usize)
                        .ok_or(ResolveError::OutOfBoundsIndex { expr: base, index })?;
                    TypeResolution::Handle(member.ty)
                }
                Ti::ValuePointer {
                    size: Some(size),
                    kind,
                    width,
                    class,
                } => {
                    if index >= size as u32 {
                        return Err(ResolveError::OutOfBoundsIndex { expr: base, index });
                    }
                    TypeResolution::Value(Ti::ValuePointer {
                        size: None,
                        kind,
                        width,
                        class,
                    })
                }
                Ti::Pointer {
                    base: ty_base,
                    class,
                } => TypeResolution::Value(match types[ty_base].inner {
                    Ti::Array { base, .. } => Ti::Pointer { base, class },
                    Ti::Vector { size, kind, width } => {
                        if index >= size as u32 {
                            return Err(ResolveError::OutOfBoundsIndex { expr: base, index });
                        }
                        Ti::ValuePointer {
                            size: None,
                            kind,
                            width,
                            class,
                        }
                    }
                    Ti::Matrix {
                        rows,
                        columns,
                        width,
                    } => {
                        if index >= columns as u32 {
                            return Err(ResolveError::OutOfBoundsIndex { expr: base, index });
                        }
                        Ti::ValuePointer {
                            size: Some(rows),
                            kind: crate::ScalarKind::Float,
                            width,
                            class,
                        }
                    }
                    Ti::Struct { ref members, .. } => {
                        let member = members
                            .get(index as usize)
                            .ok_or(ResolveError::OutOfBoundsIndex { expr: base, index })?;
                        Ti::Pointer {
                            base: member.ty,
                            class,
                        }
                    }
                    ref other => {
                        log::error!("Access index sub-type {:?}", other);
                        return Err(ResolveError::InvalidSubAccess {
                            ty: ty_base,
                            indexed: true,
                        });
                    }
                }),
                ref other => {
                    log::error!("Access index type {:?}", other);
                    return Err(ResolveError::InvalidAccess {
                        expr: base,
                        indexed: true,
                    });
                }
            },
            crate::Expression::Constant(h) => match self.constants[h].inner {
                crate::ConstantInner::Scalar { width, ref value } => {
                    TypeResolution::Value(Ti::Scalar {
                        kind: value.scalar_kind(),
                        width,
                    })
                }
                crate::ConstantInner::Composite { ty, components: _ } => TypeResolution::Handle(ty),
            },
            crate::Expression::Splat { size, value } => match *past(value).inner_with(types) {
                Ti::Scalar { kind, width } => {
                    TypeResolution::Value(Ti::Vector { size, kind, width })
                }
                ref other => {
                    log::error!("Scalar type {:?}", other);
                    return Err(ResolveError::InvalidScalar(value));
                }
            },
            crate::Expression::Swizzle {
                size,
                vector,
                pattern: _,
            } => match *past(vector).inner_with(types) {
                Ti::Vector {
                    size: _,
                    kind,
                    width,
                } => TypeResolution::Value(Ti::Vector { size, kind, width }),
                ref other => {
                    log::error!("Vector type {:?}", other);
                    return Err(ResolveError::InvalidVector(vector));
                }
            },
            crate::Expression::Compose { ty, .. } => TypeResolution::Handle(ty),
            crate::Expression::FunctionArgument(index) => {
                TypeResolution::Handle(self.arguments[index as usize].ty)
            }
            crate::Expression::GlobalVariable(h) => {
                let var = &self.global_vars[h];
                if var.class == crate::StorageClass::Handle {
                    TypeResolution::Handle(var.ty)
                } else {
                    TypeResolution::Value(Ti::Pointer {
                        base: var.ty,
                        class: var.class,
                    })
                }
            }
            crate::Expression::LocalVariable(h) => {
                let var = &self.local_vars[h];
                TypeResolution::Value(Ti::Pointer {
                    base: var.ty,
                    class: crate::StorageClass::Function,
                })
            }
            crate::Expression::Load { pointer } => match *past(pointer).inner_with(types) {
                Ti::Pointer { base, class: _ } => {
                    if let Ti::Atomic { kind, width } = types[base].inner {
                        TypeResolution::Value(Ti::Scalar { kind, width })
                    } else {
                        TypeResolution::Handle(base)
                    }
                }
                Ti::ValuePointer {
                    size,
                    kind,
                    width,
                    class: _,
                } => TypeResolution::Value(match size {
                    Some(size) => Ti::Vector { size, kind, width },
                    None => Ti::Scalar { kind, width },
                }),
                ref other => {
                    log::error!("Pointer type {:?}", other);
                    return Err(ResolveError::InvalidPointer(pointer));
                }
            },
            crate::Expression::ImageSample { image, .. }
            | crate::Expression::ImageLoad { image, .. } => match *past(image).inner_with(types) {
                Ti::Image { class, .. } => TypeResolution::Value(match class {
                    crate::ImageClass::Depth { multi: _ } => Ti::Scalar {
                        kind: crate::ScalarKind::Float,
                        width: 4,
                    },
                    crate::ImageClass::Sampled { kind, multi: _ } => Ti::Vector {
                        kind,
                        width: 4,
                        size: crate::VectorSize::Quad,
                    },
                    crate::ImageClass::Storage { format, .. } => Ti::Vector {
                        kind: format.into(),
                        width: 4,
                        size: crate::VectorSize::Quad,
                    },
                }),
                ref other => {
                    log::error!("Image type {:?}", other);
                    return Err(ResolveError::InvalidImage(image));
                }
            },
            crate::Expression::ImageQuery { image, query } => TypeResolution::Value(match query {
                crate::ImageQuery::Size { level: _ } => match *past(image).inner_with(types) {
                    Ti::Image { dim, .. } => match dim {
                        crate::ImageDimension::D1 => Ti::Scalar {
                            kind: crate::ScalarKind::Sint,
                            width: 4,
                        },
                        crate::ImageDimension::D2 | crate::ImageDimension::Cube => Ti::Vector {
                            size: crate::VectorSize::Bi,
                            kind: crate::ScalarKind::Sint,
                            width: 4,
                        },
                        crate::ImageDimension::D3 => Ti::Vector {
                            size: crate::VectorSize::Tri,
                            kind: crate::ScalarKind::Sint,
                            width: 4,
                        },
                    },
                    ref other => {
                        log::error!("Image type {:?}", other);
                        return Err(ResolveError::InvalidImage(image));
                    }
                },
                crate::ImageQuery::NumLevels
                | crate::ImageQuery::NumLayers
                | crate::ImageQuery::NumSamples => Ti::Scalar {
                    kind: crate::ScalarKind::Sint,
                    width: 4,
                },
            }),
            crate::Expression::Unary { expr, .. } => past(expr).clone(),
            crate::Expression::Binary { op, left, right } => match op {
                crate::BinaryOperator::Add
                | crate::BinaryOperator::Subtract
                | crate::BinaryOperator::Divide
                | crate::BinaryOperator::Modulo => past(left).clone(),
                crate::BinaryOperator::Multiply => {
                    let (res_left, res_right) = (past(left), past(right));
                    match (res_left.inner_with(types), res_right.inner_with(types)) {
                        (
                            &Ti::Matrix {
                                columns: _,
                                rows,
                                width,
                            },
                            &Ti::Matrix { columns, .. },
                        ) => TypeResolution::Value(Ti::Matrix {
                            columns,
                            rows,
                            width,
                        }),
                        (
                            &Ti::Matrix {
                                columns: _,
                                rows,
                                width,
                            },
                            &Ti::Vector { .. },
                        ) => TypeResolution::Value(Ti::Vector {
                            size: rows,
                            kind: crate::ScalarKind::Float,
                            width,
                        }),
                        (
                            &Ti::Vector { .. },
                            &Ti::Matrix {
                                columns,
                                rows: _,
                                width,
                            },
                        ) => TypeResolution::Value(Ti::Vector {
                            size: columns,
                            kind: crate::ScalarKind::Float,
                            width,
                        }),
                        (&Ti::Scalar { .. }, _) => res_right.clone(),
                        (_, &Ti::Scalar { .. }) => res_left.clone(),
                        (&Ti::Vector { .. }, &Ti::Vector { .. }) => res_left.clone(),
                        (tl, tr) => {
                            return Err(ResolveError::IncompatibleOperands(format!(
                                "{:?} * {:?}",
                                tl, tr
                            )))
                        }
                    }
                }
                crate::BinaryOperator::Equal
                | crate::BinaryOperator::NotEqual
                | crate::BinaryOperator::Less
                | crate::BinaryOperator::LessEqual
                | crate::BinaryOperator::Greater
                | crate::BinaryOperator::GreaterEqual
                | crate::BinaryOperator::LogicalAnd
                | crate::BinaryOperator::LogicalOr => {
                    let kind = crate::ScalarKind::Bool;
                    let width = crate::BOOL_WIDTH;
                    let inner = match *past(left).inner_with(types) {
                        Ti::Scalar { .. } => Ti::Scalar { kind, width },
                        Ti::Vector { size, .. } => Ti::Vector { size, kind, width },
                        ref other => {
                            return Err(ResolveError::IncompatibleOperands(format!(
                                "{:?}({:?}, _)",
                                op, other
                            )))
                        }
                    };
                    TypeResolution::Value(inner)
                }
                crate::BinaryOperator::And
                | crate::BinaryOperator::ExclusiveOr
                | crate::BinaryOperator::InclusiveOr
                | crate::BinaryOperator::ShiftLeft
                | crate::BinaryOperator::ShiftRight => past(left).clone(),
            },
            crate::Expression::AtomicResult {
                kind,
                width,
                comparison,
            } => {
                if comparison {
                    TypeResolution::Value(Ti::Vector {
                        size: crate::VectorSize::Bi,
                        kind,
                        width,
                    })
                } else {
                    TypeResolution::Value(Ti::Scalar { kind, width })
                }
            }
            crate::Expression::Select { accept, .. } => past(accept).clone(),
            crate::Expression::Derivative { axis: _, expr } => past(expr).clone(),
            crate::Expression::Relational { fun, argument } => match fun {
                crate::RelationalFunction::All | crate::RelationalFunction::Any => {
                    TypeResolution::Value(Ti::Scalar {
                        kind: crate::ScalarKind::Bool,
                        width: crate::BOOL_WIDTH,
                    })
                }
                crate::RelationalFunction::IsNan
                | crate::RelationalFunction::IsInf
                | crate::RelationalFunction::IsFinite
                | crate::RelationalFunction::IsNormal => match *past(argument).inner_with(types) {
                    Ti::Scalar { .. } => TypeResolution::Value(Ti::Scalar {
                        kind: crate::ScalarKind::Bool,
                        width: crate::BOOL_WIDTH,
                    }),
                    Ti::Vector { size, .. } => TypeResolution::Value(Ti::Vector {
                        kind: crate::ScalarKind::Bool,
                        width: crate::BOOL_WIDTH,
                        size,
                    }),
                    ref other => {
                        return Err(ResolveError::IncompatibleOperands(format!(
                            "{:?}({:?})",
                            fun, other
                        )))
                    }
                },
            },
            crate::Expression::Math {
                fun,
                arg,
                arg1,
                arg2: _,
                arg3: _,
            } => {
                use crate::MathFunction as Mf;
                let res_arg = past(arg);
                match fun {
                    // comparison
                    Mf::Abs |
                    Mf::Min |
                    Mf::Max |
                    Mf::Clamp |
                    // trigonometry
                    Mf::Cos |
                    Mf::Cosh |
                    Mf::Sin |
                    Mf::Sinh |
                    Mf::Tan |
                    Mf::Tanh |
                    Mf::Acos |
                    Mf::Asin |
                    Mf::Atan |
                    Mf::Atan2 |
                    Mf::Asinh |
                    Mf::Acosh |
                    Mf::Atanh |
                    // decomposition
                    Mf::Ceil |
                    Mf::Floor |
                    Mf::Round |
                    Mf::Fract |
                    Mf::Trunc |
                    Mf::Modf |
                    Mf::Frexp |
                    Mf::Ldexp |
                    // exponent
                    Mf::Exp |
                    Mf::Exp2 |
                    Mf::Log |
                    Mf::Log2 |
                    Mf::Pow => res_arg.clone(),
                    // geometry
                    Mf::Dot => match *res_arg.inner_with(types) {
                        Ti::Vector {
                            kind,
                            size: _,
                            width,
                        } => TypeResolution::Value(Ti::Scalar { kind, width }),
                        ref other =>
                            return Err(ResolveError::IncompatibleOperands(
                                format!("{:?}({:?}, _)", fun, other)
                            )),
                    },
                    Mf::Outer => {
                        let arg1 = arg1.ok_or_else(|| ResolveError::IncompatibleOperands(
                            format!("{:?}(_, None)", fun)
                        ))?;
                        match (res_arg.inner_with(types), past(arg1).inner_with(types)) {
                            (&Ti::Vector {kind: _, size: columns,width}, &Ti::Vector{ size: rows, .. }) => TypeResolution::Value(Ti::Matrix { columns, rows, width }),
                            (left, right) =>
                                return Err(ResolveError::IncompatibleOperands(
                                    format!("{:?}({:?}, {:?})", fun, left, right)
                                )),
                        }
                    },
                    Mf::Cross => res_arg.clone(),
                    Mf::Distance |
                    Mf::Length => match *res_arg.inner_with(types) {
                        Ti::Scalar {width,kind} |
                        Ti::Vector {width,kind,size:_} => TypeResolution::Value(Ti::Scalar { kind, width }),
                        ref other => return Err(ResolveError::IncompatibleOperands(
                                format!("{:?}({:?})", fun, other)
                            )),
                    },
                    Mf::Normalize |
                    Mf::FaceForward |
                    Mf::Reflect |
                    Mf::Refract => res_arg.clone(),
                    // computational
                    Mf::Sign |
                    Mf::Fma |
                    Mf::Mix |
                    Mf::Step |
                    Mf::SmoothStep |
                    Mf::Sqrt |
                    Mf::InverseSqrt => res_arg.clone(),
                    Mf::Transpose => match *res_arg.inner_with(types) {
                        Ti::Matrix {
                            columns,
                            rows,
                            width,
                        } => TypeResolution::Value(Ti::Matrix {
                            columns: rows,
                            rows: columns,
                            width,
                        }),
                        ref other => return Err(ResolveError::IncompatibleOperands(
                            format!("{:?}({:?})", fun, other)
                        )),
                    },
                    Mf::Inverse => match *res_arg.inner_with(types) {
                        Ti::Matrix {
                            columns,
                            rows,
                            width,
                        } if columns == rows => TypeResolution::Value(Ti::Matrix {
                            columns,
                            rows,
                            width,
                        }),
                        ref other => return Err(ResolveError::IncompatibleOperands(
                            format!("{:?}({:?})", fun, other)
                        )),
                    },
                    Mf::Determinant => match *res_arg.inner_with(types) {
                        Ti::Matrix {
                            width,
                            ..
                        } => TypeResolution::Value(Ti::Scalar { kind: crate::ScalarKind::Float, width }),
                        ref other => return Err(ResolveError::IncompatibleOperands(
                            format!("{:?}({:?})", fun, other)
                        )),
                    },
                    // bits
                    Mf::CountOneBits |
                    Mf::ReverseBits |
                    Mf::ExtractBits |
                    Mf::InsertBits => res_arg.clone(),
                    // data packing
                    Mf::Pack4x8snorm |
                    Mf::Pack4x8unorm |
                    Mf::Pack2x16snorm |
                    Mf::Pack2x16unorm |
                    Mf::Pack2x16float => TypeResolution::Value(Ti::Scalar { kind: crate::ScalarKind::Uint, width: 4 }),
                    // data unpacking
                    Mf::Unpack4x8snorm |
                    Mf::Unpack4x8unorm => TypeResolution::Value(Ti::Vector { size: crate::VectorSize::Quad, kind: crate::ScalarKind::Float, width: 4 }),
                    Mf::Unpack2x16snorm |
                    Mf::Unpack2x16unorm |
                    Mf::Unpack2x16float => TypeResolution::Value(Ti::Vector { size: crate::VectorSize::Bi, kind: crate::ScalarKind::Float, width: 4 }),
                }
            }
            crate::Expression::As {
                expr,
                kind,
                convert,
            } => match *past(expr).inner_with(types) {
                Ti::Scalar { kind: _, width } => TypeResolution::Value(Ti::Scalar {
                    kind,
                    width: convert.unwrap_or(width),
                }),
                Ti::Vector {
                    kind: _,
                    size,
                    width,
                } => TypeResolution::Value(Ti::Vector {
                    kind,
                    size,
                    width: convert.unwrap_or(width),
                }),
                ref other => {
                    return Err(ResolveError::IncompatibleOperands(format!(
                        "{:?} as {:?}",
                        other, kind
                    )))
                }
            },
            crate::Expression::CallResult(function) => {
                let result = self.functions[function]
                    .result
                    .as_ref()
                    .ok_or(ResolveError::FunctionReturnsVoid)?;
                TypeResolution::Handle(result.ty)
            }
            crate::Expression::ArrayLength(_) => TypeResolution::Value(Ti::Scalar {
                kind: crate::ScalarKind::Uint,
                width: 4,
            }),
        })
    }
}

#[test]
fn test_error_size() {
    use std::mem::size_of;
    assert_eq!(size_of::<ResolveError>(), 32);
}
