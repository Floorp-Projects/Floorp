/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DECODER_DATA_H_
#define DECODER_DATA_H_

#include "MediaData.h"
#include "MediaInfo.h"
#include "mozilla/Types.h"
#include "mozilla/Vector.h"
#include "nsRefPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsString.h"

namespace stagefright
{
class MetaData;
}

namespace mp4_demuxer
{

class MP4Demuxer;

struct PsshInfo
{
  PsshInfo() {}
  PsshInfo(const PsshInfo& aOther) : uuid(aOther.uuid), data(aOther.data) {}
  nsTArray<uint8_t> uuid;
  nsTArray<uint8_t> data;
};

class CryptoFile
{
public:
  CryptoFile() : valid(false) {}
  CryptoFile(const CryptoFile& aCryptoFile) : valid(aCryptoFile.valid)
  {
    pssh.AppendElements(aCryptoFile.pssh);
  }

  void Update(const uint8_t* aData, size_t aLength)
  {
    valid = DoUpdate(aData, aLength);
  }

  bool valid;
  nsTArray<PsshInfo> pssh;

private:
  bool DoUpdate(const uint8_t* aData, size_t aLength);
};

class MP4AudioInfo : public mozilla::AudioInfo
{
public:
  MP4AudioInfo() = default;

  void Update(const stagefright::MetaData* aMetaData,
              const char* aMimeType);

  virtual bool IsValid() const override;
};

class MP4VideoInfo : public mozilla::VideoInfo
{
public:
  MP4VideoInfo() = default;

  void Update(const stagefright::MetaData* aMetaData,
              const char* aMimeType);

  virtual bool IsValid() const override;
};

}

#endif
