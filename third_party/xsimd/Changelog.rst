.. Copyright (c) Serge Guelton and Johan Mabille
   Copyright (c) QuantStack

   Distributed under the terms of the BSD 3-Clause License.

   The full license is in the file LICENSE, distributed with this software.


Changelog
=========

9.0.1
-----

    * Fix potential ABI issue in SVE support, making ``xsimd::sve`` a type alias to
      size-dependent type.

9.0.0
-----

    * Support fixed size SVE

    * Fix a bug in SSSE3 ``xsimd::swizzle`` implementation for ``int8`` and ``int16``

    * Rename ``xsimd::hadd`` into ``xsimd::reduce_add``, provide ``xsimd::reduce_min`` and ``xsimd::reduce_max``

    * Properly report unsupported double for neon on arm32

    * Fill holes in xsimd scalar api

    * Fix ``find_package(xsimd)`` for xtl enabled xsimd

    * Replace ``xsimd::bool_cast`` by ``xsimd::batch_bool_cast``

    * Native ``xsimd::hadd`` for float on arm64

    * Properly static_assert when trying to instantiate an ``xsimd::batch`` of xtl complex

    * Introduce ``xsimd::batch_bool::mask()`` and ``batch_bool::from_mask(...)``

    * Flag some function with ``[[nodiscard]]``

    * Accept both relative and absolute libdir and include dir in xsimd.pc

    * Implement ``xsimd::nearbyint_as_int`` for NEON

    * Add ``xsimd::polar``

    * Speedup double -> F32/I32 gathers

    * Add ``xsimd::slide_left`` and ``xsimd::slide_right``

    * Support integral ``xsimd::swizzles`` on AVX

8.1.0
-----

    * Add ``xsimd::gather`` and ``xsimd::scatter``

    * Add ``xsimd::nearbyint_as_int``

    * Add ``xsimd::none``

    * Add ``xsimd::reciprocal``

    * Remove batch constructor from memory adress, use ``xsimd::batch<...>::load_(un)aligned`` instead

    * Leave to msvc users the opportunity to manually disable FMA3 on AVX

    * Provide ``xsimd::insert`` to modify a single value from a vector

    * Make ``xsimd::pow`` implementation resilient to ``FE_INVALID``

    * Reciprocal square root support through ``xsimd::rsqrt``

    * NEON: Improve ``xsimd::any`` and ``xsimd::all``

    * Provide type utility to explicitly require a batch of given size and type

    * Implement ``xsimd::swizzle`` on x86, neon and neon64

    * Avx support for ``xsimd::zip_lo`` and ``xsimd::zip_hi``

    * Only use ``_mm256_unpacklo_epi<N>`` on AVX2

    * Provide neon/neon64 conversion function from ``uint(32|64)_t`` to ``(float|double)``

    * Provide SSE/AVX/AVX2 conversion function from ``uint32_t`` to ``float``

    * Provide AVX2 conversion function from ``(u)int64_t`` to ``double``

    * Provide better SSE conversion function from ``uint64_t`` to ``double``

    * Provide better SSE conversion function to ``double``

    * Support logical xor for ``xsimd::batch_bool``

    * Clarify fma support:

        - FMA3 + SSE -> ``xsimd::fma3<sse4_2>``
        - FMA3 + AVX -> ``xsimd::fma3<avx>``
        - FMA3 + AVX2 -> ``xsimd::fma3<avx2>``
        - FMA4 -> ``xsimd::fma4``

    * Allow ``xsimd::transform`` to work with complex types

    * Add missing scalar version of ``xsimd::norm`` and ``xsimd::conj``

8.0.5
-----

    * Fix neon ``xsimd::hadd`` implementation

    * Detect unsupported architectures and set ``XSIMD_NO_SUPPORTED_ARCHITECTURE``
      if needs be

8.0.4
-----

    * Provide some conversion operators for ``float`` -> ``uint32``

    * Improve code generated for AVX2 signed integer comparisons

    * Enable detection of avx512cd and avx512dq, and fix avx512bw detection

    * Enable detection of AVX2+FMA

    * Pick the best compatible architecture in ``xsimd::dispatch``

    * Enables support for FMA when AVX2 is detected on Windows

    * Add missing includes / forward declaration

    * Mark all functions inline and noexcept

    * Assert when using incomplete ``std::initializer_list``

8.0.3
-----

    * Improve CI & testing, no functional change

8.0.2
-----

    * Do not use ``_mm256_srai_epi32`` under AVX, it's an AVX2 instruction

8.0.1
-----

    * Fix invalid constexpr ``std::make_tuple`` usage in neon64
