/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rusty Lynch <rusty.lynch@intel.com>
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
/*
 * Implements the SANE plugin factory class.
 */

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsSanePlugin_CID.h"
#include "nsSanePlugin.h"
#include "nsSanePluginFactory.h"
#include "plstr.h"

#define PLUGIN_NAME             "SANE Plugin"
#define PLUGIN_DESCRIPTION      "SANE Plugin is a generic scanner interface"
#define PLUGIN_MIME_DESCRIPTION "application/X-sane-plugin::Scanner/Camera"
#define PLUGIN_MIME_TYPE        "application/X-sane-plugin"

static NS_DEFINE_IID( kISupportsIID,            NS_ISUPPORTS_IID           );
static NS_DEFINE_IID( kIFactoryIID,             NS_IFACTORY_IID            );
static NS_DEFINE_IID( kIPluginIID,              NS_IPLUGIN_IID             );
static NS_DEFINE_CID( kComponentManagerCID,     NS_COMPONENTMANAGER_CID    );
static NS_DEFINE_CID( knsSanePluginControlCID,  NS_SANE_PLUGIN_CONTROL_CID );
static NS_DEFINE_CID( knsSanePluginInst,        NS_SANE_PLUGIN_CID         );

////////////////////////////////////////////////////////////////////////

nsSanePluginFactoryImpl::nsSanePluginFactoryImpl( const nsCID &aClass,
                                                  const char* className,
                                                  const char* contractID )
    : mClassID(aClass), mClassName(className), mContractID(contractID)
{
#ifdef DEBUG
    printf("nsSanePluginFactoryImpl::nsSanePluginFactoryImpl()\n");
#endif
}

nsSanePluginFactoryImpl::~nsSanePluginFactoryImpl()
{
#ifdef DEBUG
    printf("nsSanePluginFactoryImpl::~nsSanePluginFactoryImpl()\n");
#endif

    printf("mRefCnt = %i\n", mRefCnt);
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMETHODIMP
nsSanePluginFactoryImpl::QueryInterface(const nsIID &aIID, 
                                        void **aInstancePtr)
{
#ifdef DEBUG
    printf("nsSanePluginFactoryImpl::QueryInterface()\n");
#endif

    if (!aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsISupports*,this);
    } else if (aIID.Equals(kIFactoryIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                       NS_STATIC_CAST(nsIFactory*,this));
    } else if (aIID.Equals(kIPluginIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsISupports*,
                                       NS_STATIC_CAST(nsIPlugin*,this));
    } else {
        *aInstancePtr = nsnull;
        return NS_ERROR_NO_INTERFACE;
    }

    NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*,*aInstancePtr));

    return NS_OK;
}

// Standard implementation of AddRef and Release
NS_IMPL_ADDREF( nsSanePluginFactoryImpl )
NS_IMPL_RELEASE( nsSanePluginFactoryImpl )

NS_IMETHODIMP
nsSanePluginFactoryImpl::CreateInstance( nsISupports *aOuter,
                                         const nsIID &aIID,
                                         void **aResult)
{
#ifdef DEBUG
    printf("nsSanePluginFactoryImpl::CreateInstance()\n");
#endif

    if ( !aResult )
        return NS_ERROR_NULL_POINTER;
  
    if ( aOuter )
        return NS_ERROR_NO_AGGREGATION;

    nsSanePluginInstance * inst = new nsSanePluginInstance();
    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;
           
    inst->AddRef();
    *aResult = inst;
    return NS_OK;
}

nsresult 
nsSanePluginFactoryImpl::LockFactory(PRBool aLock)
{
#ifdef DEBUG
    printf("nsSanePluginFactoryImpl::LockFactory()\n");
#endif

    // Needs to be implemented

    return NS_OK;
}


NS_METHOD
nsSanePluginFactoryImpl::Initialize()
{
#ifdef DEBUG
    printf("nsSanePluginFactoryImpl::Initialize()\n");
#endif

    return NS_OK;
}


NS_METHOD
nsSanePluginFactoryImpl::Shutdown( void )
{
#ifdef DEBUG
    printf("nsSanePluginFactoryImpl::Shutdown()\n");
#endif

    return NS_OK;
}


NS_METHOD 
nsSanePluginFactoryImpl::GetMIMEDescription(const char* *result)
{
#ifdef DEBUG
    printf("nsSanePluginFactoryImpl::GetMIMEDescription()\n");
#endif

    // caller is responsible for releasing
    *result = PL_strdup( PLUGIN_MIME_DESCRIPTION );

    return NS_OK;
}

NS_METHOD
nsSanePluginFactoryImpl::GetValue( nsPluginVariable variable, void *value )
{
#ifdef DEBUG
    printf("nsSanePluginFactoryImpl::GetValue()\n");
#endif

    nsresult err = NS_OK;

    if ( variable == nsPluginVariable_NameString ) {
    
        *( ( char ** )value ) = strdup( PLUGIN_NAME );

    } else if ( variable == nsPluginVariable_DescriptionString ) {

        *( ( char ** )value ) = strdup( PLUGIN_DESCRIPTION );

    } else {
    
        err = NS_ERROR_FAILURE;

    }
  
    return err;
}


NS_IMETHODIMP 
nsSanePluginFactoryImpl::CreatePluginInstance( nsISupports *aOuter, 
                                               REFNSIID aIID, 
                                               const char* aPluginMIMEType,
                                               void **aResult)
{
#ifdef DEBUG
    printf("nsSanePluginFactoryImpl::CreatePluginInstance()\n");
#endif

    // Need to find out what this is used for.  The npsimple
    // plugin looks like it just does a CreateInstance and 
    // ignores the mime type.
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////

/**
 * The XPCOM runtime will call this to get a new factory object for the
 * CID/contractID it passes in.  XPCOM is responsible for caching the resulting
 * factory.
 *
 * return the proper factory to the caller
 */
extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory( nsISupports* aServMgr,
              const nsCID &aClass,
              const char *aClassName,
              const char *aContractID,
              nsIFactory **aFactory)
{
    if (! aFactory)
        return NS_ERROR_NULL_POINTER;
  
    nsSanePluginFactoryImpl* factory = new nsSanePluginFactoryImpl(aClass, 
                                                                   aClassName,
                                                                   aContractID);
    if ( factory == nsnull )
        return NS_ERROR_OUT_OF_MEMORY;
  
    NS_ADDREF(factory);
    *aFactory = factory;
    return NS_OK;

}

char *buf;

extern "C" PR_IMPLEMENT( nsresult )
NSRegisterSelf( nsISupports* aServMgr, const char* aPath )
{
    nsresult rv;
  
    nsCOMPtr<nsIServiceManager> servMgr( do_QueryInterface( aServMgr, &rv ) );
    if ( NS_FAILED( rv ) )
        return rv;
  
    nsCOMPtr<nsIComponentManager> compMgr = 
             do_GetService( kComponentManagerCID, &rv );
    if ( NS_FAILED( rv ) )
        return rv;
  
    // Register the plugin control portion.
    rv = compMgr->RegisterComponent(knsSanePluginControlCID,
                                    "SANE Plugin Control",
                                    "@mozilla.org/plugins/sane-control;1",
                                    aPath, PR_TRUE, PR_TRUE );
  
    // Register the plugin portion.
    nsString contractID;
    contractID.AssignWithConversion( NS_INLINE_PLUGIN_CONTRACTID_PREFIX );

    contractID.AppendWithConversion(PLUGIN_MIME_TYPE);
    buf = ( char * )calloc( 2000, sizeof( char ) );
    contractID.ToCString( buf, 1999 );
  
    rv = compMgr->RegisterComponent( knsSanePluginInst,
                                     "SANE Plugin Component",
                                     buf,
                                     aPath, PR_TRUE, PR_TRUE);
    free( buf );
  
    if ( NS_FAILED( rv ) )
        return rv;
  
    return NS_OK;

}

extern "C" PR_IMPLEMENT( nsresult )
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;
  
    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;
  
    nsCOMPtr<nsIComponentManager> compMgr = 
             do_GetService(kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;
  
    rv = compMgr->UnregisterComponent(knsSanePluginControlCID, aPath);
    if (NS_FAILED(rv)) return rv;
  
    return NS_OK;
}


















