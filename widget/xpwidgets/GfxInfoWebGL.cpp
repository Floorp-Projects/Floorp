/* vim: se cin sw=2 ts=2 et : */
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 2010
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
