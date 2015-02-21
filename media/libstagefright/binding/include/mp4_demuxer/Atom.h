/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ATOM_H_
#define ATOM_H_

namespace mp4_demuxer {

class Atom
{
public:
  Atom()
    : mValid(false)
  {
  }
  virtual bool IsValid()
  {
    return mValid;
  }
protected:
  bool mValid;
};

}

#endif // ATOM_H_
