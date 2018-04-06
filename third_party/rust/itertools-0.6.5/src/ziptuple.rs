use super::size_hint;

/// See [`multizip`](../fn.multizip.html) for more information.
#[derive(Clone)]
pub struct Zip<T> {
    t: T,
}

impl<T> Zip<T> {
    /// Deprecated: renamed to multizip
    #[deprecated(note = "Renamed to multizip")]
    pub fn new<U>(t: U) -> Zip<T>
        where Zip<T>: From<U>,
              Zip<T>: Iterator,
    {
        multizip(t)
    }
}

/// An iterator that generalizes *.zip()* and allows running multiple iterators in lockstep.
///
/// The iterator `Zip<(I, J, ..., M)>` is formed from a tuple of iterators (or values that
/// implement `IntoIterator`) and yields elements
/// until any of the subiterators yields `None`.
///
/// The iterator element type is a tuple like like `(A, B, ..., E)` where `A` to `E` are the
/// element types of the subiterator.
///
/// ```
/// use itertools::multizip;
///
/// // Iterate over three sequences side-by-side
/// let mut xs = [0, 0, 0];
/// let ys = [69, 107, 101];
///
/// for (i, a, b) in multizip((0..100, &mut xs, &ys)) {
///    *a = i ^ *b;
/// }
///
/// assert_eq!(xs, [69, 106, 103]);
/// ```
pub fn multizip<T, U>(t: U) -> Zip<T>
    where Zip<T>: From<U>,
          Zip<T>: Iterator,
{
    Zip::from(t)
}

macro_rules! impl_zip_iter {
    ($($B:ident),*) => (
        #[allow(non_snake_case)]
        impl<$($B: IntoIterator),*> From<($($B,)*)> for Zip<($($B::IntoIter,)*)> {
            fn from(t: ($($B,)*)) -> Self {
                let ($($B,)*) = t;
                Zip { t: ($($B.into_iter(),)*) }
            }
        }

        #[allow(non_snake_case)]
        #[allow(unused_assignments)]
        impl<$($B),*> Iterator for Zip<($($B,)*)>
            where
            $(
                $B: Iterator,
            )*
        {
            type Item = ($($B::Item,)*);

            fn next(&mut self) -> Option<Self::Item>
            {
                let ($(ref mut $B,)*) = self.t;

                // NOTE: Just like iter::Zip, we check the iterators
                // for None in order. We may finish unevenly (some
                // iterators gave n + 1 elements, some only n).
                $(
                    let $B = match $B.next() {
                        None => return None,
                        Some(elt) => elt
                    };
                )*
                Some(($($B,)*))
            }

            fn size_hint(&self) -> (usize, Option<usize>)
            {
                let sh = (::std::usize::MAX, None);
                let ($(ref $B,)*) = self.t;
                $(
                    let sh = size_hint::min($B.size_hint(), sh);
                )*
                sh
            }
        }

        #[allow(non_snake_case)]
        impl<$($B),*> ExactSizeIterator for Zip<($($B,)*)> where
            $(
                $B: ExactSizeIterator,
            )*
        { }
    );
}

impl_zip_iter!(A);
impl_zip_iter!(A, B);
impl_zip_iter!(A, B, C);
impl_zip_iter!(A, B, C, D);
impl_zip_iter!(A, B, C, D, E);
impl_zip_iter!(A, B, C, D, E, F);
impl_zip_iter!(A, B, C, D, E, F, G);
impl_zip_iter!(A, B, C, D, E, F, G, H);
