/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

/*
 * Helper class to allow smart pointers to stack objects to be constructed for
 * ease of unit testing. Recycled here to help expose a shared_ptr interface to
 * objects which are really raw pointers.
 */
struct null_deleter {
  void operator()(void const*) const {}
};
