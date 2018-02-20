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

/// Calculates the offset of the specified field from the start of the struct.
/// This macro supports arbitrary amount of subscripts and recursive member-accesses.
///
/// *Note*: This macro may not make much sense when used on structs that are not `#[repr(C, packed)]`
///
/// ## Examples - Simple
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
///     assert_eq!(offset_of!(Foo, c[2]), 14);
/// }
/// ```
///
/// ## Examples - Advanced
/// ```
/// #[macro_use]
/// extern crate memoffset;
///
/// #[repr(C, packed)]
/// struct UnnecessarilyComplicatedStruct {
///     member: [UnnecessarilyComplexStruct; 12]
/// }
///
/// #[repr(C, packed)]
/// struct UnnecessarilyComplexStruct {
///     a: u32,
///     b: u64,
///     c: [u8; 5]
/// }
///
///
/// fn main() {
///     assert_eq!(offset_of!(UnnecessarilyComplicatedStruct, member[3].c[3]), 66);
/// }
/// ```
#[macro_export]
macro_rules! offset_of {
    ($father:ty, $($field:tt)+) => ({
        #[allow(unused_unsafe)]
        let root: $father = unsafe { $crate::mem::uninitialized() };

        let base = &root as *const _ as usize;

        // Future error: borrow of packed field requires unsafe function or block (error E0133)
        #[allow(unused_unsafe)]
        let member =  unsafe { &root.$($field)* as *const _ as usize };

        $crate::mem::forget(root);

        member - base
    });
}

#[cfg(test)]
mod tests {
    #[repr(C, packed)]
    struct Foo {
        a: u32,
        b: [u8; 4],
        c: i64,
    }

    #[test]
    fn offset_simple() {
        assert_eq!(offset_of!(Foo, a), 0);
        assert_eq!(offset_of!(Foo, b), 4);
        assert_eq!(offset_of!(Foo, c), 8);
    }

    #[test]
    fn offset_index() {
        assert_eq!(offset_of!(Foo, b[2]), 6);
    }

    #[test]
    #[should_panic]
    fn offset_index_out_of_bounds() {
        offset_of!(Foo, b[4]);
    }

    #[test]
    fn tuple_struct() {
        #[repr(C, packed)]
        struct Tup(i32, i32);

        assert_eq!(offset_of!(Tup, 0), 0);
    }
}
