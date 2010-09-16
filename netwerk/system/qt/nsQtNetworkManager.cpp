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

//order is important - mozilla redefine qt macros
#include <QNetworkConfigurationManager>
#include <QNetworkConfiguration>
#include <QNetworkSession>

#include "nsQtNetworkManager.h"

#include "nsCOMPtr.h"
#include "nsThreadUtils.h"

#include "nsINetworkLinkService.h"

#include "nsIOService.h"
#include "nsIObserverService.h"
#include "nsIOService.h"

#include "nsINetworkLinkService.h"

static QNetworkConfigurationManager* sNetworkConfig = 0;

PRBool
nsQtNetworkManager::OpenConnectionSync()
{
    if (!sNetworkConfig)
        return PR_FALSE;

    //do not request when we are online...
    if (sNetworkConfig->isOnline())
        return PR_FALSE;

    if (!(sNetworkConfig->capabilities() & QNetworkConfigurationManager::CanStartAndStopInterfaces))
        return PR_FALSE;

    // Is there default access point, use it
    QNetworkConfiguration default_cfg = sNetworkConfig->defaultConfiguration();

    if (!default_cfg.isValid())
    {
        NS_WARNING("default configuration is not valid. Looking for any other:");
        foreach (QNetworkConfiguration cfg, sNetworkConfig->allConfigurations())
        {
            if (cfg.isValid())
                default_cfg = cfg;
        }

        if (!default_cfg.isValid())
        {
            NS_WARNING("No valid configuration found. Giving up.");
            return PR_FALSE;
        }
    }

    //do use pointer here, it will be deleted after connected!
    //Creation on stack cause appearing issues and segfaults of the connectivity dialog
    QNetworkSession* session = new QNetworkSession(default_cfg);
    QObject::connect(session, SIGNAL(opened()),
                     session, SLOT(deleteLater()));
    QObject::connect(session, SIGNAL(error(QNetworkSession::SessionError)),
                     session, SLOT(deleteLater()));
    session->open();
    return session->waitForOpened(-1);
}

void
nsQtNetworkManager::CloseConnection()
{
    NS_WARNING("nsQtNetworkManager::CloseConnection() Not implemented by QtNetwork.");
}

PRBool
nsQtNetworkManager::IsConnected()
{
    NS_ASSERTION(sNetworkConfig, "Network not initialized");
    return sNetworkConfig->isOnline();
}

PRBool
nsQtNetworkManager::GetLinkStatusKnown()
{
    return IsConnected();
}

PRBool
nsQtNetworkManager::Startup()
{
    //Dont do it if already there
    if (sNetworkConfig)
        return PR_FALSE;

    sNetworkConfig = new QNetworkConfigurationManager();

    return PR_TRUE;
}

void
nsQtNetworkManager::Shutdown()
{
    delete sNetworkConfig;
    sNetworkConfig = nsnull;
}
