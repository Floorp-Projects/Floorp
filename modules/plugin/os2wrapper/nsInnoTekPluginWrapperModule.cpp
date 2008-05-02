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
 * The Original Code is InnoTek Plugin Wrapper code.
 *
 * The Initial Developer of the Original Code is
 * InnoTek Systemberatung GmbH.
 * Portions created by the Initial Developer are Copyright (C) 2003-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   InnoTek Systemberatung GmbH / Knut St. Osmundsen
 *   Peter Weilbacher <mozilla@weilbacher.org>
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

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** define this to make a completely freestanding module (i.e. no XPCOM imports).
 * This is handy when making new builds for existing browsers since this
 * doesn't require to have the .libs for the browser in question.
 *
 * - bird 2004-11-02: This must be enabled to support 1.7 / 1.8+!
 */
#ifndef _NO_XPCOMDLL_
# ifdef IPLUGINW_OUTOFTREE
#  define _NO_XPCOMDLL_ 1
# endif
#endif
/** define this to have minimal dependencies on XPCOM. */
#ifndef _MINIMAL_XPCOMDLL_
# ifdef _NO_XPCOMDLL_
#  define _MINIMAL_XPCOMDLL_ 1
# endif
#endif


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifdef _NO_XPCOMDLL_
#define INCL_BASE
#include <os2.h>
#endif
#ifdef DEBUG
#include <stdio.h>
#endif
#include "nsCOMPtr.h"
#include "nsIPlugin.h"
#include "nsIGenericFactory.h"
#include "nsILegacyPluginWrapperOS2.h"
#include "nsInnoTekPluginWrapper.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static NS_DEFINE_CID(kPluginCID, NS_PLUGIN_CID);
static NS_DEFINE_CID(kSupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kLegacyPluginWrapperIID, NS_ILEGACYPLUGINWRAPPEROS2_IID);


/**
 * Implements a plugin wrapper factory.
 */
class nsInnoTekPluginWrapper : public nsILegacyPluginWrapperOS2
{
#ifdef _MINIMAL_XPCOMDLL_
public:
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt) AddRef(void);
    NS_IMETHOD_(nsrefcnt) Release(void);

protected:
    nsAutoRefCnt mRefCnt;
#else
    NS_DECL_ISUPPORTS
#endif

public:
    nsInnoTekPluginWrapper();
    static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    /**
     * PR_FindSymbol(,"NSGetFactory") + NSGetFactory().
     */
    /* void getFactory (in nsIServiceManagerObsolete aServMgr, in REFNSIID aClass, in string aClassName, in string aContractID, in PRLibraryPtr aLibrary, out nsIPlugin aResult); */
    NS_IMETHOD GetFactory(nsIServiceManagerObsolete *aServMgr, REFNSIID aClass, const char *aClassName, const char *aContractID, PRLibrary * aLibrary, nsIPlugin **aResult);

    /**
     * Create a wrapper for the given interface if it's a legacy interface.
     * @returns NS_OK on success.
     * @returns NS_ERROR_INTERFACE if aIID isn't supported. aOut is nsnull.
     * @returns NS_ERROR_FAILURE on other error. aOut undefined.
     * @param   aIID    Interface Identifier of aIn and aOut.
     * @param   aIn     Interface of type aIID which may be a legacy interface
     *                  requiring a wrapper.
     * @param   aOut    The native interface.
     *                  If aIn is a legacy interface, this will be a wrappre.
     *                  If aIn is not a legacy interface, this is aIn.
     */
    /* void maybeWrap (in REFNSIID aIID, in nsISupports aIn, out nsISupports aOut); */
    NS_IMETHOD MaybeWrap(REFNSIID aIID, nsISupports *aIn, nsISupports **aOut);
};



#ifdef _MINIMAL_XPCOMDLL_
NS_IMETHODIMP_(nsrefcnt) nsInnoTekPluginWrapper::AddRef(void)
{
    return ++mRefCnt;
}

NS_IMETHODIMP_(nsrefcnt) nsInnoTekPluginWrapper::Release(void)
{
    --mRefCnt;
    if (mRefCnt == 0)
    {
        mRefCnt = 1; /* stabilize */
        delete this;
        return 0;
    }
    return mRefCnt;
}

NS_IMETHODIMP nsInnoTekPluginWrapper::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (aIID.Equals(kLegacyPluginWrapperIID))
    {
        *aInstancePtr = static_cast< nsILegacyPluginWrapperOS2 * > (this);
        AddRef();
        return NS_OK;
    }

    if (aIID.Equals(kSupportsIID))
    {
        *aInstancePtr = static_cast< nsISupports * > (this);
        AddRef();
        return NS_OK;
    }

    *aInstancePtr = nsnull;
    return NS_ERROR_NO_INTERFACE;
}

#else
NS_IMPL_ISUPPORTS1(nsInnoTekPluginWrapper, nsILegacyPluginWrapperOS2)
#endif



/** Factory Constructor Object - do nothing. */
nsInnoTekPluginWrapper::nsInnoTekPluginWrapper()
{
    /* Create semaphores at a safe time */
    npXPCOMInitSems();
}

/**
 * Create the factory.
 * @returns NS_OK on success, with aResult containing the requested interface.
 * @returns NS_ERROR_* on failure, aResult is nsnull.
 */
nsresult nsInnoTekPluginWrapper::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    *aResult = nsnull;
#ifndef _MINIMAL_XPCOMDLL_
    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
#endif
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

#ifdef _MINIMAL_XPCOMDLL_
    nsILegacyPluginWrapperOS2 *
#else
    nsCOMPtr<nsILegacyPluginWrapperOS2>
#endif
        factory  = new nsInnoTekPluginWrapper();
    if (!factory)
        return NS_ERROR_OUT_OF_MEMORY;

    return factory->QueryInterface(aIID, aResult);
}


/**
 * This is where we create the initial wrapper.
 */
NS_IMETHODIMP nsInnoTekPluginWrapper::GetFactory(nsIServiceManagerObsolete *aServMgr,
                                                 REFNSIID aClass,
                                                 const char *aClassName,
                                                 const char *aContractID,
                                                 PRLibrary * aLibrary,
                                                 nsIPlugin **aResult)
{
    nsresult rc;
    dprintf(("nsInnoTekPluginWrapper::CreatePlugin: enter"));

    /*
     * Do NSGetFactory.
     */
    rc = npXPCOMGenericGetFactory(aServMgr, aClass, aClassName, aContractID, aLibrary, aResult);
    dprintf(("nsInnoTekPluginWrapper::CreatePlugin: npXPCOMGenericGetFactory -> rc=%d and *aResult=%x",
             rc, *aResult));

    return rc;
}


/**
 * Create a wrapper for the given interface if it's a legacy interface.
 * @returns NS_OK on success.
 * @returns NS_ERROR_NO_INTERFACE if aIID isn't supported. aOut is nsnull.
 * @returns NS_ERROR_FAILURE on other error. aOut undefined.
 * @param   aIID    Interface Identifier of aIn and aOut.
 * @param   aIn     Interface of type aIID which may be a legacy interface
 *                  requiring a wrapper.
 * @param   aOut    The native interface.
 *                  If aIn is a legacy interface, this will be a wrappre.
 *                  If aIn is not a legacy interface, this is aIn.
 * @remark  Typically used for the flash plugin.
 */
NS_IMETHODIMP nsInnoTekPluginWrapper::MaybeWrap(REFNSIID aIID, nsISupports *aIn, nsISupports **aOut)
{
   return npXPCOMGenericMaybeWrap(aIID, aIn, aOut);
}




/** Component Info */
static const nsModuleComponentInfo gComponentInfo[] =
{
    {
        "InnoTek Legacy XPCOM Plugin Wrapper",
        NS_LEGACY_PLUGIN_WRAPPER_CID,
        NS_LEGACY_PLUGIN_WRAPPER_CONTRACTID,
        nsInnoTekPluginWrapper::Create
    }
};



/** NSGetModule(); */
NS_IMPL_NSGETMODULE(nsInnoTekPluginWrapperModule, gComponentInfo)


#ifdef _NO_XPCOMDLL_
/**
 * Find the start of the filename.
 *
 * @returns pointer to start of filename.
 * @param   pszPath Path to examin.
 */
static char *GetBasename(char *pszPath)
{
    char *pszName;
    char *psz;

    pszName = strrchr(pszPath, '\\');
    psz = strrchr(pszName ? pszName : pszPath, '/');
    if (psz)
        pszName = psz;
    if (!pszName)
        pszName = strchr(pszPath, ':');
    if (pszName)
        pszName++;
    else
        pszName = pszPath;

    return pszName;
}

nsresult NS_NewGenericModule2(nsModuleInfo *info, nsIModule* *result)
{
    HMODULE     hmod;
    nsresult    rc = NS_ERROR_UNEXPECTED;
    APIRET      rc2;

    /*
     * Because of the LIBPATHSTRICT feature we have to specific the full path
     * of the DLL to make sure we get the right handle.
     * (We're of course making assumptions about the executable and the XPCOM.DLL
     * begin at the same location.)
     */
    char    szXPCOM[CCHMAXPATH];
    char *  pszName;
    PPIB    ppib;
    PTIB    ptib;
    DosGetInfoBlocks(&ptib, &ppib);
    pszName = GetBasename(strcpy(szXPCOM, ppib->pib_pchcmd));
    strcpy(pszName, "XPCOM.DLL");
    rc2 = DosLoadModule(NULL, 0, (PCSZ)szXPCOM, &hmod);
    if (!rc2)
    {
        NS_COM nsresult (* pfnNS_NewGenericModule2)(nsModuleInfo *info, nsIModule* *result);

        /* demangled name: NS_NewGenericModule2(nsModuleInfo*, nsIModule**) */
        rc2 = DosQueryProcAddr(hmod, 0, (PCSZ)"__Z20NS_NewGenericModule2P12nsModuleInfoPP9nsIModule",
                               (PFN*)&pfnNS_NewGenericModule2);
        if (rc2)
            /* demangled name: NS_NewGenericModule2(const nsModuleInfo*, nsIModule**) */
            rc2 = DosQueryProcAddr(hmod, 0, (PCSZ)"__Z20NS_NewGenericModule2PK12nsModuleInfoPP9nsIModule",
                                   (PFN*)&pfnNS_NewGenericModule2);
        if (rc2)
        {
            /* The mozilla guys have splitt up XPCOM.DLL in 1.8 just to make life difficult. :-) */
            DosFreeModule(hmod);
            strcpy(pszName, "XPCOMCOR.DLL");
            rc2 = DosLoadModule(NULL, 0, (PCSZ)szXPCOM, &hmod);
            if (!rc2)
            {
                rc2 = DosQueryProcAddr(hmod, 0, (PCSZ)"__Z20NS_NewGenericModule2P12nsModuleInfoPP9nsIModule",
                                       (PFN*)&pfnNS_NewGenericModule2);
                if (rc2)
                    /* demangled name: NS_NewGenericModule2(const nsModuleInfo*, nsIModule**) */
                    rc2 = DosQueryProcAddr(hmod, 0, (PCSZ)"__Z20NS_NewGenericModule2PK12nsModuleInfoPP9nsIModule",
                                           (PFN*)&pfnNS_NewGenericModule2);
            }
        }
        if (!rc2)
             rc = pfnNS_NewGenericModule2(info, result);
        #ifdef DEBUG
        else
            fprintf(stderr, "ipluginw: DosQueryProcAddr -> %ld\n", rc2);
        #endif
        DosFreeModule(hmod);
    }
    #ifdef DEBUG
    else
        fprintf(stderr, "ipluginw: DosLoadModule -> %ld\n", rc2);
    if (rc)
        fprintf(stderr, "ipluginw: NS_NewGenericModule2 -> %#x\n", rc);
    #endif

    return rc;
}
#endif


