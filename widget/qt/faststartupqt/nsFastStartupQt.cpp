/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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


static nsFastStartup* sFastStartup = nullptr;

void
GeckoThread::run()
{
  Q_EMIT symbolsLoadingFinished(mFunc(mExecPath));
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
  MozGraphicsView* view = nullptr;
  if (sFastStartup && sFastStartup->mGraphicsView) {
    view = sFastStartup->mGraphicsView;
  } else {
    view = new MozGraphicsView(parentWidget);
    Qt::WindowFlags flags = Qt::Widget;
    view->setWindowFlags(flags);
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
  IMozQWidget* fakeWidget = new MozQWidgetFast(nullptr, nullptr);
  mGraphicsView = GetStartupGraphicsView(nullptr, fakeWidget);
  mFakeWidget = fakeWidget;

  mThread->start();
  mGraphicsView->showNormal();

  // Start native loop in order to get view opened and painted once
  // Will block CreateFastStartup function and
  // exit when symbols are loaded and Static UI shown
  mLoop.exec();

  return true;
}
