/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#ifndef nsObsoleteAString_h___
#define nsObsoleteAString_h___

/*
    STRING INTERNALS

    This file defines nsObsoleteAString and nsObsoleteACString.  These are
    abstract classes (interfaces), containing only virtual functions
    corresponding to the FROZEN nsA[C]String classes.  They exist to provide
    the new non-abstract nsA[C]String classes with a mechanism to maintain
    backwards binary compatability.

    From a binary point-of-view, the new nsA[C]String classes appear to have a
    vtable pointer to the vtable of nsObsoleteA[C]StringThunk.
    nsObsoleteA[C]StringThunk is a subclass of nsObsoleteA[C]String that serves
    as a simple bridge between the virtual functions that make up the FROZEN
    nsA[C]String contract and the new ns[C]Substring, which is now our only
    subclass of nsA[C]String.

    The methods on the new nsA[C]String are now all non-virtual.  This reduces
    codesize at the callsite, and reduces the number of memory dereferences and
    jumps required to invoke a method on nsA[C]String.  The result is improved
    performance and reduced codesize.  However, to maintain binary
    compatibility, each method on nsA[C]String must check the value of its
    vtable to determine if it corresponds to the built-in implementation of
    nsObsoleteA[C]String (i.e., the address of the canonical vtable).  If it
    does, then the vtable can be ignored, and the nsA[C]String object (i.e.,
    |this|) can be cast to ns[C]Substring directly.  In which case, the
    nsA[C]String methods can be satisfied by invoking the corresponding methods
    directly on ns[C]Substring.  If the vtable address does not correspond to
    the built-in implementation, then the virtual functions on
    nsObsoleteA[C]String must be invoked instead.

    So, if a nsA[C]String reference corresponds to an external implementation
    (such as the old nsEmbed[C]String that shipped with previous versions of
    Mozilla), then methods invoked on that nsA[C]String will still act like
    virtual function calls.  This ensures binary compatibility while avoiding
    virtual function calls in most cases.

    Finally, nsObsoleteA[C]String (i.e., the FROZEN nsA[C]String vtable) is
    now a DEPRECATED interface.  See nsStringAPI.h and nsEmbedString.h for
    the new preferred way to access nsA[C]String references in an external
    component or Gecko embedding application.
                                                                             */

#ifndef nsStringFwd_h___
#include "nsStringFwd.h"
#endif

#ifndef nscore_h___
#include "nscore.h"
#endif

  // declare nsObsoleteAString
#include "string-template-def-unichar.h"
#include "nsTObsoleteAString.h"
#include "string-template-undef.h"

  // declare nsObsoleteACString
#include "string-template-def-char.h"
#include "nsTObsoleteAString.h"
#include "string-template-undef.h"

#endif // !defined(nsObsoleteAString_h___)
