/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sw=4 et tw=99 ft=cpp: */
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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *   Ms2ger <ms2ger@gmail.com>
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

#ifndef mozilla_GuardObjects_h
#define mozilla_GuardObjects_h

#include "mozilla/Types.h"
#include "mozilla/Util.h"

namespace mozilla {
  /**
   * The following classes are designed to cause assertions to detect
   * inadvertent use of guard objects as temporaries.  In other words,
   * when we have a guard object whose only purpose is its constructor and
   * destructor (and is never otherwise referenced), the intended use
   * might be:
   *     AutoRestore savePainting(mIsPainting);
   * but is is easy to accidentally write:
   *     AutoRestore(mIsPainting);
   * which compiles just fine, but runs the destructor well before the
   * intended time.
   *
   * They work by adding (#ifdef DEBUG) an additional parameter to the
   * guard object's constructor, with a default value, so that users of
   * the guard object's API do not need to do anything.  The default value
   * of this parameter is a temporary object.  C++ (ISO/IEC 14882:1998),
   * section 12.2 [class.temporary], clauses 4 and 5 seem to assume a
   * guarantee that temporaries are destroyed in the reverse of their
   * construction order, but I actually can't find a statement that that
   * is true in the general case (beyond the two specific cases mentioned
   * there).  However, it seems to be true.
   *
   * These classes are intended to be used only via the macros immediately
   * below them:
   *   MOZILLA_DECL_USE_GUARD_OBJECT_NOTIFIER declares (ifdef DEBUG) a member
   *     variable, and should be put where a declaration of a private
   *     member variable would be placed.
   *   MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM should be placed at the end of the
   *     parameters to each constructor of the guard object; it declares
   *     (ifdef DEBUG) an additional parameter.  (But use the *_ONLY_PARAM
   *     variant for constructors that take no other parameters.)
   *   MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL should likewise be used in
   *     the implementation of such constructors when they are not inline.
   *   MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT should be used in
   *     the implementation of such constructors to pass the parameter to
   *     a base class that also uses these macros
   *   MOZILLA_GUARD_OBJECT_NOTIFIER_INIT is a statement that belongs in each
   *     constructor.  It uses the parameter declared by
   *     MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM.
   *
   * For more details, and examples of using these macros, see
   * https://developer.mozilla.org/en/Using_RAII_classes_in_Mozilla
   */
#ifdef DEBUG
  class GuardObjectNotifier
  {
  private:
    bool* mStatementDone;
  public:
    GuardObjectNotifier() : mStatementDone(NULL) {}

    ~GuardObjectNotifier() {
      *mStatementDone = true;
    }

    void SetStatementDone(bool *aStatementDone) {
      mStatementDone = aStatementDone;
    }
  };

  class GuardObjectNotificationReceiver
  {
  private:
    bool mStatementDone;
  public:
    GuardObjectNotificationReceiver() : mStatementDone(false) {}

    ~GuardObjectNotificationReceiver() {
      /*
       * Assert that the guard object was not used as a temporary.
       * (Note that this assert might also fire if Init is not called
       * because the guard object's implementation is not using the
       * above macros correctly.)
       */
      MOZ_ASSERT(mStatementDone);
    }

    void Init(const GuardObjectNotifier &aNotifier) {
      /*
       * aNotifier is passed as a const reference so that we can pass a
       * temporary, but we really intend it as non-const
       */
      const_cast<GuardObjectNotifier&>(aNotifier).
          SetStatementDone(&mStatementDone);
    }
  };

  #define MOZILLA_DECL_USE_GUARD_OBJECT_NOTIFIER \
      mozilla::GuardObjectNotificationReceiver _mCheckNotUsedAsTemporary;
  #define MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM \
      , const mozilla::GuardObjectNotifier& _notifier = \
                mozilla::GuardObjectNotifier()
  #define MOZILLA_GUARD_OBJECT_NOTIFIER_ONLY_PARAM \
      const mozilla::GuardObjectNotifier& _notifier = \
              mozilla::GuardObjectNotifier()
  #define MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL \
      , const mozilla::GuardObjectNotifier& _notifier
  #define MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT \
      , _notifier
  #define MOZILLA_GUARD_OBJECT_NOTIFIER_INIT \
      PR_BEGIN_MACRO _mCheckNotUsedAsTemporary.Init(_notifier); PR_END_MACRO

#else /* defined(DEBUG) */

  #define MOZILLA_DECL_USE_GUARD_OBJECT_NOTIFIER
  #define MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM
  #define MOZILLA_GUARD_OBJECT_NOTIFIER_ONLY_PARAM
  #define MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL
  #define MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM_TO_PARENT
  #define MOZILLA_GUARD_OBJECT_NOTIFIER_INIT PR_BEGIN_MACRO PR_END_MACRO

#endif /* !defined(DEBUG) */

} // namespace mozilla

#endif /* mozilla_GuardObjects_h */
