/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *		John C. Griggs <johng@corel.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsQApplication_h__
#define nsQApplication_h__

#include "nsIEventQueue.h"
#include <qapplication.h>
#include <qsocketnotifier.h>

class nsQtEventQueue : public QObject
{
  Q_OBJECT
public:
  nsQtEventQueue(nsIEventQueue *EQueue);
  ~nsQtEventQueue();
  unsigned long IncRefCnt();
  unsigned long DecRefCnt();

public slots:
  void DataReceived();

private:
  nsIEventQueue   *mEventQueue;
  QSocketNotifier *mQSocket;
  PRUint32        mRefCnt;
  PRInt32         mID;
};

class nsQApplication : public QApplication
{
  Q_OBJECT
public:
  static nsQApplication *Instance(int argc,char **argv);

  static void Release();

  static void AddEventProcessorCallback(nsIEventQueue *EQueue);
  static void RemoveEventProcessorCallback(nsIEventQueue *EQueue);

  static QWidget *GetMasterWidget();

  ///Hook for debugging X11 Events
  bool x11EventFilter(XEvent *event);

protected:
  nsQApplication(int argc,char **argv);
  ~nsQApplication();

private:
  PRInt32                         mID;
  static QIntDict<nsQtEventQueue> mQueueDict;
  static nsQApplication           *mInstance;
  static PRUint32                 mRefCnt;
  static QWidget                  *mMasterWidget;
};

#endif // nsQApplication_h__
