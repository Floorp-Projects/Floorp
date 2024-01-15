/* -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gemmology.h>

namespace gemmology {
template struct Engine<xsimd::avx512bw>;
template void Engine<xsimd::avx512bw>::SelectColumnsB(int8_t const*, int8_t*,
                                                  size_t, uint32_t const*,
                                                  uint32_t const*);
template void Engine<xsimd::avx512bw>::Shift::Multiply(
    uint8_t const*, int8_t const*, size_t, size_t, size_t,
    gemmology::callbacks::UnquantizeAndAddBiasAndWrite);
template void Engine<xsimd::avx512bw>::Shift::PrepareBias(
    int8_t const*, size_t, size_t,
    gemmology::callbacks::UnquantizeAndAddBiasAndWrite);
}  // namespace gemmology
