/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsCycleCollector_h__
#define nsCycleCollector_h__

// NOTE: If you use header files to define DEBUG_CC, you must do so here
// *and* in nsCycleCollectionParticipant.h
//#define DEBUG_CC

class nsISupports;
class nsICycleCollectorListener;
class nsCycleCollectionParticipant;
class nsCycleCollectionTraversalCallback;

// An nsCycleCollectionLanguageRuntime is a per-language object that
// implements language-specific aspects of the cycle collection task.

struct nsCycleCollectionLanguageRuntime
{
    virtual nsresult BeginCycleCollection(nsCycleCollectionTraversalCallback &cb,
                                          bool explainLiveExpectedGarbage) = 0;
    virtual nsresult FinishTraverse() = 0;
    virtual nsresult FinishCycleCollection() = 0;
    virtual nsCycleCollectionParticipant *ToParticipant(void *p) = 0;
#ifdef DEBUG_CC
    virtual void PrintAllReferencesTo(void *p) = 0;
#endif
};

nsresult nsCycleCollector_startup();
// Returns the number of collected nodes.
PRUint32 nsCycleCollector_collect(nsICycleCollectorListener *aListener);
PRUint32 nsCycleCollector_suspectedCount();
void nsCycleCollector_shutdownThreads();
void nsCycleCollector_shutdown();

// The JS runtime is special, it needs to call cycle collection during its GC.
// If the JS runtime is registered nsCycleCollector_collect will call
// nsCycleCollectionJSRuntime::Collect which will call
// nsCycleCollector_doCollect, else nsCycleCollector_collect will call
// nsCycleCollector_doCollect directly.
struct nsCycleCollectionJSRuntime : public nsCycleCollectionLanguageRuntime
{
    /**
     * Called before/after transitioning to/from the main thread.
     */
    virtual void NotifyLeaveMainThread() = 0;
    virtual void NotifyEnterCycleCollectionThread() = 0;
    virtual void NotifyLeaveCycleCollectionThread() = 0;
    virtual void NotifyEnterMainThread() = 0;

    /**
     * Should we force a JavaScript GC before a CC?
     */
    virtual bool NeedCollect() = 0;

    /**
     * Runs the JavaScript GC.
     */
    virtual void Collect(bool shrinkingGC = false) = 0;
};

#ifdef DEBUG
void nsCycleCollector_DEBUG_shouldBeFreed(nsISupports *n);
void nsCycleCollector_DEBUG_wasFreed(nsISupports *n);
#endif

// Helpers for interacting with language-identified scripts

void nsCycleCollector_registerRuntime(PRUint32 langID, nsCycleCollectionLanguageRuntime *rt);
nsCycleCollectionLanguageRuntime * nsCycleCollector_getRuntime(PRUint32 langID);
void nsCycleCollector_forgetRuntime(PRUint32 langID);

#define NS_CYCLE_COLLECTOR_LOGGER_CID \
{ 0x58be81b4, 0x39d2, 0x437c, \
{ 0x94, 0xea, 0xae, 0xde, 0x2c, 0x62, 0x08, 0xd3 } }

extern nsresult
nsCycleCollectorLoggerConstructor(nsISupports* outer,
                                  const nsIID& aIID,
                                  void* *aInstancePtr);

#endif // nsCycleCollector_h__
