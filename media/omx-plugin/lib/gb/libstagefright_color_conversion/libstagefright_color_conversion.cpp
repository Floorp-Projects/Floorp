/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Types.h"
#include "stagefright/ColorConverter.h"

namespace android {

MOZ_EXPORT ColorConverter::ColorConverter(OMX_COLOR_FORMATTYPE srcFormat,
                                          OMX_COLOR_FORMATTYPE dstFormat)
{
}

MOZ_EXPORT bool ColorConverter::isValid() const { return false; }

MOZ_EXPORT void ColorConverter::convert(unsigned int, unsigned int,
                                        const void*, unsigned int,
                                        void*, unsigned int)
{
}

MOZ_EXPORT ColorConverter::~ColorConverter() { }

} // namespace android
