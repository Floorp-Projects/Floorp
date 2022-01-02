use core::arch::x86_64::__m128i;

use crate::memmem::{
    prefilter::{PrefilterFnTy, PrefilterState},
    NeedleInfo,
};

// Check that the functions below satisfy the Prefilter function type.
const _: PrefilterFnTy = find;

/// An SSE2 accelerated candidate finder for single-substring search.
///
/// # Safety
///
/// Callers must ensure that the sse2 CPU feature is enabled in the current
/// environment. This feature should be enabled in all x86_64 targets.
#[target_feature(enable = "sse2")]
pub(crate) unsafe fn find(
    prestate: &mut PrefilterState,
    ninfo: &NeedleInfo,
    haystack: &[u8],
    needle: &[u8],
) -> Option<usize> {
    // If the haystack is too small for SSE2, then just run memchr on the
    // rarest byte and be done with it. (It is likely that this code path is
    // rarely exercised, since a higher level routine will probably dispatch to
    // Rabin-Karp for such a small haystack.)
    fn simple_memchr_fallback(
        _prestate: &mut PrefilterState,
        ninfo: &NeedleInfo,
        haystack: &[u8],
        needle: &[u8],
    ) -> Option<usize> {
        let (rare, _) = ninfo.rarebytes.as_rare_ordered_usize();
        crate::memchr(needle[rare], haystack).map(|i| i.saturating_sub(rare))
    }
    super::super::genericsimd::find::<__m128i>(
        prestate,
        ninfo,
        haystack,
        needle,
        simple_memchr_fallback,
    )
}

#[cfg(all(test, feature = "std"))]
mod tests {
    #[test]
    #[cfg(not(miri))]
    fn prefilter_permutations() {
        use crate::memmem::prefilter::tests::PrefilterTest;
        // SAFETY: super::find is safe to call for all inputs on x86.
        unsafe { PrefilterTest::run_all_tests(super::find) };
    }
}
