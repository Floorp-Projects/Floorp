/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CertAnnotator_h
#define mozilla_CertAnnotator_h

#include "mozilla/Move.h"
#include "mozilla/Mutex.h"
#include "nsDataHashtable.h"
#include "nsIObserver.h"
#include "nsIThread.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {

class CertAnnotator final : public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void Register();

private:
  CertAnnotator() = default;
  virtual ~CertAnnotator();

  bool Init();

  void RecordInitialCertInfo();
  void RecordCertInfo(const nsAString& aLibPath, const bool aDoSerialize);

  void Serialize();
  void Annotate(const nsCString& aAnnotation);

  nsDataHashtable<nsStringHashKey, nsTArray<nsString>> mCertTable;
  nsCOMPtr<nsIThread> mAnnotatorThread;
};

} // namespace mozilla

#endif // mozilla_CertAnnotator_h

