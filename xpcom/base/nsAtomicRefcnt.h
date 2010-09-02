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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Justin Lebar <justin.lebar@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsAtomicRefcnt_h__
#define nsAtomicRefcnt_h__

#include "nscore.h"
#include "pratom.h"

class nsAutoRefCnt;

// This header defines functions for modifying refcounts which wrap the
// PR_ATOMIC_* macros.

#if defined(XP_WIN)

#if PR_BYTES_PER_LONG == 4
typedef volatile long nsAtomicRefcnt;
#else
#error "Windows should have 4 bytes per long."
#endif

#else /* !defined(XP_WIN) */

typedef PRInt32  nsAtomicRefcnt;

#endif

inline nsrefcnt
NS_AtomicIncrementRefcnt(nsrefcnt &refcnt)
{
  return (nsrefcnt) PR_ATOMIC_INCREMENT((nsAtomicRefcnt*)&refcnt);
}

inline nsrefcnt
NS_AtomicIncrementRefcnt(nsAutoRefCnt &refcnt)
{
  // This cast is safe since nsAtomicRefCnt contains just one member, its refcount.
  return (nsrefcnt) PR_ATOMIC_INCREMENT((nsAtomicRefcnt*)&refcnt);
}

inline nsrefcnt
NS_AtomicDecrementRefcnt(nsrefcnt &refcnt)
{
  return (nsrefcnt) PR_ATOMIC_DECREMENT((nsAtomicRefcnt*)&refcnt);
}

inline nsrefcnt
NS_AtomicDecrementRefcnt(nsAutoRefCnt &refcnt)
{
  return (nsrefcnt) PR_ATOMIC_DECREMENT((nsAtomicRefcnt*)&refcnt);
}

#endif
