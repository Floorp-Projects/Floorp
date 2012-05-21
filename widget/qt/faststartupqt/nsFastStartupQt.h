/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FAST_STARTUP_H
#define FAST_STARTUP_H

#include <QObject>
#include "nscore.h"
#include <QThread>
#include <QEventLoop>
#include <sys/time.h>

class QGraphicsView;
class MozMGraphicsView;
class MozQGraphicsView;
class QGraphicsWidget;
class IMozQWidget;
class QWidget;

#if defined MOZ_ENABLE_MEEGOTOUCH
typedef MozMGraphicsView MozGraphicsView;
#else
typedef MozQGraphicsView MozGraphicsView;
#endif

class nsFastStartup;
typedef bool (*GeckoLoaderFunc)(const char* execPath);
class GeckoThread : public QThread
{
  Q_OBJECT

public:
  void run();
  void SetLoader(GeckoLoaderFunc aFunc, const char* execPath)
  {
    mExecPath = execPath;
    mFunc = aFunc;
  }

Q_SIGNALS:
  void symbolsLoadingFinished(bool);

private:
  const char* mExecPath;
  GeckoLoaderFunc mFunc;
};

class NS_EXPORT nsFastStartup : public QObject
{
  Q_OBJECT

public:
  static nsFastStartup* GetSingleton();
  // Create new or get QGraphicsView which could have been created for Static UI
  static MozGraphicsView* GetStartupGraphicsView(QWidget* parentWidget, IMozQWidget* aTopChild);
  nsFastStartup();
  virtual ~nsFastStartup();
  virtual bool CreateFastStartup(int& argc, char ** argv,
                                 const char* execPath,
                                 GeckoLoaderFunc aFunc);
  // Called when first real mozilla paint happend
  void RemoveFakeLayout();
  // Final notification that Static UI show and painted
  void painted();

protected slots:
    void symbolsLoadingFinished(bool);

private:
  MozGraphicsView* mGraphicsView;
  QGraphicsWidget* mFakeWidget;
  bool mSymbolsLoaded;
  bool mWidgetPainted;
  GeckoThread* mThread;
  QEventLoop mLoop;
};

#endif
