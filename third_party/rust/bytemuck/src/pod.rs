use super::*;

/// Marker trait for "plain old data".
///
/// The point of this trait is that once something is marked "plain old data"
/// you can really go to town with the bit fiddling and bit casting. Therefore,
/// it's a relatively strong claim to make about a type. Do not add this to your
/// type casually.
///
/// **Reminder:** The results of casting around bytes between data types are
/// _endian dependant_. Little-endian machines are the most common, but
/// big-endian machines do exist (and big-endian is also used for "network
/// order" bytes).
///
/// ## Safety
///
/// * The type must be inhabited (eg: no
///   [Infallible](core::convert::Infallible)).
/// * The type must allow any bit pattern (eg: no `bool` or `char`, which have
///   illegal bit patterns).
/// * The type must not contain any padding bytes, either in the middle or on
///   the end (eg: no `#[repr(C)] struct Foo(u8, u16)`, which has padding in the
///   middle, and also no `#[repr(C)] struct Foo(u16, u8)`, which has padding on
///   the end).
/// * The type needs to have all fields also be `Pod`.
/// * The type needs to be `repr(C)` or `repr(transparent)`. In the case of
///   `repr(C)`, the `packed` and `align` repr modifiers can be used as long as
///   all other rules end up being followed.
pub unsafe trait Pod: Zeroable + Copy + 'static {}

unsafe impl Pod for () {}
unsafe impl Pod for u8 {}
unsafe impl Pod for i8 {}
unsafe impl Pod for u16 {}
unsafe impl Pod for i16 {}
unsafe impl Pod for u32 {}
unsafe impl Pod for i32 {}
unsafe impl Pod for u64 {}
unsafe impl Pod for i64 {}
unsafe impl Pod for usize {}
unsafe impl Pod for isize {}
unsafe impl Pod for u128 {}
unsafe impl Pod for i128 {}
unsafe impl Pod for f32 {}
unsafe impl Pod for f64 {}
unsafe impl<T: Pod> Pod for Wrapping<T> {}

unsafe impl Pod for Option<NonZeroI8> {}
unsafe impl Pod for Option<NonZeroI16> {}
unsafe impl Pod for Option<NonZeroI32> {}
unsafe impl Pod for Option<NonZeroI64> {}
unsafe impl Pod for Option<NonZeroI128> {}
unsafe impl Pod for Option<NonZeroIsize> {}
unsafe impl Pod for Option<NonZeroU8> {}
unsafe impl Pod for Option<NonZeroU16> {}
unsafe impl Pod for Option<NonZeroU32> {}
unsafe impl Pod for Option<NonZeroU64> {}
unsafe impl Pod for Option<NonZeroU128> {}
unsafe impl Pod for Option<NonZeroUsize> {}

unsafe impl<T: 'static> Pod for *mut T {}
unsafe impl<T: 'static> Pod for *const T {}
unsafe impl<T: 'static> Pod for Option<NonNull<T>> {}
unsafe impl<T: Pod> Pod for PhantomData<T> {}
unsafe impl<T: Pod> Pod for ManuallyDrop<T> {}

// Note(Lokathor): MaybeUninit can NEVER be Pod.

impl_unsafe_marker_for_array!(
  Pod, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
  20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 48, 64, 96, 128, 256,
  512, 1024, 2048, 4096
);

#[cfg(target_arch = "x86")]
unsafe impl Pod for x86::__m128i {}
#[cfg(target_arch = "x86")]
unsafe impl Pod for x86::__m128 {}
#[cfg(target_arch = "x86")]
unsafe impl Pod for x86::__m128d {}
#[cfg(target_arch = "x86")]
unsafe impl Pod for x86::__m256i {}
#[cfg(target_arch = "x86")]
unsafe impl Pod for x86::__m256 {}
#[cfg(target_arch = "x86")]
unsafe impl Pod for x86::__m256d {}

#[cfg(target_arch = "x86_64")]
unsafe impl Pod for x86_64::__m128i {}
#[cfg(target_arch = "x86_64")]
unsafe impl Pod for x86_64::__m128 {}
#[cfg(target_arch = "x86_64")]
unsafe impl Pod for x86_64::__m128d {}
#[cfg(target_arch = "x86_64")]
unsafe impl Pod for x86_64::__m256i {}
#[cfg(target_arch = "x86_64")]
unsafe impl Pod for x86_64::__m256 {}
#[cfg(target_arch = "x86_64")]
unsafe impl Pod for x86_64::__m256d {}
