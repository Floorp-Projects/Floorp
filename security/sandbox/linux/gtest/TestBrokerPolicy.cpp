/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "broker/SandboxBroker.h"

namespace mozilla {

static const int MAY_ACCESS = SandboxBroker::MAY_ACCESS;
static const int MAY_READ = SandboxBroker::MAY_READ;
static const int MAY_WRITE = SandboxBroker::MAY_WRITE;
// static const int MAY_CREATE = SandboxBroker::MAY_CREATE;
// static const int RECURSIVE = SandboxBroker::RECURSIVE;
static const auto AddAlways = SandboxBroker::Policy::AddAlways;

TEST(SandboxBrokerPolicyLookup, Simple)
{
  SandboxBroker::Policy p;
  p.AddPath(MAY_READ, "/dev/urandom", AddAlways);

  EXPECT_NE(0, p.Lookup("/dev/urandom")) << "Added path not found.";
  EXPECT_EQ(MAY_ACCESS | MAY_READ, p.Lookup("/dev/urandom"))
      << "Added path found with wrong perms.";
  EXPECT_EQ(0, p.Lookup("/etc/passwd")) << "Non-added path was found.";
}

TEST(SandboxBrokerPolicyLookup, CopyCtor)
{
  SandboxBroker::Policy psrc;
  psrc.AddPath(MAY_READ | MAY_WRITE, "/dev/null", AddAlways);
  SandboxBroker::Policy pdst(psrc);
  psrc.AddPath(MAY_READ, "/dev/zero", AddAlways);
  pdst.AddPath(MAY_READ, "/dev/urandom", AddAlways);

  EXPECT_EQ(MAY_ACCESS | MAY_READ | MAY_WRITE, psrc.Lookup("/dev/null"))
      << "Common path absent in copy source.";
  EXPECT_EQ(MAY_ACCESS | MAY_READ | MAY_WRITE, pdst.Lookup("/dev/null"))
      << "Common path absent in copy destination.";

  EXPECT_EQ(MAY_ACCESS | MAY_READ, psrc.Lookup("/dev/zero"))
      << "Source-only path is absent.";
  EXPECT_EQ(0, pdst.Lookup("/dev/zero"))
      << "Source-only path is present in copy destination.";

  EXPECT_EQ(0, psrc.Lookup("/dev/urandom"))
      << "Destination-only path is present in copy source.";
  EXPECT_EQ(MAY_ACCESS | MAY_READ, pdst.Lookup("/dev/urandom"))
      << "Destination-only path is absent.";

  EXPECT_EQ(0, psrc.Lookup("/etc/passwd"))
      << "Non-added path is present in copy source.";
  EXPECT_EQ(0, pdst.Lookup("/etc/passwd"))
      << "Non-added path is present in copy source.";
}

TEST(SandboxBrokerPolicyLookup, Recursive)
{
  SandboxBroker::Policy psrc;
  psrc.AddPath(MAY_READ | MAY_WRITE, "/dev/null", AddAlways);
  psrc.AddPath(MAY_READ, "/dev/zero", AddAlways);
  psrc.AddPath(MAY_READ, "/dev/urandom", AddAlways);

  EXPECT_EQ(MAY_ACCESS | MAY_READ | MAY_WRITE, psrc.Lookup("/dev/null"))
      << "Basic path is present.";
  EXPECT_EQ(MAY_ACCESS | MAY_READ, psrc.Lookup("/dev/zero"))
      << "Basic path has no extra flags";

  psrc.AddDir(MAY_READ | MAY_WRITE, "/dev/");

  EXPECT_EQ(MAY_ACCESS | MAY_READ | MAY_WRITE, psrc.Lookup("/dev/random"))
      << "Permission via recursive dir.";
  EXPECT_EQ(MAY_ACCESS | MAY_READ | MAY_WRITE, psrc.Lookup("/dev/sd/0"))
      << "Permission via recursive dir, nested deeper";
  EXPECT_EQ(0, psrc.Lookup("/dev/sd/0/")) << "Invalid path format.";
  EXPECT_EQ(0, psrc.Lookup("/usr/dev/sd")) << "Match must be a prefix.";

  psrc.AddDir(MAY_READ, "/dev/sd/");
  EXPECT_EQ(MAY_ACCESS | MAY_READ | MAY_WRITE, psrc.Lookup("/dev/sd/0"))
      << "Extra permissions from parent path granted.";
  EXPECT_EQ(0, psrc.Lookup("/dev/..")) << "Refuse attempted subdir escape.";

  psrc.AddDir(MAY_READ, "/tmp");
  EXPECT_EQ(MAY_ACCESS | MAY_READ, psrc.Lookup("/tmp/good/a"))
      << "Check whether dir add with no trailing / was sucessful.";
  EXPECT_EQ(0, psrc.Lookup("/tmp_good_but_bad"))
      << "Enforce terminator on directories.";
  EXPECT_EQ(0, psrc.Lookup("/tmp/."))
      << "Do not allow opening a directory handle.";
}

}  // namespace mozilla
