/* -*- mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* this source code form is subject to the terms of the mozilla public
 * license, v. 2.0. if a copy of the mpl was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gemmology.h>

namespace gemmology {
template struct Engine<xsimd::sse2>;
template void Engine<xsimd::sse2>::SelectColumnsB(signed char const*,
                                                  signed char*, unsigned long,
                                                  unsigned int const*,
                                                  unsigned int const*);
template void Engine<xsimd::sse2>::Shift::Multiply(
    unsigned char const*, signed char const*, unsigned long, unsigned long,
    unsigned long, gemmology::callbacks::UnquantizeAndAddBiasAndWrite);
template void Engine<xsimd::sse2>::Shift::PrepareBias(
    signed char const*, unsigned long, unsigned long,
    gemmology::callbacks::UnquantizeAndAddBiasAndWrite);
}  // namespace gemmology
