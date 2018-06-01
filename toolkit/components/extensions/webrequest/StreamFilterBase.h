/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_StreamFilterBase_h
#define mozilla_extensions_StreamFilterBase_h

#include "mozilla/LinkedList.h"
#include "nsTArray.h"

namespace mozilla {
namespace extensions {

class StreamFilterBase
{
public:
  typedef nsTArray<uint8_t> Data;

protected:
  class BufferedData : public LinkedListElement<BufferedData> {
  public:
    explicit BufferedData(Data&& aData) : mData(std::move(aData)) {}

    Data mData;
  };

  LinkedList<BufferedData> mBufferedData;

  inline void
  BufferData(Data&& aData) {
    mBufferedData.insertBack(new BufferedData(std::move(aData)));
  };
};

} // namespace extensions
} // namespace mozilla

#endif // mozilla_extensions_StreamFilterBase_h
