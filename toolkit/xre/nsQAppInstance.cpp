/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 et sw=2 tw=80: */
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com>
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

#include "nsQAppInstance.h"
#include <QApplication>
#ifdef MOZ_ENABLE_MEEGOTOUCH
#include <MComponentData>
#include <MApplicationService>
#endif
#include "prenv.h"
#include <stdlib.h>

QApplication *nsQAppInstance::sQAppInstance = NULL;
#ifdef MOZ_ENABLE_MEEGOTOUCH
MComponentData* nsQAppInstance::sMComponentData = NULL;
#endif
int nsQAppInstance::sQAppRefCount = 0;

void nsQAppInstance::AddRef(int& aArgc, char** aArgv, bool aDefaultProcess) {
  if (qApp)
    return;
  if (!sQAppInstance) {
    const char *graphicsSystem = getenv("MOZ_QT_GRAPHICSSYSTEM");
    if (graphicsSystem) {
      QApplication::setGraphicsSystem(QString(graphicsSystem));
    }
#if (MOZ_PLATFORM_MAEMO == 6)
    // Should create simple windows style for non chrome process
    // otherwise meegotouch style initialize and crash happen
    // because we don't initialize MComponent for child process
    if (!aDefaultProcess) {
      QApplication::setStyle(QLatin1String("windows"));
    }
    if (!aArgc) {
      aArgv[aArgc] = strdup("nsQAppInstance");
      aArgc++;
    }
#endif
    sQAppInstance = new QApplication(aArgc, aArgv);
#ifdef MOZ_ENABLE_MEEGOTOUCH
    if (aDefaultProcess) {
      // GLContext created by meegotouch will be under meego graphics
      // system control, and will drop GL context automatically in background mode
      // In order to use that GLContext we need to implement
      // LayerManager switch in runtime from SW<->HW
      // That not yet implemented so we need to control GL context,
      // force software mode for, and create our own QGLWidget
      gArgv[gArgc] = strdup("-software");
      gArgc++;
      sMComponentData = new MComponentData(aArgc, aArgv, "", new MApplicationService(""));
    }
#endif
  }
  sQAppRefCount++;
}

void nsQAppInstance::Release(void) {
  if (sQAppInstance && !--sQAppRefCount) {
#ifdef MOZ_ENABLE_MEEGOTOUCH
    delete sMComponentData;
    sMComponentData = NULL;
#endif
    delete sQAppInstance;
    sQAppInstance = NULL;
  }
}
