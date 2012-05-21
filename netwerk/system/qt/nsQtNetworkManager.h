/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSQTNETWORKMANAGER_H_
#define NSQTNETWORKMANAGER_H_

#include <QNetworkConfigurationManager>
#include <QObject>
#include <QTimer>
#include <QNetworkConfiguration>
#include <QNetworkSession>
#include "nscore.h"

class nsQtNetworkManager;



class nsQtNetworkManager : public QObject
{
  Q_OBJECT
  public:
    static void create();
    static void destroy();
    virtual ~nsQtNetworkManager();

    static nsQtNetworkManager* get() { return gQtNetworkManager; }

    static bool IsConnected();
    static bool GetLinkStatusKnown();
    static void enableInstance();
    bool openConnection(const QString&);
    bool isOnline();
  signals:
    void openConnectionSignal();

  public slots:
    void closeSession();
    void onlineStateChanged(bool);

  private slots:
    void openSession();

  private:
    explicit nsQtNetworkManager(QObject* parent = 0);

    static nsQtNetworkManager* gQtNetworkManager;
    QNetworkSession* networkSession;
    QNetworkConfiguration networkConfiguration;
    QNetworkConfigurationManager networkConfigurationManager;
    QTimer mBlockTimer;
    bool mOnline;
};

#endif /* NSQTNETWORKMANAGER_H_ */

