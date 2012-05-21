/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

        /**
         * The return value, if not null, should be released as needed
         * by the caller using g_object_unref.
         */
        NS_IMETHOD_(GdkPixbuf*) ConvertImageToPixbuf(imgIContainer* aImage) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIImageToPixbuf, NSIIMAGETOPIXBUF_IID)

#endif
