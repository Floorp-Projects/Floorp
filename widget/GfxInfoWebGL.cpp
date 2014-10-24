/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GfxInfoWebGL.h"

#include "nsServiceManagerUtils.h"

#include "GLDefs.h"
#include "nsIDOMWebGLRenderingContext.h"
#include "nsICanvasRenderingContextInternal.h"

using namespace mozilla::widget;

nsresult
GfxInfoWebGL::GetWebGLParameter(const nsAString& aParam, nsAString& aResult)
{
  GLenum param;

  if (aParam.EqualsLiteral("vendor")) param = LOCAL_GL_VENDOR;
  else if (aParam.EqualsLiteral("renderer")) param = LOCAL_GL_RENDERER;
  else if (aParam.EqualsLiteral("version")) param = LOCAL_GL_VERSION;
  else if (aParam.EqualsLiteral("shading_language_version")) param = LOCAL_GL_SHADING_LANGUAGE_VERSION;
  else if (aParam.EqualsLiteral("extensions")) param = LOCAL_GL_EXTENSIONS;
  else if (aParam.EqualsLiteral("full-renderer")) param = 0;
  else return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIDOMWebGLRenderingContext> webgl =
    do_CreateInstance("@mozilla.org/content/canvas-rendering-context;1?id=experimental-webgl");
  if (!webgl)
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsICanvasRenderingContextInternal> webglInternal =
    do_QueryInterface(webgl);
  if (!webglInternal)
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = webglInternal->SetDimensions(16, 16);
  NS_ENSURE_SUCCESS(rv, rv);

  if (param)
    return webgl->MozGetUnderlyingParamString(param, aResult);

  // this is the "full renderer" string, which is vendor + renderer + version

  nsAutoString str;

  rv = webgl->MozGetUnderlyingParamString(LOCAL_GL_VENDOR, str);
  NS_ENSURE_SUCCESS(rv, rv);

  aResult.Append(str);
  aResult.AppendLiteral(" -- ");

  rv = webgl->MozGetUnderlyingParamString(LOCAL_GL_RENDERER, str);
  NS_ENSURE_SUCCESS(rv, rv);

  aResult.Append(str);
  aResult.AppendLiteral(" -- ");

  rv = webgl->MozGetUnderlyingParamString(LOCAL_GL_VERSION, str);
  NS_ENSURE_SUCCESS(rv, rv);

  aResult.Append(str);

  return NS_OK;
}
