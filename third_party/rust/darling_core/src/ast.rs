//! Utility types for working with the AST.

use syn;

use {Error, FromField, FromVariant, Result};

/// A struct or enum body. 
///
/// `V` is the type which receives any encountered variants, and `F` receives struct fields.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Body<V, F> {
    Enum(Vec<V>),
    Struct(VariantData<F>),
}

impl<V, F> Body<V, F> {
    /// Creates an empty body of the same shape as the passed-in body.
    pub fn empty_from(src: &syn::Body) -> Self {
        match *src {
            syn::Body::Enum(_) => Body::Enum(vec![]),
            syn::Body::Struct(ref vd) => Body::Struct(VariantData::empty_from(vd)),
        }
    }

    /// Creates a new `Body<&'a V, &'a F>` instance from `Body<V, F>`.
    pub fn as_ref<'a>(&'a self) -> Body<&'a V, &'a F> {
        match *self {
            Body::Enum(ref variants) => Body::Enum(variants.into_iter().collect()),
            Body::Struct(ref data) => Body::Struct(data.as_ref()),
        }
    }

    /// Applies a function `V -> U` on enum variants, if this is an enum.
    pub fn map_enum_variants<T, U>(self, map: T) -> Body<U, F>
        where T: FnMut(V) -> U
    {
        match self {
            Body::Enum(v) => Body::Enum(v.into_iter().map(map).collect()),
            Body::Struct(f) => Body::Struct(f),
        }
    }

    /// Applies a function `F -> U` on struct fields, if this is a struct.
    pub fn map_struct_fields<T, U>(self, map: T) -> Body<V, U>
        where T: FnMut(F) -> U
    {
        match self {
            Body::Enum(v) => Body::Enum(v),
            Body::Struct(f) => Body::Struct(f.map(map)),
        }
    }

    /// Applies a function to the `VariantData` if this is a struct.
    pub fn map_struct<T, U>(self, mut map: T) -> Body<V, U>
        where T: FnMut(VariantData<F>) -> VariantData<U>
    {
        match self {
            Body::Enum(v) => Body::Enum(v),
            Body::Struct(f) => Body::Struct(map(f)),
        }
    }

    /// Consumes the `Body`, returning `VariantData<F>` if it was a struct.
    pub fn take_struct(self) -> Option<VariantData<F>> {
        match self {
            Body::Enum(_) => None,
            Body::Struct(f) => Some(f),
        }
    }

    /// Consumes the `Body`, returning `Vec<V>` if it was an enum.
    pub fn take_enum(self) -> Option<Vec<V>> {
        match self {
            Body::Enum(v) => Some(v),
            Body::Struct(_) => None,
        }
    }

    /// Returns `true` if this instance is `Body::Enum`.
    pub fn is_enum(&self) -> bool {
        match *self {
            Body::Enum(_) => true,
            Body::Struct(_) => false,
        }
    }

    /// Returns `true` if this instance is `Body::Struct`.
    pub fn is_struct(&self) -> bool {
        !self.is_enum()
    }
}

impl<V: FromVariant, F: FromField> Body<V, F> {
    /// Attempt to convert from a `syn::Body` instance.
    pub fn try_from(body: &syn::Body) -> Result<Self> {
        match *body {
            syn::Body::Enum(ref variants) => {
                let mut items = Vec::with_capacity(variants.len());
                let mut errors = Vec::new();
                for v_result in variants.into_iter().map(FromVariant::from_variant) {
                    match v_result {
                        Ok(val) => items.push(val),
                        Err(err) => errors.push(err)
                    }
                }

                if !errors.is_empty() {
                    Err(Error::multiple(errors))
                } else {
                    Ok(Body::Enum(items))
                }
            }
            syn::Body::Struct(ref data) => Ok(Body::Struct(VariantData::try_from(data)?)),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VariantData<T> {
    pub style: Style,
    pub fields: Vec<T>,
}

impl<T> VariantData<T> {
    pub fn empty_from(vd: &syn::VariantData) -> Self {
        VariantData {
            style: vd.into(),
            fields: Vec::new(),
        }
    }

    /// Splits the `VariantData` into its style and fields for further processing.
    /// Returns an empty `Vec` for `Unit` data.
    pub fn split(self) -> (Style, Vec<T>) {
        (self.style, self.fields)
    }

    /// Returns true if this variant's data makes it a newtype.
    pub fn is_newtype(&self) -> bool {
        self.style == Style::Tuple && self.fields.len() == 1
    }

    pub fn is_unit(&self) -> bool {
        self.style.is_unit()
    }

    pub fn is_tuple(&self) -> bool {
        self.style.is_tuple()
    }

    pub fn is_struct(&self) -> bool {
        self.style.is_struct()
    }

    pub fn as_ref<'a>(&'a self) -> VariantData<&'a T> {
        VariantData {
            style: self.style,
            fields: self.fields.iter().collect(),
        }
    }

    pub fn map<F, U>(self, map: F) -> VariantData<U> where F: FnMut(T) -> U {
        VariantData {
            style: self.style,
            fields: self.fields.into_iter().map(map).collect()
        }
    }
}

impl<F: FromField> VariantData<F> {
    pub fn try_from(data: &syn::VariantData) -> Result<Self> {
        let fields = data.fields();
        let mut items = Vec::with_capacity(fields.len());
        let mut errors = Vec::new();

        for field in fields {
            let f_result = FromField::from_field(field);
            match f_result {
                Ok(val) => items.push(val),
                Err(err) => errors.push(if let Some(ref ident) = field.ident {
                    err.at(ident.as_ref())
                } else {
                    err
                })
            }
        }

        if !errors.is_empty() {
            Err(Error::multiple(errors))
        } else {
            Ok(VariantData {
                style: data.into(),
                fields: items,
            })   
        }
    }
}

impl<T> From<Style> for VariantData<T> {
    fn from(style: Style) -> Self {
        VariantData {
            style,
            fields: Vec::new(),
        }
    }
}

impl<T, U: Into<Vec<T>>> From<(Style, U)> for VariantData<T> {
    fn from((style, fields): (Style, U)) -> Self {
        style.with_fields(fields)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Style {
    Tuple,
    Struct,
    Unit,
}

impl Style {
    pub fn is_unit(&self) -> bool {
        *self == Style::Unit
    }

    pub fn is_tuple(&self) -> bool {
        *self == Style::Tuple
    }

    pub fn is_struct(&self) -> bool {
        *self == Style::Struct
    }

    /// Creates a new `VariantData` of the specified style with the passed-in fields.
    fn with_fields<T, U: Into<Vec<T>>>(self, fields: U) -> VariantData<T> {
        VariantData {
            style: self,
            fields: fields.into(),
        }
    }
}

impl From<syn::VariantData> for Style {
    fn from(vd: syn::VariantData) -> Self {
        (&vd).into()
    }
}

impl<'a> From<&'a syn::VariantData> for Style {
    fn from(vd: &syn::VariantData) -> Self {
        match *vd {
            syn::VariantData::Struct(_) => Style::Struct,
            syn::VariantData::Tuple(_) => Style::Tuple,
            syn::VariantData::Unit => Style::Unit,
        }
    }
}