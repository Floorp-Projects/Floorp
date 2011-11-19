/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is nsAutoRestore.h.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
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

/* functions for restoring saved values at the end of a C++ scope */

#ifndef mozilla_AutoRestore_h_
#define mozilla_AutoRestore_h_

#include "mozilla/GuardObjects.h"

namespace mozilla {

  /**
   * Save the current value of a variable and restore it when the object
   * goes out of scope.  For example:
   *   {
   *     AutoRestore<bool> savePainting(mIsPainting);
   *     mIsPainting = true;
   *     
   *     // ... your code here ...
   *
   *     // mIsPainting is reset to its old value at the end of this block
   *   }
   */
  template <class T>
  class NS_STACK_CLASS AutoRestore
  {
  private:
    T& mLocation;
    T mValue;
    MOZILLA_DECL_USE_GUARD_OBJECT_NOTIFIER
  public:
    AutoRestore(T& aValue MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM)
      : mLocation(aValue), mValue(aValue)
    {
      MOZILLA_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~AutoRestore() { mLocation = mValue; }
  };

} // namespace mozilla

#endif /* !defined(mozilla_AutoRestore_h_) */
