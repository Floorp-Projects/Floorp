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
 * The Original Code is Nokia Corporation Code.
 *
 * The Initial Developer of the Original Code is
 * Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jeremias Bosch <jeremias.bosch@gmail.com>
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

nsQtNetworkManager* nsQtNetworkManager::gQtNetworkManager = nsnull;

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
    gQtNetworkManager = nsnull;
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
        emit openConnectionSignal();
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
