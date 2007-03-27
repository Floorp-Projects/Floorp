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

class nsISupports;
class nsDeque;
struct nsCycleCollectionTraversalCallback;

// An nsCycleCollectionLanguageRuntime is a per-language object that
// implements language-specific aspects of the cycle collection task.

struct nsCycleCollectionLanguageRuntime
{
    virtual nsresult BeginCycleCollection() = 0;

    virtual nsresult Traverse(void *p, nsCycleCollectionTraversalCallback &cb) = 0;

    virtual nsresult Root(const nsDeque &nodes) = 0;
    virtual nsresult Unlink(const nsDeque &nodes) = 0;
    virtual nsresult Unroot(const nsDeque &nodes) = 0;

    virtual nsresult FinishCycleCollection() = 0;
};


NS_COM PRBool nsCycleCollector_isScanSafe(nsISupports *n);
NS_COM void nsCycleCollector_suspect(nsISupports *n);
NS_COM void nsCycleCollector_suspectCurrent(nsISupports *n);
NS_COM void nsCycleCollector_forget(nsISupports *n);
nsresult nsCycleCollector_startup();
NS_COM void nsCycleCollector_collect();
void nsCycleCollector_shutdown();

#ifdef DEBUG
NS_COM void nsCycleCollector_DEBUG_shouldBeFreed(nsISupports *n);
NS_COM void nsCycleCollector_DEBUG_wasFreed(nsISupports *n);
#endif

// Helpers for interacting with language-identified scripts

NS_COM void nsCycleCollector_registerRuntime(PRUint32 langID, nsCycleCollectionLanguageRuntime *rt);
NS_COM void nsCycleCollector_forgetRuntime(PRUint32 langID);

#endif // nsCycleCollector_h__
