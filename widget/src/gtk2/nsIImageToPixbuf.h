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
 * The Original Code is mozilla.org widget code.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifndef NSIIMAGETOPIXBUF_H_
#define NSIIMAGETOPIXBUF_H_

#include "nsISupports.h"

// dfa4ac93-83f2-4ab8-9b2a-0ff7022aebe2
#define NSIIMAGETOPIXBUF_IID \
{ 0xdfa4ac93, 0x83f2, 0x4ab8, \
  { 0x9b, 0x2a, 0x0f, 0xf7, 0x02, 0x2a, 0xeb, 0xe2 } }

class imgIContainer;
typedef struct _GdkPixbuf GdkPixbuf;

/**
 * An interface that allows converting the current frame of an imgIContainer to a GdkPixbuf*.
 */
class nsIImageToPixbuf : public nsISupports {
    public:
        NS_DECLARE_STATIC_IID_ACCESSOR(NSIIMAGETOPIXBUF_IID)

        NS_IMETHOD_(GdkPixbuf*) ConvertImageToPixbuf(imgIContainer* aImage) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIImageToPixbuf, NSIIMAGETOPIXBUF_IID)

#endif
