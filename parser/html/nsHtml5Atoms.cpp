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
 * Mozilla Foundation.
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

/*
 * This class wraps up the creation (and destruction) of the standard
 * set of atoms used by the HTML5 parser; the atoms are created when
 * nsHtml5Module is loaded and they are destroyed when nsHtml5Module is
 * unloaded.
 */

#include "mozilla/Util.h"

#include "nsHtml5Atoms.h"
#include "nsStaticAtom.h"

using namespace mozilla;

// define storage for all atoms
#define HTML5_ATOM(_name, _value) nsIAtom* nsHtml5Atoms::_name;
#include "nsHtml5AtomList.h"
#undef HTML5_ATOM

#define HTML5_ATOM(name_, value_) NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "nsHtml5AtomList.h"
#undef HTML5_ATOM

static const nsStaticAtom Html5Atoms_info[] = {
#define HTML5_ATOM(name_, value_) NS_STATIC_ATOM(name_##_buffer, &nsHtml5Atoms::name_),
#include "nsHtml5AtomList.h"
#undef HTML5_ATOM
};

void nsHtml5Atoms::AddRefAtoms()
{
  NS_RegisterStaticAtoms(Html5Atoms_info, ArrayLength(Html5Atoms_info));
}
