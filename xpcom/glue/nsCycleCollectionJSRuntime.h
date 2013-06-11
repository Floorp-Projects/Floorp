/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCycleCollectionJSRuntime_h__
#define nsCycleCollectionJSRuntime_h__

class nsCycleCollectionParticipant;
class nsCycleCollectionNoteRootCallback;

// Various methods the cycle collector needs to deal with Javascript.
struct nsCycleCollectionJSRuntime
{
  virtual nsresult BeginCycleCollection(nsCycleCollectionNoteRootCallback &aCb) = 0;

  /**
   * Called before/after transitioning to/from the main thread.
   *
   * NotifyLeaveMainThread may return 'false' to prevent the cycle collector
   * from leaving the main thread.
   */
  virtual bool NotifyLeaveMainThread() = 0;
  virtual void NotifyEnterCycleCollectionThread() = 0;
  virtual void NotifyLeaveCycleCollectionThread() = 0;
  virtual void NotifyEnterMainThread() = 0;

  /**
   * Unmark gray any weak map values, as needed.
   */
  virtual void FixWeakMappingGrayBits() = 0;

  /**
   * Return true if merging content zones may greatly reduce the size of the CC graph.
   */
  virtual bool UsefulToMergeZones() = 0;

  /**
   * Should we force a JavaScript GC before a CC?
   */
  virtual bool NeedCollect() = 0;

  /**
   * Runs the JavaScript GC. |reason| is a gcreason::Reason from jsfriendapi.h.
   */
  virtual void Collect(uint32_t aReason) = 0;

  /**
   * Get the JS cycle collection participant.
   */
  virtual nsCycleCollectionParticipant *GetParticipant() = 0;

#ifdef DEBUG
  virtual void SetObjectToUnlink(void* aObject) = 0;
  virtual void AssertNoObjectsToTrace(void* aPossibleJSHolder) = 0;
#endif
};

#endif // nsCycleCollectionJSRuntime_h__
