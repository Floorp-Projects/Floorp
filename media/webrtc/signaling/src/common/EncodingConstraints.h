/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ENCODING_CONSTRAINTS_H_
#define _ENCODING_CONSTRAINTS_H_

#include <algorithm>

namespace mozilla
{
class EncodingConstraints
{
public:
  EncodingConstraints() :
    maxWidth(0),
    maxHeight(0),
    maxFps(0),
    maxFs(0),
    maxBr(0),
    maxPps(0),
    maxMbps(0),
    maxCpb(0),
    maxDpb(0),
    scaleDownBy(1.0)
  {}

  bool operator==(const EncodingConstraints& constraints) const
  {
    return
      maxWidth == constraints.maxWidth &&
      maxHeight == constraints.maxHeight &&
      maxFps == constraints.maxFps &&
      maxFs == constraints.maxFs &&
      maxBr == constraints.maxBr &&
      maxPps == constraints.maxPps &&
      maxMbps == constraints.maxMbps &&
      maxCpb == constraints.maxCpb &&
      maxDpb == constraints.maxDpb &&
      scaleDownBy == constraints.scaleDownBy;
  }

  uint32_t maxWidth;
  uint32_t maxHeight;
  uint32_t maxFps;
  uint32_t maxFs;
  uint32_t maxBr;
  uint32_t maxPps;
  uint32_t maxMbps; // macroblocks per second
  uint32_t maxCpb; // coded picture buffer size
  uint32_t maxDpb; // decoded picture buffer size
  double scaleDownBy; // To preserve resolution
};
} // namespace mozilla

#endif // _ENCODING_CONSTRAINTS_H_
