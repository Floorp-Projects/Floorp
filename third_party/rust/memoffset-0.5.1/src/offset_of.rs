// Copyright (c) 2017 Gilad Naaman
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/// Macro to create a local `base_ptr` raw pointer of the given type, avoiding UB as
/// much as is possible currently.
#[cfg(memoffset_maybe_uninit)]
#[macro_export]
#[doc(hidden)]
macro_rules! _memoffset__let_base_ptr {
    ($name:ident, $type:tt) => {
        // No UB here, and the pointer does not dangle, either.
        // But we have to make sure that `uninit` lives long enough,
        // so it has to be in the same scope as `$name`. That's why
        // `let_base_ptr` declares a variable (several, actually)
        // instad of returning one.
        let uninit = $crate::mem::MaybeUninit::<$type>::uninit();
        let $name = uninit.as_ptr();
    };
}
#[cfg(not(memoffset_maybe_uninit))]
#[macro_export]
#[doc(hidden)]
macro_rules! _memoffset__let_base_ptr {
    ($name:ident, $type:tt) => {
        // No UB right here, but we will later offset into a field
        // of this pointer, and that is UB when the pointer is dangling.
        let non_null = $crate::ptr::NonNull::<$type>::dangling();
        let $name = non_null.as_ptr() as *const $type;
    };
}

/// Deref-coercion protection macro.
#[macro_export]
#[doc(hidden)]
macro_rules! _memoffset__field_check {
    ($type:tt, $field:tt) => {
        // Make sure the field actually exists. This line ensures that a
        // compile-time error is generated if $field is accessed through a
        // Deref impl.
        let $type { $field: _, .. };
    };
}

/// Calculates the offset of the specified field from the start of the struct.
///
/// ## Examples
/// ```
/// #[macro_use]
/// extern crate memoffset;
///
/// #[repr(C, packed)]
/// struct Foo {
///     a: u32,
///     b: u64,
///     c: [u8; 5]
/// }
///
/// fn main() {
///     assert_eq!(offset_of!(Foo, a), 0);
///     assert_eq!(offset_of!(Foo, b), 4);
/// }
/// ```
#[macro_export(local_inner_macros)]
macro_rules! offset_of {
    ($parent:tt, $field:tt) => {{
        _memoffset__field_check!($parent, $field);

        // Get a base pointer.
        _memoffset__let_base_ptr!(base_ptr, $parent);
        // Get the field address. This is UB because we are creating a reference to
        // the uninitialized field.
        #[allow(unused_unsafe)] // for when the macro is used in an unsafe block
        let field_ptr = unsafe { &(*base_ptr).$field as *const _ };
        let offset = (field_ptr as usize) - (base_ptr as usize);
        offset
    }};
}

#[cfg(test)]
mod tests {
    #[test]
    fn offset_simple() {
        #[repr(C)]
        struct Foo {
            a: u32,
            b: [u8; 2],
            c: i64,
        }

        assert_eq!(offset_of!(Foo, a), 0);
        assert_eq!(offset_of!(Foo, b), 4);
        assert_eq!(offset_of!(Foo, c), 8);
    }

    #[test]
    #[cfg(not(miri))] // this creates unaligned references
    fn offset_simple_packed() {
        #[repr(C, packed)]
        struct Foo {
            a: u32,
            b: [u8; 2],
            c: i64,
        }

        assert_eq!(offset_of!(Foo, a), 0);
        assert_eq!(offset_of!(Foo, b), 4);
        assert_eq!(offset_of!(Foo, c), 6);
    }

    #[test]
    fn tuple_struct() {
        #[repr(C)]
        struct Tup(i32, i32);

        assert_eq!(offset_of!(Tup, 0), 0);
        assert_eq!(offset_of!(Tup, 1), 4);
    }
}
