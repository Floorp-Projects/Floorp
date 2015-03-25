/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef SINF_PARSER_H_
#define SINF_PARSER_H_

#include "mp4_demuxer/Atom.h"
#include "mp4_demuxer/AtomType.h"

namespace mp4_demuxer {

class Box;

class Sinf : public Atom
{
public:
  Sinf()
    : mDefaultIVSize(0)
    , mDefaultEncryptionType()
  {}
  explicit Sinf(Box& aBox);

  virtual bool IsValid() override
  {
    return !!mDefaultIVSize && !!mDefaultEncryptionType;
  }

  uint8_t mDefaultIVSize;
  AtomType mDefaultEncryptionType;
  uint8_t mDefaultKeyID[16];
};

class SinfParser
{
public:
  explicit SinfParser(Box& aBox);

  Sinf& GetSinf() { return mSinf; }
private:
  void ParseSchm(Box& aBox);
  void ParseSchi(Box& aBox);
  void ParseTenc(Box& aBox);

  Sinf mSinf;
};

}

#endif // SINF_PARSER_H_
