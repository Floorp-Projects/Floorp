/// Trait for fixed size arrays.
pub unsafe trait Array {
    /// The array's element type
    type Item;
    #[doc(hidden)]
    /// The smallest index type that indexes the array.
    type Index: Index;
    #[doc(hidden)]
    fn as_ptr(&self) -> *const Self::Item;
    #[doc(hidden)]
    fn as_mut_ptr(&mut self) -> *mut Self::Item;
    #[doc(hidden)]
    fn capacity() -> usize;
}

pub trait Index : PartialEq + Copy {
    fn to_usize(self) -> usize;
    fn from(usize) -> Self;
}

#[cfg(feature = "use_generic_array")]
unsafe impl<T, U> Array for ::generic_array::GenericArray<T, U>
    where U: ::generic_array::ArrayLength<T>
{
    type Item = T;
    type Index = usize;
    fn as_ptr(&self) -> *const Self::Item {
        (**self).as_ptr()
    }
    fn as_mut_ptr(&mut self) -> *mut Self::Item {
        (**self).as_mut_ptr()
    }
    fn capacity() -> usize {
        U::to_usize()
    }

}

impl Index for u8 {
    #[inline(always)]
    fn to_usize(self) -> usize { self as usize }
    #[inline(always)]
    fn from(ix: usize) ->  Self { ix as u8 }
}

impl Index for u16 {
    #[inline(always)]
    fn to_usize(self) -> usize { self as usize }
    #[inline(always)]
    fn from(ix: usize) ->  Self { ix as u16 }
}

impl Index for usize {
    #[inline(always)]
    fn to_usize(self) -> usize { self }
    #[inline(always)]
    fn from(ix: usize) ->  Self { ix }
}

macro_rules! fix_array_impl {
    ($index_type:ty, $len:expr ) => (
        unsafe impl<T> Array for [T; $len] {
            type Item = T;
            type Index = $index_type;
            #[inline(always)]
            fn as_ptr(&self) -> *const T { self as *const _ as *const _ }
            #[inline(always)]
            fn as_mut_ptr(&mut self) -> *mut T { self as *mut _ as *mut _}
            #[inline(always)]
            fn capacity() -> usize { $len }
        }
    )
}

macro_rules! fix_array_impl_recursive {
    ($index_type:ty, ) => ();
    ($index_type:ty, $len:expr, $($more:expr,)*) => (
        fix_array_impl!($index_type, $len);
        fix_array_impl_recursive!($index_type, $($more,)*);
    );
}

fix_array_impl_recursive!(u8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                          16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                          32, 40, 48, 56, 64, 72, 96, 128, 160, 192, 224,);
fix_array_impl_recursive!(u16, 256, 384, 512, 768, 1024, 2048, 4096, 8192, 16384, 32768,);
