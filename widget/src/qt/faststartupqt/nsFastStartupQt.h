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

#ifndef FAST_STARTUP_H
#define FAST_STARTUP_H

#include <QObject>
#include "nscore.h"
#include <QThread>
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
};

#endif
