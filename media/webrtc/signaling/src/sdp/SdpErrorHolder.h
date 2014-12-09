/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDPERRORHOLDER_H_
#define _SDPERRORHOLDER_H_

#include <vector>
#include <string>

namespace mozilla
{

class SdpErrorHolder
{
public:
  SdpErrorHolder() {}
  virtual ~SdpErrorHolder() {}

  void
  AddParseError(size_t line, const std::string& message)
  {
    mErrors.push_back(std::make_pair(line, message));
  }

  void
  ClearParseErrors()
  {
    mErrors.clear();
  }

  /**
   * Returns a reference to the list of parse errors.
   * This gets cleared out when you call Parse.
   */
  const std::vector<std::pair<size_t, std::string> >&
  GetParseErrors() const
  {
    return mErrors;
  }

private:
  std::vector<std::pair<size_t, std::string> > mErrors;
};

} // namespace mozilla

#endif
