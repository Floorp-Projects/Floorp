/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Judson Valeski
 */

#include "nsIGenericFactory.h"
#include "nsMIMEService.h"
#include "nsXMLMIMEDataSource.h"
#include "nsMIMEInfoImpl.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMIMEInfoImpl);

static nsModuleComponentInfo gResComponents[] = {
    { "The MIME mapping service", 
      NS_MIMESERVICE_CID,
      "@mozilla.org/mime;1",
      nsMIMEService::Create
    },
       { "xml mime datasource", 
      NS_XMLMIMEDATASOURCE_CID,
      NS_XMLMIMEDATASOURCE_CONTRACTID,
      nsXMLMIMEDataSource::Create
    },
        { "xml mime INFO", 
      NS_MIMEINFO_CID,
      NS_MIMEINFO_CONTRACTID,
      nsMIMEInfoImplConstructor
    },
};

NS_IMPL_NSGETMODULE("nsMIMEService", gResComponents)
