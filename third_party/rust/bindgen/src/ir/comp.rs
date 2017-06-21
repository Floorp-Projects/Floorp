//! Compound types (unions and structs) in our intermediate representation.

use super::annotations::Annotations;
use super::context::{BindgenContext, ItemId};
use super::derive::{CanDeriveCopy, CanDeriveDebug, CanDeriveDefault};
use super::dot::DotAttributes;
use super::item::Item;
use super::layout::Layout;
use super::traversal::{EdgeKind, Trace, Tracer};
use super::template::TemplateParameters;
use clang;
use codegen::struct_layout::{align_to, bytes_from_bits_pow2};
use parse::{ClangItemParser, ParseError};
use peeking_take_while::PeekableExt;
use std::cell::Cell;
use std::cmp;
use std::io;
use std::mem;

/// The kind of compound type.
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum CompKind {
    /// A struct.
    Struct,
    /// A union.
    Union,
}

/// The kind of C++ method.
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum MethodKind {
    /// A constructor. We represent it as method for convenience, to avoid code
    /// duplication.
    Constructor,
    /// A destructor.
    Destructor,
    /// A virtual destructor.
    VirtualDestructor,
    /// A static method.
    Static,
    /// A normal method.
    Normal,
    /// A virtual method.
    Virtual,
}

/// A struct representing a C++ method, either static, normal, or virtual.
#[derive(Debug)]
pub struct Method {
    kind: MethodKind,
    /// The signature of the method. Take into account this is not a `Type`
    /// item, but a `Function` one.
    ///
    /// This is tricky and probably this field should be renamed.
    signature: ItemId,
    is_const: bool,
}

impl Method {
    /// Construct a new `Method`.
    pub fn new(kind: MethodKind, signature: ItemId, is_const: bool) -> Self {
        Method {
            kind: kind,
            signature: signature,
            is_const: is_const,
        }
    }

    /// What kind of method is this?
    pub fn kind(&self) -> MethodKind {
        self.kind
    }

    /// Is this a destructor method?
    pub fn is_destructor(&self) -> bool {
        self.kind == MethodKind::Destructor ||
        self.kind == MethodKind::VirtualDestructor
    }

    /// Is this a constructor?
    pub fn is_constructor(&self) -> bool {
        self.kind == MethodKind::Constructor
    }

    /// Is this a virtual method?
    pub fn is_virtual(&self) -> bool {
        self.kind == MethodKind::Virtual ||
        self.kind == MethodKind::VirtualDestructor
    }

    /// Is this a static method?
    pub fn is_static(&self) -> bool {
        self.kind == MethodKind::Static
    }

    /// Get the `ItemId` for the `Function` signature for this method.
    pub fn signature(&self) -> ItemId {
        self.signature
    }

    /// Is this a const qualified method?
    pub fn is_const(&self) -> bool {
        self.is_const
    }
}

/// Methods common to the various field types.
pub trait FieldMethods {
    /// Get the name of this field.
    fn name(&self) -> Option<&str>;

    /// Get the type of this field.
    fn ty(&self) -> ItemId;

    /// Get the comment for this field.
    fn comment(&self) -> Option<&str>;

    /// If this is a bitfield, how many bits does it need?
    fn bitfield(&self) -> Option<u32>;

    /// Is this field marked as `mutable`?
    fn is_mutable(&self) -> bool;

    /// Get the annotations for this field.
    fn annotations(&self) -> &Annotations;

    /// The offset of the field (in bits)
    fn offset(&self) -> Option<usize>;
}

/// A contiguous set of logical bitfields that live within the same physical
/// allocation unit. See 9.2.4 [class.bit] in the C++ standard and [section
/// 2.4.II.1 in the Itanium C++
/// ABI](http://itanium-cxx-abi.github.io/cxx-abi/abi.html#class-types).
#[derive(Debug)]
pub struct BitfieldUnit {
    nth: usize,
    layout: Layout,
    bitfields: Vec<Bitfield>,
}

impl BitfieldUnit {
    /// Get the 1-based index of this bitfield unit within its containing
    /// struct. Useful for generating a Rust struct's field name for this unit
    /// of bitfields.
    pub fn nth(&self) -> usize {
        self.nth
    }

    /// Get the layout within which these bitfields reside.
    pub fn layout(&self) -> Layout {
        self.layout
    }

    /// Get the bitfields within this unit.
    pub fn bitfields(&self) -> &[Bitfield] {
        &self.bitfields
    }
}

/// A struct representing a C++ field.
#[derive(Debug)]
pub enum Field {
    /// A normal data member.
    DataMember(FieldData),

    /// A physical allocation unit containing many logical bitfields.
    Bitfields(BitfieldUnit),
}

impl Field {
    fn has_destructor(&self, ctx: &BindgenContext) -> bool {
        match *self {
            Field::DataMember(ref data) => ctx.resolve_type(data.ty).has_destructor(ctx),
            // Bitfields may not be of a type that has a destructor.
            Field::Bitfields(BitfieldUnit { .. }) => false,
        }
    }

    /// Get this field's layout.
    pub fn layout(&self, ctx: &BindgenContext) -> Option<Layout> {
        match *self {
            Field::Bitfields(BitfieldUnit { layout, ..}) => Some(layout),
            Field::DataMember(ref data) => {
                ctx.resolve_type(data.ty).layout(ctx)
            }
        }
    }
}

impl Trace for Field {
    type Extra = ();

    fn trace<T>(&self, _: &BindgenContext, tracer: &mut T, _: &())
        where T: Tracer,
    {
        match *self {
            Field::DataMember(ref data) => {
                tracer.visit_kind(data.ty, EdgeKind::Field);
            }
            Field::Bitfields(BitfieldUnit { ref bitfields, .. }) => {
                for bf in bitfields {
                    tracer.visit_kind(bf.ty(), EdgeKind::Field);
                }
            }
        }
    }
}

impl DotAttributes for Field {
    fn dot_attributes<W>(&self, ctx: &BindgenContext, out: &mut W) -> io::Result<()>
        where W: io::Write
    {
        match *self {
            Field::DataMember(ref data) => {
                data.dot_attributes(ctx, out)
            }
            Field::Bitfields(BitfieldUnit { layout, ref bitfields, .. }) => {
                writeln!(out,
                         r#"<tr>
                              <td>bitfield unit</td>
                              <td>
                                <table border="0">
                                  <tr>
                                    <td>unit.size</td><td>{}</td>
                                  </tr>
                                  <tr>
                                    <td>unit.align</td><td>{}</td>
                                  </tr>
                         "#,
                         layout.size,
                         layout.align)?;
                for bf in bitfields {
                    bf.dot_attributes(ctx, out)?;
                }
                writeln!(out, "</table></td></tr>")
            }
        }
    }
}

impl DotAttributes for FieldData {
    fn dot_attributes<W>(&self, _ctx: &BindgenContext, out: &mut W) -> io::Result<()>
        where W: io::Write
    {
        writeln!(out,
                 "<tr><td>{}</td><td>{:?}</td></tr>",
                 self.name().unwrap_or("(anonymous)"),
                 self.ty())
    }
}

impl DotAttributes for Bitfield {
    fn dot_attributes<W>(&self, _ctx: &BindgenContext, out: &mut W) -> io::Result<()>
        where W: io::Write
    {
        writeln!(out,
                 "<tr><td>{} : {}</td><td>{:?}</td></tr>",
                 self.name(),
                 self.width(),
                 self.ty())
    }
}

/// A logical bitfield within some physical bitfield allocation unit.
#[derive(Debug)]
pub struct Bitfield {
    /// Index of the bit within this bitfield's allocation unit where this
    /// bitfield's bits begin.
    offset_into_unit: usize,

    /// The field data for this bitfield.
    data: FieldData,
}

impl Bitfield {
    /// Construct a new bitfield.
    fn new(offset_into_unit: usize, raw: RawField) -> Bitfield {
        assert!(raw.bitfield().is_some());
        assert!(raw.name().is_some());

        Bitfield {
            offset_into_unit: offset_into_unit,
            data: raw.0,
        }
    }

    /// Get the index of the bit within this bitfield's allocation unit where
    /// this bitfield begins.
    pub fn offset_into_unit(&self) -> usize {
        self.offset_into_unit
    }

    /// Get the mask value that when &'ed with this bitfield's allocation unit
    /// produces this bitfield's value.
    pub fn mask(&self) -> u64 {
        use std::mem;
        use std::u64;

        let unoffseted_mask =
            if self.width() as u64 == mem::size_of::<u64>() as u64 * 8 {
                u64::MAX
            } else {
                ((1u64 << self.width()) - 1u64)
            };

        unoffseted_mask << self.offset_into_unit()
    }

    /// Get the bit width of this bitfield.
    pub fn width(&self) -> u32 {
        self.data.bitfield().unwrap()
    }

    /// Get the name of this bitfield.
    pub fn name(&self) -> &str {
        self.data.name().unwrap()
    }
}

impl FieldMethods for Bitfield {
    fn name(&self) -> Option<&str> {
        self.data.name()
    }

    fn ty(&self) -> ItemId {
        self.data.ty()
    }

    fn comment(&self) -> Option<&str> {
        self.data.comment()
    }

    fn bitfield(&self) -> Option<u32> {
        self.data.bitfield()
    }

    fn is_mutable(&self) -> bool {
        self.data.is_mutable()
    }

    fn annotations(&self) -> &Annotations {
        self.data.annotations()
    }

    fn offset(&self) -> Option<usize> {
        self.data.offset()
    }
}


/// A raw field might be either of a plain data member or a bitfield within a
/// bitfield allocation unit, but we haven't processed it and determined which
/// yet (which would involve allocating it into a bitfield unit if it is a
/// bitfield).
#[derive(Debug)]
struct RawField(FieldData);

impl RawField {
    /// Construct a new `RawField`.
    fn new(name: Option<String>,
           ty: ItemId,
           comment: Option<String>,
           annotations: Option<Annotations>,
           bitfield: Option<u32>,
           mutable: bool,
           offset: Option<usize>)
           -> RawField {
        RawField(FieldData {
            name: name,
            ty: ty,
            comment: comment,
            annotations: annotations.unwrap_or_default(),
            bitfield: bitfield,
            mutable: mutable,
            offset: offset,
        })
    }
}

impl FieldMethods for RawField {
    fn name(&self) -> Option<&str> {
        self.0.name()
    }

    fn ty(&self) -> ItemId {
        self.0.ty()
    }

    fn comment(&self) -> Option<&str> {
        self.0.comment()
    }

    fn bitfield(&self) -> Option<u32> {
        self.0.bitfield()
    }

    fn is_mutable(&self) -> bool {
        self.0.is_mutable()
    }

    fn annotations(&self) -> &Annotations {
        self.0.annotations()
    }

    fn offset(&self) -> Option<usize> {
        self.0.offset()
    }
}

/// Convert the given ordered set of raw fields into a list of either plain data
/// members, and/or bitfield units containing multiple bitfields.
fn raw_fields_to_fields_and_bitfield_units<I>(ctx: &BindgenContext,
                                              raw_fields: I)
                                              -> Vec<Field>
    where I: IntoIterator<Item=RawField>
{
    let mut raw_fields = raw_fields.into_iter().fuse().peekable();
    let mut fields = vec![];
    let mut bitfield_unit_count = 0;

    loop {
        // While we have plain old data members, just keep adding them to our
        // resulting fields. We introduce a scope here so that we can use
        // `raw_fields` again after the `by_ref` iterator adaptor is dropped.
        {
            let non_bitfields = raw_fields
                .by_ref()
                .peeking_take_while(|f| f.bitfield().is_none())
                .map(|f| Field::DataMember(f.0));
            fields.extend(non_bitfields);
        }

        // Now gather all the consecutive bitfields. Only consecutive bitfields
        // may potentially share a bitfield allocation unit with each other in
        // the Itanium C++ ABI.
        let mut bitfields = raw_fields
            .by_ref()
            .peeking_take_while(|f| f.bitfield().is_some())
            .peekable();

        if bitfields.peek().is_none() {
            break;
        }

        bitfields_to_allocation_units(ctx,
                                      &mut bitfield_unit_count,
                                      &mut fields,
                                      bitfields);
    }

    assert!(raw_fields.next().is_none(),
            "The above loop should consume all items in `raw_fields`");

    fields
}

/// Given a set of contiguous raw bitfields, group and allocate them into
/// (potentially multiple) bitfield units.
fn bitfields_to_allocation_units<E, I>(ctx: &BindgenContext,
                                       bitfield_unit_count: &mut usize,
                                       mut fields: &mut E,
                                       raw_bitfields: I)
    where E: Extend<Field>,
          I: IntoIterator<Item=RawField>
{
    assert!(ctx.collected_typerefs());

    // NOTE: What follows is reverse-engineered from LLVM's
    // lib/AST/RecordLayoutBuilder.cpp
    //
    // FIXME(emilio): There are some differences between Microsoft and the
    // Itanium ABI, but we'll ignore those and stick to Itanium for now.
    //
    // Also, we need to handle packed bitfields and stuff.
    //
    // TODO(emilio): Take into account C++'s wide bitfields, and
    // packing, sigh.

    fn flush_allocation_unit<E>(mut fields: &mut E,
                                bitfield_unit_count: &mut usize,
                                unit_size_in_bits: usize,
                                unit_align_in_bits: usize,
                                bitfields: Vec<Bitfield>)
        where E: Extend<Field>
    {
        *bitfield_unit_count += 1;
        let align = bytes_from_bits_pow2(unit_align_in_bits);
        let size = align_to(unit_size_in_bits, align * 8) / 8;
        let layout = Layout::new(size, align);
        fields.extend(Some(Field::Bitfields(BitfieldUnit {
            nth: *bitfield_unit_count,
            layout: layout,
            bitfields: bitfields,
        })));
    }

    let mut max_align = 0;
    let mut unfilled_bits_in_unit = 0;
    let mut unit_size_in_bits = 0;
    let mut unit_align = 0;
    let mut bitfields_in_unit = vec![];

    // TODO(emilio): Determine this from attributes or pragma ms_struct
    // directives. Also, perhaps we should check if the target is MSVC?
    const is_ms_struct: bool = false;

    for bitfield in raw_bitfields {
        let bitfield_width = bitfield.bitfield().unwrap() as usize;
        let bitfield_layout =
            ctx.resolve_type(bitfield.ty())
                .layout(ctx)
                .expect("Bitfield without layout? Gah!");
        let bitfield_size = bitfield_layout.size;
        let bitfield_align = bitfield_layout.align;

        let mut offset = unit_size_in_bits;
        if is_ms_struct {
            if unit_size_in_bits != 0 &&
               (bitfield_width == 0 ||
                bitfield_width > unfilled_bits_in_unit) {
                // We've reached the end of this allocation unit, so flush it
                // and its bitfields.
                unit_size_in_bits = align_to(unit_size_in_bits, unit_align * 8);
                flush_allocation_unit(fields,
                                      bitfield_unit_count,
                                      unit_size_in_bits,
                                      unit_align,
                                      mem::replace(&mut bitfields_in_unit, vec![]));

                // Now we're working on a fresh bitfield allocation unit, so reset
                // the current unit size and alignment.
                #[allow(unused_assignments)]
                {
                    unit_size_in_bits = 0;
                    offset = 0;
                    unit_align = 0;
                }
            }
        } else {
            if offset != 0 &&
                (bitfield_width == 0 ||
                 (offset & (bitfield_align * 8 - 1)) + bitfield_width > bitfield_size * 8) {
                offset = align_to(offset, bitfield_align * 8);
            }
        }

        // Only keep named bitfields around. Unnamed bitfields (with > 0
        // bitsize) are used for padding. Because the `Bitfield` struct stores
        // the bit-offset into its allocation unit where its bits begin, we
        // don't need any padding bits hereafter.
        if bitfield.name().is_some() {
            bitfields_in_unit.push(Bitfield::new(offset, bitfield));
        }

        max_align = cmp::max(max_align, bitfield_align);

        // NB: The `bitfield_width` here is completely, absolutely intentional.
        // Alignment of the allocation unit is based on the maximum bitfield
        // width, not (directly) on the bitfields' types' alignment.
        unit_align = cmp::max(unit_align, bitfield_width);

        unit_size_in_bits = offset + bitfield_width;

        // Compute what the physical unit's final size would be given what we
        // have seen so far, and use that to compute how many bits are still
        // available in the unit.
        let data_size = align_to(unit_size_in_bits, bitfield_align * 8);
        unfilled_bits_in_unit = data_size - unit_size_in_bits;
    }

    if unit_size_in_bits != 0 {
        // Flush the last allocation unit and its bitfields.
        flush_allocation_unit(fields,
                              bitfield_unit_count,
                              unit_size_in_bits,
                              unit_align,
                              bitfields_in_unit);
    }
}

/// A compound structure's fields are initially raw, and have bitfields that
/// have not been grouped into allocation units. During this time, the fields
/// are mutable and we build them up during parsing.
///
/// Then, once resolving typerefs is completed, we compute all structs' fields'
/// bitfield allocation units, and they remain frozen and immutable forever
/// after.
#[derive(Debug)]
enum CompFields {
    BeforeComputingBitfieldUnits(Vec<RawField>),
    AfterComputingBitfieldUnits(Vec<Field>),
}

impl Default for CompFields {
    fn default() -> CompFields {
        CompFields::BeforeComputingBitfieldUnits(vec![])
    }
}

impl CompFields {
    fn append_raw_field(&mut self, raw: RawField) {
        match *self {
            CompFields::BeforeComputingBitfieldUnits(ref mut raws) => {
                raws.push(raw);
            }
            CompFields::AfterComputingBitfieldUnits(_) => {
                panic!("Must not append new fields after computing bitfield allocation units");
            }
        }
    }

    fn compute_bitfield_units(&mut self, ctx: &BindgenContext) {
        let raws = match *self {
            CompFields::BeforeComputingBitfieldUnits(ref mut raws) => {
                mem::replace(raws, vec![])
            }
            CompFields::AfterComputingBitfieldUnits(_) => {
                panic!("Already computed bitfield units");
            }
        };

        let fields_and_units = raw_fields_to_fields_and_bitfield_units(ctx, raws);
        mem::replace(self, CompFields::AfterComputingBitfieldUnits(fields_and_units));
    }
}

impl Trace for CompFields {
    type Extra = ();

    fn trace<T>(&self, context: &BindgenContext, tracer: &mut T, _: &())
        where T: Tracer,
    {
        match *self {
            CompFields::BeforeComputingBitfieldUnits(ref fields) => {
                for f in fields {
                    tracer.visit_kind(f.ty(), EdgeKind::Field);
                }
            }
            CompFields::AfterComputingBitfieldUnits(ref fields) => {
                for f in fields {
                    f.trace(context, tracer, &());
                }
            }
        }
    }
}

/// Common data shared across different field types.
#[derive(Clone, Debug)]
pub struct FieldData {
    /// The name of the field, empty if it's an unnamed bitfield width.
    name: Option<String>,

    /// The inner type.
    ty: ItemId,

    /// The doc comment on the field if any.
    comment: Option<String>,

    /// Annotations for this field, or the default.
    annotations: Annotations,

    /// If this field is a bitfield, and how many bits does it contain if it is.
    bitfield: Option<u32>,

    /// If the C++ field is marked as `mutable`
    mutable: bool,

    /// The offset of the field (in bits)
    offset: Option<usize>,
}

impl FieldMethods for FieldData {
    fn name(&self) -> Option<&str> {
        self.name.as_ref().map(|n| &**n)
    }

    fn ty(&self) -> ItemId {
        self.ty
    }

    fn comment(&self) -> Option<&str> {
        self.comment.as_ref().map(|c| &**c)
    }

    fn bitfield(&self) -> Option<u32> {
        self.bitfield
    }

    fn is_mutable(&self) -> bool {
        self.mutable
    }

    fn annotations(&self) -> &Annotations {
        &self.annotations
    }

    fn offset(&self) -> Option<usize> {
        self.offset
    }
}

impl CanDeriveDebug for Field {
    type Extra = ();

    fn can_derive_debug(&self, ctx: &BindgenContext, _: ()) -> bool {
        match *self {
            Field::DataMember(ref data) => data.ty.can_derive_debug(ctx, ()),
            Field::Bitfields(BitfieldUnit { ref bitfields, .. }) => bitfields.iter().all(|b| {
                b.ty().can_derive_debug(ctx, ())
            }),
        }
    }
}

impl CanDeriveDefault for Field {
    type Extra = ();

    fn can_derive_default(&self, ctx: &BindgenContext, _: ()) -> bool {
        match *self {
            Field::DataMember(ref data) => data.ty.can_derive_default(ctx, ()),
            Field::Bitfields(BitfieldUnit { ref bitfields, .. }) => bitfields.iter().all(|b| {
                b.ty().can_derive_default(ctx, ())
            }),
        }
    }
}

impl<'a> CanDeriveCopy<'a> for Field {
    type Extra = ();

    fn can_derive_copy(&self, ctx: &BindgenContext, _: ()) -> bool {
        match *self {
            Field::DataMember(ref data) => data.ty.can_derive_copy(ctx, ()),
            Field::Bitfields(BitfieldUnit { ref bitfields, .. }) => bitfields.iter().all(|b| {
                b.ty().can_derive_copy(ctx, ())
            }),
        }
    }

    fn can_derive_copy_in_array(&self, ctx: &BindgenContext, _: ()) -> bool {
        match *self {
            Field::DataMember(ref data) => data.ty.can_derive_copy_in_array(ctx, ()),
            Field::Bitfields(BitfieldUnit { ref bitfields, .. }) => bitfields.iter().all(|b| {
                b.ty().can_derive_copy_in_array(ctx, ())
            }),
        }
    }
}


/// The kind of inheritance a base class is using.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum BaseKind {
    /// Normal inheritance, like:
    ///
    /// ```cpp
    /// class A : public B {};
    /// ```
    Normal,
    /// Virtual inheritance, like:
    ///
    /// ```cpp
    /// class A: public virtual B {};
    /// ```
    Virtual,
}

/// A base class.
#[derive(Clone, Debug)]
pub struct Base {
    /// The type of this base class.
    pub ty: ItemId,
    /// The kind of inheritance we're doing.
    pub kind: BaseKind,
}

impl Base {
    /// Whether this base class is inheriting virtually.
    pub fn is_virtual(&self) -> bool {
        self.kind == BaseKind::Virtual
    }
}

/// A compound type.
///
/// Either a struct or union, a compound type is built up from the combination
/// of fields which also are associated with their own (potentially compound)
/// type.
#[derive(Debug)]
pub struct CompInfo {
    /// Whether this is a struct or a union.
    kind: CompKind,

    /// The members of this struct or union.
    fields: CompFields,

    /// The abstract template parameters of this class. Note that these are NOT
    /// concrete template arguments, and should always be a
    /// `Type(TypeKind::Named(name))`. For concrete template arguments, see
    /// `TypeKind::TemplateInstantiation`.
    template_params: Vec<ItemId>,

    /// The method declarations inside this class, if in C++ mode.
    methods: Vec<Method>,

    /// The different constructors this struct or class contains.
    constructors: Vec<ItemId>,

    /// The destructor of this type. The bool represents whether this destructor
    /// is virtual.
    destructor: Option<(bool, ItemId)>,

    /// Vector of classes this one inherits from.
    base_members: Vec<Base>,

    /// The inner types that were declared inside this class, in something like:
    ///
    /// class Foo {
    ///     typedef int FooTy;
    ///     struct Bar {
    ///         int baz;
    ///     };
    /// }
    ///
    /// static Foo::Bar const = {3};
    inner_types: Vec<ItemId>,

    /// Set of static constants declared inside this class.
    inner_vars: Vec<ItemId>,

    /// Whether this type should generate an vtable (TODO: Should be able to
    /// look at the virtual methods and ditch this field).
    has_vtable: bool,

    /// Whether this type has destructor.
    has_destructor: bool,

    /// Whether this type has a base type with more than one member.
    ///
    /// TODO: We should be able to compute this.
    has_nonempty_base: bool,

    /// If this type has a template parameter which is not a type (e.g.: a
    /// size_t)
    has_non_type_template_params: bool,

    /// Whether this struct layout is packed.
    packed: bool,

    /// Used to know if we've found an opaque attribute that could cause us to
    /// generate a type with invalid layout. This is explicitly used to avoid us
    /// generating bad alignments when parsing types like max_align_t.
    ///
    /// It's not clear what the behavior should be here, if generating the item
    /// and pray, or behave as an opaque type.
    found_unknown_attr: bool,

    /// Used to detect if we've run in a can_derive_debug cycle while cycling
    /// around the template arguments.
    detect_derive_debug_cycle: Cell<bool>,

    /// Used to detect if we've run in a can_derive_default cycle while cycling
    /// around the template arguments.
    detect_derive_default_cycle: Cell<bool>,

    /// Used to detect if we've run in a has_destructor cycle while cycling
    /// around the template arguments.
    detect_has_destructor_cycle: Cell<bool>,

    /// Used to indicate when a struct has been forward declared. Usually used
    /// in headers so that APIs can't modify them directly.
    is_forward_declaration: bool,
}

impl CompInfo {
    /// Construct a new compound type.
    pub fn new(kind: CompKind) -> Self {
        CompInfo {
            kind: kind,
            fields: CompFields::default(),
            template_params: vec![],
            methods: vec![],
            constructors: vec![],
            destructor: None,
            base_members: vec![],
            inner_types: vec![],
            inner_vars: vec![],
            has_vtable: false,
            has_destructor: false,
            has_nonempty_base: false,
            has_non_type_template_params: false,
            packed: false,
            found_unknown_attr: false,
            detect_derive_debug_cycle: Cell::new(false),
            detect_derive_default_cycle: Cell::new(false),
            detect_has_destructor_cycle: Cell::new(false),
            is_forward_declaration: false,
        }
    }

    /// Is this compound type unsized?
    pub fn is_unsized(&self, ctx: &BindgenContext) -> bool {
        !self.has_vtable(ctx) && self.fields().is_empty() &&
        self.base_members.iter().all(|base| {
            ctx.resolve_type(base.ty).canonical_type(ctx).is_unsized(ctx)
        })
    }

    /// Does this compound type have a destructor?
    pub fn has_destructor(&self, ctx: &BindgenContext) -> bool {
        if self.detect_has_destructor_cycle.get() {
            warn!("Cycle detected looking for destructors");
            // Assume no destructor, since we don't have an explicit one.
            return false;
        }

        self.detect_has_destructor_cycle.set(true);

        let has_destructor = self.has_destructor ||
                             match self.kind {
            CompKind::Union => false,
            CompKind::Struct => {
                self.base_members.iter().any(|base| {
                    ctx.resolve_type(base.ty).has_destructor(ctx)
                }) ||
                self.fields().iter().any(|field| {
                    field.has_destructor(ctx)
                })
            }
        };

        self.detect_has_destructor_cycle.set(false);

        has_destructor
    }

    /// Compute the layout of this type.
    ///
    /// This is called as a fallback under some circumstances where LLVM doesn't
    /// give us the correct layout.
    ///
    /// If we're a union without known layout, we try to compute it from our
    /// members. This is not ideal, but clang fails to report the size for these
    /// kind of unions, see test/headers/template_union.hpp
    pub fn layout(&self, ctx: &BindgenContext) -> Option<Layout> {
        use std::cmp;

        // We can't do better than clang here, sorry.
        if self.kind == CompKind::Struct {
            return None;
        }

        let mut max_size = 0;
        let mut max_align = 0;
        for field in self.fields() {
            let field_layout = field.layout(ctx);

            if let Some(layout) = field_layout {
                max_size = cmp::max(max_size, layout.size);
                max_align = cmp::max(max_align, layout.align);
            }
        }

        Some(Layout::new(max_size, max_align))
    }

    /// Get this type's set of fields.
    pub fn fields(&self) -> &[Field] {
        match self.fields {
            CompFields::AfterComputingBitfieldUnits(ref fields) => fields,
            CompFields::BeforeComputingBitfieldUnits(_) => {
                panic!("Should always have computed bitfield units first");
            }
        }
    }

    /// Does this type have any template parameters that aren't types
    /// (e.g. int)?
    pub fn has_non_type_template_params(&self) -> bool {
        self.has_non_type_template_params
    }

    /// Does this type have a virtual table?
    pub fn has_vtable(&self, ctx: &BindgenContext) -> bool {
        self.has_vtable ||
        self.base_members().iter().any(|base| {
            ctx.resolve_type(base.ty)
                .has_vtable(ctx)
        })
    }

    /// Get this type's set of methods.
    pub fn methods(&self) -> &[Method] {
        &self.methods
    }

    /// Get this type's set of constructors.
    pub fn constructors(&self) -> &[ItemId] {
        &self.constructors
    }

    /// Get this type's destructor.
    pub fn destructor(&self) -> Option<(bool, ItemId)> {
        self.destructor
    }

    /// What kind of compound type is this?
    pub fn kind(&self) -> CompKind {
        self.kind
    }

    /// Is this a union?
    pub fn is_union(&self) -> bool {
        self.kind() == CompKind::Union
    }

    /// The set of types that this one inherits from.
    pub fn base_members(&self) -> &[Base] {
        &self.base_members
    }

    /// Construct a new compound type from a Clang type.
    pub fn from_ty(potential_id: ItemId,
                   ty: &clang::Type,
                   location: Option<clang::Cursor>,
                   ctx: &mut BindgenContext)
                   -> Result<Self, ParseError> {
        use clang_sys::*;
        assert!(ty.template_args().is_none(),
                "We handle template instantiations elsewhere");

        let mut cursor = ty.declaration();
        let mut kind = Self::kind_from_cursor(&cursor);
        if kind.is_err() {
            if let Some(location) = location {
                kind = Self::kind_from_cursor(&location);
                cursor = location;
            }
        }

        let kind = try!(kind);

        debug!("CompInfo::from_ty({:?}, {:?})", kind, cursor);

        let mut ci = CompInfo::new(kind);
        ci.is_forward_declaration =
            location.map_or(true, |cur| match cur.kind() {
                CXCursor_StructDecl |
                CXCursor_UnionDecl |
                CXCursor_ClassDecl => !cur.is_definition(),
                _ => false,
            });

        let mut maybe_anonymous_struct_field = None;
        cursor.visit(|cur| {
            if cur.kind() != CXCursor_FieldDecl {
                if let Some((ty, clang_ty, offset)) =
                    maybe_anonymous_struct_field.take() {
                    if cur.kind() == CXCursor_TypedefDecl &&
                       cur.typedef_type().unwrap().canonical_type() == clang_ty {
                        // Typedefs of anonymous structs appear later in the ast
                        // than the struct itself, that would otherwise be an
                        // anonymous field. Detect that case here, and do
                        // nothing.
                    } else {
                        let field =
                            RawField::new(None, ty, None, None, None, false, offset);
                        ci.fields.append_raw_field(field);
                    }
                }
            }

            match cur.kind() {
                CXCursor_FieldDecl => {
                    if let Some((ty, clang_ty, offset)) =
                        maybe_anonymous_struct_field.take() {
                        let mut used = false;
                        cur.visit(|child| {
                            if child.cur_type() == clang_ty {
                                used = true;
                            }
                            CXChildVisit_Continue
                        });
                        if !used {
                            let field = RawField::new(None,
                                                   ty,
                                                   None,
                                                   None,
                                                   None,
                                                   false,
                                                   offset);
                            ci.fields.append_raw_field(field);
                        }
                    }

                    let bit_width = cur.bit_width();
                    let field_type = Item::from_ty_or_ref(cur.cur_type(),
                                                          cur,
                                                          Some(potential_id),
                                                          ctx);

                    let comment = cur.raw_comment();
                    let annotations = Annotations::new(&cur);
                    let name = cur.spelling();
                    let is_mutable = cursor.is_mutable_field();
                    let offset = cur.offset_of_field().ok();

                    // Name can be empty if there are bitfields, for example,
                    // see tests/headers/struct_with_bitfields.h
                    assert!(!name.is_empty() || bit_width.is_some(),
                            "Empty field name?");

                    let name = if name.is_empty() { None } else { Some(name) };

                    let field = RawField::new(name,
                                           field_type,
                                           comment,
                                           annotations,
                                           bit_width,
                                           is_mutable,
                                           offset);
                    ci.fields.append_raw_field(field);

                    // No we look for things like attributes and stuff.
                    cur.visit(|cur| {
                        if cur.kind() == CXCursor_UnexposedAttr {
                            ci.found_unknown_attr = true;
                        }
                        CXChildVisit_Continue
                    });

                }
                CXCursor_UnexposedAttr => {
                    ci.found_unknown_attr = true;
                }
                CXCursor_EnumDecl |
                CXCursor_TypeAliasDecl |
                CXCursor_TypeAliasTemplateDecl |
                CXCursor_TypedefDecl |
                CXCursor_StructDecl |
                CXCursor_UnionDecl |
                CXCursor_ClassTemplate |
                CXCursor_ClassDecl => {
                    // We can find non-semantic children here, clang uses a
                    // StructDecl to note incomplete structs that haven't been
                    // forward-declared before, see [1].
                    //
                    // Also, clang seems to scope struct definitions inside
                    // unions, and other named struct definitions inside other
                    // structs to the whole translation unit.
                    //
                    // Let's just assume that if the cursor we've found is a
                    // definition, it's a valid inner type.
                    //
                    // [1]: https://github.com/servo/rust-bindgen/issues/482
                    let is_inner_struct = cur.semantic_parent() == cursor ||
                                          cur.is_definition();
                    if !is_inner_struct {
                        return CXChildVisit_Continue;
                    }

                    let inner = Item::parse(cur, Some(potential_id), ctx)
                        .expect("Inner ClassDecl");

                    ci.inner_types.push(inner);

                    // A declaration of an union or a struct without name could
                    // also be an unnamed field, unfortunately.
                    if cur.spelling().is_empty() &&
                       cur.kind() != CXCursor_EnumDecl {
                        let ty = cur.cur_type();
                        let offset = cur.offset_of_field().ok();
                        maybe_anonymous_struct_field =
                            Some((inner, ty, offset));
                    }
                }
                CXCursor_PackedAttr => {
                    ci.packed = true;
                }
                CXCursor_TemplateTypeParameter => {
                    let param = Item::named_type(None, cur, ctx)
                        .expect("Item::named_type should't fail when pointing \
                                 at a TemplateTypeParameter");
                    ci.template_params.push(param);
                }
                CXCursor_CXXBaseSpecifier => {
                    let is_virtual_base = cur.is_virtual_base();
                    ci.has_vtable |= is_virtual_base;

                    let kind = if is_virtual_base {
                        BaseKind::Virtual
                    } else {
                        BaseKind::Normal
                    };

                    let type_id =
                        Item::from_ty_or_ref(cur.cur_type(), cur, None, ctx);
                    ci.base_members.push(Base {
                        ty: type_id,
                        kind: kind,
                    });
                }
                CXCursor_Constructor |
                CXCursor_Destructor |
                CXCursor_CXXMethod => {
                    let is_virtual = cur.method_is_virtual();
                    let is_static = cur.method_is_static();
                    debug_assert!(!(is_static && is_virtual), "How?");

                    ci.has_destructor |= cur.kind() == CXCursor_Destructor;
                    ci.has_vtable |= is_virtual;

                    // This used to not be here, but then I tried generating
                    // stylo bindings with this (without path filters), and
                    // cried a lot with a method in gfx/Point.h
                    // (ToUnknownPoint), that somehow was causing the same type
                    // to be inserted in the map two times.
                    //
                    // I couldn't make a reduced test case, but anyway...
                    // Methods of template functions not only use to be inlined,
                    // but also instantiated, and we wouldn't be able to call
                    // them, so just bail out.
                    if !ci.template_params.is_empty() {
                        return CXChildVisit_Continue;
                    }

                    // NB: This gets us an owned `Function`, not a
                    // `FunctionSig`.
                    let signature =
                        match Item::parse(cur, Some(potential_id), ctx) {
                            Ok(item) if ctx.resolve_item(item)
                                .kind()
                                .is_function() => item,
                            _ => return CXChildVisit_Continue,
                        };

                    match cur.kind() {
                        CXCursor_Constructor => {
                            ci.constructors.push(signature);
                        }
                        CXCursor_Destructor => {
                            ci.destructor = Some((is_virtual, signature));
                        }
                        CXCursor_CXXMethod => {
                            let is_const = cur.method_is_const();
                            let method_kind = if is_static {
                                MethodKind::Static
                            } else if is_virtual {
                                MethodKind::Virtual
                            } else {
                                MethodKind::Normal
                            };

                            let method =
                                Method::new(method_kind, signature, is_const);

                            ci.methods.push(method);
                        }
                        _ => unreachable!("How can we see this here?"),
                    }
                }
                CXCursor_NonTypeTemplateParameter => {
                    ci.has_non_type_template_params = true;
                }
                CXCursor_VarDecl => {
                    let linkage = cur.linkage();
                    if linkage != CXLinkage_External &&
                       linkage != CXLinkage_UniqueExternal {
                        return CXChildVisit_Continue;
                    }

                    let visibility = cur.visibility();
                    if visibility != CXVisibility_Default {
                        return CXChildVisit_Continue;
                    }

                    if let Ok(item) = Item::parse(cur,
                                                  Some(potential_id),
                                                  ctx) {
                        ci.inner_vars.push(item);
                    }
                }
                // Intentionally not handled
                CXCursor_CXXAccessSpecifier |
                CXCursor_CXXFinalAttr |
                CXCursor_FunctionTemplate |
                CXCursor_ConversionFunction => {}
                _ => {
                    warn!("unhandled comp member `{}` (kind {:?}) in `{}` ({})",
                          cur.spelling(),
                          clang::kind_to_str(cur.kind()),
                          cursor.spelling(),
                          cur.location());
                }
            }
            CXChildVisit_Continue
        });

        if let Some((ty, _, offset)) = maybe_anonymous_struct_field {
            let field =
                RawField::new(None, ty, None, None, None, false, offset);
            ci.fields.append_raw_field(field);
        }

        Ok(ci)
    }

    fn kind_from_cursor(cursor: &clang::Cursor)
                        -> Result<CompKind, ParseError> {
        use clang_sys::*;
        Ok(match cursor.kind() {
            CXCursor_UnionDecl => CompKind::Union,
            CXCursor_ClassDecl |
            CXCursor_StructDecl => CompKind::Struct,
            CXCursor_CXXBaseSpecifier |
            CXCursor_ClassTemplatePartialSpecialization |
            CXCursor_ClassTemplate => {
                match cursor.template_kind() {
                    CXCursor_UnionDecl => CompKind::Union,
                    _ => CompKind::Struct,
                }
            }
            _ => {
                warn!("Unknown kind for comp type: {:?}", cursor);
                return Err(ParseError::Continue);
            }
        })
    }

    /// Get the set of types that were declared within this compound type
    /// (e.g. nested class definitions).
    pub fn inner_types(&self) -> &[ItemId] {
        &self.inner_types
    }

    /// Get the set of static variables declared within this compound type.
    pub fn inner_vars(&self) -> &[ItemId] {
        &self.inner_vars
    }

    /// Have we found a field with an opaque type that could potentially mess up
    /// the layout of this compound type?
    pub fn found_unknown_attr(&self) -> bool {
        self.found_unknown_attr
    }

    /// Is this compound type packed?
    pub fn packed(&self) -> bool {
        self.packed
    }

    /// Returns whether this type needs an explicit vtable because it has
    /// virtual methods and none of its base classes has already a vtable.
    pub fn needs_explicit_vtable(&self, ctx: &BindgenContext) -> bool {
        self.has_vtable(ctx) &&
        !self.base_members.iter().any(|base| {
            // NB: Ideally, we could rely in all these types being `comp`, and
            // life would be beautiful.
            //
            // Unfortunately, given the way we implement --match-pat, and also
            // that you can inherit from templated types, we need to handle
            // other cases here too.
            ctx.resolve_type(base.ty)
                .canonical_type(ctx)
                .as_comp()
                .map_or(false, |ci| ci.has_vtable(ctx))
        })
    }

    /// Returns true if compound type has been forward declared
    pub fn is_forward_declaration(&self) -> bool {
        self.is_forward_declaration
    }

    /// Compute this compound structure's bitfield allocation units.
    pub fn compute_bitfield_units(&mut self, ctx: &BindgenContext) {
        self.fields.compute_bitfield_units(ctx);
    }
}

impl DotAttributes for CompInfo {
    fn dot_attributes<W>(&self, ctx: &BindgenContext, out: &mut W) -> io::Result<()>
        where W: io::Write
    {
        writeln!(out, "<tr><td>CompKind</td><td>{:?}</td></tr>", self.kind)?;

        if self.has_vtable {
            writeln!(out, "<tr><td>has_vtable</td><td>true</td></tr>")?;
        }

        if self.has_destructor {
            writeln!(out, "<tr><td>has_destructor</td><td>true</td></tr>")?;
        }

        if self.has_nonempty_base {
            writeln!(out, "<tr><td>has_nonempty_base</td><td>true</td></tr>")?;
        }

        if self.has_non_type_template_params {
            writeln!(out, "<tr><td>has_non_type_template_params</td><td>true</td></tr>")?;
        }

        if self.packed {
            writeln!(out, "<tr><td>packed</td><td>true</td></tr>")?;
        }

        if self.is_forward_declaration {
            writeln!(out, "<tr><td>is_forward_declaration</td><td>true</td></tr>")?;
        }

        if !self.fields().is_empty() {
            writeln!(out, r#"<tr><td>fields</td><td><table border="0">"#)?;
            for field in self.fields() {
                field.dot_attributes(ctx, out)?;
            }
            writeln!(out, "</table></td></tr>")?;
        }

        Ok(())
    }
}

impl TemplateParameters for CompInfo {
    fn self_template_params(&self,
                            _ctx: &BindgenContext)
                            -> Option<Vec<ItemId>> {
        if self.template_params.is_empty() {
            None
        } else {
            Some(self.template_params.clone())
        }
    }
}

impl CanDeriveDebug for CompInfo {
    type Extra = Option<Layout>;

    fn can_derive_debug(&self,
                        ctx: &BindgenContext,
                        layout: Option<Layout>)
                        -> bool {
        if self.has_non_type_template_params() {
            return layout.map_or(false, |l| l.opaque().can_derive_debug(ctx, ()));
        }

        // We can reach here recursively via template parameters of a member,
        // for example.
        if self.detect_derive_debug_cycle.get() {
            warn!("Derive debug cycle detected!");
            return true;
        }

        if self.kind == CompKind::Union {
            if ctx.options().unstable_rust {
                return false;
            }

            return layout.unwrap_or_else(Layout::zero)
                .opaque()
                .can_derive_debug(ctx, ());
        }

        self.detect_derive_debug_cycle.set(true);

        let can_derive_debug = {
            self.base_members
                .iter()
                .all(|base| base.ty.can_derive_debug(ctx, ())) &&
            self.fields()
                .iter()
                .all(|f| f.can_derive_debug(ctx, ()))
        };

        self.detect_derive_debug_cycle.set(false);

        can_derive_debug
    }
}

impl CanDeriveDefault for CompInfo {
    type Extra = Option<Layout>;

    fn can_derive_default(&self,
                          ctx: &BindgenContext,
                          layout: Option<Layout>)
                          -> bool {
        // We can reach here recursively via template parameters of a member,
        // for example.
        if self.detect_derive_default_cycle.get() {
            warn!("Derive default cycle detected!");
            return true;
        }

        if self.kind == CompKind::Union {
            if ctx.options().unstable_rust {
                return false;
            }

            return layout.unwrap_or_else(Layout::zero)
                .opaque()
                .can_derive_default(ctx, ());
        }

        self.detect_derive_default_cycle.set(true);

        let can_derive_default = !self.has_vtable(ctx) &&
                                 !self.needs_explicit_vtable(ctx) &&
                                 self.base_members
            .iter()
            .all(|base| base.ty.can_derive_default(ctx, ())) &&
                                 self.fields()
            .iter()
            .all(|f| f.can_derive_default(ctx, ()));

        self.detect_derive_default_cycle.set(false);

        can_derive_default
    }
}

impl<'a> CanDeriveCopy<'a> for CompInfo {
    type Extra = (&'a Item, Option<Layout>);

    fn can_derive_copy(&self,
                       ctx: &BindgenContext,
                       (item, layout): (&Item, Option<Layout>))
                       -> bool {
        if self.has_non_type_template_params() {
            return layout.map_or(false, |l| l.opaque().can_derive_copy(ctx, ()));
        }

        // NOTE: Take into account that while unions in C and C++ are copied by
        // default, the may have an explicit destructor in C++, so we can't
        // defer this check just for the union case.
        if self.has_destructor(ctx) {
            return false;
        }

        if self.kind == CompKind::Union {
            if !ctx.options().unstable_rust {
                // NOTE: If there's no template parameters we can derive copy
                // unconditionally, since arrays are magical for rustc, and
                // __BindgenUnionField always implements copy.
                return true;
            }

            // https://github.com/rust-lang/rust/issues/36640
            if !self.template_params.is_empty() ||
               item.used_template_params(ctx).is_some() {
                return false;
            }
        }

        self.base_members
            .iter()
            .all(|base| base.ty.can_derive_copy(ctx, ())) &&
        self.fields().iter().all(|field| field.can_derive_copy(ctx, ()))
    }

    fn can_derive_copy_in_array(&self,
                                ctx: &BindgenContext,
                                extra: (&Item, Option<Layout>))
                                -> bool {
        self.can_derive_copy(ctx, extra)
    }
}

impl Trace for CompInfo {
    type Extra = Item;

    fn trace<T>(&self, context: &BindgenContext, tracer: &mut T, item: &Item)
        where T: Tracer,
    {
        let params = item.all_template_params(context).unwrap_or(vec![]);
        for p in params {
            tracer.visit_kind(p, EdgeKind::TemplateParameterDefinition);
        }

        for &ty in self.inner_types() {
            tracer.visit_kind(ty, EdgeKind::InnerType);
        }

        // We unconditionally trace `CompInfo`'s template parameters and inner
        // types for the the usage analysis. However, we don't want to continue
        // tracing anything else, if this type is marked opaque.
        if item.is_opaque(context) {
            return;
        }

        for base in self.base_members() {
            tracer.visit_kind(base.ty, EdgeKind::BaseMember);
        }

        self.fields.trace(context, tracer, &());

        for &var in self.inner_vars() {
            tracer.visit_kind(var, EdgeKind::InnerVar);
        }

        for method in self.methods() {
            tracer.visit_kind(method.signature, EdgeKind::Method);
        }

        for &ctor in self.constructors() {
            tracer.visit_kind(ctor, EdgeKind::Constructor);
        }
    }
}
