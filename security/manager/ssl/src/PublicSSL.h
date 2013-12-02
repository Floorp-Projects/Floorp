/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SSL_h
#define mozilla_SSL_h

namespace mozilla {

void ClearPrivateSSLState();

namespace psm {

void InitializeSSLServerCertVerificationThreads();
void StopSSLServerCertVerificationThreads();
void DisableMD5();
nsresult InitializeCipherSuite();

} //namespace psm
} // namespace mozilla

#endif

