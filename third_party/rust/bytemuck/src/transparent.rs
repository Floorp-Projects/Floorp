use super::*;

/// A trait which indicates that a type is a `repr(transparent)` wrapper around
/// the `Wrapped` value.
///
/// This allows safely creating references to `T` from those to the `Wrapped`
/// type, using the `wrap_ref` and `wrap_mut` functions.
///
/// # Safety
///
/// The safety contract of `TransparentWrapper` is relatively simple:
///
/// For a given `Wrapper` which implements `TransparentWrapper<Wrapped>`:
///
/// 1. Wrapper must be a `#[repr(transparent)]` wrapper around `Wrapped`. This
///    either means that it must be a `#[repr(transparent)]` struct which
///    contains a either a field of type `Wrapped` (or a field of some other
///    transparent wrapper for `Wrapped`) as the only non-ZST field.
///
/// 2. Any fields *other* than the `Wrapped` field must be trivially
///    constructable ZSTs, for example `PhantomData`, `PhantomPinned`, etc.
///
/// 3. The `Wrapper` may not impose additional alignment requirements over
///    `Wrapped`.
///     - Note: this is currently guaranteed by repr(transparent), but there
///       have been discussions of lifting it, so it's stated here explictly.
///
/// 4. The `wrap_ref` and `wrap_mut` functions on `TransparentWrapper` may not
///    be overridden.
///
/// ## Caveats
///
/// If the wrapper imposes additional constraints upon the wrapped type which
/// are required for safety, it's responsible for ensuring those still hold --
/// this generally requires preventing access to instances of the wrapped type,
/// as implementing `TransparentWrapper<U> for T` means anybody can call
/// `T::cast_ref(any_instance_of_u)`.
///
/// For example, it would be invalid to implement TransparentWrapper for `str`
/// to implement `TransparentWrapper` around `[u8]` because of this.
///
/// # Examples
///
/// ## Basic
///
/// ```
/// use bytemuck::TransparentWrapper;
/// # #[derive(Default)]
/// # struct SomeStruct(u32);
///
/// #[repr(transparent)]
/// struct MyWrapper(SomeStruct);
///
/// unsafe impl TransparentWrapper<SomeStruct> for MyWrapper {}
///
/// // interpret a reference to &SomeStruct as a &MyWrapper
/// let thing = SomeStruct::default();
/// let wrapped_ref: &MyWrapper = MyWrapper::wrap_ref(&thing);
///
/// // Works with &mut too.
/// let mut mut_thing = SomeStruct::default();
/// let wrapped_mut: &mut MyWrapper = MyWrapper::wrap_mut(&mut mut_thing);
///
/// # let _ = (wrapped_ref, wrapped_mut); // silence warnings
/// ```
///
/// ## Use with dynamically sized types
///
/// ```
/// use bytemuck::TransparentWrapper;
///
/// #[repr(transparent)]
/// struct Slice<T>([T]);
///
/// unsafe impl<T> TransparentWrapper<[T]> for Slice<T> {}
///
/// let s = Slice::wrap_ref(&[1u32, 2, 3]);
/// assert_eq!(&s.0, &[1, 2, 3]);
///
/// let mut buf = [1, 2, 3u8];
/// let sm = Slice::wrap_mut(&mut buf);
/// ```
pub unsafe trait TransparentWrapper<Wrapped: ?Sized> {
  /// Convert a reference to a wrapped type into a reference to the wrapper.
  ///
  /// This is a trait method so that you can write `MyType::wrap_ref(...)` in
  /// your code. It is part of the safety contract for this trait that if you
  /// implement `TransparentWrapper<_>` for your type you **must not** override
  /// this method.
  #[inline]
  fn wrap_ref(s: &Wrapped) -> &Self {
    unsafe {
      assert!(size_of::<*const Wrapped>() == size_of::<*const Self>());
      // Using a pointer cast doesn't work here because rustc can't tell that the
      // vtables match (if we lifted the ?Sized restriction, this would go away),
      // and transmute doesn't work for the usual reasons it doesn't work inside
      // generic functions.
      //
      // SAFETY: The unsafe contract requires that these have identical
      // representations. Using this transmute_copy instead of transmute here is
      // annoying, but is required as `Self` and `Wrapped` have unspecified
      // sizes still.
      let wrapped_ptr = s as *const Wrapped;
      let wrapper_ptr: *const Self = transmute_copy(&wrapped_ptr);
      &*wrapper_ptr
    }
  }

  /// Convert a mut reference to a wrapped type into a mut reference to the
  /// wrapper.
  ///
  /// This is a trait method so that you can write `MyType::wrap_mut(...)` in
  /// your code. It is part of the safety contract for this trait that if you implement
  /// `TransparentWrapper<_>` for your type you **must not** override this method.
  #[inline]
  fn wrap_mut(s: &mut Wrapped) -> &mut Self {
    unsafe {
      assert!(size_of::<*mut Wrapped>() == size_of::<*mut Self>());
      // Using a pointer cast doesn't work here because rustc can't tell that the
      // vtables match (if we lifted the ?Sized restriction, this would go away),
      // and transmute doesn't work for the usual reasons it doesn't work inside
      // generic functions.
      //
      // SAFETY: The unsafe contract requires that these have identical
      // representations. Using this transmute_copy instead of transmute here is
      // annoying, but is required as `Self` and `Wrapped` have unspecified
      // sizes still.
      let wrapped_ptr = s as *mut Wrapped;
      let wrapper_ptr: *mut Self = transmute_copy(&wrapped_ptr);
      &mut *wrapper_ptr
    }
  }
}
