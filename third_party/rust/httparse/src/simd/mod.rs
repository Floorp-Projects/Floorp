#[cfg(not(all(
    httparse_simd,
    any(
        target_arch = "x86",
        target_arch = "x86_64",
    ),
)))]
mod fallback;

#[cfg(not(all(
    httparse_simd,
    any(
        target_arch = "x86",
        target_arch = "x86_64",
    ),
)))]
pub use self::fallback::*;

#[cfg(all(
    httparse_simd,
    any(
        target_arch = "x86",
        target_arch = "x86_64",
    ),
))]
mod sse42;

#[cfg(all(
    httparse_simd,
    any(
        httparse_simd_target_feature_avx2,
        not(httparse_simd_target_feature_sse42),
    ),
    any(
        target_arch = "x86",
        target_arch = "x86_64",
    ),
))]
mod avx2;

#[cfg(all(
    httparse_simd,
    not(any(
        httparse_simd_target_feature_sse42,
        httparse_simd_target_feature_avx2,
    )),
    any(
        target_arch = "x86",
        target_arch = "x86_64",
    ),
))]
mod runtime {
    //! Runtime detection of simd features. Used when the build script
    //! doesn't notice any target features at build time.
    //!
    //! While `is_x86_feature_detected!` has it's own caching built-in,
    //! at least in 1.27.0, the functions don't inline, leaving using it
    //! actually *slower* than just using the scalar fallback.

    use core::sync::atomic::{AtomicUsize, ATOMIC_USIZE_INIT, Ordering};

    static FEATURE: AtomicUsize = ATOMIC_USIZE_INIT;

    const INIT: usize = 0;
    const SSE_42: usize = 1;
    const AVX_2: usize = 2;
    const AVX_2_AND_SSE_42: usize = 3;
    const NONE: usize = ::core::usize::MAX;

    fn detect() -> usize {
        let feat = FEATURE.load(Ordering::Relaxed);
        if feat == INIT {
            if cfg!(target_arch = "x86_64") && is_x86_feature_detected!("avx2") {
                if is_x86_feature_detected!("sse4.2") {
                    FEATURE.store(AVX_2_AND_SSE_42, Ordering::Relaxed);
                    return AVX_2_AND_SSE_42;
                } else {
                    FEATURE.store(AVX_2, Ordering::Relaxed);
                    return AVX_2;
                }
            } else if is_x86_feature_detected!("sse4.2") {
                FEATURE.store(SSE_42, Ordering::Relaxed);
                return SSE_42;
            } else {
                FEATURE.store(NONE, Ordering::Relaxed);
            }
        }
        feat
    }

    pub fn match_uri_vectored(bytes: &mut ::Bytes) {
        unsafe {
            match detect() {
                SSE_42 => super::sse42::parse_uri_batch_16(bytes),
                AVX_2 => { super::avx2::parse_uri_batch_32(bytes); },
                AVX_2_AND_SSE_42 => {
                    if let super::avx2::Scan::Found = super::avx2::parse_uri_batch_32(bytes) {
                        return;
                    }
                    super::sse42::parse_uri_batch_16(bytes)
                },
                _ => ()
            }
        }

        // else do nothing
    }

    pub fn match_header_value_vectored(bytes: &mut ::Bytes) {
        unsafe {
            match detect() {
                SSE_42 => super::sse42::match_header_value_batch_16(bytes),
                AVX_2 => { super::avx2::match_header_value_batch_32(bytes); },
                AVX_2_AND_SSE_42 => {
                    if let super::avx2::Scan::Found = super::avx2::match_header_value_batch_32(bytes) {
                        return;
                    }
                    super::sse42::match_header_value_batch_16(bytes)
                },
                _ => ()
            }
        }

        // else do nothing
    }
}

#[cfg(all(
    httparse_simd,
    not(any(
        httparse_simd_target_feature_sse42,
        httparse_simd_target_feature_avx2,
    )),
    any(
        target_arch = "x86",
        target_arch = "x86_64",
    ),
))]
pub use self::runtime::*;

#[cfg(all(
    httparse_simd,
    httparse_simd_target_feature_sse42,
    not(httparse_simd_target_feature_avx2),
    any(
        target_arch = "x86",
        target_arch = "x86_64",
    ),
))]
mod sse42_compile_time {
    pub fn match_uri_vectored(bytes: &mut ::Bytes) {
        if is_x86_feature_detected!("sse4.2") {
            unsafe {
                super::sse42::parse_uri_batch_16(bytes);
            }
        }

        // else do nothing
    }

    pub fn match_header_value_vectored(bytes: &mut ::Bytes) {
        if is_x86_feature_detected!("sse4.2") {
            unsafe {
                super::sse42::match_header_value_batch_16(bytes);
            }
        }

        // else do nothing
    }
}

#[cfg(all(
    httparse_simd,
    httparse_simd_target_feature_sse42,
    not(httparse_simd_target_feature_avx2),
    any(
        target_arch = "x86",
        target_arch = "x86_64",
    ),
))]
pub use self::sse42_compile_time::*;

#[cfg(all(
    httparse_simd,
    httparse_simd_target_feature_avx2,
    any(
        target_arch = "x86",
        target_arch = "x86_64",
    ),
))]
mod avx2_compile_time {
    pub fn match_uri_vectored(bytes: &mut ::Bytes) {
        // do both, since avx2 only works when bytes.len() >= 32
        if cfg!(target_arch = "x86_64") && is_x86_feature_detected!("avx2") {
            unsafe {
                super::avx2::parse_uri_batch_32(bytes);
            }

        } 
        if is_x86_feature_detected!("sse4.2") {
            unsafe {
                super::sse42::parse_uri_batch_16(bytes);
            }
        }

        // else do nothing
    }

    pub fn match_header_value_vectored(bytes: &mut ::Bytes) {
        // do both, since avx2 only works when bytes.len() >= 32
        if cfg!(target_arch = "x86_64") && is_x86_feature_detected!("avx2") {
            let scanned = unsafe {
                super::avx2::match_header_value_batch_32(bytes)
            };

            if let super::avx2::Scan::Found = scanned {
                return;
            }
        }
        if is_x86_feature_detected!("sse4.2") {
            unsafe {
                super::sse42::match_header_value_batch_16(bytes);
            }
        }

        // else do nothing
    }
}

#[cfg(all(
    httparse_simd,
    httparse_simd_target_feature_avx2,
    any(
        target_arch = "x86",
        target_arch = "x86_64",
    ),
))]
pub use self::avx2_compile_time::*;
