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

#include "nsIGenericFactory.h"

#include "nsGopherHandler.h"
#include "nsFtpProtocolHandler.h"
#include "nsFingerHandler.h"
#include "nsDateTimeHandler.h"
#include "nsViewSourceHandler.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsFtpProtocolHandler, Init);

///////////////////////////////////////////////////////////////////////////////
// Module implementation for the net library

static nsModuleComponentInfo gNetModuleInfo[] = {
    //gopher:
    { "The Gopher Protocol Handler", 
      NS_GOPHERHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "gopher",
      nsGopherHandler::Create
    },

    // from netwerk/protocol/ftp:
    { "The FTP Protocol Handler", 
      NS_FTPPROTOCOLHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "ftp",
      nsFtpProtocolHandlerConstructor
    },

    // from netwerk/protocol/finger:
    { "The Finger Protocol Handler", 
      NS_FINGERHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "finger",
      nsFingerHandler::Create
    },

    // from netwerk/protocol/datetime:
    { "The DateTime Protocol Handler", 
      NS_DATETIMEHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "datetime",
      nsDateTimeHandler::Create
    },

    // from netwerk/protocol/viewsource:
    { "The ViewSource Protocol Handler", 
      NS_VIEWSOURCEHANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "view-source",
      nsViewSourceHandler::Create
    }
};

NS_IMPL_NSGETMODULE(necko_secondary_protocols, gNetModuleInfo)
