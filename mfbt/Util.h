/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyrigght (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef mozilla_Util_h_
#define mozilla_Util_h_

namespace mozilla {

/**
 * DebugOnly contains a value of type T, but only in debug builds.  In
 * release builds, it does not contain a value.  This helper is
 * intended to be used along with ASSERT()-style macros, allowing one
 * to write
 *
 *   DebugOnly<bool> check = Func();
 *   ASSERT(check);
 *
 * more concisely than declaring |check| conditional on #ifdef DEBUG,
 * but also without allocating storage space for |check| in release
 * builds.
 *
 * DebugOnly instances can only be coerced to T in debug builds; in
 * release builds, they don't have a value so type coercion is not
 * well defined.
 */
template <typename T>
struct DebugOnly
{
#ifdef DEBUG
    T value;

    DebugOnly() {}
    DebugOnly(const T& other) : value(other) {}
    DebugOnly& operator=(const T& rhs) {
        value = rhs;
        return *this;
    }

    operator T&() { return value; }
    operator const T&() const { return value; }

#else
    DebugOnly() {}
    DebugOnly(const T&) {}
    DebugOnly& operator=(const T&) {}   
#endif

    // DebugOnly must always have a destructor or else it will
    // generate "unused variable" warnings, exactly what it's intended
    // to avoid!
    ~DebugOnly() {}
};

} // namespace mozilla

#endif  // mozilla_Util_h_
