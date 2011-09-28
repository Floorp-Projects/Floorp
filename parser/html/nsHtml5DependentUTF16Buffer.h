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
 * The Original Code is Dependent UTF-16 Buffer code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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

#ifndef nsHtml5DependentUTF16Buffer_h_
#define nsHtml5DependentUTF16Buffer_h_

#include "nscore.h"
#include "nsHtml5OwningUTF16Buffer.h"

class NS_STACK_CLASS nsHtml5DependentUTF16Buffer : public nsHtml5UTF16Buffer
{
  public:
    /**
     * Wraps a string without taking ownership of the buffer. aToWrap MUST NOT
     * go away or be shortened while nsHtml5DependentUTF16Buffer is in use.
     */
    nsHtml5DependentUTF16Buffer(const nsAString& aToWrap);

    ~nsHtml5DependentUTF16Buffer();

    /**
     * Copies the currently unconsumed part of this buffer into a new
     * heap-allocated nsHtml5OwningUTF16Buffer. The new object is allocated
     * with a fallible allocator. If the allocation fails, nsnull is returned.
     * @return heap-allocated copy or nsnull if memory allocation failed
     */
    already_AddRefed<nsHtml5OwningUTF16Buffer> FalliblyCopyAsOwningBuffer();
};

#endif // nsHtml5DependentUTF16Buffer_h_
