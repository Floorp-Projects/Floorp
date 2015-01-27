/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/unused.h"
#include "mp4_demuxer/SinfParser.h"
#include "mp4_demuxer/AtomType.h"
#include "mp4_demuxer/Box.h"

namespace mp4_demuxer {

Sinf::Sinf(Box& aBox)
  : mDefaultIVSize(0)
  , mDefaultEncryptionType()
{
  SinfParser parser(aBox);
  if (parser.GetSinf().IsValid()) {
    *this = parser.GetSinf();
  }
}

SinfParser::SinfParser(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("schm")) {
      ParseSchm(box);
    } else if (box.IsType("schi")) {
      ParseSchi(box);
    }
  }
}

void
SinfParser::ParseSchm(Box& aBox)
{
  BoxReader reader(aBox);

  if (reader->Remaining() < 8) {
    return;
  }

  mozilla::unused << reader->ReadU32(); // flags -- ignore
  mSinf.mDefaultEncryptionType = reader->ReadU32();

  reader->DiscardRemaining();
}

void
SinfParser::ParseSchi(Box& aBox)
{
  for (Box box = aBox.FirstChild(); box.IsAvailable(); box = box.Next()) {
    if (box.IsType("tenc")) {
      ParseTenc(box);
    }
  }
}

void
SinfParser::ParseTenc(Box& aBox)
{
  BoxReader reader(aBox);

  if (reader->Remaining() < 24) {
    return;
  }

  mozilla::unused << reader->ReadU32(); // flags -- ignore

  uint32_t isEncrypted = reader->ReadU24();
  mSinf.mDefaultIVSize = reader->ReadU8();
  memcpy(mSinf.mDefaultKeyID, reader->Read(16), 16);
}

}
