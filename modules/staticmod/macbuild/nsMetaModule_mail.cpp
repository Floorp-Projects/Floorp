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
 * Contributor(s): 
 *   Christopher Seawood <cls@seawood.org> 
 *   Doug Turner <dougt@netscape.com>
 */

#include "nsError.h"
#include "nsIModule.h"
#include "nsIFile.h"
#include "nsIGenericFactory.h"
#include "prmem.h"

#define META_DESTRUCTOR_FUNC MetaModuleDestructorMail

#define NS_METAMODULE_NAME "nsMetaModuleMail"
#define NS_METAMODULE_DESC "Meta Component Mail"
#define NS_METAMODULE_CID \
{ 0xda364d3c, 0x1dd1, 0x11b2, { 0xbd, 0xfd, 0x9d, 0x6f, 0xb7, 0x55, 0x30, 0x35 } }
#define NS_METAMODULE_CONTRACTID "@mozilla.org/metamodule_mail;1"


/*
 * NO USER EDITABLE PORTIONS AFTER THIS POINT
 *
 */

#define REGISTER_MODULE_USING(mod) { \
    nsCOMPtr<nsIModule> module; \
    mod##(aCompMgr, aPath, getter_AddRefs(module)); \
    module->RegisterSelf(aCompMgr, aPath, aRegistryLocation, aComponentType); \
}

struct nsModuleComponentInfoContainer {
    nsModuleComponentInfo		*list;
    PRUint32                    count;
};

 extern "C" nsresult nsMsgBaseModule_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  nsMsgBaseModule_NSGM_comps; extern PRUint32 nsMsgBaseModule_NSGM_comp_count;
 extern "C" nsresult IMAP_factory_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  IMAP_factory_NSGM_comps; extern PRUint32 IMAP_factory_NSGM_comp_count;
 extern "C" nsresult nsVCardModule_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  nsVCardModule_NSGM_comps; extern PRUint32 nsVCardModule_NSGM_comp_count;
 extern "C" nsresult mime_services_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  mime_services_NSGM_comps; extern PRUint32 mime_services_NSGM_comp_count;
 extern "C" nsresult nsMimeEmitterModule_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  nsMimeEmitterModule_NSGM_comps; extern PRUint32 nsMimeEmitterModule_NSGM_comp_count;
 extern "C" nsresult nsMsgNewsModule_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  nsMsgNewsModule_NSGM_comps; extern PRUint32 nsMsgNewsModule_NSGM_comp_count;
 extern "C" nsresult nsMsgComposeModule_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  nsMsgComposeModule_NSGM_comps; extern PRUint32 nsMsgComposeModule_NSGM_comp_count;
 extern "C" nsresult local_mail_services_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  local_mail_services_NSGM_comps; extern PRUint32 local_mail_services_NSGM_comp_count;
 extern "C" nsresult nsAbSyncModule_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  nsAbSyncModule_NSGM_comps; extern PRUint32 nsAbSyncModule_NSGM_comp_count;
 extern "C" nsresult nsImportServiceModule_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  nsImportServiceModule_NSGM_comps; extern PRUint32 nsImportServiceModule_NSGM_comp_count;
 extern "C" nsresult nsTextImportModule_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  nsTextImportModule_NSGM_comps; extern PRUint32 nsTextImportModule_NSGM_comp_count;
 extern "C" nsresult nsAbModule_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  nsAbModule_NSGM_comps; extern PRUint32 nsAbModule_NSGM_comp_count;
 extern "C" nsresult nsMsgDBModule_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj);
 extern nsModuleComponentInfo*  nsMsgDBModule_NSGM_comps; extern PRUint32 nsMsgDBModule_NSGM_comp_count;

static nsresult
NS_RegisterMetaModules(nsIComponentManager *aCompMgr,
                       nsIFile *aPath,
                       const char *aRegistryLocation,
                       const char *aComponentType)
{
    nsresult rv = NS_OK;
    
     REGISTER_MODULE_USING(nsMsgBaseModule_NSGetModule);
     REGISTER_MODULE_USING(IMAP_factory_NSGetModule);
     REGISTER_MODULE_USING(nsVCardModule_NSGetModule);
     REGISTER_MODULE_USING(mime_services_NSGetModule);
     REGISTER_MODULE_USING(nsMimeEmitterModule_NSGetModule);
     REGISTER_MODULE_USING(nsMsgNewsModule_NSGetModule);
     REGISTER_MODULE_USING(nsMsgComposeModule_NSGetModule);
     REGISTER_MODULE_USING(local_mail_services_NSGetModule);
     REGISTER_MODULE_USING(nsAbSyncModule_NSGetModule);
     REGISTER_MODULE_USING(nsImportServiceModule_NSGetModule);
     REGISTER_MODULE_USING(nsTextImportModule_NSGetModule);
     REGISTER_MODULE_USING(nsAbModule_NSGetModule);
     REGISTER_MODULE_USING(nsMsgDBModule_NSGetModule);
    
        {};
    
    return rv;
}

void META_DESTRUCTOR_FUNC(nsIModule *self, nsModuleComponentInfo *components)
{
    PR_Free(components);
}

class nsMetaModuleImpl : public nsISupports
{
public:
    nsMetaModuleImpl();
    virtual ~nsMetaModuleImpl();
    NS_DECL_ISUPPORTS
};

nsMetaModuleImpl::nsMetaModuleImpl()
{
}

nsMetaModuleImpl::~nsMetaModuleImpl()
{
}

NS_IMPL_ISUPPORTS1(nsMetaModuleImpl, nsISupports);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMetaModuleImpl)

static NS_METHOD nsMetaModuleRegistrationProc(nsIComponentManager *aCompMgr,
                                          nsIFile *aPath,
                                          const char *registryLocation,
                                          const char *componentType,
                                          const nsModuleComponentInfo *info)
{
    NS_RegisterMetaModules(aCompMgr, aPath, registryLocation, componentType);
    return NS_OK;
}

static NS_METHOD nsMetaModuleUnregistrationProc(nsIComponentManager *aCompMgr,
                                            nsIFile *aPath,
                                            const char *registryLocation,
                                            const nsModuleComponentInfo *info)
{
    return NS_OK;
}


static nsModuleComponentInfo components[] =
{
  { NS_METAMODULE_DESC,
    NS_METAMODULE_CID, 
    NS_METAMODULE_CONTRACTID, 
    nsMetaModuleImplConstructor,
    nsMetaModuleRegistrationProc,
    nsMetaModuleUnregistrationProc
  },
};

static nsModuleComponentInfoContainer componentsList[] = {
    { components, sizeof(components)/sizeof(components[0]) },
     { nsMsgBaseModule_NSGM_comps, nsMsgBaseModule_NSGM_comp_count },
     { IMAP_factory_NSGM_comps, IMAP_factory_NSGM_comp_count },
     { nsVCardModule_NSGM_comps, nsVCardModule_NSGM_comp_count },
     { mime_services_NSGM_comps, mime_services_NSGM_comp_count },
     { nsMimeEmitterModule_NSGM_comps, nsMimeEmitterModule_NSGM_comp_count },
     { nsMsgNewsModule_NSGM_comps, nsMsgNewsModule_NSGM_comp_count },
     { nsMsgComposeModule_NSGM_comps, nsMsgComposeModule_NSGM_comp_count },
     { local_mail_services_NSGM_comps, local_mail_services_NSGM_comp_count },
     { nsAbSyncModule_NSGM_comps, nsAbSyncModule_NSGM_comp_count },
     { nsImportServiceModule_NSGM_comps, nsImportServiceModule_NSGM_comp_count },
     { nsTextImportModule_NSGM_comps, nsTextImportModule_NSGM_comp_count },
     { nsAbModule_NSGM_comps, nsAbModule_NSGM_comp_count },
     { nsMsgDBModule_NSGM_comps, nsMsgDBModule_NSGM_comp_count }, 
    { nsnull, 0 }
};

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,      
                                          nsIFile* location,                 
                                          nsIModule** result)                
{                                                                            
    nsModuleComponentInfo *outList = nsnull;
    nsModuleComponentInfoContainer *inList = componentsList;
    PRUint32 count = 0, i = 0, k=0, msize = sizeof(nsModuleComponentInfo);

    while (inList[i].list != nsnull) {
        count += inList[i].count;
        i++;
    }

    outList = (nsModuleComponentInfo *) PR_Calloc(count, sizeof(nsModuleComponentInfo));

    i = 0; k =0;
    while (inList[i].list != nsnull) {
        memcpy(&outList[k], inList[i].list, msize * inList[i].count);
        k+= inList[i].count;
        i++;
    }
    return NS_NewGenericModule(NS_METAMODULE_NAME, count, outList, nsnull, result);
}





