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

/*
 *  ipluginw main header.
 */

#ifndef __nsInnoTekPluginWrapper_h__
#define __nsInnoTekPluginWrapper_h__

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Valid Pointer? */
#define VALID_PTR(pv)   \
    (   ((unsigned)(pv)) >= (unsigned)0x10000L    /* 64KB */ \
     && ((unsigned)(pv)) <  (unsigned)0xc0000000L /* 3GB */  \
        )

/** Valid Reference? */
#define VALID_REF(ref)  VALID_PTR(&(ref))

/** Debug printf */
#undef dprintf
#ifdef DEBUG
    #define dprintf(a)      npdprintf a
#else
    #define dprintf(a)      do { } while (0)
#endif

/** Debug Interrupt. */
#ifdef DEBUG
    #define DebugInt3()     asm("int $3")
#else
    #define DebugInt3()     do { } while (0)
#endif

/** Exception chain verify - debug only. */
#ifdef DEBUG
    #define VERIFY_EXCEPTION_CHAIN()     npVerifyExcptChain()
#else
    #define VERIFY_EXCEPTION_CHAIN()     do { } while (0)
#endif


/*******************************************************************************
*   Functions                                                                  *
*******************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif
int     npdprintf(const char *pszFormat, ...);
void _Optlink   ReleaseInt3(unsigned uEAX, unsigned uEDX, unsigned uECX);
#ifndef INCL_DEBUGONLY
void    npXPCOMInitSems();
nsresult npXPCOMGenericGetFactory(nsIServiceManagerObsolete *aServMgr, REFNSIID aClass, const char *aClassName,
                                  const char *aContractID, PRLibrary * aLibrary, nsIPlugin **aResult);
nsresult npXPCOMGenericMaybeWrap(REFNSIID aIID, nsISupports *aIn, nsISupports **aOut);
#endif

#ifdef __cplusplus
}
nsresult            downCreateWrapper(void **ppvResult, const void *pvVFT, nsresult rc);
nsresult            downCreateWrapper(void **ppvResult, REFNSIID aIID, nsresult rc);
extern "C" {
nsresult            upCreateWrapper(void **ppvResult, REFNSIID aIID, nsresult rc);
const char *        getIIDCIDName(REFNSIID aIID);
const nsID *        getIIDCIDFromName(const char *pszStrID);
#endif

#ifdef __cplusplus
}
#endif
#endif
