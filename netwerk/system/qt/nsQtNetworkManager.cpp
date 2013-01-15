/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsQtNetworkManager.h"

#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "nsINetworkLinkService.h"
#include "nsIOService.h"
#include "nsIObserverService.h"
#include "nsIOService.h"

#include <QHostInfo>
#include <QHostAddress>
#include <QTime>

nsQtNetworkManager* nsQtNetworkManager::gQtNetworkManager = nullptr;

void nsQtNetworkManager::create()
{
    if (!gQtNetworkManager) {
        gQtNetworkManager = new nsQtNetworkManager();
        connect(gQtNetworkManager, SIGNAL(openConnectionSignal()),
                gQtNetworkManager, SLOT(openSession()),
                Qt::BlockingQueuedConnection);
        connect(&gQtNetworkManager->networkConfigurationManager,
                SIGNAL(onlineStateChanged(bool)), gQtNetworkManager,
                SLOT(onlineStateChanged(bool)));
    }
}

void nsQtNetworkManager::destroy()
{
    delete gQtNetworkManager;
    gQtNetworkManager = nullptr;
}

nsQtNetworkManager::nsQtNetworkManager(QObject* parent)
  : QObject(parent), networkSession(0)
{
    mOnline = networkConfigurationManager.isOnline();
    NS_ASSERTION(NS_IsMainThread(), "nsQtNetworkManager can only initiated in Main Thread");
}

nsQtNetworkManager::~nsQtNetworkManager()
{
    closeSession();
    networkSession->deleteLater();
}

bool
nsQtNetworkManager::isOnline()
{
    static bool sForceOnlineUSB = getenv("MOZ_MEEGO_NET_ONLINE") != 0;
    return sForceOnlineUSB || mOnline;
}

void
nsQtNetworkManager::onlineStateChanged(bool online)
{
    mOnline = online;
}

/*
  This function is called from different threads, we need to make sure that
  the attempt to create a network connection is done in the mainthread

  In case this function is called by another thread than the mainthread
  we emit a signal which is connected through "BlockingQueue"-Connection Type.

  This cause that the current thread is blocked and waiting for the result.

  Of course, in case this gets called from the mainthread we must not emit the signal,
  but call the slot directly.
*/

bool
nsQtNetworkManager::openConnection(const QString& host)
{
    // we are already online -> return true.
    if (isOnline()) {
        return true;
    }

    if (NS_IsMainThread()) {
        openSession();
    } else {
        // jump to mainthread and do the work there
        Q_EMIT openConnectionSignal();
    }

    // if its claiming its online -> send one resolve request ahead.
    // this is important because on mobile the first request can take up to 10 seconds
    // sending here one will help to avoid trouble and timeouts later
    if (isOnline()) {
        QHostInfo::fromName(host);
    }

    return isOnline();
}

void
nsQtNetworkManager::openSession()
{
    if (mBlockTimer.isActive()) {
        // if openSession is called within 200 ms again, we forget this attempt since
        // its mostlike an automatic connection attempt which was not successful or canceled 200ms ago.
        // we reset the timer and start it again.

        // As example: Go in firefox mobile into AwesomeView, see that the icons are not always cached and
        // get loaded on the fly. Not having the 200 ms rule here would mean that instantly
        // after the user dismissed the one connection dialog once another
        // would get opened. The user will never be able to close / leave the view until each such attempt is gone through.

        // Basically 200 ms are working fine, its huge enough for automatic attempts to get covered and small enough to
        // still be able to react on direct user request.

        mBlockTimer.stop();
        mBlockTimer.setSingleShot(true);
        mBlockTimer.start(200);
        return;
    }

    if (isOnline()) {
        return;
    }

    // this only means we did not shutdown before...
    // renew Session every time
    // fix/workaround for prestart bug
    if (networkSession) {
        networkSession->close();
        networkSession->deleteLater();
    }

    // renew always to react on changed Configurations
    networkConfigurationManager.updateConfigurations();
    // go always with default configuration
    networkConfiguration = networkConfigurationManager.defaultConfiguration();
    networkSession = new QNetworkSession(networkConfiguration);

    networkSession->open();
    QTime current;
    current.start();
    networkSession->waitForOpened(-1);

    if (current.elapsed() < 1000) {
        NS_WARNING("Connection Creation was to fast, something is not right.");
    }

    mBlockTimer.setSingleShot(true);
    mBlockTimer.start(200);
}

void
nsQtNetworkManager::closeSession()
{
    if (networkSession) {
        networkSession->close();
    }
}
