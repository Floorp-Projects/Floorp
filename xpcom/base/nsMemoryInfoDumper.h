/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nsMemoryInfoDumper_h
#define mozilla_nsMemoryInfoDumper_h

#include "nsIMemoryInfoDumper.h"

class nsACString;

/**
 * This class facilitates dumping information about our memory usage to disk.
 *
 * Its cpp file also has Linux-only code which watches various OS signals and
 * dumps memory info upon receiving a signal.  You can activate these listeners
 * by calling Initialize().
 */
class nsMemoryInfoDumper : public nsIMemoryInfoDumper
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYINFODUMPER

  nsMemoryInfoDumper();
  virtual ~nsMemoryInfoDumper();

public:
  static void Initialize();

#ifdef MOZ_DMD
  static nsresult DumpDMD(const nsAString &aIdentifier);
#endif
};

#define NS_MEMORY_INFO_DUMPER_CID \
{ 0x00bd71fb, 0x7f09, 0x4ec3, \
{ 0x96, 0xaf, 0xa0, 0xb5, 0x22, 0xb7, 0x79, 0x69 } }

#endif
