/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsQAppInstance_h
#define nsQAppInstance_h

// declared in nsAppRunner.cpp
extern int    gArgc;
extern char **gArgv;

class QApplication;
class MComponentData;
class nsQAppInstance
{
public:
  static void AddRef(int& aArgc = gArgc,
                     char** aArgv = gArgv,
                     bool aDefaultProcess = false);
  static void Release(void);

private:
  static QApplication *sQAppInstance;
  static MComponentData* sMComponentData;
  static int sQAppRefCount;
};

#endif /* nsQAppInstance_h */
