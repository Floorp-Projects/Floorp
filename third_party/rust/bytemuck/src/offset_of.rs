#![forbid(unsafe_code)]

/// Find the offset in bytes of the given `$field` of `$Type`, using `$instance`
/// as an already-initialized value to work with.
///
/// This is similar to the macro from `memoffset`, however it's fully well
/// defined even in current versions of Rust (and uses no unsafe code).
///
/// It does by using the `$instance` argument to have an already-initialized
/// instance of `$Type` rather than trying to find a way access the fields of an
/// uninitialized one without hitting soundness problems. The value passed to
/// the macro is referenced but not moved.
///
/// This means the API is more limited, but it's also sound even in rather
/// extreme cases, like some of the examples.
///
/// ## Caveats
///
/// 1. The offset is in bytes, and so you will likely have to cast your base
///    pointers to `*const u8`/`*mut u8` before getting field addresses.
///
/// 2. The offset values of repr(Rust) types are not stable, and may change
///    wildly between releases of the compiler. Use repr(C) if you can.
///
/// 3. The value of the `$instance` parameter has no bearing on the output of
///    this macro. It is just used to avoid soundness problems. The only
///    requirement is that it be initialized. In particular, the value returned
///    is not a field pointer, or anything like that.
///
/// ## Examples
///
/// ### Use with zeroable types
/// A common requirement in GPU apis is to specify the layout of vertices. These
/// will generally be [`Zeroable`] (if not [`Pod`]), and are a good fit for
/// `offset_of!`.
/// ```
/// # use bytemuck::{Zeroable, offset_of};
/// #[repr(C)]
/// struct Vertex {
///   pos: [f32; 2],
///   uv: [u16; 2],
///   color: [u8; 4],
/// }
/// unsafe impl Zeroable for Vertex {}
///
/// let pos = offset_of!(Zeroable::zeroed(), Vertex, pos);
/// let uv = offset_of!(Zeroable::zeroed(), Vertex, uv);
/// let color = offset_of!(Zeroable::zeroed(), Vertex, color);
///
/// assert_eq!(pos, 0);
/// assert_eq!(uv, 8);
/// assert_eq!(color, 12);
/// ```
///
/// ### Use with other types
///
/// More esoteric uses are possible too, including with types generally not safe
/// to otherwise use with bytemuck. `Strings`, `Vec`s, etc.
///
/// ```
/// #[derive(Default)]
/// struct Foo {
///   a: u8,
///   b: &'static str,
///   c: i32,
/// }
///
/// let a_offset = bytemuck::offset_of!(Default::default(), Foo, a);
/// let b_offset = bytemuck::offset_of!(Default::default(), Foo, b);
/// let c_offset = bytemuck::offset_of!(Default::default(), Foo, c);
///
/// assert_ne!(a_offset, b_offset);
/// assert_ne!(b_offset, c_offset);
/// // We can't check against hardcoded values for a repr(Rust) type,
/// // but prove to ourself this way.
///
/// let foo = Foo::default();
/// // Note: offsets are in bytes.
/// let as_bytes = &foo as *const _ as *const u8;
///
/// // we're using wrapping_offset here becasue it's not worth
/// // the unsafe block, but it would be valid to use `add` instead,
/// // as it cannot overflow.
/// assert_eq!(&foo.a as *const _ as usize, as_bytes.wrapping_add(a_offset) as usize);
/// assert_eq!(&foo.b as *const _ as usize, as_bytes.wrapping_add(b_offset) as usize);
/// assert_eq!(&foo.c as *const _ as usize, as_bytes.wrapping_add(c_offset) as usize);
/// ```
#[macro_export]
macro_rules! offset_of {
  ($instance:expr, $Type:path, $field:tt) => {{
    // This helps us guard against field access going through a Deref impl.
    #[allow(clippy::unneeded_field_pattern)]
    let $Type { $field: _, .. };
    let reference: &$Type = &$instance;
    let address = reference as *const _ as usize;
    let field_pointer = &reference.$field as *const _ as usize;
    // These asserts/unwraps are compiled away at release, and defend against
    // the case where somehow a deref impl is still invoked.
    let result = field_pointer.checked_sub(address).unwrap();
    assert!(result <= $crate::__core::mem::size_of::<$Type>());
    result
  }};
}
