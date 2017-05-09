//! Helpers for code generation that need struct layout

use super::helpers::BlobTyBuilder;

use aster::struct_field::StructFieldBuilder;

use ir::comp::CompInfo;
use ir::context::BindgenContext;
use ir::layout::Layout;
use ir::ty::{Type, TypeKind};
use std::cmp;
use std::mem;

use syntax::ast;

/// Trace the layout of struct.
pub struct StructLayoutTracker<'a, 'ctx: 'a> {
    ctx: &'a BindgenContext<'ctx>,
    comp: &'a CompInfo,
    latest_offset: usize,
    padding_count: usize,
    latest_field_layout: Option<Layout>,
    max_field_align: usize,
    last_field_was_bitfield: bool,
}

/// Returns a size aligned to a given value.
pub fn align_to(size: usize, align: usize) -> usize {
    if align == 0 {
        return size;
    }

    let rem = size % align;
    if rem == 0 {
        return size;
    }

    size + align - rem
}

/// Returns the amount of bytes from a given amount of bytes, rounding up.
pub fn bytes_from_bits(n: usize) -> usize {
    if n % 8 == 0 {
        return n / 8;
    }

    n / 8 + 1
}

/// Returns the lower power of two byte count that can hold at most n bits.
pub fn bytes_from_bits_pow2(mut n: usize) -> usize {
    if n == 0 {
        return 0;
    }

    if n <= 8 {
        return 1;
    }

    if !n.is_power_of_two() {
        n = n.next_power_of_two();
    }

    n / 8
}

#[test]
fn test_align_to() {
    assert_eq!(align_to(1, 1), 1);
    assert_eq!(align_to(1, 2), 2);
    assert_eq!(align_to(1, 4), 4);
    assert_eq!(align_to(5, 1), 5);
    assert_eq!(align_to(17, 4), 20);
}

#[test]
fn test_bytes_from_bits_pow2() {
    assert_eq!(bytes_from_bits_pow2(0), 0);
    for i in 1..9 {
        assert_eq!(bytes_from_bits_pow2(i), 1);
    }
    for i in 9..17 {
        assert_eq!(bytes_from_bits_pow2(i), 2);
    }
    for i in 17..33 {
        assert_eq!(bytes_from_bits_pow2(i), 4);
    }
}

#[test]
fn test_bytes_from_bits() {
    assert_eq!(bytes_from_bits(0), 0);
    for i in 1..9 {
        assert_eq!(bytes_from_bits(i), 1);
    }
    for i in 9..17 {
        assert_eq!(bytes_from_bits(i), 2);
    }
    for i in 17..25 {
        assert_eq!(bytes_from_bits(i), 3);
    }
}

impl<'a, 'ctx> StructLayoutTracker<'a, 'ctx> {
    pub fn new(ctx: &'a BindgenContext<'ctx>, comp: &'a CompInfo) -> Self {
        StructLayoutTracker {
            ctx: ctx,
            comp: comp,
            latest_offset: 0,
            padding_count: 0,
            latest_field_layout: None,
            max_field_align: 0,
            last_field_was_bitfield: false,
        }
    }

    pub fn saw_vtable(&mut self) {
        let ptr_size = mem::size_of::<*mut ()>();
        self.latest_offset += ptr_size;
        self.latest_field_layout = Some(Layout::new(ptr_size, ptr_size));
        self.max_field_align = ptr_size;
    }

    pub fn saw_base(&mut self, base_ty: &Type) {
        if let Some(layout) = base_ty.layout(self.ctx) {
            self.align_to_latest_field(layout);

            self.latest_offset += self.padding_bytes(layout) + layout.size;
            self.latest_field_layout = Some(layout);
            self.max_field_align = cmp::max(self.max_field_align, layout.align);
        }
    }

    pub fn saw_bitfield_batch(&mut self, layout: Layout) {
        self.align_to_latest_field(layout);

        self.latest_offset += layout.size;

        debug!("Offset: <bitfield>: {} -> {}",
               self.latest_offset - layout.size,
               self.latest_offset);

        self.latest_field_layout = Some(layout);
        self.last_field_was_bitfield = true;
        // NB: We intentionally don't update the max_field_align here, since our
        // bitfields code doesn't necessarily guarantee it, so we need to
        // actually generate the dummy alignment.
    }

    pub fn saw_union(&mut self, layout: Layout) {
        self.align_to_latest_field(layout);

        self.latest_offset += self.padding_bytes(layout) + layout.size;
        self.latest_field_layout = Some(layout);
        self.max_field_align = cmp::max(self.max_field_align, layout.align);
    }

    /// Add a padding field if necessary for a given new field _before_ adding
    /// that field.
    pub fn pad_field(&mut self,
                     field_name: &str,
                     field_ty: &Type,
                     field_offset: Option<usize>)
                     -> Option<ast::StructField> {
        let mut field_layout = match field_ty.layout(self.ctx) {
            Some(l) => l,
            None => return None,
        };

        if let TypeKind::Array(inner, len) =
            *field_ty.canonical_type(self.ctx).kind() {
            // FIXME(emilio): As an _ultra_ hack, we correct the layout returned
            // by arrays of structs that have a bigger alignment than what we
            // can support.
            //
            // This means that the structs in the array are super-unsafe to
            // access, since they won't be properly aligned, but *shrug*.
            if let Some(layout) = self.ctx
                .resolve_type(inner)
                .layout(self.ctx) {
                if layout.align > mem::size_of::<*mut ()>() {
                    field_layout.size = align_to(layout.size, layout.align) *
                                        len;
                    field_layout.align = mem::size_of::<*mut ()>();
                }
            }
        }

        let will_merge_with_bitfield = self.align_to_latest_field(field_layout);

        let padding_layout = if self.comp.packed() {
            None
        } else {
            let padding_bytes = match field_offset {
                Some(offset) if offset / 8 > self.latest_offset => {
                    offset / 8 - self.latest_offset
                }
                _ if will_merge_with_bitfield || field_layout.align == 0 => 0,
                _ => self.padding_bytes(field_layout),
            };

            // Otherwise the padding is useless.
            let need_padding = padding_bytes >= field_layout.align ||
                               field_layout.align > mem::size_of::<*mut ()>();

            self.latest_offset += padding_bytes;

            debug!("Offset: <padding>: {} -> {}",
                   self.latest_offset - padding_bytes,
                   self.latest_offset);

            debug!("align field {} to {}/{} with {} padding bytes {:?}",
                   field_name,
                   self.latest_offset,
                   field_offset.unwrap_or(0) / 8,
                   padding_bytes,
                   field_layout);

            if need_padding && padding_bytes != 0 {
                Some(Layout::new(padding_bytes,
                                 cmp::min(field_layout.align,
                                          mem::size_of::<*mut ()>())))
            } else {
                None
            }
        };

        self.latest_offset += field_layout.size;
        self.latest_field_layout = Some(field_layout);
        self.max_field_align = cmp::max(self.max_field_align,
                                        field_layout.align);
        self.last_field_was_bitfield = false;

        debug!("Offset: {}: {} -> {}",
               field_name,
               self.latest_offset - field_layout.size,
               self.latest_offset);

        padding_layout.map(|layout| self.padding_field(layout))
    }

    pub fn pad_struct(&mut self,
                      name: &str,
                      layout: Layout)
                      -> Option<ast::StructField> {
        if layout.size < self.latest_offset {
            error!("Calculated wrong layout for {}, too more {} bytes",
                   name,
                   self.latest_offset - layout.size);
            return None;
        }

        let padding_bytes = layout.size - self.latest_offset;

        // We always pad to get to the correct size if the struct is one of
        // those we can't align properly.
        //
        // Note that if the last field we saw was a bitfield, we may need to pad
        // regardless, because bitfields don't respect alignment as strictly as
        // other fields.
        if padding_bytes > 0 &&
           (padding_bytes >= layout.align ||
            (self.last_field_was_bitfield &&
             padding_bytes >= self.latest_field_layout.unwrap().align) ||
            layout.align > mem::size_of::<*mut ()>()) {
            let layout = if self.comp.packed() {
                Layout::new(padding_bytes, 1)
            } else if self.last_field_was_bitfield ||
                                   layout.align > mem::size_of::<*mut ()>() {
                // We've already given up on alignment here.
                Layout::for_size(padding_bytes)
            } else {
                Layout::new(padding_bytes, layout.align)
            };

            debug!("pad bytes to struct {}, {:?}", name, layout);

            Some(self.padding_field(layout))
        } else {
            None
        }
    }

    pub fn align_struct(&self, layout: Layout) -> Option<ast::StructField> {
        if self.max_field_align < layout.align &&
           layout.align <= mem::size_of::<*mut ()>() {
            let ty = BlobTyBuilder::new(Layout::new(0, layout.align)).build();

            Some(StructFieldBuilder::named("__bindgen_align")
                .pub_()
                .build_ty(ty))
        } else {
            None
        }
    }

    fn padding_bytes(&self, layout: Layout) -> usize {
        align_to(self.latest_offset, layout.align) - self.latest_offset
    }

    fn padding_field(&mut self, layout: Layout) -> ast::StructField {
        let ty = BlobTyBuilder::new(layout).build();
        let padding_count = self.padding_count;

        self.padding_count += 1;

        let padding_field_name = format!("__bindgen_padding_{}", padding_count);

        self.max_field_align = cmp::max(self.max_field_align, layout.align);

        StructFieldBuilder::named(padding_field_name).pub_().build_ty(ty)
    }

    /// Returns whether the new field is known to merge with a bitfield.
    ///
    /// This is just to avoid doing the same check also in pad_field.
    fn align_to_latest_field(&mut self, new_field_layout: Layout) -> bool {
        if self.comp.packed() {
            // Skip to align fields when packed.
            return false;
        }

        let layout = match self.latest_field_layout {
            Some(l) => l,
            None => return false,
        };

        // If it was, we may or may not need to align, depending on what the
        // current field alignment and the bitfield size and alignment are.
        debug!("align_to_bitfield? {}: {:?} {:?}",
               self.last_field_was_bitfield,
               layout,
               new_field_layout);

        if self.last_field_was_bitfield &&
           new_field_layout.align <= layout.size % layout.align &&
           new_field_layout.size <= layout.size % layout.align {
            // The new field will be coalesced into some of the remaining bits.
            //
            // FIXME(emilio): I think this may not catch everything?
            debug!("Will merge with bitfield");
            return true;
        }

        // Else, just align the obvious way.
        self.latest_offset += self.padding_bytes(layout);
        return false;
    }
}
