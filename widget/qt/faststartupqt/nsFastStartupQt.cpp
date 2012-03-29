/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
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
 * The Original Code is Nokia.
 *
 * The Initial Developer of the Original Code is Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#include <QApplication>
#include "nsQAppInstance.h"
#include <QtOpenGL/QGLWidget>
#include <QThread>
#if defined MOZ_ENABLE_MEEGOTOUCH
#include <MScene>
#endif
#include "moziqwidget.h"
#include "mozqwidgetfast.h"
#include "nsFastStartupQt.h"
#include "nsXPCOMGlue.h"
#include "nsXULAppAPI.h"


static nsFastStartup* sFastStartup = NULL;

void
GeckoThread::run()
{
  emit symbolsLoadingFinished(mFunc(mExecPath));
}

void
nsFastStartup::symbolsLoadingFinished(bool preloaded)
{
  mSymbolsLoaded = preloaded;
  if (mWidgetPainted && mSymbolsLoaded) {
    mLoop.quit();
  }
}

void nsFastStartup::painted()
{
  mWidgetPainted = true;
  if (mWidgetPainted && mSymbolsLoaded) {
    mLoop.quit();
  }
}

MozGraphicsView*
nsFastStartup::GetStartupGraphicsView(QWidget* parentWidget, IMozQWidget* aTopChild)
{
  MozGraphicsView* view = NULL;
  if (sFastStartup && sFastStartup->mGraphicsView) {
    view = sFastStartup->mGraphicsView;
  } else {
    view = new MozGraphicsView(parentWidget);
    Qt::WindowFlags flags = Qt::Widget;
    view->setWindowFlags(flags);
#if MOZ_PLATFORM_MAEMO == 6
    view->setViewport(new QGLWidget());
#endif
  }
  view->SetTopLevel(aTopChild, parentWidget);

  return view;
}

nsFastStartup*
nsFastStartup::GetSingleton()
{
  return sFastStartup;
}

nsFastStartup::nsFastStartup()
 : mGraphicsView(0)
 , mFakeWidget(0)
 , mSymbolsLoaded(false)
 , mWidgetPainted(false)
 , mThread(0)

{
  sFastStartup = this;
}

nsFastStartup::~nsFastStartup()
{
  nsQAppInstance::Release();
  sFastStartup = 0;
}

void
nsFastStartup::RemoveFakeLayout()
{
  if (mFakeWidget && mGraphicsView) {
    mGraphicsView->scene()->removeItem(mFakeWidget);
    mFakeWidget->deleteLater();
    mFakeWidget = 0;
    // Forget GraphicsView, ownership moved to nsIWidget
    mGraphicsView = 0;
  }
}

bool
nsFastStartup::CreateFastStartup(int& argc, char ** argv,
                                 const char* execPath,
                                 GeckoLoaderFunc aFunc)
{
  gArgc = argc;
  gArgv = argv;
  // Create main QApplication instance
  nsQAppInstance::AddRef(argc, argv, true);
  // Create symbols loading thread
  mThread = new GeckoThread();
  // Setup thread loading finished callbacks
  connect(mThread, SIGNAL(symbolsLoadingFinished(bool)),
          this, SLOT(symbolsLoadingFinished(bool)));
  mThread->SetLoader(aFunc, execPath);
  // Create Static UI widget and view
  IMozQWidget* fakeWidget = new MozQWidgetFast(NULL, NULL);
  mGraphicsView = GetStartupGraphicsView(NULL, fakeWidget);
  mFakeWidget = fakeWidget;

  mThread->start();
#ifdef MOZ_PLATFORM_MAEMO
  mGraphicsView->showFullScreen();
#else
  mGraphicsView->showNormal();
#endif

  // Start native loop in order to get view opened and painted once
  // Will block CreateFastStartup function and
  // exit when symbols are loaded and Static UI shown
  mLoop.exec();

  return true;
}
