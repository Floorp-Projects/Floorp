/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_widget_GfxInfoWebGL_h__
#define __mozilla_widget_GfxInfoWebGL_h__

#include "nsString.h"

namespace mozilla {
namespace widget {

class GfxInfoWebGL {
public:
    static nsresult GetWebGLParameter(const nsAString& aParam, nsAString& aResult);

protected:
    GfxInfoWebGL() { }
};

}
}

#endif
