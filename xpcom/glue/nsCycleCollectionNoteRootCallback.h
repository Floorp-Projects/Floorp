/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCycleCollectionNoteRootCallback_h__
#define nsCycleCollectionNoteRootCallback_h__

class nsCycleCollectionParticipant;
class nsISupports;

class nsCycleCollectionNoteRootCallback
{
public:
  NS_IMETHOD_(void) NoteXPCOMRoot(nsISupports *root) = 0;
  NS_IMETHOD_(void) NoteJSRoot(void *root) = 0;
  NS_IMETHOD_(void) NoteNativeRoot(void *root, nsCycleCollectionParticipant *participant) = 0;

  NS_IMETHOD_(void) NoteWeakMapping(void *map, void *key, void *kdelegate, void *val) = 0;

  bool WantAllTraces() const { return mWantAllTraces; }
protected:
  nsCycleCollectionNoteRootCallback() : mWantAllTraces(false) {}

  bool mWantAllTraces;
};

#endif // nsCycleCollectionNoteRootCallback_h__
