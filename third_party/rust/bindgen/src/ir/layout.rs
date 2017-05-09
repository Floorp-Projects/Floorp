//! Intermediate representation for the physical layout of some type.

use super::context::BindgenContext;
use super::derive::{CanDeriveCopy, CanDeriveDebug, CanDeriveDefault};
use super::ty::{RUST_DERIVE_IN_ARRAY_LIMIT, Type, TypeKind};
use clang;
use std::{cmp, mem};

/// A type that represents the struct layout of a type.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Layout {
    /// The size (in bytes) of this layout.
    pub size: usize,
    /// The alignment (in bytes) of this layout.
    pub align: usize,
    /// Whether this layout's members are packed or not.
    pub packed: bool,
}

#[test]
fn test_layout_for_size() {
    let ptr_size = mem::size_of::<*mut ()>();
    assert_eq!(Layout::for_size(ptr_size), Layout::new(ptr_size, ptr_size));
    assert_eq!(Layout::for_size(3 * ptr_size),
               Layout::new(3 * ptr_size, ptr_size));
}

impl Layout {
    /// Construct a new `Layout` with the given `size` and `align`. It is not
    /// packed.
    pub fn new(size: usize, align: usize) -> Self {
        Layout {
            size: size,
            align: align,
            packed: false,
        }
    }

    /// Creates a non-packed layout for a given size, trying to use the maximum
    /// alignment possible.
    pub fn for_size(size: usize) -> Self {
        let mut next_align = 2;
        while size % next_align == 0 &&
              next_align <= mem::size_of::<*mut ()>() {
            next_align *= 2;
        }
        Layout {
            size: size,
            align: next_align / 2,
            packed: false,
        }
    }

    /// Is this a zero-sized layout?
    pub fn is_zero(&self) -> bool {
        self.size == 0 && self.align == 0
    }

    /// Construct a zero-sized layout.
    pub fn zero() -> Self {
        Self::new(0, 0)
    }

    /// Get this layout as an opaque type.
    pub fn opaque(&self) -> Opaque {
        Opaque(*self)
    }
}

/// When we are treating a type as opaque, it is just a blob with a `Layout`.
#[derive(Clone, Debug, PartialEq)]
pub struct Opaque(pub Layout);

impl Opaque {
    /// Construct a new opaque type from the given clang type.
    pub fn from_clang_ty(ty: &clang::Type) -> Type {
        let layout = Layout::new(ty.size(), ty.align());
        let ty_kind = TypeKind::Opaque;
        Type::new(None, Some(layout), ty_kind, false)
    }

    /// Return the known rust type we should use to create a correctly-aligned
    /// field with this layout.
    pub fn known_rust_type_for_array(&self) -> Option<&'static str> {
        Some(match self.0.align {
            8 => "u64",
            4 => "u32",
            2 => "u16",
            1 => "u8",
            _ => return None,
        })
    }

    /// Return the array size that an opaque type for this layout should have if
    /// we know the correct type for it, or `None` otherwise.
    pub fn array_size(&self) -> Option<usize> {
        if self.known_rust_type_for_array().is_some() {
            Some(self.0.size / cmp::max(self.0.align, 1))
        } else {
            None
        }
    }
}

impl CanDeriveDebug for Opaque {
    type Extra = ();

    fn can_derive_debug(&self, _: &BindgenContext, _: ()) -> bool {
        self.array_size()
            .map_or(false, |size| size <= RUST_DERIVE_IN_ARRAY_LIMIT)
    }
}

impl CanDeriveDefault for Opaque {
    type Extra = ();

    fn can_derive_default(&self, _: &BindgenContext, _: ()) -> bool {
        self.array_size()
            .map_or(false, |size| size <= RUST_DERIVE_IN_ARRAY_LIMIT)
    }
}

impl<'a> CanDeriveCopy<'a> for Opaque {
    type Extra = ();

    fn can_derive_copy(&self, _: &BindgenContext, _: ()) -> bool {
        self.array_size()
            .map_or(false, |size| size <= RUST_DERIVE_IN_ARRAY_LIMIT)
    }

    fn can_derive_copy_in_array(&self, ctx: &BindgenContext, _: ()) -> bool {
        self.can_derive_copy(ctx, ())
    }
}
