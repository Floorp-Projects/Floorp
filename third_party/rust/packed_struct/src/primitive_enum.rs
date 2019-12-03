use internal_prelude::v1::*;

/// An enum type that can be packed or unpacked from a simple primitive integer.
pub trait PrimitiveEnum where Self: Sized + Copy {
    /// The primitve type into which we serialize and deserialize ourselves.
    type Primitive: PartialEq + Sized + Copy + Debug;

    /// Convert from a primitive, might fail.
    fn from_primitive(val: Self::Primitive) -> Option<Self>;
    /// Convert to a primitive value.
    fn to_primitive(&self) -> Self::Primitive;
    /// Convert from a string value representing the variant. Case sensitive.
    fn from_str(s: &str) -> Option<Self>;
    /// Convert from a string value representing the variant. Lowercase.
    fn from_str_lower(s: &str) -> Option<Self>;
}

/// Static display formatters.
pub trait PrimitiveEnumStaticStr where Self: Sized + Copy + PrimitiveEnum {
    /// Display value, same as the name of a particular variant.
    fn to_display_str(&self) -> &'static str;
    /// A list all possible string variants.
    fn all_variants() -> &'static [Self];
}

#[cfg(any(feature="alloc", feature="std"))]
/// Dynamic display formatters.
pub trait PrimitiveEnumDynamicStr where Self: Sized + Copy + PrimitiveEnum {
    /// Display value, same as the name of a particular variant.
    fn to_display_str(&self) -> Cow<'static, str>;
    /// A list all possible string variants.
    fn all_variants() -> Cow<'static, [Self]>;
}

/// A wrapper for primitive enums that supports catching and retaining any values
/// that don't have defined discriminants.
#[derive(Copy, Clone, Debug)]
pub enum EnumCatchAll<E> where E: PrimitiveEnum {
    /// A matched discriminant
    Enum(E),
    /// Some other value, stored as the primitive type
    CatchAll(E::Primitive)
}

impl<E> EnumCatchAll<E> where E: PrimitiveEnum {
    pub fn from_enum(v: E) -> Self {
        EnumCatchAll::Enum(v)
    }
}

impl<E> From<E> for EnumCatchAll<E> where E: PrimitiveEnum {
    fn from(v: E) -> Self {
        EnumCatchAll::Enum(v)
    }
}

impl<E> PartialEq<Self> for EnumCatchAll<E> where E: PrimitiveEnum {
    fn eq(&self, other: &Self) -> bool {
        self.to_primitive() == other.to_primitive()
    }
}

impl<E> PrimitiveEnum for EnumCatchAll<E> 
    where E: PrimitiveEnum
{
    type Primitive = E::Primitive;

    fn from_primitive(val: E::Primitive) -> Option<Self> {
        match E::from_primitive(val) {
            Some(p) => Some(EnumCatchAll::Enum(p)),
            None => Some(EnumCatchAll::CatchAll(val))
        }
    }

    fn to_primitive(&self) -> E::Primitive {
        match *self {
            EnumCatchAll::Enum(p) => p.to_primitive(),
            EnumCatchAll::CatchAll(v) => v
        }
    }

    fn from_str(s: &str) -> Option<Self> {
        E::from_str(s).map(|e| EnumCatchAll::Enum(e))
    }

    fn from_str_lower(s: &str) -> Option<Self> {
        E::from_str_lower(s).map(|e| EnumCatchAll::Enum(e))
    }
}

#[cfg(any(feature="alloc", feature="std"))]
impl<E> PrimitiveEnumDynamicStr for EnumCatchAll<E> 
    where E: PrimitiveEnum + PrimitiveEnumDynamicStr
{
    /// Display value, same as the name of a particular variant.
    fn to_display_str(&self) -> Cow<'static, str> {
        match *self {
            EnumCatchAll::Enum(p) => p.to_display_str(),
            EnumCatchAll::CatchAll(v) => format!("Unknown value: {:?}", v).into()
        }
    }

    fn all_variants() -> Cow<'static, [Self]> {
        let l: Vec<_> = E::all_variants().iter().map(|v| EnumCatchAll::Enum(*v)).collect();
        Cow::from(l)
    }
}