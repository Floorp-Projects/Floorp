use crate::{encode_section, Encode, Section, SectionId};

/// Represents a subtype of possible other types in a WebAssembly module.
#[derive(Debug, Clone)]
pub struct SubType {
    /// Is the subtype final.
    pub is_final: bool,
    /// The list of supertype indexes. As of GC MVP, there can be at most one supertype.
    pub supertype_idx: Option<u32>,
    /// The structural type of the subtype.
    pub structural_type: StructuralType,
}

/// Represents a structural type in a WebAssembly module.
#[derive(Debug, Clone)]
pub enum StructuralType {
    /// The type is for a function.
    Func(FuncType),
    /// The type is for an array.
    Array(ArrayType),
    /// The type is for a struct.
    Struct(StructType),
}

/// Represents a type of a function in a WebAssembly module.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct FuncType {
    /// The combined parameters and result types.
    params_results: Box<[ValType]>,
    /// The number of parameter types.
    len_params: usize,
}

/// Represents a type of an array in a WebAssembly module.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct ArrayType(pub FieldType);

/// Represents a type of a struct in a WebAssembly module.
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct StructType {
    /// Struct fields.
    pub fields: Box<[FieldType]>,
}

/// Field type in structural types (structs, arrays).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Ord, PartialOrd)]
pub struct FieldType {
    /// Storage type of the field.
    pub element_type: StorageType,
    /// Is the field mutable.
    pub mutable: bool,
}

/// Storage type for structural type fields.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Ord, PartialOrd)]
pub enum StorageType {
    /// The `i8` type.
    I8,
    /// The `i16` type.
    I16,
    /// A value type.
    Val(ValType),
}

/// The type of a core WebAssembly value.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Ord, PartialOrd)]
pub enum ValType {
    /// The `i32` type.
    I32,
    /// The `i64` type.
    I64,
    /// The `f32` type.
    F32,
    /// The `f64` type.
    F64,
    /// The `v128` type.
    ///
    /// Part of the SIMD proposal.
    V128,
    /// A reference type.
    ///
    /// The `funcref` and `externref` type fall into this category and the full
    /// generalization here is due to the implementation of the
    /// function-references proposal.
    Ref(RefType),
}

impl FuncType {
    /// Creates a new [`FuncType`] from the given `params` and `results`.
    pub fn new<P, R>(params: P, results: R) -> Self
    where
        P: IntoIterator<Item = ValType>,
        R: IntoIterator<Item = ValType>,
    {
        let mut buffer = params.into_iter().collect::<Vec<_>>();
        let len_params = buffer.len();
        buffer.extend(results);
        Self {
            params_results: buffer.into(),
            len_params,
        }
    }

    /// Returns a shared slice to the parameter types of the [`FuncType`].
    #[inline]
    pub fn params(&self) -> &[ValType] {
        &self.params_results[..self.len_params]
    }

    /// Returns a shared slice to the result types of the [`FuncType`].
    #[inline]
    pub fn results(&self) -> &[ValType] {
        &self.params_results[self.len_params..]
    }
}

impl ValType {
    /// Alias for the `funcref` type in WebAssembly
    pub const FUNCREF: ValType = ValType::Ref(RefType::FUNCREF);
    /// Alias for the `externref` type in WebAssembly
    pub const EXTERNREF: ValType = ValType::Ref(RefType::EXTERNREF);
}

impl Encode for StorageType {
    fn encode(&self, sink: &mut Vec<u8>) {
        match self {
            StorageType::I8 => sink.push(0x78),
            StorageType::I16 => sink.push(0x77),
            StorageType::Val(vt) => vt.encode(sink),
        }
    }
}

impl Encode for ValType {
    fn encode(&self, sink: &mut Vec<u8>) {
        match self {
            ValType::I32 => sink.push(0x7F),
            ValType::I64 => sink.push(0x7E),
            ValType::F32 => sink.push(0x7D),
            ValType::F64 => sink.push(0x7C),
            ValType::V128 => sink.push(0x7B),
            ValType::Ref(rt) => rt.encode(sink),
        }
    }
}

/// A reference type.
///
/// This is largely part of the function references proposal for WebAssembly but
/// additionally is used by the `funcref` and `externref` types. The full
/// generality of this type is only exercised with function-references.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Ord, PartialOrd)]
#[allow(missing_docs)]
pub struct RefType {
    pub nullable: bool,
    pub heap_type: HeapType,
}

impl RefType {
    /// Alias for the `funcref` type in WebAssembly
    pub const FUNCREF: RefType = RefType {
        nullable: true,
        heap_type: HeapType::Func,
    };

    /// Alias for the `externref` type in WebAssembly
    pub const EXTERNREF: RefType = RefType {
        nullable: true,
        heap_type: HeapType::Extern,
    };
}

impl Encode for RefType {
    fn encode(&self, sink: &mut Vec<u8>) {
        if self.nullable {
            // Favor the original encodings of `funcref` and `externref` where
            // possible
            match self.heap_type {
                HeapType::Func => return sink.push(0x70),
                HeapType::Extern => return sink.push(0x6f),
                _ => {}
            }
        }

        if self.nullable {
            sink.push(0x63);
        } else {
            sink.push(0x64);
        }
        self.heap_type.encode(sink);
    }
}

impl From<RefType> for ValType {
    fn from(ty: RefType) -> ValType {
        ValType::Ref(ty)
    }
}

/// Part of the function references proposal.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Ord, PartialOrd)]
pub enum HeapType {
    /// Untyped (any) function.
    Func,
    /// External heap type.
    Extern,
    /// The `any` heap type. The common supertype (a.k.a. top) of all internal types.
    Any,
    /// The `none` heap type. The common subtype (a.k.a. bottom) of all internal types.
    None,
    /// The `noextern` heap type. The common subtype (a.k.a. bottom) of all external types.
    NoExtern,
    /// The `nofunc` heap type. The common subtype (a.k.a. bottom) of all function types.
    NoFunc,
    /// The `eq` heap type. The common supertype of all referenceable types on which comparison
    /// (ref.eq) is allowed.
    Eq,
    /// The `struct` heap type. The common supertype of all struct types.
    Struct,
    /// The `array` heap type. The common supertype of all array types.
    Array,
    /// The i31 heap type.
    I31,
    /// User defined type at the given index.
    Indexed(u32),
}

impl Encode for HeapType {
    fn encode(&self, sink: &mut Vec<u8>) {
        match self {
            HeapType::Func => sink.push(0x70),
            HeapType::Extern => sink.push(0x6F),
            HeapType::Any => sink.push(0x6E),
            HeapType::None => sink.push(0x71),
            HeapType::NoExtern => sink.push(0x72),
            HeapType::NoFunc => sink.push(0x73),
            HeapType::Eq => sink.push(0x6D),
            HeapType::Struct => sink.push(0x6B),
            HeapType::Array => sink.push(0x6A),
            HeapType::I31 => sink.push(0x6C),
            // Note that this is encoded as a signed type rather than unsigned
            // as it's decoded as an s33
            HeapType::Indexed(i) => i64::from(*i).encode(sink),
        }
    }
}

/// An encoder for the type section of WebAssembly modules.
///
/// # Example
///
/// ```rust
/// use wasm_encoder::{Module, TypeSection, ValType};
///
/// let mut types = TypeSection::new();
///
/// types.function([ValType::I32, ValType::I32], [ValType::I64]);
///
/// let mut module = Module::new();
/// module.section(&types);
///
/// let bytes = module.finish();
/// ```
#[derive(Clone, Debug, Default)]
pub struct TypeSection {
    bytes: Vec<u8>,
    num_added: u32,
}

impl TypeSection {
    /// Create a new module type section encoder.
    pub fn new() -> Self {
        Self::default()
    }

    /// The number of types in the section.
    pub fn len(&self) -> u32 {
        self.num_added
    }

    /// Determines if the section is empty.
    pub fn is_empty(&self) -> bool {
        self.num_added == 0
    }

    /// Define a function type in this type section.
    pub fn function<P, R>(&mut self, params: P, results: R) -> &mut Self
    where
        P: IntoIterator<Item = ValType>,
        P::IntoIter: ExactSizeIterator,
        R: IntoIterator<Item = ValType>,
        R::IntoIter: ExactSizeIterator,
    {
        let params = params.into_iter();
        let results = results.into_iter();

        self.bytes.push(0x60);
        params.len().encode(&mut self.bytes);
        params.for_each(|p| p.encode(&mut self.bytes));
        results.len().encode(&mut self.bytes);
        results.for_each(|p| p.encode(&mut self.bytes));
        self.num_added += 1;
        self
    }

    /// Define an array type in this type section.
    pub fn array(&mut self, ty: &StorageType, mutable: bool) -> &mut Self {
        self.bytes.push(0x5e);
        self.field(ty, mutable);
        self.num_added += 1;
        self
    }

    fn field(&mut self, ty: &StorageType, mutable: bool) -> &mut Self {
        ty.encode(&mut self.bytes);
        self.bytes.push(mutable as u8);
        self
    }

    /// Define a struct type in this type section.
    pub fn struct_(&mut self, fields: Vec<FieldType>) -> &mut Self {
        self.bytes.push(0x5f);
        fields.len().encode(&mut self.bytes);
        for f in fields.iter() {
            self.field(&f.element_type, f.mutable);
        }
        self.num_added += 1;
        self
    }

    /// Define an explicit subtype in this type section.
    pub fn subtype(&mut self, ty: &SubType) -> &mut Self {
        // In the GC spec, supertypes is a vector, not an option.
        let st = match ty.supertype_idx {
            Some(idx) => vec![idx],
            None => vec![],
        };
        if ty.is_final {
            self.bytes.push(0x4e);
            st.encode(&mut self.bytes);
        } else if !st.is_empty() {
            self.bytes.push(0x50);
            st.encode(&mut self.bytes);
        }

        match &ty.structural_type {
            StructuralType::Func(ty) => {
                self.function(ty.params().iter().copied(), ty.results().iter().copied());
            }
            StructuralType::Array(ArrayType(ty)) => {
                self.array(&ty.element_type, ty.mutable);
            }
            StructuralType::Struct(ty) => {
                self.struct_(ty.fields.to_vec());
            }
        }

        self
    }
}

impl Encode for TypeSection {
    fn encode(&self, sink: &mut Vec<u8>) {
        encode_section(sink, self.num_added, &self.bytes);
    }
}

impl Section for TypeSection {
    fn id(&self) -> u8 {
        SectionId::Type.into()
    }
}
