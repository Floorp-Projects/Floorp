/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_JOG_h
#define mozilla_glean_JOG_h

namespace mozilla::glean {

class JOG {
 public:
  /*
   * Returns whether JOG knows about a category by this name
   *
   * @param aCategoryName The category name to check.
   *
   * @returns true if JOG is aware of a category by the given name at this time
   */
  static bool HasCategory(const nsACString& aCategoryName);
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_JOG_h */
