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
 *  XPCOM Plugin Interface
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
# ifdef IPLUGINW_OUTOFTREE
#  ifdef __IBMCPP__
#   include "moz_VACDefines.h"
#  else
#   include "moz_GCCDefines.h"
#  endif
# endif


#define INCL_DOSSEMAPHORES
#define INCL_DOSMODULEMGR
#define INCL_ERRORS
#define INCL_PM
#include <os2.h>
#include <float.h>


/* the headers */
#include "npapi.h"

#include "nscore.h"
#include "nsError.h"
#include "nsDebug.h"
#include "nsID.h"
#include "nsrootidl.h"
#include "nsISupports.h"
#include "nsISupportsBase.h"
#include "nsISupportsImpl.h"
#include "nsISupportsUtils.h"
#include "nsIClassInfo.h"
#include "nsIFile.h"
#include "nsIEnumerator.h"
#include "nsCOMPtr.h"

#include "nsIServiceManager.h"
#include "nsIServiceManagerObsolete.h"
#include "nsIComponentManagerObsolete.h"
#include "nsIComponentManager.h"
#include "nsIFactory.h"
#include "nsIMemory.h"
#include "nsIOutputStream.h"
#include "nsIRequestObserver.h"
#include "nsIStreamListener.h"
#include "nspluginroot.h"
#include "nsplugindefs.h"
#include "nsIPlugin.h"
#include "nsIPluginManager.h"
#include "nsIEventHandler.h"
#include "nsIPluginManager2.h"
#include "nsIPluginStreamInfo.h"
#include "nsIPluginStreamListener.h"
#include "nsIPluginInstance.h"
#include "nsIPluginInstancePeer.h"
#include "nsIPluginInstancePeer2.h"
#include "nsIPluginTagInfo.h"
#include "nsIPluginTagInfo2.h"
#include "nsICookieStorage.h"
#include "nsIHTTPHeaderListener.h"

#include "nsIJRIPlugin.h"
#include "nsIJVMPluginInstance.h"
#include "nsIJVMManager.h"
#include "nsIJVMWindow.h"
#include "nsIJVMConsole.h"
#include "nsISerializable.h"
#include "nsIPrincipal.h"
#include "nsIJVMPlugin.h"
#include "nsIJVMPluginTagInfo.h"
#include "nsIJVMPrefsWindow.h"
#include "nsILiveConnectManager.h"
#include "nsISecurityContext.h"
#include "nsILiveConnect.h"
#include "nsISecureLiveConnect.h"
#include "nsISecureEnv.h"
#include "nsISymantecDebugger.h"
#include "nsISymantecDebugManager.h"
#include "nsIJVMThreadManager.h"
#include "nsIRunnable.h"
#include "nsIJVMConsole.h"

#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"

#ifdef IPLUGINW_OUTOFTREE
#include "nsStringDefines.h"
#include "nsStringFwd.h"
#include "nsBufferHandle.h"
#include "nsStringIteratorUtils.h"
#include "nsCharTraits.h"
#include "nsStringFragment.h"
#include "nsCharTraits.h"
#include "nsStringTraits.h"
#include "nsAlgorithm.h"
#include "nsStringIterator.h"
#include "nsAString.h"
#endif
#include "domstubs.h"
#include "nsIDOM3Node.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"

#include "prlink.h"

#include "nsInnoTekPluginWrapper.h"

#include "wrap_XPCOM_3rdparty.h"
#include "wrap_VFTs.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Define DO_EXTRA_FAILURE_HANDLING and we'll zero some return buffers. */
#define DO_EXTRA_FAILURE_HANDLING

/** Breakpoing macro */
#ifdef __IBMCPP__
# define BREAK_POINT()   __interrupt(3)
#else
# define BREAK_POINT()   asm("int $3")
#endif

/** Global breakpoint trigger. */
#ifdef DEBUG
# define DEBUG_GLOBAL_BREAKPOINT(cond) do { if (gfGlobalBreakPoint && (cond)) BREAK_POINT(); } while (0)
#else
# define DEBUG_GLOBAL_BREAKPOINT(cond) do {} while (0)
#endif

/** Function define to use. */
#ifdef __IBMCPP__
#define XPFUNCTION  __FUNCTION__
#else
#define XPFUNCTION  __PRETTY_FUNCTION__
#endif

/** Function name variable. */
#ifdef DEBUG
# define DEBUG_FUNCTIONNAME() static const char *pszFunction = XPFUNCTION
#else
# define DEBUG_FUNCTIONNAME() do {} while (0)
#endif


/** Creates a 'safe' VFT, meaning we add a bunch of empty entries at the end
 * of the VFT just in case someone is trying to access them.
 */
#define MAKE_SAFE_VFT(VFTType, name)    \
    const struct VFTType##WithPaddings                                  \
    {                                                                   \
        const VFTType   vft;                                            \
        unsigned        aulNulls[10];                                   \
    }   name = {

/** The NULL padding of the safe VFT. */
//#define SAFE_VFT_ZEROS()  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
#define SAFE_VFT_ZEROS()  ,{ 0x81000001, 0x81000002, 0x81000003, 0x81000004, \
    0x81000005, 0x81000006, 0x81000007, 0x81000008, 0x81000009, 0x8100000a} }


/** dprintf a nsID structure (reference to such). */
#define DPRINTF_NSID(refID)  \
    if (VALID_REF(refID))                                                                     \
        dprintf(("%s: %s={%08x,%04x,%04x,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x} %s (%p)",   \
                pszFunction, #refID, (refID).m0, (refID).m1, (refID).m2, (refID).m3[0], (refID).m3[1],   \
                (refID).m3[2], (refID).m3[3], (refID).m3[4], (refID).m3[5], (refID).m3[6], (refID).m3[7], \
                getIIDCIDName(refID), &(refID)));                                             \
    else                                                                                      \
        dprintf(("%s: %s=%p (illegal pointer!!!)", pszFunction, #refID, &(refID)))

/** dprintf a contract ID. */
#define DPRINTF_CONTRACTID(pszContractID)  \
    DPRINTF_STR(pszContractID)

/** safely dprintf a string. */
#define DPRINTF_STR(pszStr)  \
    do                                                                             \
    {                                                                              \
        if (VALID_PTR(pszStr))                                                     \
            dprintf(("%s: %s='%s' (%p)", pszFunction, #pszStr, pszStr, pszStr));   \
        else if (pszStr)                                                           \
            dprintf(("%s: %s=%p (illegal pointer!!!)", pszFunction, #pszStr, pszStr));\
        else                                                                       \
            dprintf(("%s: %s=<NULL>", pszFunction, #pszStr));                       \
    } while (0)

/** safely dprintf a string, if it's NULL it will be ignored. */
#define DPRINTF_STRNULL(pszStr)  \
    do                                                                             \
    {                                                                              \
        if (VALID_PTR(pszStr))                                                     \
            dprintf(("%s: %s='%s' (%p)", pszFunction, #pszStr, pszStr, pszStr));   \
        else if (pszStr)                                                           \
            dprintf(("%s: %s=%p (illegal pointer!!!)", pszFunction, #pszStr, pszStr)); \
    } while (0)


/** Debugging thing for stack corruption. */
#if defined(DEBUG) && 0
#define DEBUG_STACK_ENTER()                                                        \
    void *  pvStackGuard = memset(alloca(sizeof(achDebugStack)), 'k', sizeof(achDebugStack))
#else
#define DEBUG_STACK_ENTER()     do {} while (0)
#endif

/** Debugging thing for stack corruption. */
#if defined(DEBUG) && 0
static char achDebugStack[0x1000];
#define DEBUG_STACK_LEAVE()                                                                 \
    do {                                                                                    \
        if (achDebugStack[0] != 'k')                                                        \
            memset(achDebugStack, 'k', sizeof(achDebugStack));                              \
        if (memcmp(achDebugStack, pvStackGuard, sizeof(achDebugStack)))                     \
        {                                                                                   \
            dprintf(("%s: ARRRRRRRRRGGGGGGGGGGG!!!!!!!! Stack is f**ked !!!", pszFunction)); \
            DebugInt3();                                                                    \
        }                                                                                   \
    } while (0)
#else
#define DEBUG_STACK_LEAVE()     do {} while (0)
#endif


/** Save the FPU control word. */
#define SAVELOAD_FPU_CW(usNewCw) \
    unsigned short usSavedCw = _control87(usNewCw, 0xffff)

/** Restore the FPU control word. */
#define RESTORE_FPU_CW() \
    _control87(usSavedCw, 0xffff)


/** Make a interface pointer pMozI from a DOWNTHIS pointer. */
#define DOWN_MAKE_pMozI(interface, pvDownThis) \
    interface *pMozI = (interface *)((PDOWNTHIS)pvDownThis)->pvThis

/** Test and assert that the down this pointer is valid. */
#define DOWN_VALID(pvDownThis)  \
    (  VALID_PTR(pvDownThis)    \
     && !memcmp(&((PDOWNTHIS)(pvDownThis))->achMagic[0], gszDownMagicString, sizeof(gszDownMagicString)) )

/** Validate the down this pointer, complaing and returning error code if invalid. */
#define DOWN_VALIDATE_RET(pvDownThis)  \
    if (!DOWN_VALID(pvDownThis))                                             \
    {                                                                        \
        dprintf(("%s: invalid this pointer %p!", pszFunction, pvDownThis));   \
        DebugInt3();                                                         \
        DOWN_TRACE_LEAVE_INT(pvDownThis, NS_ERROR_INVALID_POINTER);          \
        return NS_ERROR_INVALID_POINTER;                                     \
    }

/** Validate the down this pointer, complaing and returning error code if invalid. */
#define DOWN_VALIDATE_NORET(pvDownThis)  \
    if (!DOWN_VALID(pvDownThis))                                             \
    {                                                                        \
        dprintf(("%s: invalid this pointer %p!", pszFunction, pvDownThis));   \
        DebugInt3();                                                         \
    }


/** Enter a DOWN worker.  */
#define DOWN_TRACE_ENTER(pvDownThis) \
    dprintf(("%s: enter this=%p (down)", pszFunction, pvDownThis));

/** Default DOWN enter code. */
#define DOWN_ENTER(pvDownThis, interface) \
    DEBUG_STACK_ENTER();                                                    \
    DEBUG_FUNCTIONNAME();                                                   \
    DOWN_TRACE_ENTER(pvDownThis);                                           \
    DOWN_VALIDATE_RET(pvDownThis);                                          \
    DOWN_MAKE_pMozI(interface, pvDownThis);                                 \
    DEBUG_GLOBAL_BREAKPOINT(1);

/** No-return DOWN enter code. */
#define DOWN_ENTER_NORET(pvDownThis, interface) \
    DEBUG_STACK_ENTER();                                                    \
    DEBUG_FUNCTIONNAME();                                                   \
    DOWN_TRACE_ENTER(pvDownThis);                                           \
    DOWN_VALIDATE_NORET(pvDownThis);                                        \
    DOWN_MAKE_pMozI(interface, pvDownThis);                                 \
    DEBUG_GLOBAL_BREAKPOINT(1);

/** Default DOWN enter code, with default rc declaration. */
#define DOWN_ENTER_RC(pvDownThis, interface) \
    DOWN_ENTER(pvDownThis, interface);       \
    nsresult    rc = NS_ERROR_UNEXPECTED


/** Leave a DOWN worker.  */
#define DOWN_TRACE_LEAVE(pvDownThis) \
    dprintf(("%s: leave this=%p (down)", pszFunction, pvDownThis))

/** Leave a DOWN worker with INT rc.  */
#define DOWN_TRACE_LEAVE_INT(pvDownThis, rc) \
    dprintf(("%s: leave this=%p rc=%x (down)", pszFunction, pvDownThis, rc))

/** Leave a DOWN worker.  */
#define DOWN_LEAVE(pvDownThis) \
    DOWN_TRACE_LEAVE(pvDownThis);                                           \
    DEBUG_GLOBAL_BREAKPOINT(1);                                             \
    DEBUG_STACK_LEAVE();

/** Leave a DOWN worker with INT rc.  */
#define DOWN_LEAVE_INT(pvDownThis, rc) \
    DOWN_TRACE_LEAVE_INT(pvDownThis, rc);                                   \
    DEBUG_GLOBAL_BREAKPOINT(1);                                             \
    DEBUG_STACK_LEAVE();

/** Magic string. */
#define DOWN_MAGIC_STRING       "DownMagicString"

/** Lock the down list. */
#define DOWN_LOCK()     downLock()

/** UnLock the down list. */
#define DOWN_UNLOCK()   downUnLock()


/** Validates a UP object. */
#define UP_VALID() \
    (*(void**)mpvThis == mpvVFTable)

/** Validates a UP object and returns NS_ERROR_UNEXPECTED if invalid. */
#define UP_VALIDATE_RET() \
    if (*(void**)mpvThis != mpvVFTable)                                         \
    {                                                                           \
        dprintf(("%s: invalid object!!! VFTable entery have changed! %x != %x", \
                 pszFunction, *(void**)mpvThis, mpvVFTable));                   \
        DebugInt3();                                                            \
        return NS_ERROR_UNEXPECTED;                                             \
    }

/** Enter a UP worker.  */
#define UP_TRACE_ENTER() \
    dprintf(("%s: enter this=%p (up)", pszFunction, mpvThis));


/** Default UP enter code. */
#define UP_ENTER__(fGDB) \
    DEBUG_FUNCTIONNAME();                          \
    UP_TRACE_ENTER();                              \
    DEBUG_GLOBAL_BREAKPOINT(fGDB)

/** Default UP enter code with default rc declaration. */
#define UP_ENTER_RC__(fGDB) \
    DEBUG_FUNCTIONNAME();                          \
    UP_TRACE_ENTER();                              \
    nsresult    rc = NS_ERROR_UNEXPECTED;          \
    UP_VALIDATE_RET();                             \
    DEBUG_GLOBAL_BREAKPOINT(fGDB)


/** Default UP enter code. */
#define UP_ENTER() \
    UP_ENTER__(1)

/** Default UP enter code - no global breakpoint. */
#define UP_ENTER_NODBGBRK() \
    UP_ENTER__(0)

/** Default UP enter code, with default rc declaration. */
#define UP_ENTER_RC() \
    UP_ENTER_RC__(1)

/** Default UP enter code, with default rc declaration - no global breakpoint. */
#define UP_ENTER_RC_NODBGBRK() \
    UP_ENTER_RC__(0)



/** Leave a UP worker.  */
#define UP_TRACE_LEAVE() \
    dprintf(("%s: leave this=%p (up)", pszFunction, mpvThis));

/** Leave a UP worker with INT rc.  */
#define UP_TRACE_LEAVE_INT(rc) \
    dprintf(("%s: leave this=%p rc=%x (up)", pszFunction, mpvThis, rc));


/** Leave a UP worker.  */
#define UP_LEAVE() \
    UP_TRACE_LEAVE();                                              \
    DEBUG_GLOBAL_BREAKPOINT(1);                                    \
    DEBUG_STACK_LEAVE()

/** Leave a UP worker with INT rc.  */
#define UP_LEAVE_INT(rc) \
    UP_TRACE_LEAVE_INT(rc);                                        \
    DEBUG_GLOBAL_BREAKPOINT(1);                                    \
    DEBUG_STACK_LEAVE()

/** Leave a UP worker with INT rc.  */
#define UP_LEAVE_INT_NODBGBRK(rc) \
    UP_TRACE_LEAVE_INT(rc);                                        \
    DEBUG_STACK_LEAVE()

/** Implements the nsISupports Interface. */
#define UP_IMPL_NSISUPPORTS() \
    NS_IMETHOD QueryInterface(REFNSIID aIID, void **aInstancePtr) { return hlpQueryInterface(aIID, aInstancePtr); } \
    NS_IMETHOD_(nsrefcnt) AddRef(void)  { return hlpAddRef(); } \
    NS_IMETHOD_(nsrefcnt) Release(void) { return hlpRelease(); } \
    //

/** Implements the nsISupports Interface. */
#define UP_IMPL_NSISUPPORTS_NOT_RELEASE() \
    NS_IMETHOD QueryInterface(REFNSIID aIID, void **aInstancePtr) { return hlpQueryInterface(aIID, aInstancePtr); } \
    NS_IMETHOD_(nsrefcnt) AddRef(void)  { return hlpAddRef(); } \
    //

/** Implements the nsIFactory Interface. */
#define UP_IMPL_NSIFACTORY() \
    NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID & aIID, void * *result)  \
    { return hlpCreateInstance(aOuter, aIID, result); }                                 \
    NS_IMETHOD LockFactory(PRBool lock)                                                 \
    { return hlpLockFactory(lock); }                                                    \
    //

/** Implements the nsIJVMWindow Interface. */
#define UP_IMPL_NSIJVMWINDOW() \
    NS_IMETHOD Show(void)                       \
    { return hlpShow(); }                       \
    NS_IMETHOD Hide(void)                       \
    { return hlpHide(); }                       \
    NS_IMETHOD IsVisible(PRBool *result)        \
    { return hlpIsVisible(result); }            \
    //

/** Implements the nsIPluginInstnacePeer Interface. */
#define UP_IMPL_NSIPLUGININSTANCEPEER() \
    NS_IMETHOD GetValue(nsPluginInstancePeerVariable aVariable, void * aValue)              \
    { return hlpGetValue(aVariable, aValue); }                                              \
    NS_IMETHOD GetMIMEType(nsMIMEType *aMIMEType)                                           \
    { return hlpGetMIMEType(aMIMEType); }                                                   \
    NS_IMETHOD GetMode(nsPluginMode *aMode)                                                 \
    { return hlpGetMode(aMode); }                                                           \
    NS_IMETHOD NewStream(nsMIMEType aType, const char *aTarget, nsIOutputStream **aResult)  \
    { return hlpNewStream(aType, aTarget, aResult); }                                       \
    NS_IMETHOD ShowStatus(const char *aMessage)                                             \
    { return hlpShowStatus(aMessage); }                                                     \
    NS_IMETHOD SetWindowSize(PRUint32 aWidth, PRUint32 aHeight)                             \
    { return hlpSetWindowSize(aWidth, aHeight); }                                           \
    //

/** Implements the nsIRequestObserver Interface. */
#define UP_IMPL_NSIREQUESTOBSERVER() \
    NS_IMETHOD OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)                      \
    { return hlpOnStartRequest(aRequest, aContext); }                                           \
    NS_IMETHOD OnStopRequest(nsIRequest *aRequest, nsISupports *aContext, nsresult aStatusCode) \
    { return hlpOnStopRequest(aRequest, aContext, aStatusCode); }                               \
    //

/** Implements the nsIRequest Interface. */
#define UP_IMPL_NSIREQUEST() \
    NS_IMETHOD GetName(nsACString & aName)                      \
    { return hlpGetName(aName); }                               \
    NS_IMETHOD IsPending(PRBool *_retval)                       \
    { return hlpIsPending(_retval); }                           \
    NS_IMETHOD GetStatus(nsresult *aStatus)                     \
    { return hlpGetStatus(aStatus); }                           \
    NS_IMETHOD Cancel(nsresult aStatus)                         \
    { return hlpCancel(aStatus); }                              \
    NS_IMETHOD Suspend(void)                                    \
    { return hlpSuspend(); }                                    \
    NS_IMETHOD Resume(void)                                     \
    { return hlpResume(); }                                     \
    NS_IMETHOD GetLoadGroup(nsILoadGroup * *aLoadGroup)         \
    { return hlpGetLoadGroup(aLoadGroup); }                     \
    NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup)          \
    { return hlpSetLoadGroup(aLoadGroup); }                     \
    NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags)            \
    { return hlpGetLoadFlags(aLoadFlags); }                     \
    NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags)             \
    { return hlpSetLoadFlags(aLoadFlags); }                     \
    //

/** Clears a java value. */
#if 1
#define ZERO_JAVAVALUE(value, type) memset(&value, 0, sizeof(jvalue))
#else
#define ZERO_JAVAVALUE(value, type) \
    do                                                                  \
    {                                                                   \
        switch (type)                                                   \
        {                                                               \
            case jobject_type:  (value).l = 0; break;                   \
            case jboolean_type: (value).z = 0; break;                   \
            case jbyte_type:    (value).b = 0; break;                   \
            case jchar_type:    (value).c = 0; break;                   \
            case jshort_type:   (value).s = 0; break;                   \
            case jint_type:     (value).i = 0; break;                   \
            case jlong_type:    (value).j = 0; break;                   \
            case jfloat_type:   (value).f = 0; break;                   \
            case jdouble_type:  (value).d = 0; break;                   \
        }                                                               \
    } while (0)
#endif



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/* forward references */
struct DownThis;
class UpWrapperBase;

/**
 * Linked list node.
 */
typedef struct WrapperNode
{
    /** Wrapper type. */
    enum enmWrapperType
    { enmDown, enmUp }
        enmType;

    /** Next */
    volatile struct WrapperNode  *pNext;

    /** The this pointer they're wrapping. */
    void *pvThis;

    /** Pointer to the wrapper. */
    union WrapperPointer
    {
        struct DownThis *   pDown;
        UpWrapperBase *     pUp;
    } u;
} WNODE, *PWNODE;


/**
 * What the pvThis pointer points at when the plugin calls back thru a 'down'
 * wrapper. (This is a struct really and I don't care that it's not proper C++.)
 */
typedef struct DownThis
{
    /** @name Public Data
     * @{
     */
    /** Pointer to virtual function table. */
    const void *    pvVFT;

    /** Some zero padding. (paranoia) */
#ifdef DEBUG
    char            achZeros[12 + 256];
#else
    char            achZeros[12 + 16];
#endif

    /** Pointer to the original class.  */
    void *          pvThis;

    /** Magic */
    char            achMagic[16];

    /** Next pointer */
    volatile struct DownThis * pNext;
    //@}

    /**
     * Initalize the struct.
     */
    inline void initialize(void *zpvThis, const void *zpvVFT)
    {
        this->pvVFT = zpvVFT;
        memset(&achZeros[0], 0, sizeof(achZeros));
        for (unsigned i = 0; i < sizeof(achZeros) / sizeof(unsigned); i += sizeof(unsigned))
            achZeros[i] = 0xC0000000 | i;
        this->pvThis = zpvThis;
        memcpy(achMagic, DOWN_MAGIC_STRING, sizeof(achMagic));
    }
} DOWNTHIS, *PDOWNTHIS;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** This variable is used to drag in XPCOM in the linking of a primary wrapper. */
int             giXPCOM;

/** Global flag for triggering breakpoing on enter and leave of wrapper methods.
 * DEBUG ONLY for use in the Debugger.
 */
#ifdef DEBUG
int             gfGlobalBreakPoint = FALSE;
#endif

/** @name Constants
 * @{
 */
/** Magic of the DOWNTHIS structure. */
const char gszDownMagicString[16] = DOWN_MAGIC_STRING;
//@}


/** Down Wrapper List.
 * Protected by gmtxDown.
 */
volatile struct DownThis * gpDownHead = NULL;

/** Down Wrapper Mutex. */
HMTX ghmtxDown = NULLHANDLE;


/*
 * Include the IDs and make them materialize here.
 *  Include twice to get the right definition.
 */
#include "moz_IDs_Generated.h"
#undef NP_DEF_ID
#define NP_DEF_ID NS_DEFINE_IID
#define NP_INCL_LOOKUP
#include "moz_IDs_Generated.h"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
const void *    downIsSupportedInterface(REFNSIID aIID);
nsresult        downCreateWrapper(void **ppvResult, const void *pvVFT, nsresult rc);
nsresult        downCreateWrapper(void **ppvResult, REFNSIID aIID, nsresult rc);
void            downLock(void);
void            downUnLock(void);
BOOL            upIsSupportedInterface(REFNSIID aIID);
nsresult        upCreateWrapper(void **ppvResult, REFNSIID aIID, nsresult rc);
int             downCreateJNIEnvWrapper(JNIEnv **ppJNIEnv, int rc);
int             upCreateJNIEnvWrapper(JNIEnv **ppJNIEnv, int rc);
void            verifyAndFixUTF8String(const char *pszString, const char *pszFunction);



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP Base Classes
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Base class for all 'up' wrappers - meaning up to mozilla.
 */
class UpWrapperBase
{
protected:
    /** This pointer of the Win32 object. */
    void *      mpvThis;

    /** This pointer of the Win32 class vftable.
     * This is actually an duplicate but it's ok for the purpose of list key. */
    void *      mpvVFTable;

    /** Interface ID. Also needed for the list stuff.*/
    REFNSIID    miid;

    /** Pointer to the interface we implement.
     * This is kind of required to get the right pointer when searching existing
     * wrappers without having to perform a bunch of stupid IID compares to find
     * the proper casing. (C++ rulez!)
     */
    void *      mpvInterface;

    /** Next wrapper */
    volatile UpWrapperBase * mpNext;

    /** List of UP wrappers.
     * Protected by hmtx.
     */
    static volatile UpWrapperBase * mpHead;

    /** Our The mutex protecting this wrapper list. */
    static HMTX                     mhmtx;


protected:
    /**
     * Constructor.
     *
     * @param   pvThis          Pointer to the Win32 class.
     * @param   pvInterface     Pointer to the interface we implement. Meaning
     *                          what's returned on a query interface.
     * @param   iid             The Interface ID of the wrapped interface.
     */
    UpWrapperBase(void *pvThis, void *pvInterface, REFNSIID iid) :
        mpvThis(pvThis), mpvVFTable(*(void**)pvThis), miid(iid), mpvInterface(pvInterface), mpNext(NULL)
    {
    }
public:
    /**
     * Destructor.
     */
    virtual ~UpWrapperBase()
    {
    }


    /**
     * Inserts the wrapper into the linked list of wrappers.
     * We hold the lock at entry.
     */
    void    upInsertWrapper(void)
    {
        mpNext = mpHead;
        mpHead = this;
    }


    /**
     * Remove this wrapper from the linked list.
     * We hold the lock at entry.
     * @returns 1 if successfully removed it.
     * @returns 0 on failure.
     */
    int     upRemoveWrapper(void)
    {
        int fUnchained;
        UpWrapperBase * pUp, *pUpPrev;
        for (pUp = (UpWrapperBase*)mpHead, pUpPrev = NULL, fUnchained = 0;
             pUp;
             pUpPrev = pUp, pUp = (UpWrapperBase*)pUp->mpNext)
        {
            if (pUp->mpvThis == mpvThis)
            {
                if (pUpPrev)
                    pUpPrev->mpNext = pUp->mpNext;
                else
                    mpHead = pUp->mpNext;

                #ifdef DEBUG
                /* paranoid as always */
                if (fUnchained)
                {
                    dprintf(("upRemoveWrapper: this=%x is linked twice !!!", this));
                    DebugInt3();
                }
                fUnchained = 1;
                #else
                fUnchained = 1;
                break;
                #endif
            }
        }

        if (fUnchained)
            this->mpNext = NULL;
        return fUnchained;
    }


    /**
     * Gets the interface pointer.
     */
    void *  getInterfacePointer()
    {
        return mpvInterface;
    }

    /**
     * Gets the Win32 this pointer.
     */
    void *  getThis()
    {
        return mpvThis;
    }

    /**
     * Gets the Win32 VFT pointer.
     */
    void *  getVFT()
    {
        return mpvVFTable;
    }

    /**
     * Locks the linked list of up wrappers.
     */
    static void upLock(void)
    {
        DEBUG_FUNCTIONNAME();

        if (!mhmtx)
        {
            int rc = DosCreateMutexSem(NULL, &mhmtx, 0, TRUE);
            if (rc)
            {
                dprintf(("%s: DosCreateMutexSem failed with rc=%d.", pszFunction, rc));
                ReleaseInt3(0xdeadbee1, 0xe000, rc);
            }
        }
        else
        {
            int rc = DosRequestMutexSem(mhmtx, SEM_INDEFINITE_WAIT);
            if (rc)
            {
                dprintf(("%s: DosRequestMutexSem failed with rc=%d.", pszFunction, rc));
                ReleaseInt3(0xdeadbee1, 0xe001, rc);
            }
            //@todo handle cases with the holder dies.
        }
    }

    /**
     * Unlocks the linked list of up wrappers.
     */
    static void upUnLock()
    {
        DEBUG_FUNCTIONNAME();

        int rc = DosReleaseMutexSem(mhmtx);
        if (rc)
        {
            dprintf(("%s: DosRequestMutexSem failed with rc=%d.", pszFunction, rc));
            ReleaseInt3(0xdeadbee1, 0xe002, rc);
        }
        //@todo handle cases with the holder dies.
    }

    /**
     * Try find an existing wrapper.
     *
     * The list is locked on entry and exit.
     *
     * @returns Pointer to existing wrapper if found.
     * @returns NULL if not found.
     * @param   pvThis  Pointer to the Win32 class.
     * @param   iid     The Interface ID of the wrapped interface.
     */
    static UpWrapperBase * findUpWrapper(void *pvThis, REFNSIID iid)
    {
        DEBUG_FUNCTIONNAME();

        /*
         * Walk the list and look...
         */
        for (UpWrapperBase * pUp = (UpWrapperBase*)mpHead; pUp; pUp = (UpWrapperBase*)pUp->mpNext)
            if (pUp->mpvThis == pvThis)
            {
                if (pUp->mpvVFTable == *((void**)pvThis))
                {
                    if (iid.Equals(pUp->miid) || iid.Equals(kSupportsIID))
                    {
                        dprintf(("%s: Found existing UP wrapper %p/%p for %p.",
                                 pszFunction, pUp, pUp->mpvInterface, pvThis));
                        return pUp;
                    }
                    else
                    {
                        dprintf(("%s: Found wrapper %p/%p for %p, but iid's wasn't matching.",
                                 pszFunction, pUp, pUp->getInterfacePointer(), pvThis));
                        DPRINTF_NSID(iid);
                        DPRINTF_NSID(pUp->miid);
                    }
                }
                else
                {
                    dprintf(("%s: Seems like an object have been release and reused again... (pvThis=%x)",
                             pszFunction, pvThis));
                    /*
                     * This happens with the IBM java.
                     * I think we safely can remove this wrapper and reuse it.
                     */
                    if (pUp->upRemoveWrapper())
                        pUp = (UpWrapperBase*)mpHead;
                }
            }


        /** @todo remove from list and such. */
        return NULL;
    }

    /**
     * Try find an existing up wrapper before creating a down wrapper.
     *
     * See findDownWrapper() for explanation.
     *
     * @returns Pointer to existing wrapper if found.
     * @returns NULL if not found.
     * @param   pvThis  Pointer to the Mozilla interface object which
     *                  may actually be an UpWrapperBase pointer.
     */
    static void * findUpWrapper(void *pvThis)
    {
        DEBUG_FUNCTIONNAME();

        /*
         * Walk the list and look.
         */
        upLock();
        for (UpWrapperBase * pUp = (UpWrapperBase *)mpHead; pUp; pUp = (UpWrapperBase *)pUp->mpNext)
            if ((void*)pUp == pvThis)
            {
                void *pvRet = pUp->mpvThis;
                upUnLock();
                dprintf(("%s: pvThis(=%x) was pointing to an up wrapper. Returning pointer to real object(=%x)",
                         pszFunction, pvThis, pvRet));
                return pvRet;
            }
        upUnLock();

        /* No luck... */
        return NULL;
    }



    /**
     * Try find an down wrapper for the desired up wrapper.
     *
     * This is the first thing we'll try when creating an up wrapper. The
     * object might actually have been created by mozilla and is sent down
     * to the plugin, like the nsIPluginInstancePeer, and is later queried
     * by mozilla. It is inefficient to wrap the object twice, also it's
     * potentially dangerous to do so.
     *
     * @returns Pointer to real object if it was sent down.
     * @returns NULL if not found.
     * @param   pvThis  Pointer to the Win32 class.
     */
    static void * findDownWrapper(void *pvThis)
    {
        DEBUG_FUNCTIONNAME();

        /*
         * Walk the list and look.
         */
        DOWN_LOCK();
        for (PDOWNTHIS pDown = (PDOWNTHIS)gpDownHead; pDown; pDown = (PDOWNTHIS)pDown->pNext)
            if (pDown == pvThis)
            {
                void *pvRet = pDown->pvThis;
                DOWN_UNLOCK();
                dprintf(("%s: pvThis(=%x) was pointing to a down wrapper. Returning pointer to real object(=%x)",
                         pszFunction, pvThis, pvRet));
                return pvRet;
            }
        DOWN_UNLOCK();

        /* No luck... */
        return NULL;
    }

};

volatile UpWrapperBase * UpWrapperBase::mpHead = NULL;
HMTX                     UpWrapperBase::mhmtx = NULLHANDLE;



/**
 * This class implements helpers for the nsISupports interface.
 *
 * Because of the way C++ treats multiple inheritance we cannot implement
 * nsISupports here and inherit the implementation automagically when
 * wrapping an interface subclassing nsISupports. Therefore we implement
 * helpers here with different names which can be called from the subclasses.
 */
class UpSupportsBase : public UpWrapperBase
{
protected:
    /**
     * Constructor.
     *
     * @param   pvThis          Pointer to the Win32 class.
     * @param   pvInterface     Pointer to the interface we implement. Meaning
     *                          what's returned on a query interface.
     * @param   iid             The  Interface ID of the wrapped interface.
     */
    UpSupportsBase(void *pvThis, void *pvInterface, REFNSIID iid) :
        UpWrapperBase(pvThis, pvInterface, iid)
    {
    }

    /**
     * A run time mechanism for interface discovery.
     * @param aIID [in] A requested interface IID
     * @param aInstancePtr [out] A pointer to an interface pointer to
     * receive the result.
     * @return <b>NS_OK</b> if the interface is supported by the associated
     * instance, <b>NS_NOINTERFACE</b> if it is not.
     * <b>NS_ERROR_INVALID_POINTER</b> if <i>aInstancePtr</i> is <b>NULL</b>.
     */
    nsresult hlpQueryInterface(REFNSIID aIID, void **aInstancePtr)
    {
        UP_ENTER_RC_NODBGBRK();
        DPRINTF_NSID(aIID);

        rc = VFTCALL2((VFTnsISupports*)mpvVFTable, QueryInterface, mpvThis, aIID, aInstancePtr);
        rc = upCreateWrapper(aInstancePtr, aIID, rc);

        UP_LEAVE_INT_NODBGBRK(rc);
        return rc;
    }

    /**
     * Increases the reference count for this interface.
     * The associated instance will not be deleted unless
     * the reference count is returned to zero.
     *
     * @return The resulting reference count.
     */
    nsrefcnt hlpAddRef()
    {
        UP_ENTER_NODBGBRK();
        nsrefcnt c = 0;
        if (UP_VALID())
            c = VFTCALL0((VFTnsISupports*)mpvVFTable, AddRef, mpvThis);
        else
        {
            dprintf(("%s: Invalid object!", pszFunction));
            DebugInt3();
        }
        UP_LEAVE_INT_NODBGBRK(c);
        return c;
    }

    /**
     * Decreases the reference count for this interface.
     * Generally, if the reference count returns to zero,
     * the associated instance is deleted.
     *
     * @return The resulting reference count.
     */
    nsrefcnt hlpRelease()
    {
        UP_ENTER_NODBGBRK();
        nsrefcnt c = 0;
        if (UP_VALID())
        {
            c = VFTCALL0((VFTnsISupports*)mpvVFTable, Release, mpvThis);
            if (!c)
            {
                dprintf(("%s: the reference count is zero! Delete this wrapper", pszFunction));
                /* (This is prohibitted by german law I think, suicide that is.) */
                UP_LEAVE_INT_NODBGBRK(0); /* exit odin context *first* */

                #ifdef DO_DELETE
                /*
                 * First unchain this object.
                 */
                upLock();
                if (upRemoveWrapper())
                {
                    upUnLock();
                    /** @todo Make a list of wrapper which is scheduled for lazy destruction.
                     * We can then easily check if we get a call to a dead object.
                     */
                    delete this;
                }
                else
                {
                    upUnLock();
                    /*
                     * Allready unchained? This shouldn't ever happen...
                     * The only possible case is that two threads calls hlpRelease()
                     * a the same time. So, we will *not* touch this node now.
                     */
                    dprintf(("%s: pvThis=%p not found in the list !!!!", pszFunction, mpvThis));
                    DebugInt3();
                }
                #endif
                return 0;
            }
        }
        else
        {
            dprintf(("%s: Invalid object!", pszFunction));
            DebugInt3();
        }
        UP_LEAVE_INT_NODBGBRK(c);
        return c;
    }
};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsISupports
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsISupports wrapper.
 * @remark No, you are NOT gonna inherit anything from this. Use UpSupportsBase.
 */
class UpSupports : public nsISupports, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * Constructor
     */
    UpSupports(void *pvThis) :
        UpSupportsBase(pvThis, (nsISupports*)this, kSupportsIID)
    {
    }
};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIFactory
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

class UpFactoryBase : public UpSupportsBase
{
protected:
    /**
      * Creates an instance of a component.
      *
      * @param aOuter Pointer to a component that wishes to be aggregated
      *               in the resulting instance. This will be nsnull if no
      *               aggregation is requested.
      * @param iid    The IID of the interface being requested in
      *               the component which is being currently created.
      * @param result [out] Pointer to the newly created instance, if successful.
      * @return NS_OK - Component successfully created and the interface
      *                 being requested was successfully returned in result.
      *         NS_NOINTERFACE - Interface not accessible.
      *         NS_ERROR_NO_AGGREGATION - if an 'outer' object is supplied, but the
      *                                   component is not aggregatable.
      *         NS_ERROR* - Method failure.
      */
    /* void createInstance (in nsISupports aOuter, in nsIIDRef iid, [iid_is (iid), retval] out nsQIResult result); */
    NS_IMETHOD hlpCreateInstance(nsISupports *aOuter, const nsIID & aIID, void * *result)
    {
        UP_ENTER_RC();
        DPRINTF_NSID(aIID);

        /*
         * Supported interface requested?
         */
        if (upIsSupportedInterface(aIID))
        {
            /*
             * Wrap the outer.
             */
            nsISupports * pDownOuter = aOuter;
            rc = downCreateWrapper((void**)&pDownOuter, downIsSupportedInterface(kSupportsIID), NS_OK);
            if (rc == NS_OK)
            {
                rc = VFTCALL3((VFTnsIFactory*)mpvVFTable, CreateInstance, mpvThis, pDownOuter, aIID, result);
                rc = upCreateWrapper(result, aIID, rc);
            }
            else
            {
                dprintf(("%s: downCreateWrapper failed for nsISupports/aOuter. rc=%x", pszFunction, rc));
                DebugInt3();
            }
        }
        else
        {
            dprintf(("%s: Unsupported interface!!!", pszFunction));
            rc = NS_NOINTERFACE;
            ReleaseInt3(0xbaddbeef, 7, aIID.m0);
        }

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
      * LockFactory provides the client a way to keep the component
      * in memory until it is finished with it. The client can call
      * LockFactory(PR_TRUE) to lock the factory and LockFactory(PR_FALSE)
      * to release the factory.
      *
      * @param lock - Must be PR_TRUE or PR_FALSE
      * @return NS_OK - If the lock operation was successful.
      *         NS_ERROR* - Method failure.
      */
    /* void lockFactory (in PRBool lock); */
    NS_IMETHOD hlpLockFactory(PRBool lock)
    {
        UP_ENTER_RC();
        dprintf(("%s: lock=%d", pszFunction, lock));
        rc = VFTCALL1((VFTnsIFactory*)mpvVFTable, LockFactory, mpvThis, lock);
        UP_LEAVE_INT(rc);
        return rc;
    }


    /** Constructor. */
    UpFactoryBase(void *pvThis, void *pvInterface, REFNSIID iid = kFactoryIID) :
        UpSupportsBase(pvThis, pvInterface, iid)
    {
    }

};

/**
 * nsIFactory wrapper.
 */
class UpFactory : public nsIFactory, public UpFactoryBase
{
public:
    UP_IMPL_NSISUPPORTS();
    UP_IMPL_NSIFACTORY();


    /** Constructor. */
    UpFactory(void *pvThis) :
        UpFactoryBase(pvThis, (nsIFactory*)this, kFactoryIID)
    {
    }
};



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIPlugin
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsIPlugin wrapper.
 */
class UpPlugin : public nsIPlugin, public UpFactoryBase
{
public:
    UP_IMPL_NSISUPPORTS();
    UP_IMPL_NSIFACTORY();

    /**
     * Creates a new plugin instance, based on a MIME type. This
     * allows different impelementations to be created depending on
     * the specified MIME type.
     */
    /* void createPluginInstance (in nsISupports aOuter, in nsIIDRef aIID, in string aPluginMIMEType, [iid_is (aIID), retval] out nsQIResult aResult); */
    NS_IMETHOD CreatePluginInstance(nsISupports *aOuter, const nsIID & aIID, const char *aPluginMIMEType, void * *aResult)
    {
        UP_ENTER_RC();
        DPRINTF_NSID(aIID);
        DPRINTF_STR(aPluginMIMEType);

        /*
         * Supported interface requested?
         */
        if (upIsSupportedInterface(aIID))
        {
            /*
             * Wrap the outer.
             */
            nsISupports * pDownOuter = aOuter;
            rc = downCreateWrapper((void**)&pDownOuter, downIsSupportedInterface(kSupportsIID), NS_OK);
            if (rc == NS_OK)
            {
                rc = VFTCALL4((VFTnsIPlugin*)mpvVFTable, CreatePluginInstance, mpvThis, aOuter, aIID, aPluginMIMEType, aResult);
                rc = upCreateWrapper(aResult, aIID, rc);
            }
            else
            {
                dprintf(("%s: downCreateWrapper failed for nsISupports/aOuter. rc=%x", pszFunction, rc));
                DebugInt3();
            }
        }
        else
        {
            dprintf(("%s: Unsupported interface!!!", pszFunction));
            rc = NS_NOINTERFACE;
            ReleaseInt3(0xbaddbeef, 8, aIID.m0);
        }

        UP_LEAVE_INT(rc);
        return rc;
    }


    /**
     * Initializes the plugin and will be called before any new instances are
     * created. It is passed browserInterfaces on which QueryInterface
     * may be used to obtain an nsIPluginManager, and other interfaces.
     *
     * @param browserInterfaces - an object that allows access to other browser
     * interfaces via QueryInterface
     * @result - NS_OK if this operation was successful
       */
    /* void initialize (); */
    NS_IMETHOD Initialize(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIPlugin*)mpvVFTable, Initialize, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Called when the browser is done with the plugin factory, or when
     * the plugin is disabled by the user.
     *
     * (Corresponds to NPP_Shutdown.)
     *
     * @result - NS_OK if this operation was successful
     */
    /* void shutdown (); */
    NS_IMETHOD Shutdown(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIPlugin*)mpvVFTable, Shutdown, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Returns the MIME description for the plugin. The MIME description
     * is a colon-separated string containg the plugin MIME type, plugin
     * data file extension, and plugin name, e.g.:
     *
     * "application/x-simple-plugin:smp:Simple LiveConnect Sample Plug-in"
     *
     * (Corresponds to NPP_GetMIMEDescription.)
     *
     * @param aMIMEDescription - the resulting MIME description
     * @result                 - NS_OK if this operation was successful
     */
    /* void getMIMEDescription (out constCharPtr aMIMEDescription); */
    NS_IMETHOD GetMIMEDescription(const char * *aMIMEDescription)
    {
        //@todo TEXT: aMIMEDescription? Doesn't apply to java plugin, and probably not called by OS/2.
        UP_ENTER_RC();
        dprintf(("%s: aMIMEDescription=%x", pszFunction, aMIMEDescription));
        rc = VFTCALL1((VFTnsIPlugin*)mpvVFTable, GetMIMEDescription, mpvThis, aMIMEDescription);
        if (NS_SUCCEEDED(rc) && VALID_PTR(aMIMEDescription))
            DPRINTF_STR(*aMIMEDescription);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Returns the value of a variable associated with the plugin.
     *
     * (Corresponds to NPP_GetValue.)
     *
     * @param aVariable - the plugin variable to get
     * @param aValue    - the address of where to store the resulting value
     * @result          - NS_OK if this operation was successful
     */
    /* void getValue (in nsPluginVariable aVariable, in voidPtr aValue); */
    NS_IMETHOD GetValue(nsPluginVariable aVariable, void * aValue)
    {
        UP_ENTER_RC();
        dprintf(("%s: aVariable=%d aValue=%x", pszFunction, aVariable, aValue));
        rc = VFTCALL2((VFTnsIPlugin*)mpvVFTable, GetValue, mpvThis, aVariable, aValue);
        UP_LEAVE_INT(rc);
        return rc;
    }


    /**
     * Constructor
     */
    UpPlugin(void *pvThis) :
        UpFactoryBase(pvThis, (nsIPlugin*)this, kPluginIID)
    {
    }
};

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIJVMPlugin
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

class UpJVMPlugin : public nsIJVMPlugin, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    // Causes the JVM to append a new directory to its classpath.
    // If the JVM doesn't support this operation, an error is returned.
    NS_IMETHOD AddToClassPath(const char* dirPath)
    {
        UP_ENTER_RC();
        DPRINTF_STR(dirPath);
        rc = VFTCALL1((VFTnsIJVMPlugin*)mpvVFTable, AddToClassPath, mpvThis, dirPath);
        UP_LEAVE_INT(rc);
        return rc;
    }

    // Causes the JVM to remove a directory from its classpath.
    // If the JVM doesn't support this operation, an error is returned.
    NS_IMETHOD RemoveFromClassPath(const char* dirPath)
    {
        UP_ENTER_RC();
        DPRINTF_STR(dirPath);
        rc = VFTCALL1((VFTnsIJVMPlugin*)mpvVFTable, RemoveFromClassPath, mpvThis, dirPath);
        UP_LEAVE_INT(rc);
        return rc;
    }

    // Returns the current classpath in use by the JVM.
    NS_IMETHOD GetClassPath(const char* *result)
    {
        UP_ENTER_RC();
        dprintf(("%s: result=%p", pszFunction, result));
        rc = VFTCALL1((VFTnsIJVMPlugin*)mpvVFTable, GetClassPath, mpvThis, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            DPRINTF_STR(*result);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetJavaWrapper(JNIEnv* jenv, jint obj, jobject *jobj)
    {
        UP_ENTER_RC();
        dprintf(("%s: jenv=%p obj=%p jobj=%p", pszFunction, jenv, obj, jobj));
        rc = VFTCALL3((VFTnsIJVMPlugin*)mpvVFTable, GetJavaWrapper, mpvThis, jenv, obj, jobj);
        if (NS_SUCCEEDED(rc) && VALID_PTR(jobj))
            dprintf(("%s: *jobj=%x", pszFunction, jobj));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * This creates a new secure communication channel with Java. The second parameter,
     * nativeEnv, if non-NULL, will be the actual thread for Java communication.
     * Otherwise, a new thread should be created.
     * @param   proxyEnv                the env to be used by all clients on the browser side
     * @return  outSecureEnv    the secure environment used by the proxyEnv
     */
    NS_IMETHOD CreateSecureEnv(JNIEnv* proxyEnv, nsISecureEnv* *outSecureEnv)
    {
        UP_ENTER_RC();
        dprintf(("%s: proxyEnv=%x outSecureEnv=%x", pszFunction, proxyEnv, outSecureEnv));
        JNIEnv *pdownProxyEnv = proxyEnv;
        rc = downCreateJNIEnvWrapper(&pdownProxyEnv, NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            rc = VFTCALL2((VFTnsIJVMPlugin*)mpvVFTable, CreateSecureEnv, mpvThis, pdownProxyEnv, outSecureEnv);
            rc = upCreateWrapper((void**)outSecureEnv, kSecureEnvIID, rc);
        }
        else
            dprintf(("%s: Failed to make JNIEnv down wrapper!!!", pszFunction));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Gives time to the JVM from the main event loop of the browser. This is
     * necessary when there aren't any plugin instances around, but Java threads exist.
     */
    NS_IMETHOD SpendTime(PRUint32 timeMillis)
    {
        UP_ENTER_RC();
        dprintf(("%s: timeMillis=%d", pszFunction, timeMillis));
        rc = VFTCALL1((VFTnsIJVMPlugin*)mpvVFTable, SpendTime, mpvThis, timeMillis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD UnwrapJavaWrapper(JNIEnv* jenv, jobject jobj, jint* obj)
    {
        UP_ENTER_RC();
        dprintf(("%s: jenv=%p jobj=%p obj=%p", pszFunction, jenv, jobj, obj));
        rc = VFTCALL3((VFTnsIJVMPlugin*)mpvVFTable, UnwrapJavaWrapper, mpvThis, jenv, jobj, obj);
        if (NS_SUCCEEDED(rc) && VALID_PTR(jobj))
            dprintf(("%s: *jobj=%p", pszFunction, jobj));
        UP_LEAVE_INT(rc);
        return rc;
    }


    /**
     * Constructor
     */
    UpJVMPlugin(void *pvThis) :
        UpSupportsBase(pvThis, (nsIJVMPlugin*)this, kJVMPluginIID)
    {
    }

};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIJVMPluginInstance
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
/**
 * nsIJVMPluginInstance
 */
class UpJVMPluginInstance : public nsIJVMPluginInstance, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();


    /* [noscript] void GetJavaObject (out jobject result); */
    NS_IMETHOD GetJavaObject(jobject *result)
    {
        UP_ENTER_RC();
        rc = VFTCALL1((VFTnsIJVMPluginInstance*)mpvVFTable, GetJavaObject, mpvThis, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /* [noscript] void GetText (in nChar result); */
    NS_IMETHOD GetText(const char ** result)
    {
        //@todo TEXT: result? Don't know, can't find anyone calling this...
        UP_ENTER_RC();
        rc = VFTCALL1((VFTnsIJVMPluginInstance*)mpvVFTable, GetText, mpvThis, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result) && VALID_PTR(*result))
            DPRINTF_STR(*result);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpJVMPluginInstance(void *pvThis) :
        UpSupportsBase(pvThis, (nsIJVMWindow*)this, kJVMPluginInstanceIID)
    {
    }

};



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsISecureEnv
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsISecureEnv Wrapper.
 */
class UpSecureEnv : public nsISecureEnv, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * Create new Java object in LiveConnect.
     *
     * @param clazz      -- Java Class object.
     * @param methodID   -- Method id
     * @param args       -- arguments for invoking the constructor.
     * @param result     -- return new Java object.
     * @param ctx        -- security context
     */
    NS_IMETHOD NewObject(/*[in]*/  jclass clazz,
                         /*[in]*/  jmethodID methodID,
                         /*[in]*/  jvalue *args,
                         /*[out]*/ jobject* result,
                         /*[in]*/  nsISecurityContext* ctx = NULL)
    {
        UP_ENTER_RC();
        dprintf(("%s: clazz=%p methodID=%p args=%p result=%p ctx=%p", pszFunction, clazz, methodID, args, result, ctx));

        nsISecurityContext *pDownCtx = ctx;
        rc = downCreateWrapper((void**)&pDownCtx, downIsSupportedInterface(kSecurityContextIID), NS_OK);
        if (rc == NS_OK)
        {
            rc = VFTCALL5((VFTnsISecureEnv*)mpvVFTable, NewObject, mpvThis, clazz, methodID, args, result, pDownCtx);
            if (NS_SUCCEEDED(rc) && VALID_PTR(result))
                dprintf(("%s: *result=%p", pszFunction, result));
        }
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Invoke method on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param obj        -- Java object.
     * @param methodID   -- method id
     * @param args       -- arguments for invoking the constructor.
     * @param result     -- return result of invocation.
     * @param ctx        -- security context
     */
    NS_IMETHOD CallMethod(/*[in]*/  jni_type type,
                          /*[in]*/  jobject obj,
                          /*[in]*/  jmethodID methodID,
                          /*[in]*/  jvalue *args,
                          /*[out]*/ jvalue* result,
                          /*[in]*/  nsISecurityContext* ctx = NULL)
    {
        UP_ENTER_RC();
        dprintf(("%s: type=%x obj=%p methodID=%p args=%p result=%p ctx=%p", pszFunction, type, obj, methodID, args, result, ctx));

        nsISecurityContext *pDownCtx = ctx;
        rc = downCreateWrapper((void**)&pDownCtx, downIsSupportedInterface(kSecurityContextIID), NS_OK);
        if (rc == NS_OK)
        {
            rc = VFTCALL6((VFTnsISecureEnv*)mpvVFTable, CallMethod, mpvThis, type, obj, methodID, args, result, pDownCtx);
            if (NS_SUCCEEDED(rc) && VALID_PTR(result))
                dprintf(("%s: *result=0x%08x", pszFunction, *(int*)result));
            #ifdef DO_EXTRA_FAILURE_HANDLING
            else if (VALID_PTR(result))
                ZERO_JAVAVALUE(*result, type);
            #endif
        }
        UP_LEAVE_INT(rc);
        return rc;
    }


    /**
     * Invoke non-virtual method on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param obj        -- Java object.
     * @param clazz      -- Class object
     * @param methodID   -- method id
     * @param args       -- arguments for invoking the constructor.
     * @param ctx        -- security context
     * @param result     -- return result of invocation.
     */
    NS_IMETHOD CallNonvirtualMethod(/*[in]*/  jni_type type,
                                    /*[in]*/  jobject obj,
                                    /*[in]*/  jclass clazz,
                                    /*[in]*/  jmethodID methodID,
                                    /*[in]*/  jvalue *args,
                                    /*[out]*/ jvalue* result,
                                    /*[in]*/  nsISecurityContext* ctx = NULL)
    {
        UP_ENTER_RC();
        dprintf(("%s: type=%x obj=%p clazz=%p methodID=%p args=%p result=%p ctx=%p", pszFunction, type, obj, clazz, methodID, args, result, ctx));

        nsISecurityContext *pDownCtx = ctx;
        rc = downCreateWrapper((void**)&pDownCtx, downIsSupportedInterface(kSecurityContextIID), NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            rc = VFTCALL7((VFTnsISecureEnv*)mpvVFTable, CallNonvirtualMethod, mpvThis, type, obj, clazz, methodID, args, result, pDownCtx);
            if (NS_SUCCEEDED(rc) && VALID_PTR(result))
                dprintf(("%s: *result=0x%08x (int)", pszFunction, *(int*)result));
            #ifdef DO_EXTRA_FAILURE_HANDLING
            else if (VALID_PTR(result))
                ZERO_JAVAVALUE(*result, type);
            #endif
        }
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Get a field on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param obj        -- Java object.
     * @param fieldID    -- field id
     * @param result     -- return field value
     * @param ctx        -- security context
     */
    NS_IMETHOD GetField(/*[in]*/  jni_type type,
                        /*[in]*/  jobject obj,
                        /*[in]*/  jfieldID fieldID,
                        /*[out]*/ jvalue* result,
                        /*[in]*/  nsISecurityContext* ctx = NULL)
    {
        UP_ENTER_RC();
        dprintf(("%s: type=%x obj=%p fieldID= result=%p ctx=%p", pszFunction, type, fieldID, result, ctx));

        nsISecurityContext *pDownCtx = ctx;
        rc = downCreateWrapper((void**)&pDownCtx, downIsSupportedInterface(kSecurityContextIID), NS_OK);
        if (rc == NS_OK)
        {
            rc = VFTCALL5((VFTnsISecureEnv*)mpvVFTable, GetField, mpvThis, type, obj, fieldID, result, pDownCtx);
            if (NS_SUCCEEDED(rc) && VALID_PTR(result))
                dprintf(("%s: *result=0x%08x (int)", pszFunction, *(int*)result));
        }
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Set a field on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param obj        -- Java object.
     * @param fieldID    -- field id
     * @param val        -- field value to set
     * @param ctx        -- security context
     */
    NS_IMETHOD SetField(/*[in]*/ jni_type type,
                        /*[in]*/ jobject obj,
                        /*[in]*/ jfieldID fieldID,
                        /*[in]*/ jvalue val,
                        /*[in]*/ nsISecurityContext* ctx = NULL)
    {
        UP_ENTER_RC();
        dprintf(("%s: type=%x, obj=%p fieldID=%p val=0x%08x (int) ctx=%p", pszFunction, type, obj, fieldID, val.i, ctx));

        nsISecurityContext *pDownCtx = ctx;
        rc = downCreateWrapper((void**)&pDownCtx, downIsSupportedInterface(kSecurityContextIID), NS_OK);
        if (rc == NS_OK)
            rc = VFTCALL5((VFTnsISecureEnv*)mpvVFTable, SetField, mpvThis, type, obj, fieldID, val, pDownCtx);
        UP_LEAVE_INT(rc);
        return rc;
    }


    /**
     * Invoke static method on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param clazz      -- Class object.
     * @param methodID   -- method id
     * @param args       -- arguments for invoking the constructor.
     * @param result     -- return result of invocation.
     * @param ctx        -- security context
     */
    NS_IMETHOD CallStaticMethod(/*[in]*/  jni_type type,
                                /*[in]*/  jclass clazz,
                                /*[in]*/  jmethodID methodID,
                                /*[in]*/  jvalue *args,
                                /*[out]*/ jvalue* result,
                                /*[in]*/  nsISecurityContext* ctx = NULL)
    {
        UP_ENTER_RC();
        dprintf(("%s: type=%d clazz=%#x methodID=%#x args=%p result=%p ctx=%p", pszFunction, type, clazz, methodID, args, result,  ctx));

        nsISecurityContext *pDownCtx = ctx;
        rc = downCreateWrapper((void**)&pDownCtx, downIsSupportedInterface(kSecurityContextIID), NS_OK);
        if (rc == NS_OK)
        {
            rc = VFTCALL6((VFTnsISecureEnv*)mpvVFTable, CallStaticMethod, mpvThis, type, clazz, methodID, args, result, pDownCtx);
            if (NS_SUCCEEDED(rc) && VALID_PTR(result))
                dprintf(("%s: *result=0x%08x (int)", pszFunction, *(int*)result));
            #ifdef DO_EXTRA_FAILURE_HANDLING
            else if (VALID_PTR(result))
                ZERO_JAVAVALUE(*result, type);
            #endif
        }
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Get a static field on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param clazz      -- Class object.
     * @param fieldID    -- field id
     * @param result     -- return field value
     * @param ctx        -- security context
     */
    NS_IMETHOD GetStaticField(/*[in]*/  jni_type type,
                              /*[in]*/  jclass clazz,
                              /*[in]*/  jfieldID fieldID,
                              /*[out]*/ jvalue* result,
                              /*[in]*/  nsISecurityContext* ctx = NULL)
    {
        UP_ENTER_RC();
        dprintf(("%s: type=%d clazz=%#x fieldID=%#x result=%p ctx=%p", pszFunction, type, clazz, fieldID, result,  ctx));

        nsISecurityContext *pDownCtx = ctx;
        rc = downCreateWrapper((void**)&pDownCtx, kSecurityContextIID, NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            rc = VFTCALL5((VFTnsISecureEnv*)mpvVFTable, GetStaticField, mpvThis, type, clazz, fieldID, result, pDownCtx);
            if (NS_SUCCEEDED(rc) && VALID_PTR(result))
                dprintf(("%s: *result=0x%08x (int)", pszFunction, *(int*)result));
        }

        UP_LEAVE_INT(rc);
        return rc;
    }


    /**
     * Set a static field on Java object in LiveConnect.
     *
     * @param type       -- Return type
     * @param clazz      -- Class object.
     * @param fieldID    -- field id
     * @param val        -- field value to set
     * @param ctx        -- security context
     */
    NS_IMETHOD SetStaticField(/*[in]*/ jni_type type,
                              /*[in]*/ jclass clazz,
                              /*[in]*/ jfieldID fieldID,
                              /*[in]*/ jvalue val,
                              /*[in]*/ nsISecurityContext* ctx = NULL)
    {
        UP_ENTER_RC();
        dprintf(("%s: type=%x, clazz=%p fieldID=%p val=0x%08x (int) ctx=%p", pszFunction, type, clazz, fieldID, val.i, ctx));

        nsISecurityContext *pDownCtx = ctx;
        rc = downCreateWrapper((void**)&pDownCtx, downIsSupportedInterface(kSecurityContextIID), NS_OK);
        if (rc == NS_OK)
            rc = VFTCALL5((VFTnsISecureEnv*)mpvVFTable, SetStaticField, mpvThis, type, clazz, fieldID, val, pDownCtx);
        UP_LEAVE_INT(rc);
        return rc;
    }


    NS_IMETHOD GetVersion(/*[out]*/ jint* version)
    {
        UP_ENTER_RC();
        rc = VFTCALL1((VFTnsISecureEnv*)mpvVFTable, GetVersion, mpvThis, version);
        if (NS_SUCCEEDED(rc) && VALID_PTR(version))
            dprintf(("%s: *version=%d", pszFunction, *version));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD DefineClass(/*[in]*/  const char* name,
                           /*[in]*/  jobject loader,
                           /*[in]*/  const jbyte *buf,
                           /*[in]*/  jsize len,
                           /*[out]*/ jclass* clazz)
    {
        UP_ENTER_RC();
        DPRINTF_STR(name);
        rc = VFTCALL5((VFTnsISecureEnv*)mpvVFTable, DefineClass, mpvThis, name, loader, buf, len, clazz);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD FindClass(/*[in]*/  const char* name,
                         /*[out]*/ jclass* clazz)
    {
        UP_ENTER_RC();
        DPRINTF_STR(name);
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, FindClass, mpvThis, name, clazz);
        if (NS_SUCCEEDED(rc) && VALID_PTR(clazz))
            dprintf(("%s: *clazz=%p", pszFunction, *clazz));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetSuperclass(/*[in]*/  jclass sub,
                             /*[out]*/ jclass* super)
    {
        UP_ENTER_RC();
        dprintf(("%s: sub=%p super=%p", pszFunction, sub, super));
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, GetSuperclass, mpvThis, sub, super);
        if (NS_SUCCEEDED(rc) && VALID_PTR(super))
            dprintf(("%s: *super=%p", pszFunction, *super));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD IsAssignableFrom(/*[in]*/  jclass sub,
                                /*[in]*/  jclass super,
                                /*[out]*/ jboolean* result)
    {
        UP_ENTER_RC();
        dprintf(("%s: sub=%p super=%p result=%p", pszFunction, sub, super, result));
        rc = VFTCALL3((VFTnsISecureEnv*)mpvVFTable, IsAssignableFrom, mpvThis, sub, super, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%d", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }


    NS_IMETHOD Throw(/*[in]*/  jthrowable obj,
                     /*[out]*/ jint* result)
    {
        UP_ENTER_RC();
        dprintf(("%s: obj=%p result=%p", pszFunction, obj, result));
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, Throw, mpvThis, obj, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD ThrowNew(/*[in]*/  jclass clazz,
                        /*[in]*/  const char *msg,
                        /*[out]*/ jint* result)
    {
        //@todo TEXT: msg?? Don't think we care to much about this. :)
        UP_ENTER_RC();
        dprintf(("%s: clazz=%p msg=%p result=%p", pszFunction, clazz, msg, result));
        DPRINTF_STR(msg);
        rc = VFTCALL3((VFTnsISecureEnv*)mpvVFTable, ThrowNew, mpvThis, clazz, msg, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD ExceptionOccurred(/*[out]*/ jthrowable* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL1((VFTnsISecureEnv*)mpvVFTable, ExceptionOccurred, mpvThis, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD ExceptionDescribe(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsISecureEnv*)mpvVFTable, ExceptionDescribe, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD ExceptionClear(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsISecureEnv*)mpvVFTable, ExceptionClear, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD FatalError(/*[in]*/ const char* msg)
    {
        UP_ENTER_RC();
        DPRINTF_STR(msg);
        rc = VFTCALL1((VFTnsISecureEnv*)mpvVFTable, FatalError, mpvThis, msg);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD NewGlobalRef(/*[in]*/  jobject lobj,
                            /*[out]*/ jobject* result)
    {
        UP_ENTER_RC();
        dprintf(("%s: lobj=%p result=%p", pszFunction, lobj, result));
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, NewGlobalRef, mpvThis, lobj, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD DeleteGlobalRef(/*[in]*/ jobject gref)
    {
        UP_ENTER_RC();
        dprintf(("%s: gref=%p", pszFunction, gref));
        rc = VFTCALL1((VFTnsISecureEnv*)mpvVFTable, DeleteGlobalRef, mpvThis, gref);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD DeleteLocalRef(/*[in]*/ jobject obj)
    {
        UP_ENTER_RC();
        dprintf(("%s: obj=%p", pszFunction, obj));
        rc = VFTCALL1((VFTnsISecureEnv*)mpvVFTable, DeleteLocalRef, mpvThis, obj);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD IsSameObject(/*[in]*/  jobject obj1,
                            /*[in]*/  jobject obj2,
                            /*[out]*/ jboolean* result)
    {
        UP_ENTER_RC();
        dprintf(("%s: obj1=%p obj2=%p result=%p", pszFunction, obj1, obj2, result));
        rc = VFTCALL3((VFTnsISecureEnv*)mpvVFTable, IsSameObject, mpvThis, obj1, obj2, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%d", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD AllocObject(/*[in]*/  jclass clazz,
                           /*[out]*/ jobject* result)
    {
        UP_ENTER_RC();
        dprintf(("%s: clazz=%p result=%p", pszFunction, clazz, result));
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, AllocObject, mpvThis, clazz, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetObjectClass(/*[in]*/  jobject obj,
                              /*[out]*/ jclass* result)
    {
        UP_ENTER_RC();
        dprintf(("%s: obj=%p result=%p", pszFunction, obj, result));
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, GetObjectClass, mpvThis, obj, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=0x%08x (int)", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD IsInstanceOf(/*[in]*/  jobject obj,
                            /*[in]*/  jclass clazz,
                            /*[out]*/ jboolean* result)
    {
        UP_ENTER_RC();
        dprintf(("%s: obj=%p clazz=%p result=%p", pszFunction, obj, clazz, result));
        rc = VFTCALL3((VFTnsISecureEnv*)mpvVFTable, IsInstanceOf, mpvThis, obj, clazz, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%d", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetMethodID(/*[in]*/  jclass clazz,
                           /*[in]*/  const char* name,
                           /*[in]*/  const char* sig,
                           /*[out]*/ jmethodID* id)
    {
        UP_ENTER_RC();
        dprintf(("%s: clazz=%#x name=%p sig=%p id=%p", pszFunction, clazz, name, sig, id));
        DPRINTF_STR(name);
        DPRINTF_STR(sig);
        rc = VFTCALL4((VFTnsISecureEnv*)mpvVFTable, GetMethodID, mpvThis, clazz, name, sig, id);
        if (NS_SUCCEEDED(rc) && VALID_PTR(id))
            dprintf(("%s: *id=%#x", pszFunction, *id));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetFieldID(/*[in]*/  jclass clazz,
                          /*[in]*/  const char* name,
                          /*[in]*/  const char* sig,
                          /*[out]*/ jfieldID* id)
    {
        UP_ENTER_RC();
        dprintf(("%s: clazz=%#x id=%p", pszFunction, clazz, id));
        DPRINTF_STR(name);
        DPRINTF_STR(sig);
        rc = VFTCALL4((VFTnsISecureEnv*)mpvVFTable, GetFieldID, mpvThis, clazz, name, sig, id);
        if (NS_SUCCEEDED(rc) && VALID_PTR(id))
            dprintf(("%s: *id=%#x", pszFunction, *id));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetStaticMethodID(/*[in]*/  jclass clazz,
                                 /*[in]*/  const char* name,
                                 /*[in]*/  const char* sig,
                                 /*[out]*/ jmethodID* id)
    {
        UP_ENTER_RC();
        dprintf(("%s: clazz=%#x id=%p", pszFunction, clazz, id));
        DPRINTF_STR(name);
        DPRINTF_STR(sig);
        rc = VFTCALL4((VFTnsISecureEnv*)mpvVFTable, GetStaticMethodID, mpvThis, clazz, name, sig, id);
        if (NS_SUCCEEDED(rc) && VALID_PTR(id))
            dprintf(("%s: *id=%#x", pszFunction, *id));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetStaticFieldID(/*[in]*/  jclass clazz,
                                /*[in]*/  const char* name,
                                /*[in]*/  const char* sig,
                                /*[out]*/ jfieldID* id)
    {
        UP_ENTER_RC();
        dprintf(("%s: clazz=%#x id=%p", pszFunction, clazz, id));
        DPRINTF_STR(name);
        DPRINTF_STR(sig);
        rc = VFTCALL4((VFTnsISecureEnv*)mpvVFTable, GetStaticFieldID, mpvThis, clazz, name, sig, id);
        if (NS_SUCCEEDED(rc) && VALID_PTR(id))
            dprintf(("%s: *id=%#x", pszFunction, *id));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD NewString(/*[in]*/  const jchar* unicode,
                         /*[in]*/  jsize len,
                         /*[out]*/ jstring* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL3((VFTnsISecureEnv*)mpvVFTable, NewString, mpvThis, unicode, len, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetStringLength(/*[in]*/  jstring str,
                               /*[out]*/ jsize* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, GetStringLength, mpvThis, str, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%d", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetStringChars(/*[in]*/  jstring str,
                              /*[in]*/  jboolean *isCopy,
                              /*[out]*/ const jchar** result)
    {
        UP_ENTER_RC();
        rc = VFTCALL3((VFTnsISecureEnv*)mpvVFTable, GetStringChars, mpvThis, str, isCopy, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD ReleaseStringChars(/*[in]*/  jstring str,
                                  /*[in]*/  const jchar *chars)
    {
        UP_ENTER_RC();
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, ReleaseStringChars, mpvThis, str, chars);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD NewStringUTF(/*[in]*/  const char *utf,
                            /*[out]*/ jstring* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, NewStringUTF, mpvThis, utf, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetStringUTFLength(/*[in]*/  jstring str,
                                  /*[out]*/ jsize* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, GetStringUTFLength, mpvThis, str, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%d", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetStringUTFChars(/*[in]*/  jstring str,
                                 /*[in]*/  jboolean *isCopy,
                                 /*[out]*/ const char** result)
    {
        UP_ENTER_RC();
        dprintf(("%s: str=%p isCopy=%p result=%p", pszFunction, str, isCopy, result));
        rc = VFTCALL3((VFTnsISecureEnv*)mpvVFTable, GetStringUTFChars, mpvThis, str, isCopy, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD ReleaseStringUTFChars(/*[in]*/  jstring str,
                                     /*[in]*/  const char *chars)
    {
        UP_ENTER_RC();
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, ReleaseStringUTFChars, mpvThis, str, chars);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetArrayLength(/*[in]*/  jarray array,
                              /*[out]*/ jsize* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, GetArrayLength, mpvThis, array, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%d", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD NewObjectArray(/*[in]*/  jsize len,
                                        /*[in]*/  jclass clazz,
                        /*[in]*/  jobject init,
                        /*[out]*/ jobjectArray* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL4((VFTnsISecureEnv*)mpvVFTable, NewObjectArray, mpvThis, len, clazz, init, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetObjectArrayElement(/*[in]*/  jobjectArray array,
                                     /*[in]*/  jsize index,
                                     /*[out]*/ jobject* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL3((VFTnsISecureEnv*)mpvVFTable, GetObjectArrayElement, mpvThis, array, index, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD SetObjectArrayElement(/*[in]*/  jobjectArray array,
                                     /*[in]*/  jsize index,
                                     /*[in]*/  jobject val)
    {
        UP_ENTER_RC();
        rc = VFTCALL3((VFTnsISecureEnv*)mpvVFTable, SetObjectArrayElement, mpvThis, array, index, val);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD NewArray(/*[in]*/ jni_type element_type,
                        /*[in]*/  jsize len,
                        /*[out]*/ jarray* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL3((VFTnsISecureEnv*)mpvVFTable, NewArray, mpvThis, element_type, len, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetArrayElements(/*[in]*/  jni_type type,
                                /*[in]*/  jarray array,
                                /*[in]*/  jboolean *isCopy,
                                /*[out]*/ void* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL4((VFTnsISecureEnv*)mpvVFTable, GetArrayElements, mpvThis, type, array, isCopy, result);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD ReleaseArrayElements(/*[in]*/ jni_type type,
                                    /*[in]*/ jarray array,
                                    /*[in]*/ void *elems,
                                    /*[in]*/ jint mode)
    {
        UP_ENTER_RC();
        rc = VFTCALL4((VFTnsISecureEnv*)mpvVFTable, ReleaseArrayElements, mpvThis, type, array, elems, mode);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetArrayRegion(/*[in]*/  jni_type type,
                              /*[in]*/  jarray array,
                              /*[in]*/  jsize start,
                              /*[in]*/  jsize len,
                              /*[out]*/ void* buf)
    {
        UP_ENTER_RC();
        rc = VFTCALL5((VFTnsISecureEnv*)mpvVFTable, GetArrayRegion, mpvThis, type, array, start, len, buf);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD SetArrayRegion(/*[in]*/  jni_type type,
                              /*[in]*/  jarray array,
                              /*[in]*/  jsize start,
                              /*[in]*/  jsize len,
                              /*[in]*/  void* buf)
    {
        UP_ENTER_RC();
        rc = VFTCALL5((VFTnsISecureEnv*)mpvVFTable, SetArrayRegion, mpvThis, type, array, start, len, buf);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD RegisterNatives(/*[in]*/  jclass clazz,
                               /*[in]*/  const JNINativeMethod *methods,
                               /*[in]*/  jint nMethods,
                               /*[out]*/ jint* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL4((VFTnsISecureEnv*)mpvVFTable, RegisterNatives, mpvThis, clazz, methods, nMethods, result);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD UnregisterNatives(/*[in]*/  jclass clazz,
                                 /*[out]*/ jint* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, UnregisterNatives, mpvThis, clazz, result);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD MonitorEnter(/*[in]*/  jobject obj,
                            /*[out]*/ jint* result)
    {
        UP_ENTER_RC();
        dprintf(("%s: obj=%p result=%p", pszFunction, obj, result));
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, MonitorEnter, mpvThis, obj, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD MonitorExit(/*[in]*/  jobject obj,
                           /*[out]*/ jint* result)
    {
        UP_ENTER_RC();
        dprintf(("%s: obj=%p result=%p", pszFunction, obj, result));
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, MonitorExit, mpvThis, obj, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%p", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetJavaVM(/*[in]*/  JavaVM **vm,
                         /*[out]*/ jint* result)
    {
        UP_ENTER_RC();
        rc = VFTCALL2((VFTnsISecureEnv*)mpvVFTable, GetJavaVM, mpvThis, vm, result);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Constructor
     */
    UpSecureEnv(void *pvThis) :
        UpSupportsBase(pvThis, (nsISecureEnv*)this, kSecureEnvIID)
    {
    }

};



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIPluginInstance
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
class UpPluginInstance : public nsIPluginInstance, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * Initializes a newly created plugin instance, passing to it the plugin
     * instance peer which it should use for all communication back to the browser.
     *
     * @param aPeer - the corresponding plugin instance peer
     * @result      - NS_OK if this operation was successful
     */
    /* void initialize (in nsIPluginInstancePeer aPeer); */
    NS_IMETHOD Initialize(nsIPluginInstancePeer *aPeer)
    {
        UP_ENTER_RC();
        nsIPluginInstancePeer *pDownPeer = aPeer;
        rc = downCreateWrapper((void**)&pDownPeer, kPluginInstancePeerIID, NS_OK);
        if (rc == NS_OK)
            rc = VFTCALL1((VFTnsIPluginInstance*)mpvVFTable, Initialize, mpvThis, pDownPeer);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Returns a reference back to the plugin instance peer. This method is
     * used whenever the browser needs to obtain the peer back from a plugin
     * instance. The implementation of this method should be sure to increment
     * the reference count on the peer by calling AddRef.
     *
     * @param aPeer - the resulting plugin instance peer
     * @result      - NS_OK if this operation was successful
     */
    /* readonly attribute nsIPluginInstancePeer peer; */
    NS_IMETHOD GetPeer(nsIPluginInstancePeer * *aPeer)
    {
        UP_ENTER_RC();
        rc = VFTCALL1((VFTnsIPluginInstance*)mpvVFTable, GetPeer, mpvThis, aPeer);
        rc = upCreateWrapper((void**)aPeer, kPluginInstancePeerIID, rc);
        UP_LEAVE_INT(rc);
        return rc;
    }


    /**
     * Called to instruct the plugin instance to start. This will be called after
     * the plugin is first created and initialized, and may be called after the
     * plugin is stopped (via the Stop method) if the plugin instance is returned
     * to in the browser window's history.
     *
     * @result - NS_OK if this operation was successful
     */
    /* void start (); */
    NS_IMETHOD Start(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIPluginInstance*)mpvVFTable, Start, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Called to instruct the plugin instance to stop, thereby suspending its state.
     * This method will be called whenever the browser window goes on to display
     * another page and the page containing the plugin goes into the window's history
     * list.
     *
     * @result - NS_OK if this operation was successful
     */
    /* void stop (); */
    NS_IMETHOD Stop(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIPluginInstance*)mpvVFTable, Stop, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Called to instruct the plugin instance to destroy itself. This is called when
     * it become no longer possible to return to the plugin instance, either because
     * the browser window's history list of pages is being trimmed, or because the
     * window containing this page in the history is being closed.
     *
     * @result - NS_OK if this operation was successful
     */
    /* void destroy (); */
    NS_IMETHOD Destroy(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIPluginInstance*)mpvVFTable, Destroy, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Called when the window containing the plugin instance changes.
     *
     * (Corresponds to NPP_SetWindow.)
     *
     * @param aWindow - the plugin window structure
     * @result        - NS_OK if this operation was successful
     */
    /* void setWindow (in nsPluginWindowPtr aWindow); */
    NS_IMETHOD SetWindow(nsPluginWindow * aWindow)
    {
        UP_ENTER_RC();
        rc = VFTCALL1((VFTnsIPluginInstance*)mpvVFTable, SetWindow, mpvThis, aWindow);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Called to tell the plugin that the initial src/data stream is
     * ready.  Expects the plugin to return a nsIPluginStreamListener.
     *
     * (Corresponds to NPP_NewStream.)
     *
     * @param aListener - listener the browser will use to give the plugin the data
     * @result          - NS_OK if this operation was successful
     */
    /* void newStream (out nsIPluginStreamListener aListener); */
    NS_IMETHOD NewStream(nsIPluginStreamListener **aListener)
    {
        UP_ENTER_RC();
        rc = VFTCALL1((VFTnsIPluginInstance*)mpvVFTable, NewStream, mpvThis, aListener);
        rc = upCreateWrapper((void**)aListener, kPluginStreamListenerIID, rc);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * Called to instruct the plugin instance to print itself to a printer.
       *
       * (Corresponds to NPP_Print.)
       *
     * @param aPlatformPrint - platform-specific printing information
     * @result               - NS_OK if this operation was successful
     */
    /* void print (in nsPluginPrintPtr aPlatformPrint); */
    NS_IMETHOD Print(nsPluginPrint * aPlatformPrint)
    {
        UP_ENTER_RC();
        dprintf(("%s: aPlatformPrint=%p", pszFunction, aPlatformPrint));
        if (aPlatformPrint)
        {
            if (aPlatformPrint->mode == nsPluginMode_Embedded)
                dprintf(("%s: Embed: platformPrint=%08x windows: windows=%08x, (x,y,width,height)=(%d,%d,%d,%d) type=%d",
                         pszFunction,
                         aPlatformPrint->print.embedPrint.platformPrint,
                         aPlatformPrint->print.embedPrint.window.window,
                         aPlatformPrint->print.embedPrint.window.x,
                         aPlatformPrint->print.embedPrint.window.y,
                         aPlatformPrint->print.embedPrint.window.width,
                         aPlatformPrint->print.embedPrint.window.height,
                         aPlatformPrint->print.embedPrint.window.type));
            else if (aPlatformPrint->mode == nsPluginMode_Full)
                dprintf(("%s: Full: platformPrint=%08x pluginPrinted=%d printOne=%d",
                         pszFunction,
                         aPlatformPrint->print.fullPrint.platformPrint,
                         aPlatformPrint->print.fullPrint.pluginPrinted,
                         aPlatformPrint->print.fullPrint.printOne));
            else
                dprintf(("%s: Unknown mode!", pszFunction));
        }

        rc = VFTCALL1((VFTnsIPluginInstance*)mpvVFTable, Print, mpvThis, aPlatformPrint);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Returns the value of a variable associated with the plugin instance.
     *
     * @param aVariable - the plugin instance variable to get
     * @param aValue    - the address of where to store the resulting value
     * @result          - NS_OK if this operation was successful
     */
    /* void getValue (in nsPluginInstanceVariable aVariable, in voidPtr aValue); */
    NS_IMETHOD GetValue(nsPluginInstanceVariable aVariable, void * aValue)
    {
        UP_ENTER_RC();
        dprintf(("%s: aVariable=%d, aValue=%p", pszFunction, aVariable, aValue));
        rc = VFTCALL2((VFTnsIPluginInstance*)mpvVFTable, GetValue, mpvThis, aVariable, aValue);
        if (VALID_PTR(aValue))
            dprintf(("%s: *aValue=%p", pszFunction, *(void**)aValue));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Handles an event. An nsIEventHandler can also get registered with with
     * nsIPluginManager2::RegisterWindow and will be called whenever an event
     * comes in for that window.
     *
     * Note that for Unix and Mac the nsPluginEvent structure is different
     * from the old NPEvent structure -- it's no longer the native event
     * record, but is instead a struct. This was done for future extensibility,
     * and so that the Mac could receive the window argument too. For Windows
     * and OS2, it's always been a struct, so there's no change for them.
     *
     * (Corresponds to NPP_HandleEvent.)
     *
     * @param aEvent   - the event to be handled
     * @param aHandled - set to PR_TRUE if event was handled
     * @result - NS_OK if this operation was successful
     */
    /* void handleEvent (in nsPluginEventPtr aEvent, out boolean aHandled); */
    NS_IMETHOD HandleEvent(nsPluginEvent * aEvent, PRBool *aHandled)
    {
        UP_ENTER_RC();
        dprintf(("%s: aEvent=%p, aHandled=%p", pszFunction, aEvent, aHandled));
        rc = VFTCALL2((VFTnsIPluginInstance*)mpvVFTable, HandleEvent, mpvThis, aEvent, aHandled);
        if (VALID_PTR(aHandled))
            dprintf(("%s: *aHandled=%d", pszFunction, *aHandled));
        UP_LEAVE_INT(rc);
        return rc;
    }


    /**
     * Constructor.
     */
    UpPluginInstance(void *pvThis) :
        UpSupportsBase(pvThis, (nsIPluginInstance*)this, kPluginInstanceIID)
    {
    }

    /**
     * Destructor.
     */
    ~UpPluginInstance()
    {
    }
};



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIPluginInstancePeer
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsIPluginInstancePeer Base Wrapper
 */
class UpPluginInstancePeerBase : public UpSupportsBase
{
protected:
    /**
     * Returns the value of a variable associated with the plugin manager.
     *
     * (Corresponds to NPN_GetValue.)
     *
     * @param aVariable - the plugin manager variable to get
     * @param aValue    - the address of where to store the resulting value
     * @result          - NS_OK if this operation was successful
     */
    /* void getValue (in nsPluginInstancePeerVariable aVariable, in voidPtr aValue); */
    NS_IMETHOD hlpGetValue(nsPluginInstancePeerVariable aVariable, void * aValue)
    {
        UP_ENTER_RC();
        dprintf(("%s: aVariable=%d, aValue=%p", pszFunction, aVariable, aValue));
        rc = VFTCALL2((VFTnsIPluginInstancePeer*)mpvVFTable, GetValue, mpvThis, aVariable, aValue);
        if (VALID_PTR(aValue))
            dprintf(("%s: *aValue=%p", pszFunction, aValue));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * Returns the MIME type of the plugin instance.
       *
       * (Corresponds to NPP_New's MIMEType argument.)
       *
     * @param aMIMEType - resulting MIME type
     * @result          - NS_OK if this operation was successful
       */
    /* readonly attribute nsMIMEType MIMEType; */
    NS_IMETHOD hlpGetMIMEType(nsMIMEType *aMIMEType)
    {
        UP_ENTER_RC();
        dprintf(("%s: aMIMEType=%p", pszFunction, aMIMEType));
        rc = VFTCALL1((VFTnsIPluginInstancePeer*)mpvVFTable, GetMIMEType, mpvThis, aMIMEType);
        if (NS_SUCCEEDED(rc) && VALID_PTR(aMIMEType) && VALID_PTR(*aMIMEType))
            DPRINTF_STR(*aMIMEType);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * Returns the mode of the plugin instance, i.e. whether the plugin is
       * embedded in the html, or full page.
       *
       * (Corresponds to NPP_New's mode argument.)
       *
     * @param result - the resulting mode
     * @result       - NS_OK if this operation was successful
       */
    /* readonly attribute nsPluginMode mode; */
    NS_IMETHOD hlpGetMode(nsPluginMode *aMode)
    {
        UP_ENTER_RC();
        dprintf(("%s: aMode=%p", pszFunction, aMode));
        rc = VFTCALL1((VFTnsIPluginInstancePeer*)mpvVFTable, GetMode, mpvThis, aMode);
        if (NS_SUCCEEDED(rc) && VALID_PTR(aMode))
            dprintf(("%s: *aMode=%d", pszFunction, aMode));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * This operation is called by the plugin instance when it wishes to send
       * a stream of data to the browser. It constructs a new output stream to which
       * the plugin may send the data. When complete, the Close and Release methods
       * should be called on the output stream.
       *
       * (Corresponds to NPN_NewStream.)
       *
     * @param aType   - MIME type of the stream to create
     * @param aTarget - the target window name to receive the data
     * @param aResult - the resulting output stream
     * @result        - NS_OK if this operation was successful
       */
    /* void newStream (in nsMIMEType aType, in string aTarget, out nsIOutputStream aResult); */
    NS_IMETHOD hlpNewStream(nsMIMEType aType, const char *aTarget, nsIOutputStream **aResult)
    {
        //@todo TEXT: aTarget? Doesn't apply to java plugin I think.
        UP_ENTER_RC();
        dprintf(("%s: aResult=%p", pszFunction, aResult));
        DPRINTF_STR(aTarget);
        DPRINTF_STR(aType);

        rc = VFTCALL3((VFTnsIPluginInstancePeer*)mpvVFTable, NewStream, mpvThis, aType, aTarget, aResult);
        rc = upCreateWrapper((void**)aResult, kOutputStreamIID, rc);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * This operation causes status information to be displayed on the window
       * associated with the plugin instance.
       *
       * (Corresponds to NPN_Status.)
       *
       * @param aMessage - the status message to display
       * @result         - NS_OK if this operation was successful
       */
    /* void showStatus (in string aMessage); */
    NS_IMETHOD hlpShowStatus(const char *aMessage)
    {
        //@todo TEXT: aMessage? This must be further tested!
        UP_ENTER_RC();
        DPRINTF_STR(aMessage);
        rc = VFTCALL1((VFTnsIPluginInstancePeer*)mpvVFTable, ShowStatus, mpvThis, aMessage);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * Set the desired size of the window in which the plugin instance lives.
       *
     * @param aWidth  - new window width
     * @param aHeight - new window height
     * @result        - NS_OK if this operation was successful
       */
    /* void setWindowSize (in unsigned long aWidth, in unsigned long aHeight); */
    NS_IMETHOD hlpSetWindowSize(PRUint32 aWidth, PRUint32 aHeight)
    {
        UP_ENTER_RC();
        dprintf(("%s: aWidth=%d, aHeight=%d", pszFunction, aWidth, aHeight));
        rc = VFTCALL2((VFTnsIPluginInstancePeer*)mpvVFTable, SetWindowSize, mpvThis, aWidth, aHeight);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Constructor.
     *
     * @param   pvThis          Pointer to the Win32 class.
     * @param   pvInterface     Pointer to the interface we implement. Meaning
     *                          what's returned on a query interface.
     * @param   iid             The  Interface ID of the wrapped interface.
     */
    UpPluginInstancePeerBase(void *pvThis, void *pvInterface, REFNSIID iid) :
        UpSupportsBase(pvThis, pvInterface, iid)
    {
    }
};


/**
 * nsIPluginInstancePeer Wrapper
 */
class UpPluginInstancePeer : public nsIPluginInstancePeer, public UpPluginInstancePeerBase
{
public:
    UP_IMPL_NSISUPPORTS();
    UP_IMPL_NSIPLUGININSTANCEPEER();

    /**
     * Constructor
     */
    UpPluginInstancePeer(void *pvThis) :
        UpPluginInstancePeerBase(pvThis, (nsIPluginInstancePeer*)this, kPluginInstancePeerIID)
    {
    }


};




//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIPluginInstancePeer2
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
/**
 * nsIPluginInstancePeer2 Wrapper
 */
class UpPluginInstancePeer2 : public nsIPluginInstancePeer2, public UpPluginInstancePeerBase
{
public:
    UP_IMPL_NSISUPPORTS();
    UP_IMPL_NSIPLUGININSTANCEPEER();


    /**
     * Get the JavaScript window object corresponding to this plugin instance.
     *
     * @param aJSWindow - the resulting JavaScript window object
     * @result          - NS_OK if this operation was successful
     */
    /* readonly attribute JSObjectPtr JSWindow; */
    NS_IMETHOD GetJSWindow(JSObject * *aJSWindow)
    {
        UP_ENTER_RC();
        rc = VFTCALL1((VFTnsIPluginInstancePeer2*)mpvVFTable, GetJSWindow, mpvThis, aJSWindow);
        if (NS_SUCCEEDED(rc) && VALID_PTR(aJSWindow))
        {
            //@todo wrap aJSWindow? YES (*aJSWindow)->map->ops must be wrapped.
            //      struct JSObjectOps in jsapi.h defines that vtable.
            dprintf(("%s: aJSWindow=%p - JS Wrapping !!!", pszFunction, aJSWindow));
            DebugInt3();
        }
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Get the JavaScript execution thread corresponding to this plugin instance.
     *
     * @param aJSThread - the resulting JavaScript thread id
     * @result            - NS_OK if this operation was successful
     */
    /* readonly attribute unsigned long JSThread; */
    NS_IMETHOD GetJSThread(PRUint32 *aJSThread)
    {
        UP_ENTER_RC();
        rc = VFTCALL1((VFTnsIPluginInstancePeer2*)mpvVFTable, GetJSThread, mpvThis, aJSThread);
        if (NS_SUCCEEDED(rc) && VALID_PTR(aJSThread))
            dprintf(("%s: aJSThread=%u", pszFunction, aJSThread));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Get the JavaScript context to this plugin instance.
     *
     * @param aJSContext - the resulting JavaScript context
     * @result           - NS_OK if this operation was successful
     */
    /* readonly attribute JSContextPtr JSContext; */
    NS_IMETHOD GetJSContext(JSContext * *aJSContext)
    {
        UP_ENTER_RC();
        rc = VFTCALL1((VFTnsIPluginInstancePeer2*)mpvVFTable, GetJSContext, mpvThis, aJSContext);
        if (NS_SUCCEEDED(rc) && VALID_PTR(aJSContext))
        {
            //@todo wrap aJSContext? YES YES YES YES
            //      struct JSObjectOps in jsapi.h defines that vtable.
            dprintf(("%s: aJSContext=%p - JS Wrapping !!!", pszFunction, aJSContext));
            DebugInt3();
        }
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
      * Drop our reference to our owner.
      */
    NS_IMETHOD InvalidateOwner() { return NS_ERROR_NOT_IMPLEMENTED; }

    /**
     * Constructor
     */
    UpPluginInstancePeer2(void *pvThis) :
        UpPluginInstancePeerBase(pvThis, (nsIPluginInstancePeer2*)this, kPluginInstancePeer2IID)
    {
    }


};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIPluginTagInfo
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
class UpPluginTagInfo : public nsIPluginTagInfo, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * QueryInterface on nsIPluginInstancePeer to get this.
     *
     * (Corresponds to NPP_New's argc, argn, and argv arguments.)
     * Get a ptr to the paired list of attribute names and values,
     * returns the length of the array.
     *
     * Each name or value is a null-terminated string.
     */
    /* void getAttributes (in PRUint16Ref aCount, in constCharStarConstStar aNames, in constCharStarConstStar aValues); */
    NS_IMETHOD GetAttributes(PRUint16 & aCount, const char* const* & aNames, const char* const* & aValues)
    {
        UP_ENTER_RC();
        rc = VFTCALL3((VFTnsIPluginTagInfo*)mpvVFTable, GetAttributes, mpvThis, aCount, aNames, aValues);
        if (NS_SUCCEEDED(rc))
        {
            dprintf(("%s: aCount=%d", pszFunction, aCount));
            for (int i = 0; i < aCount; i++)
                dprintf(("%s: aNames[%d]='%s' (%p) aValues[%d]='%s' (%p)", pszFunction,
                         i, aNames[i], aNames[i], i, aValues[i], aValues[i]));
        }
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * Gets the value for the named attribute.
       *
     * @param aName   - the name of the attribute to find
     * @param aResult - the resulting attribute
       * @result - NS_OK if this operation was successful, NS_ERROR_FAILURE if
       * this operation failed. result is set to NULL if the attribute is not found
       * else to the found value.
       */
    /* void getAttribute (in string aName, out constCharPtr aResult); */
    NS_IMETHOD GetAttribute(const char *aName, const char * *aResult)
    {
        //@todo TEXT: aName? aResult? I believe that this one works as the text format is that of the document.
        UP_ENTER_RC();
        rc = VFTCALL2((VFTnsIPluginTagInfo*)mpvVFTable, GetAttribute, mpvThis, aName, aResult);
        if (NS_SUCCEEDED(rc) && VALID_PTR(aResult) && VALID_PTR(*aResult))
            DPRINTF_STR(*aResult);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Constructor.
     */
    UpPluginTagInfo(void *pvThis) :
        UpSupportsBase(pvThis, (nsIPluginTagInfo*)pvThis, kPluginTagInfoIID)
    {
    }
};



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIJVMWindow
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Base class for nsIJVMWindow decendants.
 */
class UpJVMWindowBase : public UpSupportsBase
{
protected:
    NS_IMETHOD hlpShow(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIJVMWindow*)mpvVFTable, Show, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD hlpHide(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIJVMWindow*)mpvVFTable, Hide, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD hlpIsVisible(PRBool *result)
    {
        UP_ENTER_RC();
        rc = VFTCALL1((VFTnsIJVMWindow*)mpvVFTable, IsVisible, mpvThis, result);
        if (NS_SUCCEEDED(rc) && VALID_PTR(result))
            dprintf(("%s: *result=%d", pszFunction, *result));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpJVMWindowBase(void *pvThis, void *pvInterface, REFNSIID aIID) :
        UpSupportsBase(pvThis, pvInterface, aIID)
    {
    }
};

/**
 * nsIJVMWindow
 */
class UpJVMWindow : public nsIJVMWindow, public UpJVMWindowBase
{
public:
    UP_IMPL_NSISUPPORTS();
    UP_IMPL_NSIJVMWINDOW();

    /** Constructor */
    UpJVMWindow(void *pvThis) :
        UpJVMWindowBase(pvThis, (nsIJVMWindow*)this, kJVMWindowIID)
    {
    }
};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIJVMConsole
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
/**
 * nsIJVMConsole
 */
class UpJVMConsole : public nsIJVMConsole, public UpJVMWindowBase
{
public:
    UP_IMPL_NSISUPPORTS();
    UP_IMPL_NSIJVMWINDOW();


    // Prints a message to the Java console. The encodingName specifies the
    // encoding of the message, and if NULL, specifies the default platform
    // encoding.
    NS_IMETHOD Print(const char* msg, const char* encodingName = NULL)
    {
        UP_ENTER_RC();
        DPRINTF_STR(msg);
        DPRINTF_STRNULL(encodingName);
        rc = VFTCALL2((VFTnsIJVMConsole*)mpvVFTable, Print, mpvThis, msg, encodingName);
        UP_LEAVE_INT(rc);
        return rc;
    }


    /** Constructor */
    UpJVMConsole(void *pvThis) :
        UpJVMWindowBase(pvThis, (nsIJVMWindow*)this, kJVMConsoleIID)
    {
    }
};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIEventHandler
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsIEventHandler
 */
class UpEventHandler : public nsIEventHandler, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * Handles an event. An nsIEventHandler can also get registered with with
     * nsIPluginManager2::RegisterWindow and will be called whenever an event
     * comes in for that window.
     *
     * Note that for Unix and Mac the nsPluginEvent structure is different
     * from the old NPEvent structure -- it's no longer the native event
     * record, but is instead a struct. This was done for future extensibility,
     * and so that the Mac could receive the window argument too. For Windows
     * and OS2, it's always been a struct, so there's no change for them.
     *
     * (Corresponds to NPP_HandleEvent.)
     *
     * @param aEvent   - the event to be handled
     * @param aHandled - set to PR_TRUE if event was handled
     * @result         - NS_OK if this operation was successful
     */
    /* void handleEvent (in nsPluginEventPtr aEvent, out boolean aHandled); */
    NS_IMETHOD HandleEvent(nsPluginEvent *aEvent, PRBool *aHandled)
    {
        UP_ENTER_RC();
        rc = VFTCALL2((VFTnsIEventHandler*)mpvVFTable, HandleEvent, mpvThis, aEvent, aHandled);
        if (NS_SUCCEEDED(rc) && VALID_PTR(aHandled))
            dprintf(("%s: *aHandled=%d", pszFunction, *aHandled));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpEventHandler(void *pvThis) :
        UpSupportsBase(pvThis, (nsIEventHandler*)this, kEventHandlerIID)
    {
    }
};



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIRunnable
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsIRunnable
 */
class UpRunnable : public nsIRunnable, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * Defines an entry point for a newly created thread.
     */
    NS_IMETHOD Run()
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIRunnable*)mpvVFTable, Run, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpRunnable(void *pvThis) :
        UpSupportsBase(pvThis, (nsIRunnable*)this, kRunnableIID)
    {
    }
};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: UpSecurityContext
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * UpSecurityContext
 */
class UpSecurityContext : public nsISecurityContext, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();


    /**
     * Get the security context to be used in LiveConnect.
     * This is used for JavaScript <--> Java.
     *
     * @param target        -- Possible target.
     * @param action        -- Possible action on the target.
     * @return              -- NS_OK if the target and action is permitted on the security context.
     *                      -- NS_FALSE otherwise.
     */
    NS_IMETHOD Implies(const char* target, const char* action, PRBool *bAllowedAccess)
    {
        UP_ENTER_RC();
        dprintf(("%s: target=%p action=%p bAllowedAccess=%p", pszFunction, target, action, bAllowedAccess));
        DPRINTF_STR(target);
        DPRINTF_STR(action);
        rc = VFTCALL3((VFTnsISecurityContext*)mpvVFTable, Implies, mpvThis, target, action, bAllowedAccess);
        if (NS_SUCCEEDED(rc) && VALID_PTR(bAllowedAccess))
            dprintf(("%s: *bAllowedAccess=%d", pszFunction, *bAllowedAccess));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Get the origin associated with the context.
     *
     * @param buf        -- Result buffer (managed by the caller.)
     * @param len        -- Buffer length.
     * @return           -- NS_OK if the codebase string was obtained.
     *                   -- NS_FALSE otherwise.
     */
    NS_IMETHOD GetOrigin(char* buf, int len)
    {
        UP_ENTER_RC();
        dprintf(("%s: buf=%p len=%d ", pszFunction, buf, len));
        rc = VFTCALL2((VFTnsISecurityContext*)mpvVFTable, GetOrigin, mpvThis, buf, len);
        if (NS_SUCCEEDED(rc) && VALID_PTR(buf))
            DPRINTF_STR(buf);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Get the certificate associated with the context.
     *
     * @param buf        -- Result buffer (managed by the caller.)
     * @param len        -- Buffer length.
     * @return           -- NS_OK if the codebase string was obtained.
     *                   -- NS_FALSE otherwise.
     */
    NS_IMETHOD GetCertificateID(char* buf, int len)
    {
        UP_ENTER_RC();
        dprintf(("%s: buf=%p len=%d ", pszFunction, buf, len));
        rc = VFTCALL2((VFTnsISecurityContext*)mpvVFTable, GetCertificateID, mpvThis, buf, len);
        if (NS_SUCCEEDED(rc) && VALID_PTR(buf))
            DPRINTF_STR(*buf);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpSecurityContext(void *pvThis) :
        UpSupportsBase(pvThis, (nsISecurityContext*)this, kSecurityContextIID)
    {
    }
};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIRequestObserver
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Base class for nsIJVMWindow decendants.
 */
class UpRequestObserverBase : public UpSupportsBase
{
protected:
   /**
     * Called to signify the beginning of an asynchronous request.
     *
     * @param aRequest request being observed
     * @param aContext user defined context
     *
     * An exception thrown from onStartRequest has the side-effect of
     * causing the request to be canceled.
     */
    /* void onStartRequest (in nsIRequest aRequest, in nsISupports aContext); */
    NS_IMETHOD hlpOnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
    {
        UP_ENTER_RC();
        dprintf(("%s: aRequest=%p aContext=%p", pszFunction, aRequest, aContext));

        nsIRequest *pDownRequest = aRequest;
        rc = downCreateWrapper((void**)&pDownRequest, kRequestIID, NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            nsISupports *pDownSupports = aContext;
            rc = downCreateWrapper((void**)&pDownSupports, kSupportsIID, NS_OK);
            if (NS_SUCCEEDED(rc))
                rc = VFTCALL2((VFTnsIRequestObserver*)mpvVFTable, OnStartRequest, mpvThis, pDownRequest, pDownSupports);
        }

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Called to signify the end of an asynchronous request.  This
     * call is always preceded by a call to onStartRequest.
     *
     * @param aRequest request being observed
     * @param aContext user defined context
     * @param aStatusCode reason for stopping (NS_OK if completed successfully)
     *
     * An exception thrown from onStopRequest is generally ignored.
     */
    /* void onStopRequest (in nsIRequest aRequest, in nsISupports aContext, in nsresult aStatusCode); */
    NS_IMETHOD hlpOnStopRequest(nsIRequest *aRequest, nsISupports *aContext, nsresult aStatusCode)
    {
        UP_ENTER_RC();
        dprintf(("%s: aRequest=%p aContext=%p aStatusCode", pszFunction, aRequest, aContext, aStatusCode));

        nsIRequest *pDownRequest = aRequest;
        rc = downCreateWrapper((void**)&pDownRequest, kRequestIID, NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            nsISupports *pDownSupports = aContext;
            rc = downCreateWrapper((void**)&pDownSupports, kSupportsIID, NS_OK);
            if (NS_SUCCEEDED(rc))
                rc = VFTCALL3((VFTnsIRequestObserver*)mpvVFTable, OnStopRequest, mpvThis, pDownRequest, pDownSupports, aStatusCode);
        }

        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpRequestObserverBase(void *pvThis, void *pvInterface, REFNSIID aIID) :
        UpSupportsBase(pvThis, pvInterface, aIID)
    {
    }
};


/**
 * nsRequestObserver
 */
class UpRequestObserver : public nsIRequestObserver, public UpRequestObserverBase
{
 public:
     UP_IMPL_NSISUPPORTS();
     UP_IMPL_NSIREQUESTOBSERVER();

     /** Constructor */
     UpRequestObserver(void *pvThis) :
         UpRequestObserverBase(pvThis, (nsIRequestObserver*)this, kRequestObserverIID)
     {
     }
};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIStreamListener
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * UpSecurityContext
 */
class UpStreamListener : public nsIStreamListener, public UpRequestObserverBase
{
public:
    UP_IMPL_NSISUPPORTS();
    UP_IMPL_NSIREQUESTOBSERVER();

    /**
     * Called when the next chunk of data (corresponding to the request) may
     * be read without blocking the calling thread.  The onDataAvailable impl
     * must read exactly |aCount| bytes of data before returning.
     *
     * @param aRequest request corresponding to the source of the data
     * @param aContext user defined context
     * @param aInputStream input stream containing the data chunk
     * @param aOffset current stream position
     * @param aCount number of bytes available in the stream
     *
     * An exception thrown from onDataAvailable has the side-effect of
     * causing the request to be canceled.
     */
    /* void onDataAvailable (in nsIRequest aRequest, in nsISupports aContext, in nsIInputStream aInputStream, in unsigned long aOffset, in unsigned long aCount); */
    NS_IMETHOD OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext, nsIInputStream *aInputStream, PRUint32 aOffset, PRUint32 aCount)
    {
        UP_ENTER_RC();
        dprintf(("%s: aRequest=%p aContext=%p aInputStream=%p aOffset=%d aCount=%d", pszFunction, aRequest, aContext, aInputStream, aOffset, aCount));

        nsIRequest *pDownRequest = aRequest;
        rc = downCreateWrapper((void**)&pDownRequest, kRequestIID, NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            nsISupports *pDownSupports = aContext;
            rc = downCreateWrapper((void**)&pDownSupports, kSupportsIID, NS_OK);
            if (NS_SUCCEEDED(rc))
            {
                nsIInputStream *pDownInputStream = aInputStream;
                rc = downCreateWrapper((void**)&pDownInputStream, kInputStreamIID, NS_OK);
                if (NS_SUCCEEDED(rc))
                    rc = VFTCALL5((VFTnsIStreamListener*)mpvVFTable, OnDataAvailable, mpvThis,
                                  pDownRequest, pDownSupports, pDownInputStream, aOffset, aCount);
            }
        }

        UP_LEAVE_INT(rc);
        return rc;
    }


    /** Constructor */
    UpStreamListener(void *pvThis) :
        UpRequestObserverBase(pvThis, (nsIStreamListener*)this, kStreamListenerIID)
    {
    }
};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIRequest
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsIRequest
 */
class UpRequestBase : public UpSupportsBase
{
protected:

    /**
       * The name of the request.  Often this is the URI of the request.
       */
    /* readonly attribute AUTF8String name; */
    NS_IMETHOD hlpGetName(nsACString & aName)
    {
        UP_ENTER_RC();
        dprintf(("%s: &aName=%p", pszFunction, &aName));
        /** @todo Wrap nsACString */
        dprintf(("%s: nsACString wrapping isn't supported.", pszFunction));
        ReleaseInt3(0xbaddbeef, 32, 0);

        rc = VFTCALL1((VFTnsIRequest*)mpvVFTable, GetName, mpvThis, aName);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * @return TRUE if the request has yet to reach completion.
       * @return FALSE if the request has reached completion (e.g., after
       *   OnStopRequest has fired).
       * Suspended requests are still considered pending.
       */
    /* boolean isPending (); */
    NS_IMETHOD hlpIsPending(PRBool *_retval)
    {
        UP_ENTER_RC();
        dprintf(("%s: _retval=%p", pszFunction, _retval));

        rc = VFTCALL1((VFTnsIRequest*)mpvVFTable, IsPending, mpvThis, _retval);
        if (VALID_PTR(_retval))
            dprintf(("%s: *_retval=%d", pszFunction, *_retval));

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * The error status associated with the request.
       */
    /* readonly attribute nsresult status; */
    NS_IMETHOD hlpGetStatus(nsresult *aStatus)
    {
        UP_ENTER_RC();
        dprintf(("%s: aStatus=%p", pszFunction, aStatus));

        rc = VFTCALL1((VFTnsIRequest*)mpvVFTable, GetStatus, mpvThis, aStatus);
        if (VALID_PTR(aStatus))
            dprintf(("%s: *aStatus=%d", pszFunction, *aStatus));

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * Cancels the current request.  This will close any open input or
       * output streams and terminate any async requests.  Users should
       * normally pass NS_BINDING_ABORTED, although other errors may also
       * be passed.  The error passed in will become the value of the
       * status attribute.
       *
       * @param aStatus the reason for canceling this request.
       *
       * NOTE: most nsIRequest implementations expect aStatus to be a
       * failure code; however, some implementations may allow aStatus to
       * be a success code such as NS_OK.  In general, aStatus should be
       * a failure code.
       */
    /* void cancel (in nsresult aStatus); */
    NS_IMETHOD hlpCancel(nsresult aStatus)
    {
        UP_ENTER_RC();
        dprintf(("%s: aStatus=%p", pszFunction, aStatus));

        rc = VFTCALL1((VFTnsIRequest*)mpvVFTable, Cancel, mpvThis, aStatus);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * Suspends the current request.  This may have the effect of closing
       * any underlying transport (in order to free up resources), although
       * any open streams remain logically opened and will continue delivering
       * data when the transport is resumed.
       *
       * NOTE: some implementations are unable to immediately suspend, and
       * may continue to deliver events already posted to an event queue. In
       * general, callers should be capable of handling events even after
       * suspending a request.
       */
    /* void suspend (); */
    NS_IMETHOD hlpSuspend(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIRequest*)mpvVFTable, Suspend, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * Resumes the current request.  This may have the effect of re-opening
       * any underlying transport and will resume the delivery of data to
       * any open streams.
       */
    /* void resume (); */
    NS_IMETHOD hlpResume(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIRequest*)mpvVFTable, Resume, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * The load group of this request.  While pending, the request is a
       * member of the load group.  It is the responsibility of the request
       * to implement this policy.
       */
    /* attribute nsILoadGroup loadGroup; */
    NS_IMETHOD hlpGetLoadGroup(nsILoadGroup * *aLoadGroup)
    {
        UP_ENTER_RC();
        dprintf(("%s: aLoadGroup=%p\n", pszFunction, aLoadGroup));

        rc = VFTCALL1((VFTnsIRequest*)mpvVFTable, GetLoadGroup, mpvThis, aLoadGroup);
        rc = upCreateWrapper((void**)aLoadGroup, kLoadGroupIID, rc);

        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD hlpSetLoadGroup(nsILoadGroup * aLoadGroup)
    {
        UP_ENTER_RC();
        dprintf(("%s: aLoadGroup=%p\n", pszFunction, aLoadGroup));

        nsILoadGroup *pDownLoadGroup = aLoadGroup;
        rc = downCreateWrapper((void**)&pDownLoadGroup, kLoadGroupIID, NS_OK);
        if (NS_SUCCEEDED(rc))
          rc = VFTCALL1((VFTnsIRequest*)mpvVFTable, SetLoadGroup, mpvThis, pDownLoadGroup);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * The load flags of this request.  Bits 0-15 are reserved.
       *
       * When added to a load group, this request's load flags are merged with
       * the load flags of the load group.
       */
    /* attribute nsLoadFlags loadFlags; */
    NS_IMETHOD hlpGetLoadFlags(nsLoadFlags *aLoadFlags)
    {
        UP_ENTER_RC();
        dprintf(("%s: aLoadFlags=%p\n", pszFunction, aLoadFlags));

        rc = VFTCALL1((VFTnsIRequest*)mpvVFTable, GetLoadFlags, mpvThis, aLoadFlags);
        if (VALID_PTR(aLoadFlags))
            dprintf(("%s: *aLoadFlags=%d", pszFunction, *aLoadFlags));

        UP_LEAVE_INT(rc);
        return rc;
    }


    NS_IMETHOD hlpSetLoadFlags(nsLoadFlags aLoadFlags)
    {
        UP_ENTER_RC();
        dprintf(("%s: aLoadFlags=%#x\n", pszFunction, aLoadFlags));

        rc = VFTCALL1((VFTnsIRequest*)mpvVFTable, SetLoadFlags, mpvThis, aLoadFlags);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpRequestBase(void *pvThis, void *pvInterface, REFNSIID aIID) :
        UpSupportsBase(pvThis, pvInterface, aIID)
    {
    }

};


/**
 * nsRequest
 */
class UpRequest : public nsIRequest, public UpRequestBase
{
public:
     UP_IMPL_NSISUPPORTS();
     UP_IMPL_NSIREQUEST();

     /** Constructor */
     UpRequest(void *pvThis) :
         UpRequestBase(pvThis, (nsIRequest*)this, kRequestIID)
     {
     }
};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsILoadGroup
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsILoadGroup
 */
class UpLoadGroup : public nsILoadGroup, public UpRequestBase
{
public:
    UP_IMPL_NSISUPPORTS();
    UP_IMPL_NSIREQUEST();

    /**
     * The group observer is notified when requests are added to and removed
     * from this load group.  The groupObserver is weak referenced.
     */
    /* attribute nsIRequestObserver groupObserver; */
    NS_IMETHOD GetGroupObserver(nsIRequestObserver * *aGroupObserver)
    {
        UP_ENTER_RC();
        dprintf(("%s: aGroupObserver=%p\n", pszFunction, aGroupObserver));

        rc = VFTCALL1((VFTnsILoadGroup*)mpvVFTable, GetGroupObserver, mpvThis, aGroupObserver);
        rc = upCreateWrapper((void**)aGroupObserver, kRequestObserverIID, rc);

        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD SetGroupObserver(nsIRequestObserver * aGroupObserver)
    {
        UP_ENTER_RC();
        dprintf(("%s: aGroupObserver=%p\n", pszFunction, aGroupObserver));

        nsIRequestObserver *pDownGroupObserver = aGroupObserver;
        rc = downCreateWrapper((void**)&pDownGroupObserver, kRequestObserverIID, NS_OK);
        if (NS_SUCCEEDED(rc))
          rc = VFTCALL1((VFTnsILoadGroup*)mpvVFTable, SetGroupObserver, mpvThis, pDownGroupObserver);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Accesses the default load request for the group.  Each time a number
     * of requests are added to a group, the defaultLoadRequest may be set
     * to indicate that all of the requests are related to a base request.
     *
     * The load group inherits its load flags from the default load request.
     * If the default load request is NULL, then the group's load flags are
     * not changed.
     */
    /* attribute nsIRequest defaultLoadRequest; */
    NS_IMETHOD GetDefaultLoadRequest(nsIRequest * *aDefaultLoadRequest)
    {
        UP_ENTER_RC();
        dprintf(("%s: aDefaultLoadRequest=%p\n", pszFunction, aDefaultLoadRequest));

        rc = VFTCALL1((VFTnsILoadGroup*)mpvVFTable, GetDefaultLoadRequest, mpvThis, aDefaultLoadRequest);
        rc = upCreateWrapper((void**)aDefaultLoadRequest, kRequestIID, rc);

        UP_LEAVE_INT(rc);
        return rc;
    }
    NS_IMETHOD SetDefaultLoadRequest(nsIRequest * aDefaultLoadRequest)
    {
        UP_ENTER_RC();
        dprintf(("%s: aDefaultLoadRequest=%p\n", pszFunction, aDefaultLoadRequest));

        nsIRequest *pDownDefaultLoadRequest = aDefaultLoadRequest;
        rc = downCreateWrapper((void**)&pDownDefaultLoadRequest, kRequestIID, NS_OK);
        if (NS_SUCCEEDED(rc))
          rc = VFTCALL1((VFTnsILoadGroup*)mpvVFTable, SetDefaultLoadRequest, mpvThis, pDownDefaultLoadRequest);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Adds a new request to the group.  This will cause the default load
     * flags to be applied to the request.  If this is a foreground
     * request then the groupObserver's onStartRequest will be called.
     *
     * If the request is the default load request or if the default load
     * request is null, then the load group will inherit its load flags from
     * the request.
     */
    /* void addRequest (in nsIRequest aRequest, in nsISupports aContext); */
    NS_IMETHOD AddRequest(nsIRequest *aRequest, nsISupports *aContext)
    {
        UP_ENTER_RC();
        dprintf(("%s: aRequest=%p aContext=%p", pszFunction, aRequest, aContext));

        nsIRequest *pDownRequest = aRequest;
        rc = downCreateWrapper((void**)&pDownRequest, kRequestIID, NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            nsISupports *pDownSupports = aContext;
            rc = downCreateWrapper((void**)&pDownSupports, kSupportsIID, NS_OK);
            if (NS_SUCCEEDED(rc))
                rc = VFTCALL2((VFTnsILoadGroup*)mpvVFTable, AddRequest, mpvThis, pDownRequest, pDownSupports);
        }

        UP_LEAVE_INT(rc);
        return rc;
    }


    /**
     * Removes a request from the group.  If this is a foreground request
     * then the groupObserver's onStopRequest will be called.
     */
    /* void removeRequest (in nsIRequest aRequest, in nsISupports aContext, in nsresult aStatus); */
    NS_IMETHOD RemoveRequest(nsIRequest *aRequest, nsISupports *aContext, nsresult aStatus)
    {
        UP_ENTER_RC();
        dprintf(("%s: aRequest=%p aContext=%p aStatus=%d", pszFunction, aRequest, aContext, aStatus));

        nsIRequest *pDownRequest = aRequest;
        rc = downCreateWrapper((void**)&pDownRequest, kRequestIID, NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            nsISupports *pDownSupports = aContext;
            rc = downCreateWrapper((void**)&pDownSupports, kSupportsIID, NS_OK);
            if (NS_SUCCEEDED(rc))
                rc = VFTCALL3((VFTnsILoadGroup*)mpvVFTable, RemoveRequest, mpvThis, pDownRequest, pDownSupports, aStatus);
        }

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Returns the requests contained directly in this group.
     * Enumerator element type: nsIRequest.
     */
    /* readonly attribute nsISimpleEnumerator requests; */
    NS_IMETHOD GetRequests(nsISimpleEnumerator * *aRequests)
    {
        UP_ENTER_RC();
        dprintf(("%s: aRequests=%p", pszFunction, aRequests));

        rc = VFTCALL1((VFTnsILoadGroup*)mpvVFTable, GetRequests, mpvThis, aRequests);
        rc = upCreateWrapper((void**)aRequests, kSimpleEnumeratorIID, rc);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Returns the count of "active" requests (ie. requests without the
     * LOAD_BACKGROUND bit set).
     */
    /* readonly attribute unsigned long activeCount; */
    NS_IMETHOD GetActiveCount(PRUint32 *aActiveCount)
    {
        UP_ENTER_RC();
        dprintf(("%s: aActiveCount=%p", pszFunction, aActiveCount));

        rc = VFTCALL1((VFTnsILoadGroup*)mpvVFTable, GetActiveCount, mpvThis, aActiveCount);
        if (VALID_PTR(aActiveCount))
            dprintf(("%s: *aActiveCount=%d", pszFunction, *aActiveCount));

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Notification callbacks for the load group.
     */
    /* attribute nsIInterfaceRequestor notificationCallbacks; */
    NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks)
    {
        UP_ENTER_RC();
        dprintf(("%s: aNotificationCallbacks=%p", pszFunction, aNotificationCallbacks));

        rc = VFTCALL1((VFTnsILoadGroup*)mpvVFTable, GetNotificationCallbacks, mpvThis, aNotificationCallbacks);
        rc = upCreateWrapper((void**)aNotificationCallbacks, kInterfaceRequestorIID, rc);

        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks)
    {
        UP_ENTER_RC();
        dprintf(("%s: aNotificationCallbacks=%p", pszFunction, aNotificationCallbacks));

        nsIInterfaceRequestor *pDownNotificationCallbacks = aNotificationCallbacks;
        rc = downCreateWrapper((void**)&pDownNotificationCallbacks, kInterfaceRequestorIID, rc);
        if (NS_SUCCEEDED(rc))
            rc = VFTCALL1((VFTnsILoadGroup*)mpvVFTable, SetNotificationCallbacks, mpvThis, pDownNotificationCallbacks);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpLoadGroup(void *pvThis) :
        UpRequestBase(pvThis, (nsILoadGroup*)this, kLoadGroupIID)
    {
    }

};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsISimpleEnumerator
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsISimpleEnumerator
 */
class UpSimpleEnumerator : public nsISimpleEnumerator, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * Called to determine whether or not the enumerator has
     * any elements that can be returned via getNext(). This method
     * is generally used to determine whether or not to initiate or
     * continue iteration over the enumerator, though it can be
     * called without subsequent getNext() calls. Does not affect
     * internal state of enumerator.
     *
     * @see getNext()
     * @return PR_TRUE if there are remaining elements in the enumerator.
     *         PR_FALSE if there are no more elements in the enumerator.
     */
    /* boolean hasMoreElements (); */
    NS_IMETHOD HasMoreElements(PRBool *_retval)
    {
        UP_ENTER_RC();
        dprintf(("%s: _retval=%p\n", pszFunction, _retval));

        rc = VFTCALL1((VFTnsISimpleEnumerator*)mpvVFTable, HasMoreElements, mpvThis, _retval);
        if (VALID_PTR(_retval))
            dprintf(("%s: *_retval=%d", pszFunction, *_retval));

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Called to retrieve the next element in the enumerator. The "next"
     * element is the first element upon the first call. Must be
     * pre-ceeded by a call to hasMoreElements() which returns PR_TRUE.
     * This method is generally called within a loop to iterate over
     * the elements in the enumerator.
     *
     * @see hasMoreElements()
     * @return NS_OK if the call succeeded in returning a non-null
     *               value through the out parameter.
     *         NS_ERROR_FAILURE if there are no more elements
     *                          to enumerate.
     * @return the next element in the enumeration.
     */
    /* nsISupports getNext (); */
    NS_IMETHOD GetNext(nsISupports **_retval)
    {
        UP_ENTER_RC();
        dprintf(("%s: _retval=%p", pszFunction, _retval));

        rc = VFTCALL1((VFTnsISimpleEnumerator*)mpvVFTable, GetNext, mpvThis, _retval);
        rc = upCreateWrapper((void**)_retval, kSupportsIID, rc);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpSimpleEnumerator(void *pvThis) :
        UpSupportsBase(pvThis, (nsISimpleEnumerator*)this, kSimpleEnumeratorIID)
    {
    }
};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIInterfaceRequestor
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsIInterfaceRequstor
 */
class UpInterfaceRequestor : public nsIInterfaceRequestor, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * Retrieves the specified interface pointer.
     *
     * @param uuid The IID of the interface being requested.
     * @param result [out] The interface pointer to be filled in if
     *               the interface is accessible.
     * @return NS_OK - interface was successfully returned.
     *         NS_NOINTERFACE - interface not accessible.
     *         NS_ERROR* - method failure.
     */
    /* void getInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
    NS_IMETHOD GetInterface(const nsIID & uuid, void * *result)
    {
        UP_ENTER_RC();
        DPRINTF_NSID(uuid);

        rc = VFTCALL2((VFTnsIInterfaceRequestor*)mpvVFTable, GetInterface, mpvThis, uuid, result);
        rc = upCreateWrapper(result, uuid, rc);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpInterfaceRequestor(void *pvThis) :
        UpSupportsBase(pvThis, (nsIInterfaceRequestor*)this, kInterfaceRequestorIID)
    {
    }
};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIOutputStream
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Special wrapper function which get called from a little stub we create
 * in UpOutputStream::WriteSegments. The stub pushed the entry point of
 * the original function before calling us.
 */
nsresult __cdecl UpOutputStream_nsReadSegmentWrapper(nsReadSegmentFun pfnOrg, void *pvRet,
     nsIOutputStream *aOutStream, void *aClosure, char *aToSegment,
     PRUint32 aFromOffset, PRUint32 aCount, PRUint32 *aReadCount)
{
    DEBUG_FUNCTIONNAME();
    nsresult rc;
    dprintf(("%s: pfnOrg=%p pvRet=%p aOutStream=%p aClosure=%p aToSegment=%p aFromOffset=%d aCount=%d aReadCount=%p\n",
             pszFunction, pfnOrg, pvRet, aOutStream, aClosure, aToSegment, aFromOffset, aCount, aReadCount));

    nsIOutputStream * pupOutStream = aOutStream;
    rc = upCreateWrapper((void**)&pupOutStream, kOutputStreamIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        rc = pfnOrg(pupOutStream, aClosure, aToSegment, aFromOffset, aCount, aReadCount);
        if (VALID_PTR(aReadCount))
            dprintf(("%s: *aReadCount=%d\n", pszFunction, *aReadCount));
    }

    dprintf(("%s: rc=%d\n", pszFunction, rc));
    return rc;
}


/**
 * nsIOutputStream
 */
class UpOutputStream : public nsIOutputStream, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * Close the stream. Forces the output stream to flush any buffered data.
     *
     * @throws NS_BASE_STREAM_WOULD_BLOCK if unable to flush without blocking
     *   the calling thread (non-blocking mode only)
     */
    /* void close (); */
    NS_IMETHOD Close(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIOutputStream*)mpvVFTable, Close, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Flush the stream.
     *
     * @throws NS_BASE_STREAM_WOULD_BLOCK if unable to flush without blocking
     *   the calling thread (non-blocking mode only)
     */
    /* void flush (); */
    NS_IMETHOD Flush(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIOutputStream*)mpvVFTable, Flush, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Write data into the stream.
     *
     * @param aBuf the buffer containing the data to be written
     * @param aCount the maximum number of bytes to be written
     *
     * @return number of bytes written (may be less than aCount)
     *
     * @throws NS_BASE_STREAM_WOULD_BLOCK if writing to the output stream would
     *   block the calling thread (non-blocking mode only)
     * @throws <other-error> on failure
     */
    /* unsigned long write (in string aBuf, in unsigned long aCount); */
    NS_IMETHOD Write(const char *aBuf, PRUint32 aCount, PRUint32 *_retval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aBuf=%p aCount=%d _retval=%p", pszFunction, aBuf, aCount, _retval));
        rc = VFTCALL3((VFTnsIOutputStream*)mpvVFTable, Write, mpvThis, aBuf, aCount, _retval);
        if (VALID_PTR(_retval))
            dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Writes data into the stream from an input stream.
     *
     * @param aFromStream the stream containing the data to be written
     * @param aCount the maximum number of bytes to be written
     *
     * @return number of bytes written (may be less than aCount)
     *
     * @throws NS_BASE_STREAM_WOULD_BLOCK if writing to the output stream would
     *    block the calling thread (non-blocking mode only)
     * @throws <other-error> on failure
     *
     * NOTE: This method is defined by this interface in order to allow the
     * output stream to efficiently copy the data from the input stream into
     * its internal buffer (if any). If this method was provided as an external
     * facility, a separate char* buffer would need to be used in order to call
     * the output stream's other Write method.
     */
    /* unsigned long writeFrom (in nsIInputStream aFromStream, in unsigned long aCount); */
    NS_IMETHOD WriteFrom(nsIInputStream *aFromStream, PRUint32 aCount, PRUint32 *_retval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aFromStream=%p aCount=%d _retval=%p", pszFunction, aFromStream, aCount, _retval));

        nsIInputStream *pDownFromStream = aFromStream;
        rc = downCreateWrapper((void**)&pDownFromStream, kInputStreamIID, NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            rc = VFTCALL3((VFTnsIOutputStream*)mpvVFTable, WriteFrom, mpvThis, pDownFromStream, aCount, _retval);
            if (VALID_PTR(_retval))
                dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));
        }
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Low-level write method that has access to the stream's underlying buffer.
     * The reader function may be called multiple times for segmented buffers.
     * WriteSegments is expected to keep calling the reader until either there
     * is nothing left to write or the reader returns an error.  WriteSegments
     * should not call the reader with zero bytes to provide.
     *
     * @param aReader the "provider" of the data to be written
     * @param aClosure opaque parameter passed to reader
     * @param aCount the maximum number of bytes to be written
     *
     * @return number of bytes written (may be less than aCount)
     *
     * @throws NS_BASE_STREAM_WOULD_BLOCK if writing to the output stream would
     *    block the calling thread (non-blocking mode only)
     * @throws <other-error> on failure
     *
     * NOTE: this function may be unimplemented if a stream has no underlying
     * buffer (e.g., socket output stream).
     */
    /* [noscript] unsigned long writeSegments (in nsReadSegmentFun aReader, in voidPtr aClosure, in unsigned long aCount); */
    NS_IMETHOD WriteSegments(nsReadSegmentFun aReader, void * aClosure, PRUint32 aCount, PRUint32 *_retval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aReader=%p aClosure=%p aCount=%d _retval=%p", pszFunction, aReader, aClosure, aCount, _retval));

        int i;
        char achWrapper[32];

        /* push aReader */
        achWrapper[0] = 0x68;
        *((unsigned*)&achWrapper[1]) = (unsigned)aReader;
        i = 5;

#if VFT_VAC365
        /* optlink - mov [ebp + 08h], eax */
        achWrapper[i++] = 0x89;
        achWrapper[i++] = 0x45;
        achWrapper[i++] = 0x08;
        /* optlink - mov [ebp + 0ch], edx */
        achWrapper[i++] = 0x89;
        achWrapper[i++] = 0x55;
        achWrapper[i++] = 0x0c;
        /* optlink - mov [ebp + 10h], ecx */
        achWrapper[i++] = 0x89;
        achWrapper[i++] = 0x4d;
        achWrapper[i++] = 0x10;
#endif
        /* call UpOutputStream_nsReadSegmentWrapper */
        achWrapper[i] = 0xe8;
        *((unsigned*)&achWrapper[i+1]) = (unsigned)UpOutputStream_nsReadSegmentWrapper - (unsigned)&achWrapper[i+5];
        i += 5;

        /* add esp, 4 */
        achWrapper[i++] = 0x83;
        achWrapper[i++] = 0xc4;
        achWrapper[i++] = 0x04;

#if VFT_VAC365
        /* _Optlink - ret */
        achWrapper[i++] = 0xc3;
#elif VFT_VC60
        /* __stdcall - ret (6*4) */
        achWrapper[i++] = 0xc2;
        achWrapper[i++] = 6*4;
        achWrapper[i++] = 0;
#else
#error fixme! neither VFT_VC60 nor VFT_VAC365 was set.
#endif
        achWrapper[i++] = 0xcc;
        achWrapper[i] = 0xcc;

        /* do call - this better not be asynchronous stuff. */
        rc = VFTCALL4((VFTnsIOutputStream*)mpvVFTable, WriteSegments, mpvThis,
                      (nsReadSegmentFun)((void*)&achWrapper[0]), aClosure, aCount, _retval);
        if (VALID_PTR(_retval))
            dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * @return true if stream is non-blocking
     *
     * NOTE: writing to a blocking output stream will block the calling thread
     * until all given data can be consumed by the stream.
     */
    /* boolean isNonBlocking (); */
    NS_IMETHOD IsNonBlocking(PRBool *_retval)
    {
        UP_ENTER_RC();
        dprintf(("%s: _retval=%p", pszFunction, _retval));

        rc = VFTCALL1((VFTnsIOutputStream*)mpvVFTable, IsNonBlocking, mpvThis, _retval);
        if (VALID_PTR(_retval))
            dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));

        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpOutputStream(void *pvThis) :
        UpSupportsBase(pvThis, (nsIOutputStream*)this, kOutputStreamIID)
    {
    }
};



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIPluginStreamListener
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsIPluginStreamListener
 */
class UpPluginStreamListener : public nsIPluginStreamListener, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * Notify the observer that the URL has started to load.  This method is
     * called only once, at the beginning of a URL load.<BR><BR>
     *
     * @param aPluginInfo  - plugin stream info
     * @return             - the return value is currently ignored, in the future
     * it may be used to cancel the URL load..
     */
    /* void onStartBinding (in nsIPluginStreamInfo aPluginInfo); */
    NS_IMETHOD OnStartBinding(nsIPluginStreamInfo *aPluginInfo)
    {
        UP_ENTER_RC();
        dprintf(("%s: sPluginInfo=%p", pszFunction, aPluginInfo));

        nsIPluginStreamInfo *pDownPluginInfo = aPluginInfo;
        rc = downCreateWrapper((void**)&pDownPluginInfo, kPluginStreamInfoIID, NS_OK);
        if (NS_SUCCEEDED(rc))
            rc = VFTCALL1((VFTnsIPluginStreamListener*)mpvVFTable, OnStartBinding, mpvThis, pDownPluginInfo);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Notify the client that data is available in the input stream.  This
     * method is called whenver data is written into the input stream by the
     * networking library...<BR><BR>
     *
     * @param aPluginInfo  - plugin stream info
     * @param aInputStream - the input stream containing the data. This stream can
     * be either a blocking or non-blocking stream.
     * @param aLength      - the amount of data that was just pushed into the stream.
     * @return             - the return value is currently ignored.
     */
    /* void onDataAvailable (in nsIPluginStreamInfo aPluginInfo, in nsIInputStream aInputStream, in unsigned long aLength); */
    NS_IMETHOD OnDataAvailable(nsIPluginStreamInfo *aPluginInfo, nsIInputStream *aInputStream, PRUint32 aLength)
    {
        UP_ENTER_RC();
        dprintf(("%s: sPluginInfo=%p aInputStream=%p aLength=%d", pszFunction, aPluginInfo, aInputStream, aLength));

        nsIPluginStreamInfo *pDownPluginInfo = aPluginInfo;
        rc = downCreateWrapper((void**)&pDownPluginInfo, kPluginStreamInfoIID, NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            nsIInputStream *pDownInputStream = aInputStream;
            rc = downCreateWrapper((void**)&pDownInputStream, kInputStreamIID, NS_OK);
            if (NS_SUCCEEDED(rc))
                rc = VFTCALL3((VFTnsIPluginStreamListener*)mpvVFTable, OnDataAvailable, mpvThis, pDownPluginInfo, pDownInputStream, aLength);
        }

        UP_LEAVE_INT(rc);
        return rc;
    }


    /**
     * Notify the client that data is available in the file.
     *
     * @param aPluginInfo - plugin stream info
     * @param aFileName   - the name of the file containing the data
     * @return            - the return value is currently ignored.
     */
    /* void onFileAvailable (in nsIPluginStreamInfo aPluginInfo, in string aFileName); */
    NS_IMETHOD OnFileAvailable(nsIPluginStreamInfo *aPluginInfo, const char *aFileName)
    {
        UP_ENTER_RC();
        /** @todo NLS/Codepage Text for aFileName. */
        dprintf(("%s: sPluginInfo=%p aFileName=%p", pszFunction, aPluginInfo, aFileName));
        DPRINTF_STR(aFileName);

        nsIPluginStreamInfo *pDownPluginInfo = aPluginInfo;
        rc = downCreateWrapper((void**)&pDownPluginInfo, kPluginStreamInfoIID, NS_OK);
        if (NS_SUCCEEDED(rc))
            rc = VFTCALL2((VFTnsIPluginStreamListener*)mpvVFTable, OnFileAvailable, mpvThis, pDownPluginInfo, aFileName);

        UP_LEAVE_INT(rc);
        return rc;
    }


    /**
     * Notify the observer that the URL has finished loading.  This method is
     * called once when the networking library has finished processing the
     * URL transaction initiatied via the nsINetService::Open(...) call.<BR><BR>
     *
     * This method is called regardless of whether the URL loaded successfully.<BR><BR>
     *
     * @param aPluginInfo - plugin stream info
     * @param aStatus     - reason why the stream has been terminated
     * @return            - the return value is currently ignored.
     */
    /* void onStopBinding (in nsIPluginStreamInfo aPluginInfo, in nsresult aStatus); */
    NS_IMETHOD OnStopBinding(nsIPluginStreamInfo *aPluginInfo, nsresult aStatus)
    {
        UP_ENTER_RC();
        dprintf(("%s: sPluginInfo=%p aStatus=%d", pszFunction, aPluginInfo, aStatus));

        nsIPluginStreamInfo *pDownPluginInfo = aPluginInfo;
        rc = downCreateWrapper((void**)&pDownPluginInfo, kPluginStreamInfoIID, NS_OK);
        if (NS_SUCCEEDED(rc))
            rc = VFTCALL2((VFTnsIPluginStreamListener*)mpvVFTable, OnStopBinding, mpvThis, pDownPluginInfo, aStatus);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Gets the type of the stream
     *
     * @param aStreamType - the type of the stream
     */
    /* readonly attribute nsPluginStreamType streamType; */
    NS_IMETHOD GetStreamType(nsPluginStreamType *aStreamType)
    {
        UP_ENTER_RC();
        dprintf(("%s: aStreamType=%p", pszFunction, aStreamType));

        rc = VFTCALL1((VFTnsIPluginStreamListener*)mpvVFTable, GetStreamType, mpvThis, aStreamType);
        if (VALID_PTR(aStreamType))
            dprintf(("%s: *aStreamType=%d\n", pszFunction, aStreamType));

        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpPluginStreamListener(void *pvThis) :
        UpSupportsBase(pvThis, (nsIPluginStreamListener*)this, kPluginStreamListenerIID)
    {
    }
};



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: FlashIObject7
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//


/**
 * FlashIObject7
 */
class UpFlashIObject7 : public FlashIObject7, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * Notify the observer that the URL has started to load.  This method is
     * called only once, at the beginning of a URL load.<BR><BR>
     *
     * @param aPluginInfo  - plugin stream info
     * @return             - the return value is currently ignored, in the future
     * it may be used to cancel the URL load..
     */
    /* void onStartBinding (in nsIPluginStreamInfo aPluginInfo); */
    NS_IMETHOD Evaluate(const char *aString, FlashIObject7 **aFlashObject)
    {
        UP_ENTER_RC();
        dprintf(("%s: aString=%p aFlashObject=%p", pszFunction, aString, aFlashObject));
        DPRINTF_STR(aString);

        rc = VFTCALL2((VFTFlashIObject7*)mpvVFTable, Evaluate, mpvThis, aString, aFlashObject);
        rc = upCreateWrapper((void**)aFlashObject, kFlashIObject7IID, NS_OK);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpFlashIObject7(void *pvThis) :
        UpSupportsBase(pvThis, (FlashIObject7*)this, kFlashIObject7IID)
    {
    }
};



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: FlashIScriptablePlugin7
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * FlashIScriptablePlugin (d458fe9c-518c-11d6-84cb-0005029bc257)
 * Flash version 7r14
 */
class UpFlashIScriptablePlugin7 : public FlashIScriptablePlugin7, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    NS_IMETHOD IsPlaying(PRBool *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aretval=%p", pszFunction, aretval));
        rc = VFTCALL1((VFTFlashIScriptablePlugin7*)mpvVFTable, IsPlaying, mpvThis, aretval);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD Play(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTFlashIScriptablePlugin7*)mpvVFTable, Play, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD StopPlay(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTFlashIScriptablePlugin7*)mpvVFTable, StopPlay, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TotalFrames(PRInt32 *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aretval=%p", pszFunction, aretval));
        rc = VFTCALL1((VFTFlashIScriptablePlugin7*)mpvVFTable, TotalFrames, mpvThis, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%d", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD CurrentFrame(PRInt32 *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aretval=%p", pszFunction, aretval));
        rc = VFTCALL1((VFTFlashIScriptablePlugin7*)mpvVFTable, CurrentFrame, mpvThis, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%d", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GotoFrame(PRInt32 aFrame)
    {
        UP_ENTER_RC();
        dprintf(("%s: aFrame=%d", pszFunction, aFrame));
        rc = VFTCALL1((VFTFlashIScriptablePlugin7*)mpvVFTable, GotoFrame, mpvThis, aFrame);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD Rewind(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTFlashIScriptablePlugin7*)mpvVFTable, Rewind, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD Back(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTFlashIScriptablePlugin7*)mpvVFTable, Back, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD Forward(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTFlashIScriptablePlugin7*)mpvVFTable, Forward, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD Pan(PRInt32 aX, PRInt32 aY, PRInt32 aMode)
    {
        UP_ENTER_RC();
        dprintf(("%s: aX=%d aY=%d aMode=%d", pszFunction, aX, aY, aMode));
        rc = VFTCALL3((VFTFlashIScriptablePlugin7*)mpvVFTable, Pan, mpvThis, aX, aY, aMode);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD PercentLoaded(PRInt32 *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aretval=%p", pszFunction, aretval));
        rc = VFTCALL1((VFTFlashIScriptablePlugin7*)mpvVFTable, PercentLoaded, mpvThis, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%d", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD FrameLoaded(PRInt32 aFrame, PRBool *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aFrame=%d aretval=%p", pszFunction, aFrame, aretval));
        rc = VFTCALL2((VFTFlashIScriptablePlugin7*)mpvVFTable, FrameLoaded, mpvThis, aFrame, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%d", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD FlashVersion(PRInt32 *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aretval=%p", pszFunction, aretval));
        rc = VFTCALL1((VFTFlashIScriptablePlugin7*)mpvVFTable, FlashVersion, mpvThis, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%d", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD Zoom(PRInt32 aZoom)
    {
        UP_ENTER_RC();
        dprintf(("%s: aZoom=%p", pszFunction, aZoom));
        rc = VFTCALL1((VFTFlashIScriptablePlugin7*)mpvVFTable, Zoom, mpvThis, aZoom);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD SetZoomRect(PRInt32 aLeft, PRInt32 aTop, PRInt32 aRight, PRInt32 aBottom)
    {
        UP_ENTER_RC();
        dprintf(("%s: aLeft=%d aTop=%d aRight=%d aBottom=%d", pszFunction, aLeft, aTop, aRight, aBottom));
        rc = VFTCALL4((VFTFlashIScriptablePlugin7*)mpvVFTable, SetZoomRect, mpvThis, aLeft, aTop, aRight, aBottom);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD LoadMovie(PRInt32 aLayer, PRUnichar *aURL)
    {
        UP_ENTER_RC();
        dprintf(("%s: aLayer=%d aURL=%ls", pszFunction, aLayer, aURL));
        rc = VFTCALL2((VFTFlashIScriptablePlugin7*)mpvVFTable, LoadMovie, mpvThis, aLayer, aURL);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TGotoFrame(PRUnichar *aTarget, PRInt32 aFrameNumber)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%ls aFrameNumber=%d", pszFunction, aTarget, aFrameNumber));
        rc = VFTCALL2((VFTFlashIScriptablePlugin7*)mpvVFTable, TGotoFrame, mpvThis, aTarget, aFrameNumber);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TGotoLabel(PRUnichar *aTarget, PRUnichar *aLabel)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%ls aLabel=%ls", pszFunction, aTarget, aLabel));
        rc = VFTCALL2((VFTFlashIScriptablePlugin7*)mpvVFTable, TGotoLabel, mpvThis, aTarget, aLabel);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TCurrentFrame(PRUnichar *aTarget, PRInt32 *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%ls aretval=%p", pszFunction, aTarget, aretval));
        rc = VFTCALL2((VFTFlashIScriptablePlugin7*)mpvVFTable, TCurrentFrame, mpvThis, aTarget, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%d", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TCurrentLabel(PRUnichar *aTarget, PRUnichar **aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%ls aretval=%p", pszFunction, aTarget, aretval));
        rc = VFTCALL2((VFTFlashIScriptablePlugin7*)mpvVFTable, TCurrentLabel, mpvThis, aTarget, aretval);
        if (VALID_PTR(aretval) && VALID_PTR(*aretval))
            dprintf(("%s: *aretval=%ls", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TPlay(PRUnichar *aTarget)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%ls", pszFunction, aTarget));
        rc = VFTCALL1((VFTFlashIScriptablePlugin7*)mpvVFTable, TPlay, mpvThis, aTarget);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TStopPlay(PRUnichar *aTarget)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%ls", pszFunction, aTarget));
        rc = VFTCALL1((VFTFlashIScriptablePlugin7*)mpvVFTable, TStopPlay, mpvThis, aTarget);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD SetVariable(PRUnichar *aVariable, PRUnichar *aValue)
    {
        UP_ENTER_RC();
        dprintf(("%s: aVariable=%ls aValue=%ls", pszFunction, aVariable, aValue));
        rc = VFTCALL2((VFTFlashIScriptablePlugin7*)mpvVFTable, SetVariable, mpvThis, aVariable, aValue);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetVariable(PRUnichar *aVariable, PRUnichar **aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aVariable=%ls aretval=%p", pszFunction, aVariable, aretval));
        rc = VFTCALL2((VFTFlashIScriptablePlugin7*)mpvVFTable, GetVariable, mpvThis, aVariable, aretval);
        if (VALID_PTR(aretval) && VALID_PTR(*aretval))
            dprintf(("%s: *aretval=%ls", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TSetProperty(PRUnichar *aTarget, PRInt32 aProperty, PRUnichar *aValue)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%ls aProperty=%d aValue=%ls", pszFunction, aTarget, aProperty, aValue));
        rc = VFTCALL3((VFTFlashIScriptablePlugin7*)mpvVFTable, TSetProperty, mpvThis, aTarget, aProperty, aValue);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TGetProperty(PRUnichar *aTarget, PRInt32 aProperty, PRUnichar **aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%ls aProperty=%d aretval=%p", pszFunction, aTarget, aProperty, aretval));
        rc = VFTCALL3((VFTFlashIScriptablePlugin7*)mpvVFTable, TGetProperty, mpvThis, aTarget, aProperty, aretval);
        if (VALID_PTR(aretval) && VALID_PTR(*aretval))
            dprintf(("%s: *aretval=%ls", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TGetPropertyAsNumber(PRUnichar *aTarget, PRInt32 aProperty, double **aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%ls aProperty=%d aretval=%p", pszFunction, aTarget, aProperty, aretval));
        rc = VFTCALL3((VFTFlashIScriptablePlugin7*)mpvVFTable, TGetPropertyAsNumber, mpvThis, aTarget, aProperty, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%f", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TCallLabel(PRUnichar *aTarget, PRUnichar *aLabel)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%ls aLabel=%ls", pszFunction, aTarget, aLabel));
        rc = VFTCALL2((VFTFlashIScriptablePlugin7*)mpvVFTable, TCallLabel, mpvThis, aTarget, aLabel);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TCallFrame(PRUnichar *aTarget, PRInt32 aFrameNumber)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%ls aFrameNumber=%d", pszFunction, aTarget, aFrameNumber));
        rc = VFTCALL2((VFTFlashIScriptablePlugin7*)mpvVFTable, TCallFrame, mpvThis, aTarget, aFrameNumber);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD SetWindow(FlashIObject7 *aFlashObject, PRInt32 a1)
    {
        UP_ENTER_RC();
        dprintf(("%s: aFlashObject=%p a1=%d", pszFunction, aFlashObject, a1));

        FlashIObject7 *pDownFlashObject = aFlashObject;
        rc = downCreateWrapper((void**)&pDownFlashObject, kFlashIObject7IID, NS_OK);
        if (NS_SUCCEEDED(rc))
            rc = VFTCALL2((VFTFlashIScriptablePlugin7*)mpvVFTable, SetWindow, mpvThis, pDownFlashObject, a1);

        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpFlashIScriptablePlugin7(void *pvThis) :
        UpSupportsBase(pvThis, (FlashIScriptablePlugin7*)this, kFlashIScriptablePlugin7IID)
    {
    }
};




//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIFlash5
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsIPlugin5 (d27cdb6e-ae6d-11cf-96b8-444553540000)
 * Flash 5 rXY for OS/2
 */
class UpFlash5 : public nsIFlash5, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    NS_IMETHOD IsPlaying(PRBool *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aretval=%p", pszFunction, aretval));
        rc = VFTCALL1((VFTnsIFlash5*)mpvVFTable, IsPlaying, mpvThis, aretval);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD Play(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIFlash5*)mpvVFTable, Play, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD StopPlay(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIFlash5*)mpvVFTable, StopPlay, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TotalFrames(PRInt32 *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aretval=%p", pszFunction, aretval));
        rc = VFTCALL1((VFTnsIFlash5*)mpvVFTable, TotalFrames, mpvThis, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%d", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD CurrentFrame(PRInt32 *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aretval=%p", pszFunction, aretval));
        rc = VFTCALL1((VFTnsIFlash5*)mpvVFTable, CurrentFrame, mpvThis, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%d", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GotoFrame(PRInt32 aFrame)
    {
        UP_ENTER_RC();
        dprintf(("%s: aFrame=%d", pszFunction, aFrame));
        rc = VFTCALL1((VFTnsIFlash5*)mpvVFTable, GotoFrame, mpvThis, aFrame);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD Rewind(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIFlash5*)mpvVFTable, Rewind, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD Back(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIFlash5*)mpvVFTable, Back, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD Forward(void)
    {
        UP_ENTER_RC();
        rc = VFTCALL0((VFTnsIFlash5*)mpvVFTable, Forward, mpvThis);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD PercentLoaded(PRInt32 *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aretval=%p", pszFunction, aretval));
        rc = VFTCALL1((VFTnsIFlash5*)mpvVFTable, PercentLoaded, mpvThis, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%d", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD FrameLoaded(PRInt32 aFrame, PRBool *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aFrame=%d aretval=%p", pszFunction, aFrame, aretval));
        rc = VFTCALL2((VFTnsIFlash5*)mpvVFTable, FrameLoaded, mpvThis, aFrame, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%d", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD FlashVersion(PRInt32 *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aretval=%p", pszFunction, aretval));
        rc = VFTCALL1((VFTnsIFlash5*)mpvVFTable, FlashVersion, mpvThis, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%d", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD Pan(PRInt32 aX, PRInt32 aY, PRInt32 aMode)
    {
        UP_ENTER_RC();
        dprintf(("%s: aX=%d aY=%d aMode=%d", pszFunction, aX, aY, aMode));
        rc = VFTCALL3((VFTnsIFlash5*)mpvVFTable, Pan, mpvThis, aX, aY, aMode);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD Zoom(PRInt32 aZoom)
    {
        UP_ENTER_RC();
        dprintf(("%s: aZoom=%p", pszFunction, aZoom));
        rc = VFTCALL1((VFTnsIFlash5*)mpvVFTable, Zoom, mpvThis, aZoom);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD SetZoomRect(PRInt32 aLeft, PRInt32 aTop, PRInt32 aRight, PRInt32 aBottom)
    {
        UP_ENTER_RC();
        dprintf(("%s: aLeft=%d aTop=%d aRight=%d aBottom=%d", pszFunction, aLeft, aTop, aRight, aBottom));
        rc = VFTCALL4((VFTnsIFlash5*)mpvVFTable, SetZoomRect, mpvThis, aLeft, aTop, aRight, aBottom);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD LoadMovie(PRInt32 aLayer, const char *aURL)
    {
        UP_ENTER_RC();
        dprintf(("%s: aLayer=%d aURL=%p", pszFunction, aLayer, aURL));
        DPRINTF_STR(aURL);
        rc = VFTCALL2((VFTnsIFlash5*)mpvVFTable, LoadMovie, mpvThis, aLayer, aURL);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TGotoFrame(const char *aTarget, PRInt32 aFrameNumber)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%p aFrameNumber=%d", pszFunction, aTarget, aFrameNumber));
        DPRINTF_STR(aTarget);
        rc = VFTCALL2((VFTnsIFlash5*)mpvVFTable, TGotoFrame, mpvThis, aTarget, aFrameNumber);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TGotoLabel(const char *aTarget, const char *aLabel)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%p aLabel=%p", pszFunction, aTarget, aLabel));
        DPRINTF_STR(aTarget);
        DPRINTF_STR(aLabel);
        rc = VFTCALL2((VFTnsIFlash5*)mpvVFTable, TGotoLabel, mpvThis, aTarget, aLabel);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TCurrentFrame(const char *aTarget, PRInt32 *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%p aretval=%p", pszFunction, aTarget, aretval));
        DPRINTF_STR(aTarget);
        rc = VFTCALL2((VFTnsIFlash5*)mpvVFTable, TCurrentFrame, mpvThis, aTarget, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%d", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TCurrentLabel(const char *aTarget, char **aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%p aretval=%p", pszFunction, aTarget, aretval));
        DPRINTF_STR(aTarget);
        rc = VFTCALL2((VFTnsIFlash5*)mpvVFTable, TCurrentLabel, mpvThis, aTarget, aretval);
        if (VALID_PTR(aretval))
            DPRINTF_STR(*aretval);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TPlay(const char *aTarget)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%p", pszFunction, aTarget));
        DPRINTF_STR(aTarget);
        rc = VFTCALL1((VFTnsIFlash5*)mpvVFTable, TPlay, mpvThis, aTarget);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TStopPlay(const char *aTarget)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%p", pszFunction, aTarget));
        DPRINTF_STR(aTarget);
        rc = VFTCALL1((VFTnsIFlash5*)mpvVFTable, TStopPlay, mpvThis, aTarget);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD SetVariable(const char *aVariable, const char *aValue)
    {
        UP_ENTER_RC();
        dprintf(("%s: aVariable=%p aValue=%p", pszFunction, aVariable, aValue));
        DPRINTF_STR(aVariable);
        DPRINTF_STR(aValue);
        rc = VFTCALL2((VFTnsIFlash5*)mpvVFTable, SetVariable, mpvThis, aVariable, aValue);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD GetVariable(const char *aVariable, char **aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aVariable=%p aretval=%p", pszFunction, aVariable, aretval));
        DPRINTF_STR(aVariable);
        rc = VFTCALL2((VFTnsIFlash5*)mpvVFTable, GetVariable, mpvThis, aVariable, aretval);
        if (VALID_PTR(aretval) && VALID_PTR(*aretval))
            DPRINTF_STR(aretval);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TSetProperty(const char *aTarget, PRInt32 aProperty, const char *aValue)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%p aProperty=%d aValue=%p", pszFunction, aTarget, aProperty, aValue));
        DPRINTF_STR(aTarget);
        DPRINTF_STR(aValue);
        rc = VFTCALL3((VFTnsIFlash5*)mpvVFTable, TSetProperty, mpvThis, aTarget, aProperty, aValue);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TGetProperty(const char *aTarget, PRInt32 aProperty, char **aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%p aProperty=%d aretval=%p", pszFunction, aTarget, aProperty, aretval));
        DPRINTF_STR(aTarget);
        rc = VFTCALL3((VFTnsIFlash5*)mpvVFTable, TGetProperty, mpvThis, aTarget, aProperty, aretval);
        if (VALID_PTR(aretval))
            DPRINTF_STR(*aretval);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TCallFrame(const char *aTarget, PRInt32 aFrame)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%p aFrameNumber=%d", pszFunction, aTarget, aFrame));
        DPRINTF_STR(aTarget);
        rc = VFTCALL2((VFTnsIFlash5*)mpvVFTable, TCallFrame, mpvThis, aTarget, aFrame);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TCallLabel(const char *aTarget, const char *aLabel)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%p aLabel=%p", pszFunction, aTarget, aLabel));
        DPRINTF_STR(aTarget);
        DPRINTF_STR(aLabel);
        rc = VFTCALL2((VFTnsIFlash5*)mpvVFTable, TCallLabel, mpvThis, aTarget, aLabel);
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TGetPropertyAsNumber(const char *aTarget, PRInt32 aProperty, double *aretval)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%p aProperty=%d aretval=%p", pszFunction, aTarget, aProperty, aretval));
        DPRINTF_STR(aTarget);
        rc = VFTCALL3((VFTnsIFlash5*)mpvVFTable, TGetPropertyAsNumber, mpvThis, aTarget, aProperty, aretval);
        if (VALID_PTR(aretval))
            dprintf(("%s: *aretval=%f", pszFunction, *aretval));
        UP_LEAVE_INT(rc);
        return rc;
    }

    NS_IMETHOD TSetPropertyAsNumber(const char *aTarget, PRInt32 aProperty, double aValue)
    {
        UP_ENTER_RC();
        dprintf(("%s: aTarget=%p aProperty=%d aValue=%f", pszFunction, aTarget, aProperty, aValue));
        DPRINTF_STR(aTarget);
        rc = VFTCALL3((VFTnsIFlash5*)mpvVFTable, TSetPropertyAsNumber, mpvThis, aTarget, aProperty, aValue);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /** Constructor */
    UpFlash5(void *pvThis) :
        UpSupportsBase(pvThis, (nsIFlash5*)this, kFlash5IID)
    {
    }

};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIClassInfo
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * nsIClassInfo
 */
class UpClassInfo : public nsIClassInfo, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * Get an ordered list of the interface ids that instances of the class
     * promise to implement. Note that nsISupports is an implicit member
     * of any such list and need not be included.
     *
     * Should set *count = 0 and *array = null and return NS_OK if getting the
     * list is not supported.
     */
    /* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] out nsIIDPtr array); */
    NS_IMETHOD GetInterfaces(PRUint32 *count, nsIID * **array)
    {
        UP_ENTER_RC();
        dprintf(("%s: count=%p array=%p", pszFunction, count, array));
        rc = VFTCALL2((VFTnsIClassInfo*)mpvVFTable, GetInterfaces, mpvThis, count, array);
        if (NS_SUCCEEDED(rc) && VALID_PTR(count) && VALID_PTR(array) && VALID_PTR(*array))
        {
            dprintf(("%s: *count=%d\n", *count));
            for (unsigned i = 0; i < *count; i++)
                DPRINTF_NSID(*((*array)[i]));
        }
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Get a language mapping specific helper object that may assist in using
     * objects of this class in a specific lanaguage. For instance, if asked
     * for the helper for nsIProgrammingLanguage::JAVASCRIPT this might return
     * an object that can be QI'd into the nsIXPCScriptable interface to assist
     * XPConnect in supplying JavaScript specific behavior to callers of the
     * instance object.
     *
     * see: nsIProgrammingLanguage.idl
     *
     * Should return null if no helper available for given language.
     */
    /* nsISupports getHelperForLanguage (in PRUint32 language); */
    NS_IMETHOD GetHelperForLanguage(PRUint32 language, nsISupports **_retval)
    {
        UP_ENTER_RC();
        dprintf(("%s: language=%d _retval=%p", pszFunction, language, _retval));
        rc = VFTCALL2((VFTnsIClassInfo*)mpvVFTable, GetHelperForLanguage, mpvThis, language, _retval);
        rc = upCreateWrapper((void**)_retval, kSupportsIID, rc);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * A contract ID through which an instance of this class can be created
     * (or accessed as a service, if |flags & SINGLETON|), or null.
     */
    /* readonly attribute string contractID; */
    NS_IMETHOD GetContractID(char * *aContractID)
    {
        UP_ENTER_RC();
        dprintf(("%s: aContractID=%p", pszFunction, aContractID));
        rc = VFTCALL1((VFTnsIClassInfo*)mpvVFTable, GetContractID, mpvThis, aContractID);
        DPRINTF_STRNULL(aContractID);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * A human readable string naming the class, or null.
     */
    /* readonly attribute string classDescription; */
    NS_IMETHOD GetClassDescription(char * *aClassDescription)
    {
        UP_ENTER_RC();
        dprintf(("%s: aClassDescription=%p", pszFunction, aClassDescription));
        rc = VFTCALL1((VFTnsIClassInfo*)mpvVFTable, GetClassDescription, mpvThis, aClassDescription);
        DPRINTF_STRNULL(aClassDescription);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * A class ID through which an instance of this class can be created
     * (or accessed as a service, if |flags & SINGLETON|), or null.
     */
    /* readonly attribute nsCIDPtr classID; */
    NS_IMETHOD GetClassID(nsCID * *aClassID)
    {
        UP_ENTER_RC();
        dprintf(("%s: aClassID=%p", pszFunction, aClassID));
        rc = VFTCALL1((VFTnsIClassInfo*)mpvVFTable, GetClassID, mpvThis, aClassID);
        DPRINTF_NSID(**aClassID);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Return language type from list in nsIProgrammingLanguage
     */
    /* readonly attribute PRUint32 implementationLanguage; */
    NS_IMETHOD GetImplementationLanguage(PRUint32 *aImplementationLanguage)
    {
        UP_ENTER_RC();
        dprintf(("%s: aImplementationLanguage=%p", pszFunction, aImplementationLanguage));
        rc = VFTCALL1((VFTnsIClassInfo*)mpvVFTable, GetImplementationLanguage, mpvThis, aImplementationLanguage);
        if (VALID_PTR(aImplementationLanguage))
            dprintf(("%s: *aImplementationLanguage=%d", pszFunction, *aImplementationLanguage));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /* readonly attribute PRUint32 flags; */
    NS_IMETHOD GetFlags(PRUint32 *aFlags)
    {
        UP_ENTER_RC();
        dprintf(("%s: aFlags=%p", pszFunction, aFlags));
        rc = VFTCALL1((VFTnsIClassInfo*)mpvVFTable, GetFlags, mpvThis, aFlags);
        if (VALID_PTR(aFlags))
            dprintf(("%s: *aFlags=%d", pszFunction, *aFlags));
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
     * Also a class ID through which an instance of this class can be created
     * (or accessed as a service, if |flags & SINGLETON|).  If the class does
     * not have a CID, it should return NS_ERROR_NOT_AVAILABLE.  This attribute
     * exists so C++ callers can avoid allocating and freeing a CID, as would
     * happen if they used classID.
     */
    /* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
    NS_IMETHOD GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
    {
        UP_ENTER_RC();
        dprintf(("%s: aClassIDNoAlloc=%p", pszFunction, aClassIDNoAlloc));
        rc = VFTCALL1((VFTnsIClassInfo*)mpvVFTable, GetClassIDNoAlloc, mpvThis, aClassIDNoAlloc);
        DPRINTF_NSID(*aClassIDNoAlloc);
        UP_LEAVE_INT(rc);
        return rc;
    }


    /** Constructor */
    UpClassInfo(void *pvThis) :
        UpSupportsBase(pvThis, (nsIClassInfo*)this, kClassInfoIID)
    {
    }
};



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP: nsIHTTPHeaderListener
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

class UpHTTPHeaderListener : public nsIHTTPHeaderListener, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
     * Called for each HTTP Response header.
     * NOTE: You must copy the values of the params.
     */
    /* void newResponseHeader (in string headerName, in string headerValue); */
    NS_IMETHOD NewResponseHeader(const char *headerName, const char *headerValue)
    {
        UP_ENTER_RC();
        DPRINTF_STR(headerName);
        DPRINTF_STR(headerValue);
        rc = VFTCALL2((VFTnsIHTTPHeaderListener *)mpvVFTable, NewResponseHeader, mpvThis, headerName, headerValue);
        UP_LEAVE_INT(rc);
        return rc;
    }


    /** Constructor */
    UpHTTPHeaderListener(void *pvThis) :
        UpSupportsBase(pvThis, (nsIHTTPHeaderListener *)this, kHTTPHeaderListenerIID)
    {
    }

    nsresult StatusLine(const char *line) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
};


class UpMemory : public nsIMemory, public UpSupportsBase
{
public:
    UP_IMPL_NSISUPPORTS();

    /**
       * Allocates a block of memory of a particular size. If the memory
       * cannot be allocated (because of an out-of-memory condition), null
       * is returned.
       *
       * @param size - the size of the block to allocate
       * @result the block of memory
       */
    /* [noscript, notxpcom] voidPtr alloc (in size_t size); */
    NS_IMETHOD_(void *) Alloc(size_t size)
    {
        UP_ENTER();
        dprintf(("%s: size=%d", pszFunction, size));
        void * pv = VFTCALL1((VFTnsIMemory*)mpvVFTable, Alloc, mpvThis, size);
        UP_LEAVE_INT((unsigned)pv);
        return pv;
    }

    /**
       * Reallocates a block of memory to a new size.
       *
       * @param ptr - the block of memory to reallocate
       * @param size - the new size
       * @result the reallocated block of memory
       *
       * If ptr is null, this function behaves like malloc.
       * If s is the size of the block to which ptr points, the first
       * min(s, size) bytes of ptr's block are copied to the new block.
       * If the allocation succeeds, ptr is freed and a pointer to the
       * new block returned.  If the allocation fails, ptr is not freed
       * and null is returned. The returned value may be the same as ptr.
       */
    /* [noscript, notxpcom] voidPtr realloc (in voidPtr ptr, in size_t newSize); */
    NS_IMETHOD_(void *) Realloc(void * ptr, size_t newSize)
    {
        UP_ENTER();
        dprintf(("%s: ptr=%p newSize=%d", pszFunction, ptr, newSize));
        void * pv = VFTCALL2((VFTnsIMemory*)mpvVFTable, Realloc, mpvThis, ptr, newSize);
        UP_LEAVE_INT((unsigned)pv);
        return pv;
    }

    /**
       * Frees a block of memory. Null is a permissible value, in which case
       * nothing happens.
       *
       * @param ptr - the block of memory to free
       */
    /* [noscript, notxpcom] void free (in voidPtr ptr); */
    NS_IMETHOD_(void) Free(void * ptr)
    {
        UP_ENTER();
        dprintf(("%s: ptr=%d", pszFunction, ptr));
        VFTCALL1((VFTnsIMemory*)mpvVFTable, Free, mpvThis, ptr);
        UP_LEAVE();
        return;
    }

    /**
       * Attempts to shrink the heap.
       * @param immediate - if true, heap minimization will occur
       *   immediately if the call was made on the main thread. If
       *   false, the flush will be scheduled to happen when the app is
       *   idle.
       * @return NS_ERROR_FAILURE if 'immediate' is set an the call
       *   was not on the application's main thread.
       */
    /* void heapMinimize (in boolean immediate); */
    NS_IMETHOD HeapMinimize(PRBool immediate)
    {
        UP_ENTER_RC();
        dprintf(("%s: immediate=%d", pszFunction, immediate));
        rc = VFTCALL1((VFTnsIMemory*)mpvVFTable, HeapMinimize, mpvThis, immediate);
        UP_LEAVE_INT(rc);
        return rc;
    }

    /**
       * This predicate can be used to determine if we're in a low-memory
       * situation (what constitutes low-memory is platform dependent). This
       * can be used to trigger the memory pressure observers.
       */
    /* boolean isLowMemory (); */
    NS_IMETHOD IsLowMemory(PRBool *_retval)
    {
        UP_ENTER_RC();
        dprintf(("%s: immediate=%p", pszFunction, _retval));
        rc = VFTCALL1((VFTnsIMemory*)mpvVFTable, IsLowMemory, mpvThis, _retval);
        if (VALID_PTR(_retval))
            dprintf(("%s: *_retval=%d\n", *_retval));
        UP_LEAVE_INT(rc);
        return rc;
    }


    /** Constructor */
    UpMemory(void *pvThis) :
        UpSupportsBase(pvThis, (nsIMemory *)this, kMemoryIID)
    {
    }
};


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// UP Helpers
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Check if the specified interface is supported by the 'UP' type wrapper.
 * @returns TRUE if supported.
 * @returns FALSE if not supported.
 * @param   aIID    Interface ID in question.
 */
BOOL            upIsSupportedInterface(REFNSIID aIID)
{
    static const nsID * aIIDs[] =
    {
        &kSupportsIID,
        &kFactoryIID,
        &kPluginIID,
        &kJVMPluginIID,
        &kSecureEnvIID,
        &kPluginInstanceIID,
        &kPluginInstancePeerIID,
        &kPluginInstancePeer2IID,
        &kPluginTagInfoIID,
        &kJVMWindowIID,
        &kJVMConsoleIID,
        &kEventHandlerIID,
        &kJVMPluginInstanceIID,
        &kRunnableIID,
        &kSecurityContextIID,
        &kRequestObserverIID,
        &kStreamListenerIID,
        &kRequestIID,
        &kLoadGroupIID,
        &kSimpleEnumeratorIID,
        &kInterfaceRequestorIID,
        &kOutputStreamIID,
        &kPluginStreamListenerIID,
        &kFlashIObject7IID,
        &kFlashIScriptablePlugin7IID,
        &kFlash5IID,
        &kClassInfoIID,
        &kHTTPHeaderListenerIID,
        &kMemoryIID,
    };

    for (unsigned iInterface = 0; iInterface < sizeof(aIIDs) / sizeof(aIIDs[0]); iInterface++)
        if (aIIDs[iInterface]->Equals(aIID))
            return TRUE;
    return FALSE;
}

/**
 * Create a wrapper for an Interface.
 *
 * @returns Pointer to the crated interface wrapper.
 * @returns NULL on if unsupported interface.
 * @returns NULL on failure.
 * @returns NULL if pvThis is NULL.
 * @param   pvThis  Pointer to the interface.
 * @param   aIID    Reference to Interface ID.
 */
nsresult    upCreateWrapper(void **ppvResult, REFNSIID aIID, nsresult rc)
{
    DEBUG_FUNCTIONNAME();
    dprintf(("%s: pvvResult=%x,,rc=%x", pszFunction, ppvResult, rc));
    DPRINTF_NSID(aIID);

    if (VALID_PTR(ppvResult))
    {
        if (VALID_PTR(*ppvResult))
        {
            dprintf(("%s: *pvvResult=%x", pszFunction, *ppvResult));
            if (NS_SUCCEEDED(rc))
            {
                void *pvThis = *ppvResult;
                /*
                 * First try check if there is an existing down wrapper for
                 * this object, if so no wrapping is needed at all.
                 */
                void *pvNoWrapper = UpWrapperBase::findDownWrapper(pvThis);
                if (pvNoWrapper)
                {
                    dprintf(("%s: COOL! pvThis(%x) is an down wrapper, no wrapping needed. returns real obj=%x",
                             pszFunction,  pvThis, pvNoWrapper));
                    *ppvResult = pvNoWrapper;
                    return rc;
                }

                #if 1 // did this to minimize heap access.
                /*
                 * Now lock the list and search it for existing wrapper.
                 */
                UpWrapperBase::upLock();
                UpWrapperBase *pExisting = UpWrapperBase::findUpWrapper(pvThis, aIID);
                UpWrapperBase::upUnLock();
                if (pExisting)
                {
                    dprintf(("%s: Reusing existing wrapper %p/%p for %p!", pszFunction, pExisting, pExisting->getInterfacePointer(), pvThis));
                    *ppvResult = pExisting->getInterfacePointer();
                    return rc;
                }
                #endif

                /*
                 * Third, try check if we've allready have created a wrapper for this object.
                 *
                 *  Now, to do make sure we will never see duplicates here, we will:
                 *
                 *      Create a wrapper object.
                 *      Lock the up list.
                 *      Check if there is an existing wrapper.
                 *      If so Then
                 *          Use it.
                 *          Destroy the one we've just created.
                 *      Else
                 *          Insert the wrapper we've created.
                 *      EndIf
                 *      Unlock the up list.
                 *      return the resulting wrapper.
                 *
                 */

                /*
                 * Create the wrapper.
                 *
                 * IMPORTANT!!! DON'T FORGET UPDATING THE TABLE IN upIsSupportedInterface() TOO!!!!
                 */
                BOOL fFound = TRUE;
                UpWrapperBase * pWrapper = NULL;
                if (aIID.Equals(kSupportsIID))
                    pWrapper =  new UpSupports(pvThis);
                else if (aIID.Equals(kFactoryIID))
                    pWrapper =  new UpFactory(pvThis);
                else if (aIID.Equals(kPluginIID))
                    pWrapper =  new UpPlugin(pvThis);
                else if (aIID.Equals(kJVMPluginIID))
                    pWrapper =  new UpJVMPlugin(pvThis);
                else if (aIID.Equals(kSecureEnvIID))
                    pWrapper =  new UpSecureEnv(pvThis);
                else if (aIID.Equals(kPluginInstanceIID))
                    pWrapper =  new UpPluginInstance(pvThis);
                else if (aIID.Equals(kPluginInstancePeerIID))
                    pWrapper =  new UpPluginInstancePeer(pvThis);
                else if (aIID.Equals(kPluginInstancePeer2IID))
                    pWrapper =  new UpPluginInstancePeer2(pvThis);
                else if (aIID.Equals(kPluginTagInfoIID))
                    pWrapper =  new UpPluginTagInfo(pvThis);
                else if (aIID.Equals(kJVMWindowIID))
                    pWrapper =  new UpJVMWindow(pvThis);
                else if (aIID.Equals(kJVMConsoleIID))
                    pWrapper =  new UpJVMConsole(pvThis);
                else if (aIID.Equals(kEventHandlerIID))
                    pWrapper =  new UpEventHandler(pvThis);
                else if (aIID.Equals(kJVMPluginInstanceIID))
                    pWrapper =  new UpJVMPluginInstance(pvThis);
                else if (aIID.Equals(kRunnableIID))
                    pWrapper =  new UpRunnable(pvThis);
                else if (aIID.Equals(kSecurityContextIID))
                    pWrapper = new UpSecurityContext(pvThis);
                else if (aIID.Equals(kRequestObserverIID))
                    pWrapper = new UpRequestObserver(pvThis);
                else if (aIID.Equals(kStreamListenerIID))
                    pWrapper = new UpStreamListener(pvThis);
                else if (aIID.Equals(kRequestIID))
                    pWrapper = new UpRequest(pvThis);
                else if (aIID.Equals(kLoadGroupIID))
                    pWrapper = new UpLoadGroup(pvThis);
                else if (aIID.Equals(kSimpleEnumeratorIID))
                    pWrapper = new UpSimpleEnumerator(pvThis);
                else if (aIID.Equals(kInterfaceRequestorIID))
                    pWrapper = new UpInterfaceRequestor(pvThis);
                else if (aIID.Equals(kOutputStreamIID))
                    pWrapper = new UpOutputStream(pvThis);
                else if (aIID.Equals(kPluginStreamListenerIID))
                    pWrapper = new UpPluginStreamListener(pvThis);
                else if (aIID.Equals(kFlashIScriptablePlugin7IID))
                    pWrapper = new UpFlashIScriptablePlugin7(pvThis);
                else if (aIID.Equals(kFlashIObject7IID))
                    pWrapper = new UpFlashIObject7(pvThis);
                else if (aIID.Equals(kFlash5IID))
                    pWrapper = new UpFlash5(pvThis);
                else if (aIID.Equals(kClassInfoIID))
                    pWrapper = new UpClassInfo(pvThis);
                else if (aIID.Equals(kHTTPHeaderListenerIID))
                    pWrapper = new UpHTTPHeaderListener(pvThis);
                else if (aIID.Equals(kMemoryIID))
                    pWrapper = new UpMemory(pvThis);
                else
                    fFound = FALSE;

                /*
                 * Successfully create wrapper?
                 */
                if (fFound && pWrapper)
                {
                    /*
                     * Now lock the list and search it for existing wrapper.
                     */
                    UpWrapperBase::upLock();
                    UpWrapperBase *pExisting2 = UpWrapperBase::findUpWrapper(pvThis, aIID);
                    if (pExisting2)
                    {
                        UpWrapperBase::upUnLock();
                        delete pWrapper;
                        pWrapper = pExisting2;
                        dprintf(("%s: Reusing existing wrapper %p/%p for %p!", pszFunction, pWrapper, pWrapper->getInterfacePointer(), pvThis));
                    }
                    else
                    {
                        pWrapper->upInsertWrapper();
                        UpWrapperBase::upUnLock();
                        dprintf(("%s: Successfully create wrapper %p/%p for %p!", pszFunction, pWrapper, pWrapper->getInterfacePointer(), pvThis));
                    }

                    *ppvResult = pWrapper->getInterfacePointer();
                    return rc;
                }


                /*
                 * What's wrong?
                 */
                if (!fFound)
                {
                    ReleaseInt3(0xbaddbeef, 11, aIID.m0);
                    dprintf(("%s: Unsupported interface!!!", pszFunction));
                    rc = NS_ERROR_NOT_IMPLEMENTED;
                }
                else
                {
                    dprintf(("%s: new failed! (how is that possible?)", pszFunction));
                    rc = NS_ERROR_OUT_OF_MEMORY;
                }
            }
            else
                dprintf(("%s: The passed in rc means failure (rc=%x)", pszFunction, rc));
            *ppvResult = nsnull;
        }
        else
            dprintf(("%s: *ppvResult (=%p) is invalid (rc=%x)", pszFunction, *ppvResult, rc));
    }
    else
        dprintf(("%s: ppvResult (=%p) is invalid (rc=%x)", pszFunction, ppvResult, rc));

    return rc;
}


/**
 * Create a wrapper for an Interface - simple version.
 *
 * @returns Pointer to the crated interface wrapper.
 * @returns NULL on if unsupported interface.
 * @returns NULL on failure.
 * @returns NULL if pvThis is NULL.
 * @param   pvThis  Pointer to the interface.
 * @param   aIID    Reference to Interface ID.
 */
void *  upCreateWrapper2(void *pvThis, REFNSIID aIID)
{
//    DEBUG_FUNCTIONNAME();
    void *      pvResult = pvThis;
    nsresult    rc = upCreateWrapper(&pvResult, aIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        return pvResult;
    return NULL;
}


/**
 * Create a JNIEnv wrapper for sending up to mozilla.
 * @returns rc on success
 * @returns rc or othere error code on failure.
 * @param   ppJNIEnv    Where to get and store the JNIEnv wrapper.
 * @param   rc          Current rc.
 */
int upCreateJNIEnvWrapper(JNIEnv **ppJNIEnv, int rc)
{
    DEBUG_FUNCTIONNAME();
    dprintf(("%s: ppJNIEnv=%x, rc=%x", pszFunction, ppJNIEnv, rc));

    if (VALID_PTR(ppJNIEnv))
    {
        if (VALID_PTR(*ppJNIEnv))
        {
            if (NS_SUCCEEDED(rc))
            {
                    /*
                     * Success!
                     */
                    return rc;
            }
            else
                dprintf(("%s: The query method failed with rc=%x", pszFunction, rc));
            *ppJNIEnv = nsnull;
        }
        else if (*ppJNIEnv || rc != NS_OK) /* don't complain about the obvious.. we use this combination. */
            dprintf(("%s: *ppJNIEnv (=%p) is invalid (rc=%x)", pszFunction, *ppJNIEnv, rc));
    }
    else
        dprintf(("%s: ppJNIEnv (=%p) is invalid (rc=%x)", pszFunction, ppJNIEnv, rc));

    return rc;
}


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsISupports
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
nsresult VFTCALL downQueryInterface(void *pvThis, REFNSIID aIID, void** aInstancePtr)
{
    DOWN_ENTER_RC(pvThis, nsISupports);
    DPRINTF_NSID(aIID);

    /*
     * Is this a supported interface?
     */
    const void *pvVFT = downIsSupportedInterface(aIID);
    if (pvVFT)
    {
        dprintf(("%s: Supported interface. Calling the real QueryInterface...", pszFunction));
        rc = pMozI->QueryInterface(aIID, aInstancePtr);
        rc = downCreateWrapper(aInstancePtr, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported interface!!!", pszFunction));
        rc = NS_NOINTERFACE;
        if (aInstancePtr)
            *aInstancePtr = nsnull;
        ReleaseInt3(0xbaddbeef, 1, aIID.m0);
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * nsISupport::AddRef() wrapper.
 */
nsrefcnt VFTCALL downAddRef(void *pvThis)
{
    DOWN_ENTER(pvThis, nsISupports);
    nsrefcnt c = pMozI->AddRef();
    DOWN_LEAVE_INT(pvThis, c);
    return c;
}

/**
 * nsISupport::Release() wrapper.
 */
nsrefcnt VFTCALL downRelease(void *pvThis)
{
    DOWN_ENTER(pvThis, nsISupports);
    nsrefcnt c = pMozI->Release();
    if (!c)
    {
        dprintf(("%s: c=0! deleting wrapper structure!", pszFunction));

        #ifdef DO_DELETE
        /**
         * Unchain the wrapper first.
         */
        DOWN_LOCK();
        int fUnchained;
        PDOWNTHIS pDown, pPrev;
        for (pDown = (PDOWNTHIS)gpDownHead, pPrev = NULL, fUnchained = 0;
             pDown;
             pPrev = pDown, pDown = (PDOWNTHIS)pDown->pNext)
        {
            if (pDown == pvThis)
            {
                if (pPrev)
                    pPrev->pNext = pDown->pNext;
                else
                    gpDownHead = pDown->pNext;

                #ifdef DEBUG
                /* paranoid as always */
                if (fUnchained)
                {
                    dprintf(("%s: pvThis=%x was linked twice!", pszFunction, pvThis));
                    DebugInt3();
                }
                fUnchained = 1;
                #else
                fUnchained = 1;
                break;
                #endif
            }
        }
        DOWN_UNLOCK();

        if (fUnchained)
        {   /** @todo concider a delayed freeing, meaning we inster the them in
             * a FIFO and at a certain threshold we'll start killing them.
             * This will require the up/downCreateWrapper to also search the FIFO
             * before creating the a new wrapper.
             */
            pDown = (PDOWNTHIS)pvThis;
            memset(pDown, 0, sizeof(*pDown));
            delete pDown;
        }
        else
        {   /*
             * Allready unchained? This shouldn't ever happen...
             * The only possible case is that two threads calls hlpRelease()
             * a the same time. So, we will *not* touch this node now.
             */
            dprintf(("%s: pvThis=%p not found in the list !!!!", pszFunction, pvThis));
            DebugInt3();
        }
        #endif
        pvThis = NULL;
    }
    DOWN_LEAVE_INT(pvThis, c);
    return c;
}


/** VFT for the nsISupports Wrapper. */
MAKE_SAFE_VFT(VFTnsISupports, downVFTnsISupports)
{
    VFTFIRST_VAL()
    downQueryInterface,         VFTDELTA_VAL()
    downAddRef,                 VFTDELTA_VAL()
    downRelease,                VFTDELTA_VAL()

}
SAFE_VFT_ZEROS();



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIServiceManager
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * getServiceByContractID
 *
 * Returns the instance that implements aClass or aContractID and the
 * interface aIID.  This may result in the instance being created.
 *
 * @param aClass or aContractID : aClass or aContractID of object
 *                                instance requested
 * @param aIID : IID of interface requested
 * @param result : resulting service
 */
/* void getService (in nsCIDRef aClass, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
nsresult VFTCALL downGetService(void *pvThis, const nsCID & aClass, const nsIID & aIID, void * *result)
{
    DOWN_ENTER_RC(pvThis, nsIServiceManager);
    DPRINTF_NSID(aClass);
    DPRINTF_NSID(aIID);

    /*
     * Is this a supported interface?
     */
    const void *pvVFT = downIsSupportedInterface(&aIID ? aIID : kSupportsIID);
    if (pvVFT)
    {
        dprintf(("%s: Supported interface. Calling the real GetService...", pszFunction));
        rc = pMozI->GetService(aClass, aIID, result);
        rc = downCreateWrapper(result, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported interface!!!", pszFunction));
        rc = NS_NOINTERFACE;
        if (result)
            *result = nsnull;
        ReleaseInt3(0xbaddbeef, 2, aIID.m0);
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/* void getServiceByContractID (in string aContractID, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
nsresult VFTCALL downGetServiceByContractID(void *pvThis, const char *aContractID, const nsIID & aIID, void * *result)
{
    //@todo Check UNICODE vs. UTF8 vs. Codepage issues here.
    DOWN_ENTER_RC(pvThis, nsIServiceManager);
    DPRINTF_CONTRACTID(aContractID);
    DPRINTF_NSID(aIID);

    /*
     * Is this a supported interface?
     */
    const void *pvVFT = downIsSupportedInterface(&aIID ? aIID : kSupportsIID);
    if (pvVFT)
    {
        dprintf(("%s: Supported interface. Calling the real GetServiceByContractID...", pszFunction));
        rc = pMozI->GetServiceByContractID(aContractID, aIID, result);
        rc = downCreateWrapper(result, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported interface!!!", pszFunction));
        rc = NS_NOINTERFACE;
        if (result)
            *result = nsnull;
        ReleaseInt3(0xbaddbeef, 3, aIID.m0);
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * isServiceInstantiated
 *
 * isServiceInstantiated will return a true if the service has already
 * been created, otherwise false
 *
 * @param aClass or aContractID : aClass or aContractID of object
 *                                instance requested
 * @param aIID : IID of interface requested
 * @param aIID : IID of interface requested
 */
/* boolean isServiceInstantiated (in nsCIDRef aClass, in nsIIDRef aIID); */
nsresult VFTCALL downIsServiceInstantiated(void *pvThis, const nsCID & aClass, const nsIID & aIID, PRBool *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIServiceManager);
    DPRINTF_NSID(aClass);
    DPRINTF_NSID(aIID);
    rc = pMozI->IsServiceInstantiated(aClass, aIID, _retval);
    if (VALID_PTR(_retval))
        dprintf(("%s: *_retval=%d", pszFunction, *_retval));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/* boolean isServiceInstantiatedByContractID (in string aContractID, in nsIIDRef aIID); */
nsresult VFTCALL downIsServiceInstantiatedByContractID(void *pvThis, const char *aContractID, const nsIID & aIID, PRBool *_retval)
{
    //@todo Check UNICODE vs. UTF8 vs. Codepage issues here.
    DOWN_ENTER_RC(pvThis, nsIServiceManager);
    DPRINTF_CONTRACTID(aContractID);
    DPRINTF_NSID(aIID);
    rc = pMozI->IsServiceInstantiatedByContractID(aContractID, aIID, _retval);
    if (VALID_PTR(_retval))
        dprintf(("%s: *_retval=%d", pszFunction, *_retval));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


/** VFT for the nsIServerManager Wrapper. */
MAKE_SAFE_VFT(VFTnsIServiceManager, downVFTnsIServiceManager)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                 VFTDELTA_VAL()
    downAddRef,                                         VFTDELTA_VAL()
    downRelease,                                        VFTDELTA_VAL()
 },
    downGetService,                                     VFTDELTA_VAL()
    downGetServiceByContractID,                         VFTDELTA_VAL()
    downIsServiceInstantiated,                          VFTDELTA_VAL()
    downIsServiceInstantiatedByContractID,              VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();




//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIServiceManagerObsolete
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
/**
 * RegisterService may be called explicitly to register a service
 * with the service manager. If a service is not registered explicitly,
 * the component manager will be used to create an instance according
 * to the class ID specified.
 */
nsresult VFTCALL downISMORegisterService(void *pvThis, const nsCID& aClass, nsISupports* aService)
{
    DOWN_ENTER_RC(pvThis, nsIServiceManagerObsolete);
    DPRINTF_NSID(aClass);

    nsISupports *pupService = aService;
    rc = upCreateWrapper((void**)&pupService, kSupportsIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->RegisterService(aClass, pupService);
    else
        dprintf(("%s: Unable to create wrapper for nsISupports!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Requests a service to be shut down, possibly unloading its DLL.
 *
 * @returns NS_OK - if shutdown was successful and service was unloaded,
 * @returns NS_ERROR_SERVICE_NOT_FOUND - if shutdown failed because
 *          the service was not currently loaded
 * @returns NS_ERROR_SERVICE_IN_USE - if shutdown failed because some
 *          user of the service wouldn't voluntarily release it by using
 *          a shutdown listener.
 */
nsresult VFTCALL downISMOUnregisterService(void *pvThis, const nsCID& aClass)
{
    DOWN_ENTER_RC(pvThis, nsIServiceManagerObsolete);
    DPRINTF_NSID(aClass);
    rc = pMozI->UnregisterService(aClass);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

nsresult VFTCALL downISMOGetService(void *pvThis, const nsCID& aClass, const nsIID& aIID,
                                       nsISupports* *result,
                                       nsIShutdownListener* shutdownListener/* = nsnull*/)
{
    DOWN_ENTER_RC(pvThis, nsIServiceManagerObsolete);
    DPRINTF_NSID(aClass);
    DPRINTF_NSID(aIID);

    /*
     * Is this a supported interface?
     */
    const void *pvVFT = downIsSupportedInterface(&aIID ? aIID : kSupportsIID);
    if (pvVFT)
    {
        dprintf(("%s: Supported interface. Calling the real downISMOGetService...", pszFunction));
        nsIShutdownListener *pupShutdownListener = shutdownListener;
        rc = upCreateWrapper((void**)&pupShutdownListener, kShutdownListenerIID, NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            rc = pMozI->GetService(aClass, aIID, result, pupShutdownListener);
            rc = downCreateWrapper((void**)result, pvVFT, rc);
        }
        else
            dprintf(("%s: Unable to create 'UP' wrapper for nsIShutdownListener!", pszFunction));
    }
    else
    {
        dprintf(("%s: Unsupported interface!!!", pszFunction));
        rc = NS_NOINTERFACE;
        if (result)
            *result = nsnull;
        ReleaseInt3(0xbaddbeef, 4, aIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


/* OBSOLETE: use NS_RELEASE(service) instead. */
nsresult VFTCALL downISMOReleaseService(void *pvThis, const nsCID& aClass, nsISupports* service,
                                           nsIShutdownListener* shutdownListener/* = nsnull*/)
{
    DOWN_ENTER_RC(pvThis, nsIServiceManagerObsolete);
    DPRINTF_NSID(aClass);

    nsISupports *pupService = service;
    rc = upCreateWrapper((void**)&pupService, kSupportsIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsIShutdownListener *pupShutdownListener = shutdownListener;
        rc = upCreateWrapper((void**)&pupShutdownListener, kShutdownListenerIID, rc);
        if (NS_SUCCEEDED(rc))
            rc = pMozI->ReleaseService(aClass, pupService, pupShutdownListener);
        else
            dprintf(("%s: Unable to create 'UP' wrapper for nsIShutdownListener!", pszFunction));
    }
    else
        dprintf(("%s: Unable to create 'UP' wrapper for nsISupports!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;

}


////////////////////////////////////////////////////////////////////////////
// let's do it again, this time with ContractIDs...

nsresult VFTCALL downISMORegisterServiceByContractID(void *pvThis, const char* aContractID, nsISupports* aService)
{
    DOWN_ENTER_RC(pvThis, nsIServiceManagerObsolete);
    DPRINTF_CONTRACTID(aContractID);

    nsISupports *pupService = aService;
    rc = upCreateWrapper((void**)&pupService, kSupportsIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->RegisterService(aContractID, pupService);
    else
        dprintf(("%s: Unable to create wrapper for nsISupports!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


nsresult VFTCALL downISMOUnregisterServiceByContractID(void *pvThis, const char* aContractID)
{
    DOWN_ENTER_RC(pvThis, nsIServiceManagerObsolete);
    DPRINTF_CONTRACTID(aContractID);
    rc = pMozI->UnregisterService(aContractID);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


nsresult VFTCALL downISMOGetServiceByContractID(void *pvThis, const char* aContractID, const nsIID& aIID,
                                                   nsISupports* *result,
                                                   nsIShutdownListener* shutdownListener/* = nsnull */)
{
    DOWN_ENTER_RC(pvThis, nsIServiceManagerObsolete);
    DPRINTF_CONTRACTID(aContractID);
    DPRINTF_NSID(aIID);

    /*
     * Is this a supported interface?
     */
    const void *pvVFT = downIsSupportedInterface(&aIID ? aIID : kSupportsIID);
    if (pvVFT)
    {
        nsIShutdownListener *pupShutdownListener = shutdownListener;
        rc = upCreateWrapper((void**)&pupShutdownListener, kShutdownListenerIID, NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            rc = pMozI->GetService(aContractID, aIID, result, pupShutdownListener);
            rc = downCreateWrapper((void**)result, pvVFT, rc);
        }
        else
            dprintf(("%s: Unable to create 'UP' wrapper for nsIShutdownListener!", pszFunction));
    }
    else
    {
        dprintf(("%s: Unsupported interface!!!", pszFunction));
        rc = NS_NOINTERFACE;
        ReleaseInt3(0xbaddbeef, 5, aIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


/* OBSOLETE */
nsresult VFTCALL downISMOReleaseServiceByContractID(void *pvThis, const char* aContractID, nsISupports* service,
                                                       nsIShutdownListener* shutdownListener/* = nsnull*/)
{
    DOWN_ENTER_RC(pvThis, nsIServiceManagerObsolete);
    DPRINTF_CONTRACTID(aContractID);

    nsISupports *pupService = service;
    rc = upCreateWrapper((void**)&pupService, kSupportsIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsIShutdownListener *pupShutdownListener = shutdownListener;
        rc = upCreateWrapper((void**)&pupShutdownListener, kShutdownListenerIID, rc);
        if (NS_SUCCEEDED(rc))
            rc = pMozI->ReleaseService(aContractID, pupService, pupShutdownListener);
        else
            dprintf(("%s: Unable to create 'UP' wrapper for nsIShutdownListener!", pszFunction));
    }
    else
        dprintf(("%s: Unable to create 'UP' wrapper for nsISupports!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;

}


MAKE_SAFE_VFT(VFTnsIServiceManagerObsolete, downVFTnsIServiceManagerObsolete)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
#ifdef VFT_VAC365
    downISMORegisterService,                                VFTDELTA_VAL()
    downISMORegisterServiceByContractID,                    VFTDELTA_VAL()
    downISMOUnregisterService,                              VFTDELTA_VAL()
    downISMOUnregisterServiceByContractID,                  VFTDELTA_VAL()
    downISMOGetService,                                     VFTDELTA_VAL()
    downISMOGetServiceByContractID,                         VFTDELTA_VAL()
    downISMOReleaseService,                                 VFTDELTA_VAL()
    downISMOReleaseServiceByContractID,                     VFTDELTA_VAL()
#else
    downISMORegisterServiceByContractID,                    VFTDELTA_VAL()
    downISMORegisterService,                                VFTDELTA_VAL()
    downISMOUnregisterServiceByContractID,                  VFTDELTA_VAL()
    downISMOUnregisterService,                              VFTDELTA_VAL()
    downISMOGetServiceByContractID,                         VFTDELTA_VAL()
    downISMOGetService,                                     VFTDELTA_VAL()
    downISMOReleaseServiceByContractID,                     VFTDELTA_VAL()
    downISMOReleaseService,                                 VFTDELTA_VAL()
#endif
}
SAFE_VFT_ZEROS();



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIPluginManager
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Returns the value of a variable associated with the plugin manager.
 *
 * (Corresponds to NPN_GetValue.)
 *
 * @param variable - the plugin manager variable to get
 * @param value - the address of where to store the resulting value
 * @result - NS_OK if this operation was successful
 */
/* [noscript] void GetValue (in nsPluginManagerVariable variable, in nativeVoid value); */
nsresult VFTCALL downIPMGetValue(void *pvThis, nsPluginManagerVariable variable, void * value)
{   /* kso: this is not doing anything on NO_X11 platforms */
    DOWN_ENTER_RC(pvThis, nsIPluginManager);
    dprintf(("%s: variable=%d value=%x", pszFunction, variable, value));
    rc = pMozI->GetValue(variable, value);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Causes the plugins directory to be searched again for new plugin
 * libraries.
 *
 * (Corresponds to NPN_ReloadPlugins.)
 *
 * @param reloadPages - indicates whether currently visible pages should
 * also be reloaded
 */
/* void reloadPlugins (in boolean reloadPages); */
nsresult VFTCALL downIPMReloadPlugins(void *pvThis, PRBool reloadPages)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager);
    dprintf(("%s: reloadPages=%d", pszFunction, reloadPages));
    rc = pMozI->ReloadPlugins(reloadPages);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Returns the user agent string for the browser.
 *
 * (Corresponds to NPN_UserAgent.)
 *
 * @param resultingAgentString - the resulting user agent string
 */
/* [noscript] void UserAgent (in nativeChar resultingAgentString); */
nsresult VFTCALL downIPMUserAgent(void *pvThis, const char * * resultingAgentString)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager);
    dprintf(("%s: resultingAgentString=%p", pszFunction, resultingAgentString));
    rc = pMozI->UserAgent(resultingAgentString);
    if (NS_SUCCEEDED(rc) && VALID_PTR(resultingAgentString))
        DPRINTF_STR(*resultingAgentString);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Fetches a URL.
 *
 * (Corresponds to NPN_GetURL and NPN_GetURLNotify.)
 *
 * @param pluginInst - the plugin making the request. If NULL, the URL
 *   is fetched in the background.
 * @param url - the URL to fetch
 * @param target - the target window into which to load the URL, or NULL if
 *   the data should be returned to the plugin via streamListener.
 * @param streamListener - a stream listener to be used to return data to
 *   the plugin. May be NULL if target is not NULL.
 * @param altHost - an IP-address string that will be used instead of the
 *   host specified in the URL. This is used to prevent DNS-spoofing
 *   attacks. Can be defaulted to NULL meaning use the host in the URL.
 * @param referrer - the referring URL (may be NULL)
 * @param forceJSEnabled - forces JavaScript to be enabled for 'javascript:'
 *   URLs, even if the user currently has JavaScript disabled (usually
 *   specify PR_FALSE)
 * @result - NS_OK if this operation was successful
 */
nsresult VFTCALL downIPMGetURL(void *pvThis, nsISupports* pluginInst, const char* url, const char* target = NULL,
                                  nsIPluginStreamListener* streamListener = NULL, const char* altHost = NULL, const char* referrer = NULL,
                                  PRBool forceJSEnabled = PR_FALSE)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager);
    dprintf(("%s: pluginInst=%x  streamListener=%p  forceJSEnabled=%d",
             pszFunction, pluginInst, streamListener, forceJSEnabled));
    DPRINTF_STR(url);
    DPRINTF_STR(target);
    DPRINTF_STR(altHost);
    DPRINTF_STR(referrer);

    nsIPluginStreamListener *pupStreamListener = streamListener;
    rc = upCreateWrapper((void**)&pupStreamListener, kPluginStreamListenerIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports * pupPluginInst = pluginInst;
        rc = upCreateWrapper((void**)&pupPluginInst, kSupportsIID, rc);
        if (rc == NS_OK)
            rc = pMozI->GetURL(pupPluginInst, url, target, pupStreamListener, altHost, referrer, forceJSEnabled);
        else
            dprintf(("%s: Failed to create wrapper for nsISupports (pluginInst=%p)!!!!", pszFunction, pluginInst));
    }
    else
        dprintf(("%s: Failed to create wrapper for nsIPluginStreamListener!!!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Posts to a URL with post data and/or post headers.
 *
 * (Corresponds to NPN_PostURL and NPN_PostURLNotify.)
 *
 * @param pluginInst - the plugin making the request. If NULL, the URL
 *   is fetched in the background.
 * @param url - the URL to fetch
 * @param postDataLength - the length of postData (if non-NULL)
 * @param postData - the data to POST. NULL specifies that there is not post
 *   data
 * @param isFile - whether the postData specifies the name of a file to
 *   post instead of data. The file will be deleted afterwards.
 * @param target - the target window into which to load the URL, or NULL if
 *   the data should be returned to the plugin via streamListener.
 * @param streamListener - a stream listener to be used to return data to
 *   the plugin. May be NULL if target is not NULL.
 * @param altHost - an IP-address string that will be used instead of the
 *   host specified in the URL. This is used to prevent DNS-spoofing
 *   attacks. Can be defaulted to NULL meaning use the host in the URL.
 * @param referrer - the referring URL (may be NULL)
 * @param forceJSEnabled - forces JavaScript to be enabled for 'javascript:'
 *   URLs, even if the user currently has JavaScript disabled (usually
 *   specify PR_FALSE)
 * @param postHeadersLength - the length of postHeaders (if non-NULL)
 * @param postHeaders - the headers to POST. Must be in the form of
 * "HeaderName: HeaderValue\r\n".  Each header, including the last,
 * must be followed by "\r\n".  NULL specifies that there are no
 * post headers
 * @result - NS_OK if this operation was successful
 */
nsresult VFTCALL downIPMPostURL(void *pvThis, nsISupports* pluginInst, const char* url, PRUint32 postDataLen, const char* postData,
                                   PRBool isFile = PR_FALSE, const char* target = NULL, nsIPluginStreamListener* streamListener = NULL,
                                   const char* altHost = NULL, const char* referrer = NULL, PRBool forceJSEnabled = PR_FALSE,
                                   PRUint32 postHeadersLength = 0, const char* postHeaders = NULL)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager);
    dprintf(("%s: pluginInst=%x  postDataLen=%d postData='%.*s' (%p) isFile=%d, streamListener=%p",
             pszFunction, pluginInst, postDataLen, postDataLen, postData, postData, isFile, streamListener));
    dprintf(("%s: forceJSEnabled=%d postHeadersLength=%d postHeaders='%*.s' (%p)",
             pszFunction, forceJSEnabled, postHeadersLength, postHeadersLength, postHeaders, postHeaders));
    DPRINTF_STR(url);
    DPRINTF_STR(target);
    DPRINTF_STR(altHost);
    DPRINTF_STR(referrer);

    nsIPluginStreamListener *pupStreamListener = streamListener;
    rc = upCreateWrapper((void**)&pupStreamListener, kPluginStreamListenerIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports * pupPluginInst = pluginInst;
        rc = upCreateWrapper((void**)&pupPluginInst, kSupportsIID, rc);
        if (rc == NS_OK)
            rc = pMozI->PostURL(pupPluginInst, url, postDataLen, postData, isFile, target, pupStreamListener,
                                altHost, referrer, forceJSEnabled, postHeadersLength, postHeaders);
        else
            dprintf(("%s: Failed to create wrapper for nsISupports (pluginInst=%p)!!!!", pszFunction, pluginInst));
    }
    else
        dprintf(("%s: Failed to create wrapper for nsIPluginStreamListener!!!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Persistently register a plugin with the plugin
 * manager. aMimeTypes, aMimeDescriptions, and aFileExtensions are
 * parallel arrays that contain information about the MIME types
 * that the plugin supports.
 *
 * @param aCID - the plugin's CID
 * @param aPluginName - the plugin's name
 * @param aDescription - a description of the plugin
 * @param aMimeTypes - an array of MIME types that the plugin
 *   is prepared to handle
 * @param aMimeDescriptions - an array of descriptions for the
 *   MIME types that the plugin can handle.
 * @param aFileExtensions - an array of file extensions for
 *   the MIME types that the plugin can handle.
 * @param aCount - the number of elements in the aMimeTypes,
 *   aMimeDescriptions, and aFileExtensions arrays.
 * @result - NS_OK if the operation was successful.
 */
/* [noscript] void RegisterPlugin (in REFNSIID aCID, in string aPluginName, in string aDescription, in nativeChar aMimeTypes, in nativeChar aMimeDescriptions, in nativeChar aFileExtensions, in long aCount); */
nsresult VFTCALL downIPMRegisterPlugin(void *pvThis, REFNSIID aCID, const char *aPluginName, const char *aDescription, const char * * aMimeTypes,
                                          const char * * aMimeDescriptions, const char * * aFileExtensions, PRInt32 aCount)
{
    //@todo TEXT: aMimeDescriptions? aDescription? Doesn't apply to java plugin I think.
    DOWN_ENTER_RC(pvThis, nsIPluginManager);
    dprintf(("%s: aCount=%d", pszFunction, aCount));
    DPRINTF_NSID(aCID);
    DPRINTF_STR(aPluginName);
    DPRINTF_STR(aDescription);

    for (int i = 0; i < aCount; i++)
        dprintf(("%s: aMimeTypes[%d]='%s' (%p)  aMimeDescriptions[%d]='%s' (%p)  aFileExtensions[%d]='%s' (%p)",
                 pszFunction, i, aMimeTypes[i], aMimeTypes[i],
                 i, aMimeDescriptions[i], aMimeDescriptions[i],
                 i, aFileExtensions[i], aFileExtensions[i]));
    rc = pMozI->RegisterPlugin(aCID, aPluginName, aDescription, aMimeTypes, aMimeDescriptions, aFileExtensions, aCount);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Unregister a plugin from the plugin manager
 *
 * @param aCID the CID of the plugin to unregister.
 * @result - NS_OK if the operation was successful.
 */
/* [noscript] void UnregisterPlugin (in REFNSIID aCID); */
nsresult VFTCALL downIPMUnregisterPlugin(void *pvThis, REFNSIID aCID)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager);
    DPRINTF_NSID(aCID);
    rc = pMozI->UnregisterPlugin(aCID);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Fetches a URL, with Headers
 * @see GetURL.  Identical except for additional params headers and
 * headersLen
 * @param getHeadersLength - the length of getHeaders (if non-NULL)
 * @param getHeaders - the headers to GET. Must be in the form of
 * "HeaderName: HeaderValue\r\n".  Each header, including the last,
 * must be followed by "\r\n".  NULL specifies that there are no
 * get headers
 * @result - NS_OK if this operation was successful
 */
nsresult VFTCALL downIPMGetURLWithHeaders(void *pvThis, nsISupports* pluginInst, const char* url, const char* target = NULL,
                                             nsIPluginStreamListener* streamListener = NULL, const char* altHost = NULL, const char* referrer = NULL,
                                             PRBool forceJSEnabled = PR_FALSE, PRUint32 getHeadersLength = 0, const char* getHeaders = NULL)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager);
    dprintf(("%s: pluginInst=%x  streamListener=%p  forceJSEnabled=%d getHeadersLength=%d getHeaders='%.*s' (%p)",
             pszFunction, pluginInst, streamListener, forceJSEnabled, getHeadersLength, getHeadersLength, getHeaders, getHeaders));
    DPRINTF_STR(url);
    DPRINTF_STR(target);
    DPRINTF_STR(altHost);
    DPRINTF_STR(referrer);

    nsIPluginStreamListener *pupStreamListener = streamListener;
    rc = upCreateWrapper((void**)&pupStreamListener, kPluginStreamListenerIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports * pupPluginInst = pluginInst;
        rc = upCreateWrapper((void**)&pupPluginInst, kSupportsIID, rc);
        if (rc == NS_OK)
            rc = pMozI->GetURLWithHeaders(pupPluginInst, url, target, pupStreamListener, altHost, referrer, forceJSEnabled, getHeadersLength, getHeaders);
        else
            dprintf(("%s: Failed to create wrapper for nsISupports (pluginInst=%p)!!!!", pszFunction, pluginInst));
    }
    else
        dprintf(("%s: Failed to create wrapper for nsIPluginStreamListener!!!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsIPluginManager, downVFTnsIPluginManager)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downIPMGetValue,                                        VFTDELTA_VAL()
    downIPMReloadPlugins,                                   VFTDELTA_VAL()
    downIPMUserAgent,                                       VFTDELTA_VAL()
    downIPMGetURL,                                          VFTDELTA_VAL()
    downIPMPostURL,                                         VFTDELTA_VAL()
    downIPMRegisterPlugin,                                  VFTDELTA_VAL()
    downIPMUnregisterPlugin,                                VFTDELTA_VAL()
    downIPMGetURLWithHeaders,                               VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIPluginManager2
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Puts up a wait cursor.
 *
 * @result - NS_OK if this operation was successful
 */
/* void beginWaitCursor (); */
nsresult VFTCALL downIPM2BeginWaitCursor(void *pvThis)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager2);
    rc = pMozI->BeginWaitCursor();
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Restores the previous (non-wait) cursor.
 *
 * @result - NS_OK if this operation was successful
 */
/* void endWaitCursor (); */
nsresult VFTCALL downIPM2EndWaitCursor(void *pvThis)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager2);
    rc = pMozI->EndWaitCursor();
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Returns true if a URL protocol (e.g. "http") is supported.
 *
 * @param aProtocol - the protocol name
 * @param aResult   - true if the protocol is supported
 * @result          - NS_OK if this operation was successful
 */
/* void supportsURLProtocol (in string aProtocol, out boolean aResult); */
nsresult VFTCALL downIPM2SupportsURLProtocol(void *pvThis, const char *aProtocol, PRBool *aResult)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager2);
    DPRINTF_STR(aProtocol);

    rc = pMozI->SupportsURLProtocol(aProtocol, aResult);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aResult))
        dprintf(("%s: *aResult=%d", pszFunction, *aResult));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * This method may be called by the plugin to indicate that an error
 * has occurred, e.g. that the plugin has failed or is shutting down
 * spontaneously. This allows the browser to clean up any plugin-specific
 * state.
 *
 * @param aPlugin - the plugin whose status is changing
 * @param aStatus - the error status value
 * @result        - NS_OK if this operation was successful
 */
/* void notifyStatusChange (in nsIPlugin aPlugin, in nsresult aStatus); */
nsresult VFTCALL downIPM2NotifyStatusChange(void *pvThis, nsIPlugin *aPlugin, nsresult aStatus)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager2);
    dprintf(("%s: aPlugin=%x aStatus=%d", pszFunction, aPlugin, aStatus));

    nsIPlugin * pupPlugin = aPlugin;
    rc = upCreateWrapper((void**)&pupPlugin, kPluginIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->NotifyStatusChange(pupPlugin, aStatus);
    else
        dprintf(("%s: failed to create up wrapper for aPlugin=%x!!!", pszFunction, aPlugin));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Returns the proxy info for a given URL. The caller is required to
 * free the resulting memory with nsIMalloc::Free. The result will be in the
 * following format
 *
 *   i)   "DIRECT"  -- no proxy
 *   ii)  "PROXY xxx.xxx.xxx.xxx"   -- use proxy
 *   iii) "SOCKS xxx.xxx.xxx.xxx"  -- use SOCKS
 *   iv)  Mixed. e.g. "PROXY 111.111.111.111;PROXY 112.112.112.112",
 *                    "PROXY 111.111.111.111;SOCKS 112.112.112.112"....
 *
 * Which proxy/SOCKS to use is determined by the plugin.
 */
/* void findProxyForURL (in string aURL, out string aResult); */
nsresult VFTCALL downIPM2FindProxyForURL(void *pvThis, const char *aURL, char **aResult)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager2);
    DPRINTF_STR(aURL);
    rc = pMozI->FindProxyForURL(aURL, aResult);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aResult) && VALID_PTR(*aResult))
    {
        DPRINTF_STR(*aResult);

        /* I have a feeling that the ibm plugin is cheating here. or that nsIMalloc::Free
         * isn't well implemented...
         *
         * !!HACKALLERT!!
         * For npoji16.dll we will allocate this string using it's own CRT.
         * First we must detect if this is npoji16.dll
         */
        char        szModName[9];
        HMODULE     hmod;
        ULONG       iObj;
        ULONG       offObj;
        if (!DosQueryModFromEIP(&hmod, &iObj, sizeof(szModName), &szModName[0], &offObj, ((unsigned *)(void*)&pvThis)[-1]))
        {
            if (!stricmp(szModName, "NPOJI6"))
            {
                if (!DosQueryModuleHandle("JV12MI36", &hmod))
                {
                    char *(*_Optlink pfnstrdup)(const char *);
                    /* we might have to dosloadmodule (verb) the stupid thing */
                    int rc2 = DosQueryProcAddr(hmod, 0, "strdup", (PFN*)&pfnstrdup);
                    if (   rc2 == ERROR_INVALID_HANDLE
                        && !DosLoadModule(NULL, 0, "JV12MI36", &hmod))
                        rc2 = DosQueryProcAddr(hmod, 0, "strdup", (PFN*)&pfnstrdup);
                    if (!rc2)
                    {
                        char *psz = pfnstrdup(*aResult);
                        if (psz)
                        {
                            free(*aResult); /* We're using the right CRT.. */
                            *aResult = psz;
                        }
                    }
                }
            }
        }
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Registers a top-level window with the browser. Events received by that
 * window will be dispatched to the event handler specified.
 *
 * @param aHandler - the event handler for the window
 * @param aWindow  - the platform window reference
 * @result         - NS_OK if this operation was successful
 */
/* void registerWindow (in nsIEventHandler aHandler, in nsPluginPlatformWindowRef aWindow); */
nsresult VFTCALL downIPM2RegisterWindow(void *pvThis, nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager2);
    dprintf(("%s: Incomplete!!!", pszFunction));
    dprintf(("%s: aHandler=%p aWindow=%x", pszFunction, aHandler, aWindow));

    nsIEventHandler * pupHandler = aHandler;
    rc = upCreateWrapper((void**)&pupHandler, kEventHandlerIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        //@todo wrap window handle somehow...
        DebugInt3();
        nsPluginPlatformWindowRef   os2Window = aWindow;
        if (NS_SUCCEEDED(rc))
        {
            rc = pMozI->RegisterWindow(pupHandler, os2Window);
        }
        else
            dprintf(("%s: failed to create window handle wrapper for %x!!!", pszFunction, aWindow));
    }
    else
        dprintf(("%s: failed to create up wrapper for nsIEventHandler %x!!!", pszFunction, aHandler));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Unregisters a top-level window with the browser. The handler and window pair
 * should be the same as that specified to RegisterWindow.
 *
 * @param aHandler - the event handler for the window
 * @param aWindow  - the platform window reference
 * @result         - NS_OK if this operation was successful
 */
/* void unregisterWindow (in nsIEventHandler aHandler, in nsPluginPlatformWindowRef aWindow); */
nsresult VFTCALL downIPM2UnregisterWindow(void *pvThis, nsIEventHandler *aHandler, nsPluginPlatformWindowRef aWindow)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager2);
    dprintf(("%s: Incomplete!!!", pszFunction));
    dprintf(("%s: aHandler=%p aWindow=%x", pszFunction, aHandler, aWindow));

    nsIEventHandler * pupHandler = aHandler;
    rc = upCreateWrapper((void**)&pupHandler, kEventHandlerIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        //@todo wrap window handle somehow...
        DebugInt3();
        nsPluginPlatformWindowRef   os2Window = aWindow;
        if (NS_SUCCEEDED(rc))
        {
            rc = pMozI->UnregisterWindow(pupHandler, os2Window);
        }
        else
            dprintf(("%s: failed to create window handle wrapper for %x!!!", pszFunction, aWindow));
    }
    else
        dprintf(("%s: failed to create up wrapper for nsIEventHandler %x!!!", pszFunction, aHandler));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Allocates a new menu ID (for the Mac).
 *
 * @param aHandler   - the event handler for the window
 * @param aIsSubmenu - whether this is a sub-menu ID or not
 * @param aResult    - the resulting menu ID
 * @result           - NS_OK if this operation was successful
 */
/* void allocateMenuID (in nsIEventHandler aHandler, in boolean aIsSubmenu, out short aResult); */
nsresult VFTCALL downIPM2AllocateMenuID(void *pvThis, nsIEventHandler *aHandler, PRBool aIsSubmenu, PRInt16 *aResult)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager2);
    dprintf(("%s: Incomplete!!!", pszFunction));
    dprintf(("%s: aHandler=%p aIsSubmenu=%d aResult=%p", pszFunction, aHandler, aIsSubmenu, aResult));

    nsIEventHandler * pupHandler = aHandler;
    rc = upCreateWrapper((void**)&pupHandler, kEventHandlerIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        rc = pMozI->AllocateMenuID(pupHandler, aIsSubmenu, aResult);
        if (NS_SUCCEEDED(rc) && VALID_PTR(aResult))
            dprintf(("%s: *aResult=%d (0x%x)", pszFunction, *aResult, *aResult));
    }
    else
        dprintf(("%s: failed to create up wrapper for nsIEventHandler %x !!!", pszFunction, aHandler));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Deallocates a menu ID (for the Mac).
 *
 * @param aHandler - the event handler for the window
 * @param aMenuID  - the menu ID
 * @result         - NS_OK if this operation was successful
 */
/* void deallocateMenuID (in nsIEventHandler aHandler, in short aMenuID); */
nsresult VFTCALL downIPM2DeallocateMenuID(void *pvThis, nsIEventHandler *aHandler, PRInt16 aMenuID)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager2);
    dprintf(("%s: aHandler=%p aMenuID=%d", pszFunction, aHandler, aMenuID));

    nsIEventHandler * pupHandler = aHandler;
    rc = upCreateWrapper((void**)&pupHandler, kEventHandlerIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->DeallocateMenuID(pupHandler, aMenuID);
    else
        dprintf(("%s: failed to create up wrapper for nsIEventHandler %x !!!", pszFunction, aHandler));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Indicates whether this event handler has allocated the given menu ID.
 *
 * @param aHandler - the event handler for the window
 * @param aMenuID  - the menu ID
 * @param aResult  - returns PR_TRUE if the menu ID is allocated
 * @result         - NS_OK if this operation was successful
 */
/* void hasAllocatedMenuID (in nsIEventHandler aHandler, in short aMenuID, out boolean aResult); */
nsresult VFTCALL downIPM2HasAllocatedMenuID(void *pvThis, nsIEventHandler *aHandler, PRInt16 aMenuID, PRBool *aResult)
{
    DOWN_ENTER_RC(pvThis, nsIPluginManager2);
    dprintf(("%s: aHandler=%p aMenuID=%d aResult=%p", pszFunction, aHandler, aMenuID, aResult));

    nsIEventHandler * pupHandler = aHandler;
    rc = upCreateWrapper((void**)&pupHandler, kEventHandlerIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        rc = pMozI->HasAllocatedMenuID(pupHandler, aMenuID, aResult);
        if (NS_SUCCEEDED(rc) && VALID_PTR(aResult))
            dprintf(("%s: *aResult=%d", pszFunction, *aResult));
    }
    else
        dprintf(("%s: failed to create up wrapper for nsIEventHandler %x !!!", pszFunction, aHandler));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsIPluginManager2, downVFTnsIPluginManager2)
{
 {
  {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
  },
    downIPMGetValue,                                        VFTDELTA_VAL()
    downIPMReloadPlugins,                                   VFTDELTA_VAL()
    downIPMUserAgent,                                       VFTDELTA_VAL()
    downIPMGetURL,                                          VFTDELTA_VAL()
    downIPMPostURL,                                         VFTDELTA_VAL()
    downIPMRegisterPlugin,                                  VFTDELTA_VAL()
    downIPMUnregisterPlugin,                                VFTDELTA_VAL()
    downIPMGetURLWithHeaders,                               VFTDELTA_VAL()
 },
    downIPM2BeginWaitCursor,                                VFTDELTA_VAL()
    downIPM2EndWaitCursor,                                  VFTDELTA_VAL()
    downIPM2SupportsURLProtocol,                            VFTDELTA_VAL()
    downIPM2NotifyStatusChange,                             VFTDELTA_VAL()
    downIPM2FindProxyForURL,                                VFTDELTA_VAL()
    downIPM2RegisterWindow,                                 VFTDELTA_VAL()
    downIPM2UnregisterWindow,                               VFTDELTA_VAL()
    downIPM2AllocateMenuID,                                 VFTDELTA_VAL()
    downIPM2DeallocateMenuID,                               VFTDELTA_VAL()
    downIPM2HasAllocatedMenuID,                             VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();




//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIPluginInstancePeer
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
* Returns the value of a variable associated with the plugin manager.
*
* (Corresponds to NPN_GetValue.)
*
* @param aVariable - the plugin manager variable to get
* @param aValue    - the address of where to store the resulting value
* @result          - NS_OK if this operation was successful
*/
/* void getValue (in nsPluginInstancePeerVariable aVariable, in voidPtr aValue); */
nsresult VFTCALL downIPIPGetValue(void *pvThis, nsPluginInstancePeerVariable aVariable, void * aValue)
{
    DOWN_ENTER_RC(pvThis, nsIPluginInstancePeer);
    dprintf(("%s: aVariable=%d aValue=%p", pszFunction, aVariable, aValue));
    rc = pMozI->GetValue(aVariable, aValue);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Returns the MIME type of the plugin instance.
 *
 * (Corresponds to NPP_New's MIMEType argument.)
 *
* @param aMIMEType - resulting MIME type
* @result          - NS_OK if this operation was successful
 */
/* readonly attribute nsMIMEType MIMEType; */
nsresult VFTCALL downIPIPGetMIMEType(void *pvThis, nsMIMEType *aMIMEType)
{
    DOWN_ENTER_RC(pvThis, nsIPluginInstancePeer);
    dprintf(("%s: aMIMEType=%p", pszFunction, aMIMEType));
    rc = pMozI->GetMIMEType(aMIMEType);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aMIMEType) && VALID_PTR(*aMIMEType))
        DPRINTF_STR(*aMIMEType);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Returns the mode of the plugin instance, i.e. whether the plugin is
 * embedded in the html, or full page.
 *
 * (Corresponds to NPP_New's mode argument.)
 *
* @param result - the resulting mode
* @result       - NS_OK if this operation was successful
 */
/* readonly attribute nsPluginMode mode; */
nsresult VFTCALL downIPIPGetMode(void *pvThis, nsPluginMode *aMode)
{
    DOWN_ENTER_RC(pvThis, nsIPluginInstancePeer);
    dprintf(("%s: aMode=%p", pszFunction, aMode));
    rc = pMozI->GetMode(aMode);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aMode))
        dprintf(("%s: *aMode=%d", pszFunction, *aMode));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * This operation is called by the plugin instance when it wishes to send
 * a stream of data to the browser. It constructs a new output stream to which
 * the plugin may send the data. When complete, the Close and Release methods
 * should be called on the output stream.
 *
 * (Corresponds to NPN_NewStream.)
 *
 * @param aType   - MIME type of the stream to create
 * @param aTarget - the target window name to receive the data
 * @param aResult - the resulting output stream
 * @result        - NS_OK if this operation was successful
 */
/* void newStream (in nsMIMEType aType, in string aTarget, out nsIOutputStream aResult); */
nsresult VFTCALL downIPIPNewStream(void *pvThis, nsMIMEType aType, const char *aTarget, nsIOutputStream **aResult)
{
    //@todo TEXT: aTarget? Doesn't apply to java plugin I think.
    DOWN_ENTER_RC(pvThis, nsIPluginInstancePeer);
    dprintf(("%s: aResult=%p", pszFunction, aResult));
    DPRINTF_STR(aType);
    DPRINTF_STR(aTarget);

    const void *pvVFT = downIsSupportedInterface(kOutputStreamIID);
    if (pvVFT)
    {
        rc = pMozI->NewStream(aType, aTarget, aResult);
        rc = downCreateWrapper((void**)aResult, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported Interface!!!", pszFunction));
        rc = NS_NOINTERFACE;
        ReleaseInt3(0xbaddbeef, 21, kOutputStreamIID.m0);
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * This operation causes status information to be displayed on the window
 * associated with the plugin instance.
 *
 * (Corresponds to NPN_Status.)
 *
 * @param aMessage - the status message to display
 * @result         - NS_OK if this operation was successful
 */
/* void showStatus (in string aMessage); */
nsresult VFTCALL downIPIPShowStatus(void *pvThis, const char *aMessage)
{
    //@todo TEXT: aMessage? This must be further tested!
    DOWN_ENTER_RC(pvThis, nsIPluginInstancePeer);
    DPRINTF_STR(aMessage);
    verifyAndFixUTF8String(aMessage, XPFUNCTION);
    rc = pMozI->ShowStatus(aMessage);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Set the desired size of the window in which the plugin instance lives.
 *
 * @param aWidth  - new window width
 * @param aHeight - new window height
 * @result        - NS_OK if this operation was successful
 */
/* void setWindowSize (in unsigned long aWidth, in unsigned long aHeight); */
nsresult VFTCALL downIPIPSetWindowSize(void *pvThis, PRUint32 aWidth, PRUint32 aHeight)
{
    DOWN_ENTER_RC(pvThis, nsIPluginInstancePeer);
    dprintf(("%s: aWidth=%d aHeight=%d", pszFunction, aWidth, aHeight));
    rc = pMozI->SetWindowSize(aWidth, aHeight);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsIPluginInstancePeer, downVFTnsIPluginInstancePeer)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downIPIPGetValue,                                       VFTDELTA_VAL()
    downIPIPGetMIMEType,                                    VFTDELTA_VAL()
    downIPIPGetMode,                                        VFTDELTA_VAL()
    downIPIPNewStream,                                      VFTDELTA_VAL()
    downIPIPShowStatus,                                     VFTDELTA_VAL()
    downIPIPSetWindowSize,                                  VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();





//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIPluginInstancePeer2
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
/**
 * Get the JavaScript window object corresponding to this plugin instance.
 *
 * @param aJSWindow - the resulting JavaScript window object
 * @result          - NS_OK if this operation was successful
 */
/* readonly attribute JSObjectPtr JSWindow; */
nsresult VFTCALL downIPIP2GetJSWindow(void *pvThis, JSObject * *aJSWindow)
{
    DOWN_ENTER_RC(pvThis, nsIPluginInstancePeer2);
    rc = pMozI->GetJSWindow(aJSWindow);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aJSWindow))
    {
        //@todo wrap aJSWindow?
        dprintf(("%s: aJSWindow=%p - not really wrapped !!!", pszFunction, aJSWindow));
        DebugInt3();
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Get the JavaScript execution thread corresponding to this plugin instance.
 *
 * @param aJSThread - the resulting JavaScript thread id
 * @result            - NS_OK if this operation was successful
 */
/* readonly attribute unsigned long JSThread; */
nsresult VFTCALL downIPIP2GetJSThread(void *pvThis, PRUint32 *aJSThread)
{
    DOWN_ENTER_RC(pvThis, nsIPluginInstancePeer2);
    rc = pMozI->GetJSThread(aJSThread);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aJSThread))
        dprintf(("%s: aJSThread=%u", pszFunction, aJSThread));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Get the JavaScript context to this plugin instance.
 *
 * @param aJSContext - the resulting JavaScript context
 * @result           - NS_OK if this operation was successful
 */
/* readonly attribute JSContextPtr JSContext; */
nsresult VFTCALL downIPIP2GetJSContext(void *pvThis, JSContext * *aJSContext)
{
    DOWN_ENTER_RC(pvThis, nsIPluginInstancePeer2);
    rc = pMozI->GetJSContext(aJSContext);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aJSContext))
    {
        //@todo wrap aJSContext?
        dprintf(("%s: aJSContext=%p - not really wrapped !!!", pszFunction, aJSContext));
        DebugInt3();
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

MAKE_SAFE_VFT(VFTnsIPluginInstancePeer2, downVFTnsIPluginInstancePeer2)
{
 {
  {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
  },
    downIPIPGetValue,                                       VFTDELTA_VAL()
    downIPIPGetMIMEType,                                    VFTDELTA_VAL()
    downIPIPGetMode,                                        VFTDELTA_VAL()
    downIPIPNewStream,                                      VFTDELTA_VAL()
    downIPIPShowStatus,                                     VFTDELTA_VAL()
    downIPIPSetWindowSize,                                  VFTDELTA_VAL()
 },
    downIPIP2GetJSWindow,                                   VFTDELTA_VAL()
    downIPIP2GetJSThread,                                   VFTDELTA_VAL()
    downIPIP2GetJSContext,                                  VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN:
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
* QueryInterface on nsIPluginInstancePeer to get this.
*
* (Corresponds to NPP_New's argc, argn, and argv arguments.)
* Get a ptr to the paired list of attribute names and values,
* returns the length of the array.
*
* Each name or value is a null-terminated string.
*/
/* void getAttributes (in PRUint16Ref aCount, in constCharStarConstStar aNames, in constCharStarConstStar aValues); */
nsresult VFTCALL downIPTIGetAttributes(void *pvThis, PRUint16 & aCount, const char* const* & aNames, const char* const* & aValues)
{
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo);
    rc = pMozI->GetAttributes(aCount, aNames, aValues);
    if (NS_SUCCEEDED(rc))
    {
        dprintf(("%s: aCount=%d", pszFunction, aCount));
        for (int i = 0; i < aCount; i++)
            dprintf(("%s: aNames[%d]='%s' (%p) aValues[%d]='%s' (%p)", pszFunction,
                     i, aNames[i], aNames[i], i, aValues[i], aValues[i]));
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Gets the value for the named attribute.
 *
* @param aName   - the name of the attribute to find
* @param aResult - the resulting attribute
 * @result - NS_OK if this operation was successful, NS_ERROR_FAILURE if
 * this operation failed. result is set to NULL if the attribute is not found
 * else to the found value.
 */
/* void getAttribute (in string aName, out constCharPtr aResult); */
nsresult VFTCALL downIPTIGetAttribute(void *pvThis, const char *aName, const char * *aResult)
{
    //@todo TEXT: aName? aResult? I believe that this one works as the text format is that of the document.
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo);
    DPRINTF_STR(aName);
    rc = pMozI->GetAttribute(aName, aResult);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aResult) && VALID_PTR(*aResult))
        DPRINTF_STR(*aResult);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsIPluginTagInfo, downVFTnsIPluginTagInfo)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downIPTIGetAttributes,                                  VFTDELTA_VAL()
    downIPTIGetAttribute,                                   VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Get the type of the HTML tag that was used ot instantiate this
 * plugin.  Currently supported tags are EMBED, OBJECT and APPLET.
 */
/* readonly attribute nsPluginTagType tagType; */
nsresult VFTCALL downIPTI2GetTagType(void *pvThis, nsPluginTagType *aTagType)
{
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);
    rc = pMozI->GetTagType(aTagType);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aTagType))
        dprintf(("%s: *aTagType=%d", pszFunction, *aTagType));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
* Get the complete text of the HTML tag that was used to instantiate this plugin.
 */
/* void getTagText (out constCharPtr aTagText); */
nsresult VFTCALL downIPTI2GetTagText(void *pvThis, const char * *aTagText)
{
    //@todo TEXT: aTagText? Can't find any usage, but I guess this is also in the format of the document.
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);
    rc = pMozI->GetTagText(aTagText);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aTagText) && VALID_PTR(*aTagText))
        DPRINTF_STR(*aTagText);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Get a ptr to the paired list of parameter names and values,
 * returns the length of the array.
 *
 * Each name or value is a null-terminated string.
 */
/* void getParameters (in PRUint16Ref aCount, in constCharStarConstStar aNames, in constCharStarConstStar aValues); */
nsresult VFTCALL downIPTI2GetParameters(void *pvThis, PRUint16 & aCount, const char* const* & aNames, const char* const* & aValues)
{
    //@todo TEXT: aNames? aValues? I believe that this one works as the text format is that of the document.
    //            kmelektronik uses this successfully.
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);
    rc = pMozI->GetParameters(aCount, aNames, aValues);
    if (NS_SUCCEEDED(rc))
    {
        dprintf(("%s: aCount=%d", pszFunction, aCount));
        for (int i = 0; i < aCount; i++)
            dprintf(("%s: aNames[%d]='%s' (%p) aValues[%d]='%s' (%p)", pszFunction,
                     i, aNames[i], aNames[i], i, aValues[i], aValues[i]));
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Get the value for the named parameter.  Returns null
 * if the parameter was not set.
*
* @param aName   - name of the parameter
* @param aResult - parameter value
* @result        - NS_OK if this operation was successful
 */
/* void getParameter (in string aName, out constCharPtr aResult); */
nsresult VFTCALL downIPTI2GetParameter(void *pvThis, const char *aName, const char * *aResult)
{
    //@todo TEXT: aName? aResult? Can't find any usage, but I guess this is also in the format of the document.
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);
    DPRINTF_STR(aName);
    rc = pMozI->GetParameter(aName, aResult);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aResult) && VALID_PTR(*aResult))
        DPRINTF_STR(*aResult);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
* Get the document base
*/
/* void getDocumentBase (out constCharPtr aDocumentBase); */
nsresult VFTCALL downIPTI2GetDocumentBase(void *pvThis, const char * *aDocumentBase)
{
    //@todo TEXT: aDocumentBase? This is just getting the URL. Need further investigation.
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);
    rc = pMozI->GetDocumentBase(aDocumentBase);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aDocumentBase) && VALID_PTR(*aDocumentBase))
        DPRINTF_STR(*aDocumentBase);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Return an encoding whose name is specified in:
 * http://java.sun.com/products/jdk/1.1/docs/guide/intl/intl.doc.html#25303
 */
/* void getDocumentEncoding (out constCharPtr aDocumentEncoding); */
nsresult VFTCALL downIPTI2GetDocumentEncoding(void *pvThis, const char * *aDocumentEncoding)
{
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);
    rc = pMozI->GetDocumentEncoding(aDocumentEncoding);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aDocumentEncoding) && VALID_PTR(*aDocumentEncoding))
        DPRINTF_STR(*aDocumentEncoding);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
* Get object alignment
*/
/* void getAlignment (out constCharPtr aElignment); */
nsresult VFTCALL downIPTI2GetAlignment(void *pvThis, const char * *aElignment)
{
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);
    rc = pMozI->GetAlignment(aElignment);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aElignment) && VALID_PTR(*aElignment))
        DPRINTF_STR(*aElignment);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
* Get object width
*/
/* readonly attribute unsigned long width; */
nsresult VFTCALL downIPTI2GetWidth(void *pvThis, PRUint32 *aWidth)
{
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);
    rc = pMozI->GetWidth(aWidth);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aWidth))
        dprintf(("%s: *aWidth=%d", pszFunction, *aWidth));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
* Get object height
*/
/* readonly attribute unsigned long height; */
nsresult VFTCALL downIPTI2GetHeight(void *pvThis, PRUint32 *aHeight)
{
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);
    rc = pMozI->GetHeight(aHeight);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aHeight))
        dprintf(("%s: *aHeight=%d", pszFunction, *aHeight));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
* Get border vertical space
*/
/* readonly attribute unsigned long borderVertSpace; */
nsresult VFTCALL downIPTI2GetBorderVertSpace(void *pvThis, PRUint32 *aBorderVertSpace)
{
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);
    rc = pMozI->GetBorderVertSpace(aBorderVertSpace);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aBorderVertSpace))
        dprintf(("%s: *aBorderVertSpace=%d", pszFunction, *aBorderVertSpace));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
* Get border horizontal space
*/
/* readonly attribute unsigned long borderHorizSpace; */
nsresult VFTCALL downIPTI2GetBorderHorizSpace(void *pvThis, PRUint32 *aBorderHorizSpace)
{
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);
    rc = pMozI->GetBorderHorizSpace(aBorderHorizSpace);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aBorderHorizSpace))
        dprintf(("%s: *aBorderHorizSpace=%d", pszFunction, *aBorderHorizSpace));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
* Returns a unique id for the current document containing plugin
 */
/* readonly attribute unsigned long uniqueID; */
nsresult VFTCALL downIPTI2GetUniqueID(void *pvThis, PRUint32 *aUniqueID)
{
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);
    rc = pMozI->GetUniqueID(aUniqueID);
    if (NS_SUCCEEDED(rc) && VALID_PTR(aUniqueID))
        dprintf(("%s: *aUniqueID=%d", pszFunction, *aUniqueID));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Returns the DOM element corresponding to the tag which references
 * this plugin in the document.
 *
* @param aDOMElement - resulting DOM element
 * @result - NS_OK if this operation was successful
 */
/* readonly attribute nsIDOMElement DOMElement; */
nsresult VFTCALL downIPTI2GetDOMElement(void *pvThis, nsIDOMElement * *aDOMElement)
{
    DOWN_ENTER_RC(pvThis, nsIPluginTagInfo2);

    const void *pvVFT = downIsSupportedInterface(kDOMElementIID);
    if (pvVFT)
    {
        rc = pMozI->GetDOMElement(aDOMElement);
        rc = downCreateWrapper((void**)aDOMElement, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported Interface!!!", pszFunction));
        rc = NS_NOINTERFACE;
        ReleaseInt3(0xbaddbeef, 14, kDOMElementIID.m0);
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}



MAKE_SAFE_VFT(VFTnsIPluginTagInfo2, downVFTnsIPluginTagInfo2)
{
 {
  {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
  },
    downIPTIGetAttributes,                                  VFTDELTA_VAL()
    downIPTIGetAttribute,                                   VFTDELTA_VAL()
 },
    downIPTI2GetTagType,                                    VFTDELTA_VAL()
    downIPTI2GetTagText,                                    VFTDELTA_VAL()
    downIPTI2GetParameters,                                 VFTDELTA_VAL()
    downIPTI2GetParameter,                                  VFTDELTA_VAL()
    downIPTI2GetDocumentBase,                               VFTDELTA_VAL()
    downIPTI2GetDocumentEncoding,                           VFTDELTA_VAL()
    downIPTI2GetAlignment,                                  VFTDELTA_VAL()
    downIPTI2GetWidth,                                      VFTDELTA_VAL()
    downIPTI2GetHeight,                                     VFTDELTA_VAL()
    downIPTI2GetBorderVertSpace,                            VFTDELTA_VAL()
    downIPTI2GetBorderHorizSpace,                           VFTDELTA_VAL()
    downIPTI2GetUniqueID,                                   VFTDELTA_VAL()
    downIPTI2GetDOMElement,                                 VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsICookieStorage
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Retrieves a cookie from the browser's persistent cookie store.
 * @param aCookieURL    - URL string to look up cookie with.
 * @param aCookieBuffer - buffer large enough to accomodate cookie data.
 * @param aCookieSize   - on input, size of the cookie buffer, on output cookie's size.
 */
/* void getCookie (in string aCookieURL, in voidPtr aCookieBuffer, in PRUint32Ref aCookieSize); */
nsresult VFTCALL downICSGetCookie(void *pvThis, const char *aCookieURL, void * aCookieBuffer, PRUint32 & aCookieSize)
{
    DOWN_ENTER_RC(pvThis, nsICookieStorage);
    dprintf(("%s: aCookieURL=%x aCookieBuffer=%x aCookieSize=%x",
             pszFunction, aCookieURL, aCookieBuffer, VALID_REF(aCookieSize) ? aCookieSize : 0xdeadbeef));
    DPRINTF_STR(aCookieURL);
    rc = pMozI->GetCookie(aCookieURL, aCookieBuffer, aCookieSize);
    if (NS_SUCCEEDED(rc) && VALID_REF(aCookieSize))
        dprintf(("%s: aCookieSize=%d", pszFunction, aCookieSize));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Stores a cookie in the browser's persistent cookie store.
 * @param aCookieURL    - URL string store cookie with.
 * @param aCookieBuffer - buffer containing cookie data.
 * @param aCookieSize   - specifies  size of cookie data.
 */
/* void setCookie (in string aCookieURL, in constVoidPtr aCookieBuffer, in unsigned long aCookieSize); */
nsresult VFTCALL downICSSetCookie(void *pvThis, const char *aCookieURL, const void * aCookieBuffer, PRUint32 aCookieSize)
{
    DOWN_ENTER_RC(pvThis, nsICookieStorage);
    dprintf(("%s: aCookieURL=%x aCookieBuffer=%x aCookieSize=%x",
             pszFunction, aCookieURL, aCookieBuffer, aCookieSize));
    DPRINTF_STR(aCookieURL);
    rc = pMozI->SetCookie(aCookieURL, aCookieBuffer, aCookieSize);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsICookieStorage, downVFTnsICookieStorage)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downICSGetCookie,                                       VFTDELTA_VAL()
    downICSSetCookie,                                       VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();






//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIJVMThreadManager
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Returns a unique identifier for the "current" system thread.
 */
#ifdef IPLUGINW_OUTOFTREE
nsresult VFTCALL downITMGetCurrentThread(void *pvThis, nsPluginThread* *threadID)
#else
nsresult VFTCALL downITMGetCurrentThread(void *pvThis, PRThread **threadID)
#endif
{
    DOWN_ENTER_RC(pvThis, nsIJVMThreadManager);
    rc = pMozI->GetCurrentThread(threadID);
    if (NS_SUCCEEDED(rc) && VALID_PTR(threadID))
        dprintf(("%s: *threadID=%d (0x%x)", pszFunction, *threadID, *threadID));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Pauses the current thread for the specified number of milliseconds.
 * If milli is zero, then merely yields the CPU if another thread of
 * greater or equal priority.
 */
nsresult VFTCALL downITMSleep(void *pvThis, PRUint32 milli/* = 0*/)
{
    DOWN_ENTER_RC(pvThis, nsIJVMThreadManager);
    dprintf(("%s: milli=%d", pszFunction, milli));
    rc = pMozI->Sleep(milli);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Creates a unique monitor for the specified address, and makes the
 * current system thread the owner of the monitor.
 */
nsresult VFTCALL downITMEnterMonitor(void *pvThis, void* address)
{
    DOWN_ENTER_RC(pvThis, nsIJVMThreadManager);
    dprintf(("%s: address=%p", pszFunction, address));
    rc = pMozI->EnterMonitor(address);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Exits the monitor associated with the address.
 */
nsresult VFTCALL downITMExitMonitor(void *pvThis, void* address)
{
    DOWN_ENTER_RC(pvThis, nsIJVMThreadManager);
    dprintf(("%s: address=%p", pszFunction, address));
    rc = pMozI->ExitMonitor(address);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Waits on the monitor associated with the address (must be entered already).
 * If milli is 0, wait indefinitely.
 */
nsresult VFTCALL downITMWait(void *pvThis, void* address, PRUint32 milli/* = 0*/)
{
    DOWN_ENTER_RC(pvThis, nsIJVMThreadManager);
    dprintf(("%s: address=%p milli=%u", pszFunction, address, milli));
    rc = pMozI->Wait(address, milli);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Notifies a single thread waiting on the monitor associated with the address (must be entered already).
 */
nsresult VFTCALL downITMNotify(void *pvThis, void* address)
{
    DOWN_ENTER_RC(pvThis, nsIJVMThreadManager);
    dprintf(("%s: address=%p", pszFunction, address));
    rc = pMozI->Notify(address);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Notifies all threads waiting on the monitor associated with the address (must be entered already).
 */
nsresult VFTCALL downITMNotifyAll(void *pvThis, void* address)
{
    DOWN_ENTER_RC(pvThis, nsIJVMThreadManager);
    dprintf(("%s: address=%p", pszFunction, address));
    rc = pMozI->NotifyAll(address);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Creates a new thread, calling the specified runnable's Run method (a la Java).
 */
#ifdef IPLUGINW_OUTOFTREE
nsresult VFTCALL downITMCreateThread(void *pvThis, PRUint32* threadID, nsIRunnable* runnable)
#else
nsresult VFTCALL downITMCreateThread(void *pvThis, PRThread **threadID, nsIRunnable* runnable)
#endif
{
    DOWN_ENTER_RC(pvThis, nsIJVMThreadManager);
    dprintf(("%s: threadID=%p runnable=%p", pszFunction, threadID, runnable));

    nsIRunnable * pupRunnable = runnable;
    rc = upCreateWrapper((void**)&pupRunnable, kRunnableIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        rc = pMozI->CreateThread(threadID, pupRunnable);
        if (NS_SUCCEEDED(rc) && VALID_PTR(threadID))
            dprintf(("%s: *threadID=%d", pszFunction, *threadID));
    }
    else
        dprintf(("%s: failed to create up wrapper for nsIRunnable %x !!!", pszFunction, runnable));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Posts an event to specified thread, calling the runnable from that thread.
 * @param threadID thread to call runnable from
 * @param runnable object to invoke from thread
 * @param async if true, won't block current thread waiting for result
 */
#ifdef IPLUGINW_OUTOFTREE
nsresult VFTCALL downITMPostEvent(void *pvThis, PRUint32 threadID, nsIRunnable* runnable, PRBool async)
#else
nsresult VFTCALL downITMPostEvent(void *pvThis, PRThread *threadID, nsIRunnable* runnable, PRBool async)
#endif
{
    DOWN_ENTER_RC(pvThis, nsIJVMThreadManager);
    dprintf(("%s: threadID=%d runnable=%p async=%d", pszFunction, threadID, runnable, async));

    nsIRunnable * pupRunnable = runnable;
    rc = upCreateWrapper((void**)&pupRunnable, kRunnableIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->PostEvent(threadID, pupRunnable, async);
    else
        dprintf(("%s: failed to create up wrapper for nsIRunnable %x !!!", pszFunction, runnable));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}



MAKE_SAFE_VFT(VFTnsIJVMThreadManager, downVFTnsIJVMThreadManager)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downITMGetCurrentThread,                                VFTDELTA_VAL()
    downITMSleep,                                           VFTDELTA_VAL()
    downITMEnterMonitor,                                    VFTDELTA_VAL()
    downITMExitMonitor,                                     VFTDELTA_VAL()
    downITMWait,                                            VFTDELTA_VAL()
    downITMNotify,                                          VFTDELTA_VAL()
    downITMNotifyAll,                                       VFTDELTA_VAL()
    downITMCreateThread,                                    VFTDELTA_VAL()
    downITMPostEvent,                                       VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIJVMManager
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Creates a proxy JNI with an optional secure environment (which can be NULL).
 * There is a one-to-one correspondence between proxy JNIs and threads, so
 * calling this method multiple times from the same thread will return
 * the same proxy JNI.
 */
nsresult VFTCALL downCreateProxyJNI(void *pvThis, nsISecureEnv *secureEnv, JNIEnv * *outProxyEnv)
{
    DOWN_ENTER_RC(pvThis, nsIJVMManager);

    nsISecureEnv *pupSecureEnv = secureEnv;
    rc = upCreateWrapper((void**)&pupSecureEnv, kSecureEnvIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        rc = pMozI->CreateProxyJNI(pupSecureEnv, outProxyEnv);
        rc = downCreateJNIEnvWrapper(outProxyEnv, rc);
    }
    else
        dprintf(("%s: failed to create up wrapper for kSecureEnvIID %x !!!", pszFunction, secureEnv));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Returns the proxy JNI associated with the current thread, or NULL if no
 * such association exists.
 */
nsresult VFTCALL downGetProxyJNI(void *pvThis, JNIEnv * *outProxyEnv)
{
    DOWN_ENTER_RC(pvThis, nsIJVMManager);

    rc = pMozI->GetProxyJNI(outProxyEnv);
    rc = downCreateJNIEnvWrapper(outProxyEnv, rc);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


/**
 * Called from Java Console menu item.
 */
nsresult VFTCALL downShowJavaConsole(void *pvThis)
{
    DOWN_ENTER_RC(pvThis, nsIJVMManager);

    dprintf(("%s", pszFunction));
    rc = pMozI->ShowJavaConsole();

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * isAllPermissionGranted [Deprecated Sep-10-2000]
 */
nsresult VFTCALL downIsAllPermissionGranted(void *pvThis, const char *lastFingerprint, const char *lastCommonName, const char *rootFingerprint, const char *rootCommonName, PRBool *_retval)
{
    //@todo TEXT: lastFingerprint? lastFingerprint? lastCommonName? rootCommonName? No idea. Need find usage.
    DOWN_ENTER_RC(pvThis, nsIJVMManager);
    dprintf(("%s _retval=%x", pszFunction, _retval));
    DPRINTF_STR(lastFingerprint);
    DPRINTF_STR(lastCommonName);
    DPRINTF_STR(rootFingerprint);
    DPRINTF_STR(rootCommonName);

    rc = pMozI->IsAllPermissionGranted(lastFingerprint, lastCommonName, rootFingerprint, rootCommonName, _retval);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * isAppletTrusted
 */
nsresult VFTCALL downIsAppletTrusted(void *pvThis, const char *aRSABuf, PRUint32 aRSABufLen, const char *aPlaintext, PRUint32 aPlaintextLen, PRBool *isTrusted, nsIPrincipal **_retval)
{
    //@todo TEXT: aRSABuf? aPlaintext? No idea. Need find usage.
    DOWN_ENTER_RC(pvThis, nsIJVMManager);

    dprintf(("%s aRSABuf=%x aRSABufLen=%d aPlaintext=%x aPlaintextLen=%d isTrusted=%x _retval=%x", pszFunction, aRSABuf, aRSABufLen, aPlaintext, aPlaintextLen, isTrusted, _retval));
    rc = pMozI->IsAppletTrusted(aRSABuf, aRSABufLen, aPlaintext, aPlaintextLen, isTrusted, _retval);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
  * The JavaEnabled variable to see if Java is Enabled or not
  */
nsresult VFTCALL downGetJavaEnabled(void *pvThis, PRBool *aJavaEnabled)
{
    DOWN_ENTER_RC(pvThis, nsIJVMManager);

    dprintf(("%s aJavaEnabled=%x", pszFunction, aJavaEnabled));
    rc = pMozI->GetJavaEnabled(aJavaEnabled);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsIJVMManager, downVFTnsIJVMManager)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downCreateProxyJNI,                                     VFTDELTA_VAL()
    downGetProxyJNI,                                        VFTDELTA_VAL()
    downShowJavaConsole,                                    VFTDELTA_VAL()
    downIsAllPermissionGranted,                             VFTDELTA_VAL()
    downIsAppletTrusted,                                    VFTDELTA_VAL()
    downGetJavaEnabled,                                     VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();




//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsILiveconnect
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * get member of a Native JSObject for a given name.
 *
 * @param obj        - A Native JS Object.
 * @param name       - Name of a member.
 * @param pjobj      - return parameter as a java object representing
 *                     the member. If it is a basic data type it is converted to
 *                     a corresponding java type. If it is a NJSObject, then it is
 *                     wrapped up as java wrapper netscape.javascript.JSObject.
 */
nsresult VFTCALL downGetMember(void *pvThis, JNIEnv *jEnv, jsobject jsobj, const jchar *name, jsize length, void* principalsArray[],
                                  int numPrincipals, nsISupports *securitySupports, jobject *pjobj)
{
    DOWN_ENTER_RC(pvThis, nsILiveconnect);
    dprintf(("%s: jEnv=%p jsobj=%p name=%p length=%d principalsArray=%p numPrincipals=%d securitySupports=%p pjobj=%p",
             pszFunction, jEnv, jsobj, name, length, principalsArray, numPrincipals, securitySupports, pjobj));
    if (VALID_PTR(name))
        dprintf(("%s: name=%ls", pszFunction, name));

    if (numPrincipals)
    {
        /** @todo check out what this array contains. (not used by oji plugin I believe)
         * Guess it's of type nsIPrincipal or some decendant, can't see mozilla caring much about it...
         */
        dprintf(("%s: numPrincipals/princiapalsArray isn't supported !!!", pszFunction));
        ReleaseInt3(0xdead0000, 0, 7);
    }

    JNIEnv * pupJNIEnv = jEnv;
    rc = upCreateJNIEnvWrapper(&pupJNIEnv, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports *pupSecuritySupports = securitySupports;
        rc = upCreateWrapper((void**)&pupSecuritySupports, kSupportsIID, rc);
        if (NS_SUCCEEDED(rc))
        {
            rc = pMozI->GetMember(pupJNIEnv, jsobj, name, length, principalsArray, numPrincipals, pupSecuritySupports, pjobj);
            if (NS_SUCCEEDED(rc) && VALID_PTR(pjobj))
                dprintf(("%s: *pjobj=%p", pszFunction, *pjobj));
        }
        else
            dprintf(("%s: Failed make up wrapper for nsISupports !!!", pszFunction));
    }
    else
        dprintf(("%s: Failed make up wrapper for JNIEnv !!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * get member of a Native JSObject for a given index.
 *
 * @param obj        - A Native JS Object.
 * @param slot      - Index of a member.
 * @param pjobj      - return parameter as a java object representing
 *                     the member.
 */
nsresult VFTCALL downGetSlot(void *pvThis, JNIEnv *jEnv, jsobject jsobj, jint slot, void* principalsArray[],
                                int numPrincipals, nsISupports *securitySupports, jobject *pjobj)
{
    DOWN_ENTER_RC(pvThis, nsILiveconnect);
    dprintf(("%s: jEnv=%p jsobj=%p slot=%p principalsArray=%p numPrincipals=%d securitySupports=%p pjobj=%p",
             pszFunction, jEnv, jsobj, slot, principalsArray, numPrincipals, securitySupports, pjobj));

    if (numPrincipals)
    {
        /** @todo check out what this array contains. (not used by oji plugin I believe)
         * Guess it's of type nsIPrincipal or some decendant, can't see mozilla caring much about it...
         */
        dprintf(("%s: numPrincipals/princiapalsArray isn't supported !!!", pszFunction));
        ReleaseInt3(0xdead0000, 0, 8);
    }

    JNIEnv * pupJNIEnv = jEnv;
    rc = upCreateJNIEnvWrapper(&pupJNIEnv, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports *pupSecuritySupports = securitySupports;
        rc = upCreateWrapper((void**)&pupSecuritySupports, kSupportsIID, rc);
        if (NS_SUCCEEDED(rc))
        {
            rc = pMozI->GetSlot(pupJNIEnv, jsobj, slot, principalsArray, numPrincipals, pupSecuritySupports, pjobj);
            if (NS_SUCCEEDED(rc) && VALID_PTR(pjobj))
                dprintf(("%s: *pjobj=%p", pszFunction, *pjobj));
        }
        else
            dprintf(("%s: Failed make up wrapper for nsISupports !!!", pszFunction));
    }
    else
        dprintf(("%s: Failed make up wrapper for JNIEnv !!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * set member of a Native JSObject for a given name.
 *
 * @param obj        - A Native JS Object.
 * @param name       - Name of a member.
 * @param jobj       - Value to set. If this is a basic data type, it is converted
 *                     using standard JNI calls but if it is a wrapper to a JSObject
 *                     then a internal mapping is consulted to convert to a NJSObject.
 */
nsresult VFTCALL downSetMember(void *pvThis, JNIEnv *jEnv, jsobject jsobj, const jchar* name, jsize length, jobject jobj, void* principalsArray[],
                                  int numPrincipals, nsISupports *securitySupports)
{
    DOWN_ENTER_RC(pvThis, nsILiveconnect);
    dprintf(("%s: jEnv=%p jsobj=%p name=%p length=%d jobj=%p principalsArray=%p numPrincipals=%d securitySupports=%p",
             pszFunction, jEnv, jsobj, name, length, jobj, principalsArray, numPrincipals, securitySupports));
    DPRINTF_STR(name);

    if (numPrincipals)
    {
        /** @todo check out what this array contains. (not used by oji plugin I believe)
         * Guess it's of type nsIPrincipal or some decendant, can't see mozilla caring much about it...
         */
        dprintf(("%s: numPrincipals/princiapalsArray isn't supported !!!", pszFunction));
        ReleaseInt3(0xdead0000, 0, 9);
    }

    JNIEnv * pupJNIEnv = jEnv;
    rc = upCreateJNIEnvWrapper(&pupJNIEnv, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports *pupSecuritySupports = securitySupports;
        rc = upCreateWrapper((void**)&pupSecuritySupports, kSupportsIID, rc);
        if (NS_SUCCEEDED(rc))
            rc = pMozI->SetMember(pupJNIEnv, jsobj, name, length, jobj, principalsArray, numPrincipals, pupSecuritySupports);
        else
            dprintf(("%s: Failed make up wrapper for nsISupports !!!", pszFunction));
    }
    else
        dprintf(("%s: Failed make up wrapper for JNIEnv !!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * set member of a Native JSObject for a given index.
 *
 * @param obj        - A Native JS Object.
 * @param index      - Index of a member.
 * @param jobj       - Value to set. If this is a basic data type, it is converted
 *                     using standard JNI calls but if it is a wrapper to a JSObject
 *                     then a internal mapping is consulted to convert to a NJSObject.
 */
nsresult VFTCALL downSetSlot(void *pvThis, JNIEnv *jEnv, jsobject jsobj, jint slot, jobject jobj, void* principalsArray[],
                                int numPrincipals, nsISupports *securitySupports)
{
    DOWN_ENTER_RC(pvThis, nsILiveconnect);
    dprintf(("%s: jEnv=%p jsobj=%p slot=%p jobj=%p principalsArray=%p numPrincipals=%d securitySupports=%p",
             pszFunction, jEnv, jsobj, slot, jobj, principalsArray, numPrincipals, securitySupports));

    if (numPrincipals)
    {
        /** @todo check out what this array contains. (not used by oji plugin I believe)
         * Guess it's of type nsIPrincipal or some decendant, can't see mozilla caring much about it...
         */
        dprintf(("%s: numPrincipals/princiapalsArray isn't supported !!!", pszFunction));
        ReleaseInt3(0xdead0000, 0, 8);
    }

    JNIEnv * pupJNIEnv = jEnv;
    rc = upCreateJNIEnvWrapper(&pupJNIEnv, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports *pupSecuritySupports = securitySupports;
        rc = upCreateWrapper((void**)&pupSecuritySupports, kSupportsIID, rc);
        if (NS_SUCCEEDED(rc))
            rc = pMozI->SetSlot(pupJNIEnv, jsobj, slot, jobj, principalsArray, numPrincipals, pupSecuritySupports);
        else
            dprintf(("%s: Failed make up wrapper for nsISupports !!!", pszFunction));
    }
    else
        dprintf(("%s: Failed make up wrapper for JNIEnv !!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * remove member of a Native JSObject for a given name.
 *
 * @param obj        - A Native JS Object.
 * @param name       - Name of a member.
 */
nsresult VFTCALL downRemoveMember(void *pvThis, JNIEnv *jEnv, jsobject jsobj, const jchar* name, jsize length,  void* principalsArray[],
                                     int numPrincipals, nsISupports *securitySupports)
{
    DOWN_ENTER_RC(pvThis, nsILiveconnect);
    dprintf(("%s: jEnv=%p jsobj=%p name=%p length=%d principalsArray=%p numPrincipals=%d securitySupports=%p",
             pszFunction, jEnv, jsobj, name, length, principalsArray, numPrincipals, securitySupports));
    if (VALID_PTR(name))
        dprintf(("%s: name=%ls", pszFunction, name));

    if (numPrincipals)
    {
        /** @todo check out what this array contains. (not used by oji plugin I believe)
         * Guess it's of type nsIPrincipal or some decendant, can't see mozilla caring much about it...
         */
        dprintf(("%s: numPrincipals/princiapalsArray isn't supported !!!", pszFunction));
        ReleaseInt3(0xdead0000, 0, 10);
    }

    JNIEnv * pupJNIEnv = jEnv;
    rc = upCreateJNIEnvWrapper(&pupJNIEnv, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports *pupSecuritySupports = securitySupports;
        rc = upCreateWrapper((void**)&pupSecuritySupports, kSupportsIID, rc);
        if (NS_SUCCEEDED(rc))
            rc = pMozI->RemoveMember(pupJNIEnv, jsobj, name, length, principalsArray, numPrincipals, pupSecuritySupports);
        else
            dprintf(("%s: Failed make up wrapper for nsISupports !!!", pszFunction));
    }
    else
        dprintf(("%s: Failed make up wrapper for JNIEnv !!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * call a method of Native JSObject.
 *
 * @param obj        - A Native JS Object.
 * @param name       - Name of a method.
 * @param jobjArr    - Array of jobjects representing parameters of method being caled.
 * @param pjobj      - return value.
 */
nsresult VFTCALL downCall(void *pvThis, JNIEnv *jEnv, jsobject jsobj, const jchar* name, jsize length, jobjectArray jobjArr,  void* principalsArray[],
                             int numPrincipals, nsISupports *securitySupports, jobject *pjobj)
{
    DOWN_ENTER_RC(pvThis, nsILiveconnect);
    dprintf(("%s: jEnv=%p jsobj=%p name=%p length=%d jobjArr=%p principalsArray=%p numPrincipals=%d securitySupports=%p pjobj=%p",
             pszFunction, jEnv, jsobj, name, length, jobjArr, principalsArray, numPrincipals, securitySupports, pjobj));
    if (VALID_PTR(name))
        dprintf(("%s: name=%ls", pszFunction, name));

    if (numPrincipals)
    {
        /** @todo check out what this array contains. (not used by oji plugin I believe)
         * Guess it's of type nsIPrincipal or some decendant, can't see mozilla caring much about it...
         */
        dprintf(("%s: numPrincipals/princiapalsArray isn't supported !!!", pszFunction));
        ReleaseInt3(0xdead0000, 0, 11);
    }

    JNIEnv * pupJNIEnv = jEnv;
    rc = upCreateJNIEnvWrapper(&pupJNIEnv, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports *pupSecuritySupports = securitySupports;
        rc = upCreateWrapper((void**)&pupSecuritySupports, kSupportsIID, rc);
        if (NS_SUCCEEDED(rc))
        {
            rc = pMozI->Call(pupJNIEnv, jsobj, name, length, jobjArr, principalsArray, numPrincipals, pupSecuritySupports, pjobj);
            if (NS_SUCCEEDED(rc) && VALID_PTR(pjobj))
                dprintf(("%s: *pjobj=%p", pszFunction, *pjobj));
        }
        else
            dprintf(("%s: Failed make up wrapper for nsISupports !!!", pszFunction));
    }
    else
        dprintf(("%s: Failed make up wrapper for JNIEnv !!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Evaluate a script with a Native JS Object representing scope.
 *
 * @param jsobj              - A Native JS Object.
 * @param principalsArray    - Array of principals to be used to compare privileges.
 * @param numPrincipals      - Number of principals being passed.
 * @param script             - Script to be executed.
 * @param pjobj              - return value.
 */
nsresult VFTCALL downEval(void *pvThis, JNIEnv *jEnv, jsobject jsobj, const jchar *script, jsize length, void* principalsArray[],
                             int numPrincipals, nsISupports *securitySupports, jobject *pjobj)
{
    DOWN_ENTER_RC(pvThis, nsILiveconnect);
    dprintf(("%s: jEnv=%p jsobj=%p script=%p length=%d principalsArray=%p numPrincipals=%d securitySupports=%p pjobj=%p",
             pszFunction, jEnv, jsobj, script, length, principalsArray, numPrincipals, securitySupports, pjobj));
    if (VALID_PTR(script))
        dprintf(("%s: script=%ls", pszFunction, script));

    if (numPrincipals)
    {
        /** @todo check out what this array contains. (not used by oji plugin I believe)
         * Guess it's of type nsIPrincipal or some decendant, can't see mozilla caring much about it...
         */
        dprintf(("%s: numPrincipals/princiapalsArray isn't supported !!!", pszFunction));
        ReleaseInt3(0xdead0000, 0, 12);
    }

    JNIEnv * pupJNIEnv = jEnv;
    rc = upCreateJNIEnvWrapper(&pupJNIEnv, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports *pupSecuritySupports = securitySupports;
        rc = upCreateWrapper((void**)&pupSecuritySupports, kSupportsIID, rc);
        if (NS_SUCCEEDED(rc))
        {
            rc = pMozI->Eval(pupJNIEnv, jsobj, script, length, principalsArray, numPrincipals, pupSecuritySupports, pjobj);
            if (NS_SUCCEEDED(rc) && VALID_PTR(pjobj))
                dprintf(("%s: *pjobj=%p", pszFunction, *pjobj));
        }
        else
            dprintf(("%s: Failed make up wrapper for nsISupports !!!", pszFunction));
    }
    else
        dprintf(("%s: Failed make up wrapper for JNIEnv !!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Get the window object for a plugin instance.
 *
 * @param pJavaObject        - Either a jobject or a pointer to a plugin instance
 *                             representing the java object.
 * @param pjobj              - return value. This is a native js object
 *                             representing the window object of a frame
 *                             in which a applet/bean resides.
 */
nsresult VFTCALL downGetWindow(void *pvThis, JNIEnv *jEnv, void *pJavaObject, void* principalsArray[],
                                  int numPrincipals, nsISupports *securitySupports, jsobject *pobj)
{
    DOWN_ENTER_RC(pvThis, nsILiveconnect);
    dprintf(("%s: jEnv=%p pJavaObject=%p principalsArray=%p numPrincipals=%d securitySupports=%p pObj=%p",
             pszFunction, jEnv, pJavaObject, principalsArray, numPrincipals, securitySupports, pobj));
    if (numPrincipals)
    {
        //@todo check out what this array contains. (not used by oji plugin I believe)
        dprintf(("%s: numPrincipals/princiapalsArray isn't supported !!!", pszFunction));
        ReleaseInt3(0xdead0000, 0, 6);
    }

    /*
     * JNI Wrapper
     */
    JNIEnv * pupJNIEnv = jEnv;
    rc = upCreateJNIEnvWrapper(&pupJNIEnv, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports *pupSecuritySupports = securitySupports;
        rc = upCreateWrapper((void**)&pupSecuritySupports, kSupportsIID, rc);
        if (NS_SUCCEEDED(rc))
        {
            /**
             * @todo: how can we distinguish between a java and instance object??
             *        the java plugin seems to only use an instance pointer for calling
             *        this function
             *  kso:  Good question, but it seems like assuming nsIPluginInstance should
             *        work for us. At least looking at the code path of GetWindow.
             *        It ends up casting the pJavaObject to nsIPluginInstance in
             *        map_jsj_thread_to_js_context_impl() mozilla\modules\oji\src\Icglue.cpp.
             */
            nsIPluginInstance *pupIPluginInstance = (nsIPluginInstance *)pJavaObject;
            rc = upCreateWrapper((void**)&pupIPluginInstance, kPluginInstanceIID, rc);
            if (NS_SUCCEEDED(rc))
            {
                rc = pMozI->GetWindow(pupJNIEnv, pupIPluginInstance, principalsArray, numPrincipals, pupSecuritySupports, pobj);
                if (NS_SUCCEEDED(rc) && VALID_PTR(pobj))
                    //@todo ilink crash with this code! f**k ilink/icc!!!
                    //dprintf(("%s: *pobj=%d (0x%x)", pszFunction, *pobj, *pobj));
                    dprintf(("%s: *pobj=%d (0x%x)", XPFUNCTION, *pobj, *pobj));
            }
            else
                dprintf(("%s: Failed to create up wrapper for nsISupports %p !!!", pszFunction, securitySupports));
        }
        else
            dprintf(("%s: Failed to create up wrapper for nsISupports %p !!!", pszFunction, securitySupports));
    }
    else
    {
        dprintf(("%s: Failed to make up wrapper for JNIEnv !!!", pszFunction));
        rc = NS_ERROR_UNEXPECTED;
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Get the window object for a plugin instance.
 *
 * @param jEnv       - JNIEnv on which the call is being made.
 * @param obj        - A Native JS Object.
 */
nsresult VFTCALL downFinalizeJSObject(void *pvThis, JNIEnv *jEnv, jsobject jsobj)
{
    DOWN_ENTER_RC(pvThis, nsILiveconnect);
    dprintf(("%s: jEnv=%p jsobj=%p", pszFunction, jEnv, jsobj));

    JNIEnv * pupJNIEnv = jEnv;
    rc = upCreateJNIEnvWrapper(&pupJNIEnv, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->FinalizeJSObject(pupJNIEnv, jsobj);
    else
        dprintf(("%s: Failed make up wrapper for JNIEnv !!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Get the window object for a plugin instance.
 *
 * @param jEnv       - JNIEnv on which the call is being made.
 * @param obj        - A Native JS Object.
 * @param jstring    - Return value as a jstring representing a JS object.
 */
nsresult VFTCALL downToString(void *pvThis, JNIEnv *jEnv, jsobject obj, jstring *pjstring)
{
    DOWN_ENTER_RC(pvThis, nsILiveconnect);
    dprintf(("%s: jEnv=%p obj=%p pjstring=%p", pszFunction, jEnv, obj, pjstring));

    JNIEnv * pupJNIEnv = jEnv;
    rc = upCreateJNIEnvWrapper(&pupJNIEnv, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        rc = pMozI->ToString(pupJNIEnv, obj, pjstring);
        if (NS_SUCCEEDED(rc) && VALID_PTR(pjstring))
            dprintf(("%s: *pjstring=%p", pszFunction, pjstring));
    }
    else
        dprintf(("%s: Failed make up wrapper for JNIEnv !!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsILiveconnect, downVFTnsILiveconnect)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downGetMember,                                          VFTDELTA_VAL()
    downGetSlot,                                            VFTDELTA_VAL()
    downSetMember,                                          VFTDELTA_VAL()
    downSetSlot,                                            VFTDELTA_VAL()
    downRemoveMember,                                       VFTDELTA_VAL()
    downCall,                                               VFTDELTA_VAL()
    downEval,                                               VFTDELTA_VAL()
    downGetWindow,                                          VFTDELTA_VAL()
    downFinalizeJSObject,                                   VFTDELTA_VAL()
    downToString,                                           VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsISecureLiveconnect
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Evaluate a script with a Native JS Object representing scope.
 *
 * @param jEnv               - JNIEnv pointer
 * @param jsobj              - A Native JS Object.
 * @param script             - JavaScript to be executed.
 * @param pNSIPrincipaArray  - Array of principals to be used to compare privileges.
 * @param numPrincipals      - Number of principals being passed.
 * @param context            - Security context.
 * @param pjobj              - return value.
 */
nsresult VFTCALL downSecureLiveConnectEval(
     void *pvThis, JNIEnv *jEnv, jsobject jsobj, const jchar *script, jsize length, void **pNSIPrincipaArray,
     int numPrincipals, void *pNSISecurityContext, jobject *pjobj)
{
    DOWN_ENTER_RC(pvThis, nsISecureLiveconnect);
    dprintf(("%s: jEnv=%p jsobj=%p script=%p length=%d principalsArray=%p numPrincipals=%d pNSISecurityContext=%p pjobj=%p",
             pszFunction, jEnv, jsobj, script, length, pNSIPrincipaArray, numPrincipals, pNSISecurityContext, pjobj));
    if (VALID_PTR(script))
        dprintf(("%s: script=%ls", pszFunction, script));

    /**@todo I believe this interface isn't implemented in Mozilla (yet). Can't find
     * any references to use of it. So, I'm very curious about a few details.
     */
    DebugInt3();

    if (numPrincipals)
    {
        /** @todo check out what this array contains. (not used by oji plugin I believe)
         * Guess it's of type nsIPrincipal or some decendant, can't see mozilla caring much about it...
         */
        dprintf(("%s: numPrincipals/princiapalsArray isn't supported !!!", pszFunction));
        ReleaseInt3(0xdead0000, 0, 14);
    }

    JNIEnv * pupJNIEnv = jEnv;
    rc = upCreateJNIEnvWrapper(&pupJNIEnv, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        DebugInt3(); //@todo check that following wrapper is correct!!!
        nsISecurityContext *pupSecurityContext = (nsISecurityContext*)pNSISecurityContext;
        rc = upCreateWrapper((void**)&pupSecurityContext, kSecurityContextIID, rc);
        if (NS_SUCCEEDED(rc))
        {
            rc = pMozI->Eval(pupJNIEnv, jsobj, script, length, pNSIPrincipaArray, numPrincipals, pupSecurityContext, pjobj);
            if (NS_SUCCEEDED(rc) && VALID_PTR(pjobj))
                dprintf(("%s: *pjobj=%p", pszFunction, *pjobj));
        }
        else
            dprintf(("%s: Failed make up wrapper for nsISupports !!!", pszFunction));
    }
    else
        dprintf(("%s: Failed make up wrapper for JNIEnv !!!", pszFunction));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

MAKE_SAFE_VFT(VFTnsISecureLiveconnect, downVFTnsISecureLiveconnect)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downSecureLiveConnectEval,                              VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsISecurityContext
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Get the security context to be used in LiveConnect.
 * This is used for JavaScript <--> Java.
 *
 * @param target        -- Possible target.
 * @param action        -- Possible action on the target.
 * @return              -- NS_OK if the target and action is permitted on the security context.
 *                      -- NS_FALSE otherwise.
 */
nsresult VFTCALL downImplies(void *pvThis, const char* target, const char* action, PRBool *bAllowedAccess)
{
    DOWN_ENTER_RC(pvThis, nsISecurityContext);
    dprintf(("%s: target=%x action=%x bAllowedAccess=%x", pszFunction, target, action, bAllowedAccess));
    DPRINTF_STR(target);
    DPRINTF_STR(action);

    rc = pMozI->Implies(target, action, bAllowedAccess);
    if (/*NS_SUCCEEDED(rc) && */VALID_PTR(bAllowedAccess))
        dprintf(("%s: *bAllowedAccess=%d", pszFunction, *bAllowedAccess));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Get the origin associated with the context.
 *
 * @param buf        -- Result buffer (managed by the caller.)
 * @param len        -- Buffer length.
 * @return           -- NS_OK if the codebase string was obtained.
 *                   -- NS_FALSE otherwise.
 */
nsresult VFTCALL downGetOrigin(void *pvThis, char* buf, int len)
{
    DOWN_ENTER_RC(pvThis, nsISecurityContext);
    dprintf(("%s: buf=%p len=%d", pszFunction, buf, len));

    rc = pMozI->GetOrigin(buf, len);
    if (NS_SUCCEEDED(rc) && VALID_PTR(buf))
        DPRINTF_STR(buf);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Get the certificate associated with the context.
 *
 * @param buf        -- Result buffer (managed by the caller.)
 * @param len        -- Buffer length.
 * @return           -- NS_OK if the codebase string was obtained.
 *                   -- NS_FALSE otherwise.
 */
nsresult VFTCALL downGetCertificateID(void *pvThis, char* buf, int len)
{
    DOWN_ENTER_RC(pvThis, nsISecurityContext);
    dprintf(("%s: buf=%p len=%d", pszFunction, buf, len));

    rc = pMozI->GetCertificateID(buf, len);
    if (NS_SUCCEEDED(rc) && VALID_PTR(buf))
        DPRINTF_STR(buf);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsISecurityContext, downVFTnsISecurityContext)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downImplies,                                            VFTDELTA_VAL()
    downGetOrigin,                                          VFTDELTA_VAL()
    downGetCertificateID,                                   VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIComponentManagerObsolete
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * findFactory
 *
 * Returns the factory object that can be used to create instances of
 * CID aClass
 *
 * @param aClass The classid of the factory that is being requested
 */
/* nsIFactory findFactory (in nsCIDRef aClass); */
nsresult VFTCALL downFindFactory(void *pvThis, const nsCID & aClass, nsIFactory **_retval)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);

    const void * pvVFT = downIsSupportedInterface(kFactoryIID);
    if (pvVFT)
    {
        rc = pMozI->FindFactory(aClass, _retval);
        rc = downCreateWrapper((void**)_retval, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported down interface kFactoryIID!!!", pszFunction));
        rc = NS_NOINTERFACE;
        ReleaseInt3(0xbaddbeef, 29, kFactoryIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * getClassObject
 *
 * @param aClass : CID of the class whose class object is requested
 * @param aIID : IID of an interface that the class object is known to
 *               to implement. nsISupports and nsIFactory are known to
 *               be implemented by the class object.
 */
/* [noscript] voidPtr getClassObject (in nsCIDRef aClass, in nsIIDRef aIID); */
nsresult VFTCALL downGetClassObject(void *pvThis, const nsCID & aClass, const nsIID & aIID, void * *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);
    DPRINTF_NSID(aIID);

    /*
     * Supported interface requested?
     */
    const void * pvVFT = downIsSupportedInterface(aIID);
    if (pvVFT)
    {
        rc = pMozI->GetClassObject(aClass, aIID, _retval);
        rc = downCreateWrapper(_retval, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported down interface !!!", pszFunction));
        rc = NS_NOINTERFACE;
        ReleaseInt3(0xbaddbeef, 27, aIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * contractIDToClassID
 *
 * Get the ClassID for a given ContractID. Many ClassIDs may implement a
 * ContractID. In such a situation, this returns the preferred ClassID, which
 * happens to be the last registered ClassID.
 *
 * @param aContractID : Contractid for which ClassID is requested
 * @return aClass : ClassID return
 */
/* [notxpcom] nsresult contractIDToClassID (in string aContractID, out nsCID aClass); */
nsresult VFTCALL downContractIDToClassID(void *pvThis, const char *aContractID, nsCID *aClass)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_CONTRACTID(aContractID);

    rc = pMozI->ContractIDToClassID(aContractID, aClass);
    if (NS_SUCCEEDED(rc))
        DPRINTF_NSID(*aClass);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * classIDToContractid
 *
 * Get the ContractID for a given ClassID. A ClassIDs may implement multiple
 * ContractIDs. This function return the last registered ContractID.
 *
 * @param aClass : ClassID for which ContractID is requested.
 * @return aClassName : returns class name asssociated with aClass
 * @return : ContractID last registered for aClass
 */
/* string CLSIDToContractID (in nsCIDRef aClass, out string aClassName); */
nsresult VFTCALL downCLSIDToContractID(void *pvThis, const nsCID & aClass, char **aClassName, char **_retval)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);

    rc = pMozI->CLSIDToContractID(aClass, aClassName, _retval);
    if (NS_SUCCEEDED(rc))
    {
        if (VALID_PTR(aClassName))
            DPRINTF_STR(aClassName);
        if (VALID_PTR(_retval))
            DPRINTF_STR(_retval);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * createInstance
 *
 * Create an instance of the CID aClass and return the interface aIID.
 *
 * @param aClass : ClassID of object instance requested
 * @param aDelegate : Used for aggregation
 * @param aIID : IID of interface requested
 */
/* [noscript] voidPtr createInstance (in nsCIDRef aClass, in nsISupports aDelegate, in nsIIDRef aIID); */
nsresult VFTCALL downCreateInstance(void *pvThis, const nsCID & aClass, nsISupports *aDelegate, const nsIID & aIID, void * *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);
    DPRINTF_NSID(aIID);
    dprintf(("%s: aDelegate=%p _retval=%p", pszFunction, aDelegate, _retval));


    /*
     * Supported interface requested?
     */
    const void * pvVFT = downIsSupportedInterface(aIID);
    if (pvVFT)
    {
        /*
         * Wrap the aDelegate.
         *  (Pray this isn't supposed to be the same kind of wrapper as nsIID.
         *   Haven't found any examples using aDelegate unfortunately.)
         */
        nsISupports * pupOuter = aDelegate;
        rc = upCreateWrapper((void**)&pupOuter, kSupportsIID, NS_OK);
        if (rc == NS_OK)
        {
            rc = pMozI->CreateInstance(aClass, pupOuter, aIID, _retval);
            rc = downCreateWrapper(_retval, pvVFT, rc);
        }
        else
            dprintf(("%s: failed to create up wrapper for kSupportsIID %x !!!", pszFunction, aDelegate));
    }
    else
    {
        dprintf(("%s: Unsupported down interface!!!", pszFunction));
        rc = NS_NOINTERFACE;
        ReleaseInt3(0xbaddbeef, 9, aIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * createInstanceByContractID
 *
 * Create an instance of the CID that implements aContractID and return the
 * interface aIID. This is a convenience function that effectively does
 * ContractIDToClassID() followed by CreateInstance().
 *
 * @param aContractID : aContractID of object instance requested
 * @param aDelegate : Used for aggregation
 * @param aIID : IID of interface requested
 */
/* [noscript] voidPtr createInstanceByContractID (in string aContractID, in nsISupports aDelegate, in nsIIDRef IID); */
nsresult VFTCALL downCreateInstanceByContractID(void *pvThis, const char *aContractID, nsISupports *aDelegate, const nsIID & aIID, void * *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_CONTRACTID(aContractID);
    DPRINTF_NSID(aIID);
    dprintf(("%s: aDelegate=%p _retval=%p", pszFunction, aDelegate, _retval));


    /*
     * Supported interface requested?
     */
    const void * pvVFT = downIsSupportedInterface(aIID);
    if (pvVFT)
    {
        /*
         * Wrap the aDelegate.
         *  (Pray this isn't supposed to be the same kind of wrapper as nsIID.
         *   Haven't found any examples using aDelegate unfortunately.)
         */
        nsISupports * pupOuter = aDelegate;
        rc = upCreateWrapper((void**)&pupOuter, kSupportsIID, NS_OK);
        if (rc == NS_OK)
        {
            rc = pMozI->CreateInstanceByContractID(aContractID, pupOuter, aIID, _retval);
            rc = downCreateWrapper(_retval, pvVFT, rc);
        }
        else
            dprintf(("%s: failed to create up wrapper for kSupportsIID %x !!!", pszFunction, aDelegate));
    }
    else
    {
        dprintf(("%s: Unsupported down interface!!!", pszFunction));
        ReleaseInt3(0xbaddbeef, 30, aIID.m0);
        rc = NS_NOINTERFACE;
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * registryLocationForSpec
 *
 * Given a file specification, return the registry representation of
 * the filename. Files that are found relative to the components
 * directory will have a registry representation
 * "rel:<relative-native-path>" while filenames that are not, will have
 * "abs:<full-native-path>".
 */
/* string registryLocationForSpec (in nsIFile aSpec); */
nsresult VFTCALL downRegistryLocationForSpec(void *pvThis, nsIFile *aSpec, char **_retval)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    dprintf(("%s: aSpec=%p _retval=%p", pszFunction, aSpec, _retval));

    nsIFile *   pupFile = aSpec;
    rc = upCreateWrapper((void**)&pupFile, kFileIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        rc = pMozI->RegistryLocationForSpec(pupFile, _retval);
        if (NS_SUCCEEDED(rc) && VALID_PTR(_retval) && VALID_PTR(*_retval))
            DPRINTF_STR(*_retval);
    }
    else
    {
        dprintf(("%s: Unsupported up interface nsIFile !!!", pszFunction));
        ReleaseInt3(0xbaddbeef, 20, kFileIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * specForRegistyLocation
 *
 * Create a file specification for the registry representation (rel:/abs:)
 * got via registryLocationForSpec.
 */
/* nsIFile specForRegistryLocation (in string aLocation); */
nsresult VFTCALL downSpecForRegistryLocation(void *pvThis, const char *aLocation, nsIFile **_retval)
{
    //@todo TEXT: aLocation? Yes. Do java use this?
    //Sources say: i18n: assuming aLocation is encoded for the current locale
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_STR(aLocation);
    dprintf(("%s: _retval=%p", pszFunction, _retval));

    const void *pvVFT = downIsSupportedInterface(kFileIID);
    if (pvVFT)
    {
        rc = pMozI->SpecForRegistryLocation(aLocation, _retval);
        rc = downCreateWrapper((void**)_retval, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported down interface nsIFile !!!", pszFunction));
        ReleaseInt3(0xbaddbeef, 22, kFileIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * registerFactory
 *
 * Register a factory and ContractID associated with CID aClass
 *
 * @param aClass : CID of object
 * @param aClassName : Class Name of CID
 * @param aContractID : ContractID associated with CID aClass
 * @param aFactory : Factory that will be registered for CID aClass
 * @param aReplace : Boolean that indicates whether to replace a previous
 *                   registration for the CID aClass.
 */
/* void registerFactory (in nsCIDRef aClass, in string aClassName, in string aContractID, in nsIFactory aFactory, in boolean aReplace); */
nsresult VFTCALL downRegisterFactory(void *pvThis, const nsCID & aClass, const char *aClassName, const char *aContractID, nsIFactory *aFactory, PRBool aReplace)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);
    DPRINTF_CONTRACTID(aContractID);
    DPRINTF_STR(aClassName);
    dprintf(("%s: aFactory=%p aReplace=%d", pszFunction, aFactory, aReplace));

    nsIFactory  *pupFactory = aFactory;
    rc = upCreateWrapper((void**)&pupFactory, kFactoryIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->RegisterFactory(aClass, aClassName, aContractID, pupFactory, aReplace);
    else
    {
        dprintf(("%s: Failed to make up wrapper for nsIFactoryIID %p !!!", pszFunction, aFactory));
        ReleaseInt3(0xbaddbeef, 13, kFactoryIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * registerComponent
 *
 * Register a native dll module via its registry representation as returned
 * by registryLocationForSpec() as the container of CID implemenation
 * aClass and associate aContractID and aClassName to the CID aClass. Native
 * dll component type is assumed.
 *
 * @param aClass : CID implemenation contained in module
 * @param aClassName : Class name associated with CID aClass
 * @param aContractID : ContractID associated with CID aClass
 * @param aLocation : Location of module (dll). Format of this is the
 *                    registry representation as returned by
 *                    registryLocationForSpec()
 * @param aReplace : Boolean that indicates whether to replace a previous
 *                   module registration for aClass.
 * @param aPersist : Remember this registration across sessions.
 */
/* void registerComponent (in nsCIDRef aClass, in string aClassName, in string aContractID, in string aLocation, in boolean aReplace, in boolean aPersist); */
nsresult VFTCALL downRegisterComponent(void *pvThis, const nsCID & aClass, const char *aClassName, const char *aContractID, const char *aLocation, PRBool aReplace, PRBool aPersist)
{
    //@todo TEXT: location? Yes. Don't think java uses this.
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);
    DPRINTF_CONTRACTID(aContractID);
    DPRINTF_STR(aClassName);
    DPRINTF_STR(aLocation);
    dprintf(("%s: aReplace=%d aPersist=%d", pszFunction, aReplace, aPersist));

    rc = pMozI->RegisterComponent(aClass, aClassName, aContractID, aLocation, aReplace, aPersist);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * registerComponentWithType
 *
 * Register a module's location via its registry representation
 * as returned by registryLocationForSpec() as the container of CID implemenation
 * aClass of type aType and associate aContractID and aClassName to the CID aClass.
 *
 * @param aClass : CID implemenation contained in module
 * @param aClassName : Class name associated with CID aClass
 * @param aContractID : ContractID associated with CID aClass
 * @param aSpec : Filename spec for module's location.
 * @param aLocation : Location of module of type aType. Format of this string
 *                    is the registry representation as returned by
 *                    registryLocationForSpec()
 * @param aReplace : Boolean that indicates whether to replace a previous
 *                   loader registration for aClass.
 * @param aPersist : Remember this registration across sessions.
 * @param aType : Component Type of CID aClass.
 */
/* void registerComponentWithType (in nsCIDRef aClass, in string aClassName, in string aContractID, in nsIFile aSpec, in string aLocation, in boolean aReplace, in boolean aPersist, in string aType); */
nsresult VFTCALL downRegisterComponentWithType(void *pvThis, const nsCID & aClass, const char *aClassName, const char *aContractID, nsIFile *aSpec, const char *aLocation, PRBool aReplace, PRBool aPersist, const char *aType)
{
    //@todo TEXT: aLocation? Yes. Don't think java uses this.
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);
    DPRINTF_CONTRACTID(aContractID);
    DPRINTF_STR(aClassName);
    DPRINTF_STR(aLocation);
    DPRINTF_STR(aType);
    dprintf(("%s: aReplace=%d aPersist=%d", pszFunction, aReplace, aPersist));

    rc = pMozI->RegisterComponentWithType(aClass, aClassName, aContractID, aSpec, aLocation, aReplace, aPersist, aType);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * registerComponentSpec
 *
 * Register a native dll module via its file specification as the container
 * of CID implemenation aClass and associate aContractID and aClassName to the
 * CID aClass. Native dll component type is assumed.
 *
 * @param aClass : CID implemenation contained in module
 * @param aClassName : Class name associated with CID aClass
 * @param aContractID : ContractID associated with CID aClass
 * @param aLibrary : File specification Location of module (dll).
 * @param aReplace : Boolean that indicates whether to replace a previous
 *                   module registration for aClass.
 * @param aPersist : Remember this registration across sessions.
 */
/* void registerComponentSpec (in nsCIDRef aClass, in string aClassName, in string aContractID, in nsIFile aLibrary, in boolean aReplace, in boolean aPersist); */
nsresult VFTCALL downRegisterComponentSpec(void *pvThis, const nsCID & aClass, const char *aClassName, const char *aContractID, nsIFile *aLibrary, PRBool aReplace, PRBool aPersist)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);
    DPRINTF_CONTRACTID(aContractID);
    DPRINTF_STR(aClassName);
    DPRINTF_STR(aLibrary);
    dprintf(("%s: aReplace=%d aPersist=%d", pszFunction, aReplace, aPersist));

    nsIFile *pupLibrary = aLibrary;
    rc = upCreateWrapper((void**)&pupLibrary, kFileIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->RegisterComponentSpec(aClass, aClassName, aContractID, pupLibrary, aReplace, aPersist);
    else
    {
        dprintf(("%s: Failed to make up wrapper for nsIFile %p !!!", pszFunction, aLibrary));
        ReleaseInt3(0xbaddbeef, 23, kFileIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * registerComponentLib
 *
 * Register a native dll module via its dll name (not full path) as the
 * container of CID implemenation aClass and associate aContractID and aClassName
 * to the CID aClass. Native dll component type is assumed and the system
 * services will be used to load this dll.
 *
 * @param aClass : CID implemenation contained in module
 * @param aClassName : Class name associated with CID aClass
 * @param aContractID : ContractID associated with CID aClass
 * @param aDllNameLocation : Dll name of module.
 * @param aReplace : Boolean that indicates whether to replace a previous
 *                   module registration for aClass.
 * @param aPersist : Remember this registration across sessions.
 */
/* void registerComponentLib (in nsCIDRef aClass, in string aClassName, in string aContractID, in string aDllName, in boolean aReplace, in boolean aPersist); */
nsresult VFTCALL downRegisterComponentLib(void *pvThis, const nsCID & aClass, const char *aClassName, const char *aContractID, const char *aDllName, PRBool aReplace, PRBool aPersist)
{
    //@todo TEXT: aDllName? Yes. Don't think java uses this.
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);
    DPRINTF_CONTRACTID(aContractID);
    DPRINTF_STR(aClassName);
    DPRINTF_STR(aDllName);
    dprintf(("%s: aReplace=%d aPersist=%d", pszFunction, aReplace, aPersist));

    rc = pMozI->RegisterComponentLib(aClass, aClassName, aContractID, aDllName, aReplace, aPersist);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * unregisterFactory
 *
 * Unregister a factory associated with CID aClass.
 *
 * @param aClass : ClassID being unregistered
 * @param aFactory : Factory previously registered to create instances of
 *                   ClassID aClass.
 */
/* void unregisterFactory (in nsCIDRef aClass, in nsIFactory aFactory); */
nsresult VFTCALL downUnregisterFactory(void *pvThis, const nsCID & aClass, nsIFactory *aFactory)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);
    dprintf(("%s: aFactory=%p", pszFunction, aFactory));

    nsIFactory *pupFactory = aFactory;
    rc = upCreateWrapper((void**)&pupFactory, kFactoryIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->UnregisterFactory(aClass, pupFactory);
    else
    {
        dprintf(("%s: Failed to make up wrapper for nsIFactory %p !!!", pszFunction, aFactory));
        ReleaseInt3(0xbaddbeef, 15, kFactoryIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * unregisterComponent
 *
 * Disassociate module aLocation represented as registry location as returned
 * by registryLocationForSpec() as containing ClassID aClass.
 *
 * @param aClass : ClassID being unregistered
 * @param aLocation : Location of module. Format of this is the registry
 *                    representation as returned by registryLocationForSpec().
 *                    Components of any type will be unregistered.
 */
/* void unregisterComponent (in nsCIDRef aClass, in string aLocation); */
nsresult VFTCALL downUnregisterComponent(void *pvThis, const nsCID & aClass, const char *aLocation)
{
    //@todo TEXT: aLocation? Yes. Don't think java uses this.
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);
    DPRINTF_STR(aLocation);

    rc = pMozI->UnregisterComponent(aClass, aLocation);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * unregisterComponentSpec
 *
 * Disassociate module references by file specification aLibrarySpec as
 * containing ClassID aClass.
 */
/* void unregisterComponentSpec (in nsCIDRef aClass, in nsIFile aLibrarySpec); */
nsresult VFTCALL downUnregisterComponentSpec(void *pvThis, const nsCID & aClass, nsIFile *aLibrarySpec)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);
    dprintf(("%s: aLibrarySpec=%p", pszFunction, aLibrarySpec));

    nsIFile * pupLibrarySpec = aLibrarySpec;
    rc = upCreateWrapper((void**)&pupLibrarySpec, kFileIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->UnregisterComponentSpec(aClass, pupLibrarySpec);
    else
    {
        dprintf(("%s: Failed to make up wrapper for nsIFile %p !!!", pszFunction, aLibrarySpec));
        ReleaseInt3(0xbaddbeef, 16, kFileIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * freeLibraries
 *
 * Enumerates all loaded modules and unloads unused modules.
 */
/* void freeLibraries (); */
nsresult VFTCALL downFreeLibraries(void *pvThis)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);

    rc = pMozI->FreeLibraries();

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * autoRegister
 *
 * Enumerates directory looking for modules of all types and registers
 * modules who have changed (modtime or size) since the last time
 * autoRegister() was invoked.
 *
 * @param when : ID values of when the call is being made.
 * @param directory : Directory the will be enumerated.
 */
/* void autoRegister (in long when, in nsIFile directory); */
nsresult VFTCALL downAutoRegister(void *pvThis, PRInt32 when, nsIFile *directory)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    dprintf(("%s: when=%d directory=%p", pszFunction, when, directory));

    nsIFile * pupDirectory = directory;
    rc = upCreateWrapper((void**)&pupDirectory, kFileIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->AutoRegister(when, pupDirectory);
    else
    {
        dprintf(("%s: Failed to make up wrapper for nsIFile %p !!!", pszFunction, directory));
        ReleaseInt3(0xbaddbeef, 24, kFileIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * autoRegisterComponent
 *
 * Loads module using appropriate loader and gives it an opportunity to
 * register its CIDs if module's modtime or size changed since the last
 * time this was called.
 *
 * @param when : ID values of when the call is being made.
 * @param aFileLocation : File specification of module.
 */
/* void autoRegisterComponent (in long when, in nsIFile aFileLocation); */
nsresult VFTCALL downAutoRegisterComponent(void *pvThis, PRInt32 when, nsIFile *aFileLocation)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    dprintf(("%s: when=%d aFileLocation=%p", pszFunction, when, aFileLocation));

    nsIFile * pupFileLocation = aFileLocation;
    rc = upCreateWrapper((void**)&pupFileLocation, kFileIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->AutoRegisterComponent(when, pupFileLocation);
    else
    {
        dprintf(("%s: Failed to make up wrapper for nsIFile %p !!!", pszFunction, aFileLocation));
        ReleaseInt3(0xbaddbeef, 25, kFileIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * autoUnregisterComponent
 *
 * Loads module using approriate loader and gives it an opportunity to
 * unregister its CIDs
 */
/* void autoUnregisterComponent (in long when, in nsIFile aFileLocation); */
nsresult VFTCALL downAutoUnregisterComponent(void *pvThis, PRInt32 when, nsIFile *aFileLocation)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    dprintf(("%s: when=%d aFileLocation=%p", pszFunction, when, aFileLocation));

    nsIFile * pupFileLocation = aFileLocation;
    rc = upCreateWrapper((void**)&pupFileLocation, kFileIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->AutoUnregisterComponent(when, aFileLocation);
    else
    {
        dprintf(("%s: Failed to make up wrapper for nsIFile %p !!!", pszFunction, aFileLocation));
        ReleaseInt3(0xbaddbeef, 26, kFileIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * isRegistered
 *
 * Returns true if a factory or module is registered for CID aClass.
 *
 * @param aClass : ClassID queried for registeration
 * @return : true if a factory or module is registered for CID aClass.
 *           false otherwise.
 */
/* boolean isRegistered (in nsCIDRef aClass); */
nsresult VFTCALL downIsRegistered(void *pvThis, const nsCID & aClass, PRBool *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);
    DPRINTF_NSID(aClass);

    rc = pMozI->IsRegistered(aClass, _retval);
    if (NS_SUCCEEDED(rc) && VALID_PTR(_retval))
        dprintf(("%s: _retval=%d", pszFunction, *_retval));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * enumerateCLSIDs
 *
 * Enumerate the list of all registered ClassIDs.
 *
 * @return : enumerator for ClassIDs.
 */
/* nsIEnumerator enumerateCLSIDs (); */
nsresult VFTCALL downEnumerateCLSIDs(void *pvThis, nsIEnumerator **_retval)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);

    const void *pvVFT = downIsSupportedInterface(kEnumeratorIID);
    if (pvVFT)
    {
        rc = pMozI->EnumerateCLSIDs(_retval);
        rc = downCreateWrapper((void**)_retval, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported down interface nsIEnumerator !!!", pszFunction));
        ReleaseInt3(0xbaddbeef, 17, kEnumeratorIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * enumerateContractIDs
 *
 * Enumerate the list of all registered ContractIDs.
 *
 * @return : enumerator for ContractIDs.
 */
/* nsIEnumerator enumerateContractIDs (); */
nsresult VFTCALL downEnumerateContractIDs(void *pvThis, nsIEnumerator **_retval)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManagerObsolete);

    const void *pvVFT = downIsSupportedInterface(kEnumeratorIID);
    if (pvVFT)
    {
        rc = pMozI->EnumerateContractIDs(_retval);
        rc = downCreateWrapper((void**)_retval, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported down interface nsIEnumerator !!!", pszFunction));
        ReleaseInt3(0xbaddbeef, 18, kEnumeratorIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsIComponentManagerObsolete, downVFTnsIComponentManagerObsolete)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downFindFactory,                                        VFTDELTA_VAL()
    downGetClassObject,                                     VFTDELTA_VAL()
    downContractIDToClassID,                                VFTDELTA_VAL()
    downCLSIDToContractID,                                  VFTDELTA_VAL()
    downCreateInstance,                                     VFTDELTA_VAL()
    downCreateInstanceByContractID,                         VFTDELTA_VAL()
    downRegistryLocationForSpec,                            VFTDELTA_VAL()
    downSpecForRegistryLocation,                            VFTDELTA_VAL()
    downRegisterFactory,                                    VFTDELTA_VAL()
    downRegisterComponent,                                  VFTDELTA_VAL()
    downRegisterComponentWithType,                          VFTDELTA_VAL()
    downRegisterComponentSpec,                              VFTDELTA_VAL()
    downRegisterComponentLib,                               VFTDELTA_VAL()
    downUnregisterFactory,                                  VFTDELTA_VAL()
    downUnregisterComponent,                                VFTDELTA_VAL()
    downUnregisterComponentSpec,                            VFTDELTA_VAL()
    downFreeLibraries,                                      VFTDELTA_VAL()
    downAutoRegister,                                       VFTDELTA_VAL()
    downAutoRegisterComponent,                              VFTDELTA_VAL()
    downAutoUnregisterComponent,                            VFTDELTA_VAL()
    downIsRegistered,                                       VFTDELTA_VAL()
    downEnumerateCLSIDs,                                    VFTDELTA_VAL()
    downEnumerateContractIDs,                               VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIComponentManager
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * getClassObject
 *
 * Returns the factory object that can be used to create instances of
 * CID aClass
 *
 * @param aClass The classid of the factory that is being requested
 */
/* void getClassObject (in nsCIDRef aClass, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
nsresult VFTCALL downCMGetClassObject(void *pvThis, const nsCID & aClass, const nsIID & aIID, void * *result)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManager);
    DPRINTF_NSID(aClass);
    DPRINTF_NSID(aIID);

    /*
     * Supported interface requested?
     */
    const void * pvVFT = downIsSupportedInterface(aIID);
    if (pvVFT)
    {
        rc = pMozI->GetClassObject(aClass, aIID, result);
        rc = downCreateWrapper(result, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported down interface !!!", pszFunction));
        rc = NS_NOINTERFACE;
        ReleaseInt3(0xbaddbeef, 28, aIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


/**
 * getClassObjectByContractID
 *
 * Returns the factory object that can be used to create instances of
 * CID aClass
 *
 * @param aClass The classid of the factory that is being requested
 */
/* void getClassObjectByContractID (in string aContractID, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
nsresult VFTCALL downCMGetClassObjectByContractID(void *pvThis, const char *aContractID, const nsIID & aIID, void * *result)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManager);
    DPRINTF_CONTRACTID(aContractID);
    DPRINTF_NSID(aIID);

    /*
     * Supported interface requested?
     */
    const void * pvVFT = downIsSupportedInterface(aIID);
    if (pvVFT)
    {
        rc = pMozI->GetClassObjectByContractID(aContractID, aIID, result);
        rc = downCreateWrapper(result, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported down interface !!!", pszFunction));
        rc = NS_NOINTERFACE;
        ReleaseInt3(0xbaddbeef, 19, aIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * createInstance
 *
 * Create an instance of the CID aClass and return the interface aIID.
 *
 * @param aClass : ClassID of object instance requested
 * @param aDelegate : Used for aggregation
 * @param aIID : IID of interface requested
 */
/* void createInstance (in nsCIDRef aClass, in nsISupports aDelegate, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
nsresult VFTCALL downCMCreateInstance(void *pvThis, const nsCID & aClass, nsISupports *aDelegate, const nsIID & aIID, void * *result)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManager);
    DPRINTF_NSID(aClass);
    DPRINTF_NSID(aIID);
    dprintf(("%s: aDelegate=%p result=%p", pszFunction, aDelegate, result));


    /*
     * Supported interface requested?
     */
    const void * pvVFT = downIsSupportedInterface(aIID);
    if (pvVFT)
    {
        /*
         * Wrap the aDelegate.
         *  (Pray this isn't supposed to be the same kind of wrapper as nsIID.
         *   Haven't found any examples using aDelegate unfortunately.)
         */
        nsISupports * pupOuter = aDelegate;
        rc = upCreateWrapper((void**)&pupOuter, kSupportsIID, NS_OK);
        if (rc == NS_OK)
        {
            rc = pMozI->CreateInstance(aClass, pupOuter, aIID, result);
            rc = downCreateWrapper(result, pvVFT, rc);
        }
        else
            dprintf(("%s: failed to create up wrapper for kSupportsIID %x !!!", pszFunction, aDelegate));
    }
    else
    {
        dprintf(("%s: Unsupported down interface!!!", pszFunction));
        rc = NS_NOINTERFACE;
        ReleaseInt3(0xbaddbeef, 31, aIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


/**
 * createInstanceByContractID
 *
 * Create an instance of the CID that implements aContractID and return the
 * interface aIID.
 *
 * @param aContractID : aContractID of object instance requested
 * @param aDelegate : Used for aggregation
 * @param aIID : IID of interface requested
 */
/* void createInstanceByContractID (in string aContractID, in nsISupports aDelegate, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
nsresult VFTCALL downCMCreateInstanceByContractID(void *pvThis, const char *aContractID, nsISupports *aDelegate, const nsIID & aIID, void * *result)
{
    DOWN_ENTER_RC(pvThis, nsIComponentManager);
    DPRINTF_CONTRACTID(aContractID);
    DPRINTF_NSID(aIID);
    dprintf(("%s: aDelegate=%p result=%p", pszFunction, aDelegate, result));


    /*
     * Supported interface requested?
     */
    const void * pvVFT = downIsSupportedInterface(aIID);
    if (pvVFT)
    {
        /*
         * Wrap the aDelegate.
         *  (Pray this isn't supposed to be the same kind of wrapper as nsIID.
         *   Haven't found any examples using aDelegate unfortunately.)
         */
        nsISupports * pupOuter = aDelegate;
        rc = upCreateWrapper((void**)&pupOuter, kSupportsIID, NS_OK);
        if (NS_SUCCEEDED(rc))
        {
            rc = pMozI->CreateInstanceByContractID(aContractID, pupOuter, aIID, result);
            rc = downCreateWrapper(result, pvVFT, rc);
        }
        else
            dprintf(("%s: failed to create up wrapper for kSupportsIID %x !!!", pszFunction, aDelegate));
    }
    else
    {
        dprintf(("%s: Unsupported down interface!!!", pszFunction));
        rc = NS_NOINTERFACE;
        ReleaseInt3(0xbaddbeef, 10, aIID.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}




MAKE_SAFE_VFT(VFTnsIComponentManager, downVFTnsIComponentManager)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downCMGetClassObject,                                   VFTDELTA_VAL()
    downCMGetClassObjectByContractID,                       VFTDELTA_VAL()
    downCMCreateInstance,                                   VFTDELTA_VAL()
    downCMCreateInstanceByContractID,                       VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();





//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIEnumerator
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//


/** First will reset the list. will return NS_FAILED if no items
 */
/* void first (); */
nsresult VFTCALL downEFirst(void *pvThis)
{
    DOWN_ENTER_RC(pvThis, nsIEnumerator);
    rc = pMozI->First();
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/** Next will advance the list. will return failed if already at end
 */
/* void next (); */
nsresult VFTCALL  downENext(void *pvThis)
{
    DOWN_ENTER_RC(pvThis, nsIEnumerator);
    rc = pMozI->Next();
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/** CurrentItem will return the CurrentItem item it will fail if the
 *  list is empty
 */
/* nsISupports currentItem (); */
nsresult VFTCALL  downECurrentItem(void *pvThis, nsISupports **_retval)
{
    DOWN_ENTER_RC(pvThis, nsIEnumerator);
    rc = pMozI->First();
    rc = downCreateWrapper((void**)_retval, kSupportsIID, rc);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/** return if the collection is at the end.  that is the beginning following
 *  a call to Prev and it is the end of the list following a call to next
 */
/* void isDone (); */
nsresult VFTCALL  downEIsDone(void *pvThis)
{
    DOWN_ENTER_RC(pvThis, nsIEnumerator);
    rc = pMozI->IsDone();
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsIEnumerator, downVFTnsIEnumerator)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downEFirst,                                             VFTDELTA_VAL()
    downENext,                                              VFTDELTA_VAL()
    downECurrentItem,                                       VFTDELTA_VAL()
    downEIsDone,                                            VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();





//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIFactory
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//


/**
 * Creates an instance of a component.
 *
 * @param aOuter Pointer to a component that wishes to be aggregated
 *               in the resulting instance. This will be nsnull if no
 *               aggregation is requested.
 * @param iid    The IID of the interface being requested in
 *               the component which is being currently created.
 * @param result [out] Pointer to the newly created instance, if successful.
 * @return NS_OK - Component successfully created and the interface
 *                 being requested was successfully returned in result.
 *         NS_NOINTERFACE - Interface not accessible.
 *         NS_ERROR_NO_AGGREGATION - if an 'outer' object is supplied, but the
 *                                   component is not aggregatable.
 *         NS_ERROR* - Method failure.
 */
/* void createInstance (in nsISupports aOuter, in nsIIDRef iid, [iid_is (iid), retval] out nsQIResult result); */
nsresult VFTCALL downFactory_CreateInstance(void *pvThis, nsISupports *aOuter, const nsIID & iid, void * *result)
{
    DOWN_ENTER_RC(pvThis, nsIFactory);
    dprintf(("%s: aOuter result=%p", pszFunction, aOuter, result));
    DPRINTF_NSID(iid);

    /*
     * Supported interface requested?
     */
    const void * pvVFT = downIsSupportedInterface(iid);
    if (pvVFT)
    {
        /*
         * Wrap the aOuter.
         *  (Pray this isn't supposed to be the same kind of wrapper as nsIID.
         *   Haven't found any examples using aDelegate unfortunately.)
         */
        nsISupports * pupOuter = aOuter;
        rc = upCreateWrapper((void**)&pupOuter, kSupportsIID, NS_OK);
        if (rc == NS_OK)
        {
            rc = pMozI->CreateInstance(pupOuter, iid, result);
            rc = downCreateWrapper(result, pvVFT, rc);
        }
        else
            dprintf(("%s: failed to create up wrapper for kSupportsIID %x !!!", pszFunction, aOuter));
    }
    else
    {
        dprintf(("%s: Unsupported down interface!!!", pszFunction));
        rc = NS_NOINTERFACE;
        ReleaseInt3(0xbaddbeef, 33, iid.m0);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


/**
 * LockFactory provides the client a way to keep the component
 * in memory until it is finished with it. The client can call
 * LockFactory(PR_TRUE) to lock the factory and LockFactory(PR_FALSE)
 * to release the factory.
 *
 * @param lock - Must be PR_TRUE or PR_FALSE
 * @return NS_OK - If the lock operation was successful.
 *         NS_ERROR* - Method failure.
 */
/* void lockFactory (in PRBool lock); */
nsresult VFTCALL downFactory_LockFactory(void *pvThis, PRBool lock)
{
    DOWN_ENTER_RC(pvThis, nsIFactory);
    dprintf(("%s: lock=%d\n", pszFunction, lock));
    rc = pMozI->LockFactory(lock);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsIFactory, downVFTnsIFactory)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downFactory_CreateInstance,                             VFTDELTA_VAL()
    downFactory_LockFactory,                                VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();




//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIInputStream
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//


nsresult VFTCALL downInputStream_Close(void *pvThis)
{
    DOWN_ENTER_RC(pvThis, nsIInputStream);
    rc = pMozI->Close();
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


/**
 * @return number of bytes currently available in the stream
 */
/* unsigned long available (); */
nsresult VFTCALL downInputStream_Available(void *pvThis, PRUint32 *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIInputStream);
    dprintf(("%s: _retval=%p\n", pszFunction, _retval));
    rc = pMozI->Available(_retval);
    if (VALID_PTR(_retval))
        dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Read data from the stream.
 *
 * @param aBuf the buffer into which the data is to be read
 * @param aCount the maximum number of bytes to be read
 *
 * @return number of bytes read (may be less than aCount).
 * @return 0 if reached end of file
 *
 * @throws NS_BASE_STREAM_WOULD_BLOCK if reading from the input stream would
 *   block the calling thread (non-blocking mode only)
 * @throws <other-error> on failure
 */
/* [noscript] unsigned long read (in charPtr aBuf, in unsigned long aCount); */
nsresult VFTCALL downInputStream_Read(void *pvThis, char * aBuf, PRUint32 aCount, PRUint32 *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIInputStream);
    dprintf(("%s: aBuf=%p aCount=%d _retval=%p\n", pszFunction, aBuf, aCount, _retval));
    rc = pMozI->Read(aBuf, aCount, _retval);
    if (VALID_PTR(_retval))
        dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}



/**
 * Special wrapper function which get called from a little stub we create
 * in downInputStream_ReadSegments. The stub pushed the entry point of
 * the original function before calling us.
 */
nsresult __cdecl downOutputStream_nsWriteSegmentWrapper(
    nsresult (VFTCALL pfnOrg)(nsIInputStream *aInStream, void *aClosure, const char *aFromSegment, PRUint32 aToOffset, PRUint32 aCount, PRUint32 *aWriteCount),
    void *pvRet,
    nsIInputStream *aInStream, void *aClosure, const char *aFromSegment,
    PRUint32 aToOffset, PRUint32 aCount, PRUint32 *aWriteCount)
{
    DEBUG_FUNCTIONNAME();
    nsresult rc;
    dprintf(("%s: pfnOrg=%p pvRet=%p aInStream=%p aClosure=%p aFromSegment=%p aToOffset=%d aCount=%d aWriteCount=%p\n",
             pszFunction, pfnOrg, pvRet, aInStream, aClosure, aFromSegment, aToOffset, aCount, aWriteCount));

    nsIInputStream * pDownInStream = aInStream;
    rc = downCreateWrapper((void**)&pDownInStream, kInputStreamIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        rc = pfnOrg(pDownInStream, aClosure, aFromSegment, aToOffset, aCount, aWriteCount);
        if (VALID_PTR(aWriteCount))
            dprintf(("%s: *aWriteCount=%d\n", pszFunction, *aWriteCount));
    }

    dprintf(("%s: rc=%d\n", pszFunction, rc));
    return rc;
}

/**
 * Low-level read method that has access to the stream's underlying buffer.
 * The writer function may be called multiple times for segmented buffers.
 * ReadSegments is expected to keep calling the writer until either there is
 * nothing left to read or the writer returns an error.  ReadSegments should
 * not call the writer with zero bytes to consume.
 *
 * @param aWriter the "consumer" of the data to be read
 * @param aClosure opaque parameter passed to writer
 * @param aCount the maximum number of bytes to be read
 *
 * @return number of bytes read (may be less than aCount)
 * @return 0 if reached end of file (or if aWriter refused to consume data)
 *
 * @throws NS_BASE_STREAM_WOULD_BLOCK if reading from the input stream would
 *   block the calling thread (non-blocking mode only)
 * @throws <other-error> on failure
 *
 * NOTE: this function may be unimplemented if a stream has no underlying
 * buffer (e.g., socket input stream).
 */
/* [noscript] unsigned long readSegments (in nsWriteSegmentFun aWriter, in voidPtr aClosure, in unsigned long aCount); */
nsresult VFTCALL downInputStream_ReadSegments(void *pvThis, nsWriteSegmentFun aWriter, void * aClosure, PRUint32 aCount, PRUint32 *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIInputStream);
    dprintf(("%s: aWriter=%p aClosure=%p aCount=%d _retval=%p", pszFunction, aWriter, aClosure, aCount, _retval));


    int i;
    char achWrapper[32];

    /* push aReader */
    achWrapper[0] = 0x68;
    *((unsigned*)&achWrapper[1]) = (unsigned)aWriter;
    i = 5;

#ifdef __IBMCPP__
    /* optlink - mov [ebp + 08h], eax */
    achWrapper[i++] = 0x89;
    achWrapper[i++] = 0x45;
    achWrapper[i++] = 0x08;
    /* optlink - mov [ebp + 0ch], edx */
    achWrapper[i++] = 0x89;
    achWrapper[i++] = 0x55;
    achWrapper[i++] = 0x0c;
    /* optlink - mov [ebp + 10h], ecx */
    achWrapper[i++] = 0x89;
    achWrapper[i++] = 0x4d;
    achWrapper[i++] = 0x10;
#endif
    /* call UpOutputStream_nsReadSegmentWrapper */
    achWrapper[i] = 0xe8;
    *((unsigned*)&achWrapper[i+1]) = (unsigned)downOutputStream_nsWriteSegmentWrapper - (unsigned)&achWrapper[i+5];
    i += 5;

    /* add esp, 4 */
    achWrapper[i++] = 0x83;
    achWrapper[i++] = 0xc4;
    achWrapper[i++] = 0x04;

    /* ret */
    achWrapper[i++] = 0xc3;

    achWrapper[i] = 0xcc;
    achWrapper[i] = 0xcc;

    /* do call - this better not be asynchronous stuff. */
    rc = pMozI->ReadSegments((nsWriteSegmentFun)((void*)&achWrapper[0]), aClosure, aCount, _retval);
    if (VALID_PTR(_retval))
        dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


/**
 * @return true if stream is non-blocking
 */
/* boolean isNonBlocking (); */
nsresult VFTCALL downInputStream_IsNonBlocking(void *pvThis, PRBool *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIInputStream);
    dprintf(("%s: _retval=%p\n", pszFunction, _retval));
    rc = pMozI->IsNonBlocking(_retval);
    if (VALID_PTR(_retval))
        dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}




MAKE_SAFE_VFT(VFTnsIInputStream, downVFTnsIInputStream)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downInputStream_Close,                                  VFTDELTA_VAL()
    downInputStream_Available,                              VFTDELTA_VAL()
    downInputStream_Read,                                   VFTDELTA_VAL()
    downInputStream_ReadSegments,                           VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIOutputStream
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * Close the stream. Forces the output stream to flush any buffered data.
 *
 * @throws NS_BASE_STREAM_WOULD_BLOCK if unable to flush without blocking
 *   the calling thread (non-blocking mode only)
 */
/* void close (); */
nsresult VFTCALL downOutputStream_Close(void *pvThis)
{
    DOWN_ENTER_RC(pvThis, nsIOutputStream);
    rc = pMozI->Close();
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Flush the stream.
 *
 * @throws NS_BASE_STREAM_WOULD_BLOCK if unable to flush without blocking
 *   the calling thread (non-blocking mode only)
 */
/* void flush (); */
nsresult VFTCALL downOutputStream_Flush(void *pvThis)
{
    DOWN_ENTER_RC(pvThis, nsIOutputStream);
    rc = pMozI->Flush();
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Write data into the stream.
 *
 * @param aBuf the buffer containing the data to be written
 * @param aCount the maximum number of bytes to be written
 *
 * @return number of bytes written (may be less than aCount)
 *
 * @throws NS_BASE_STREAM_WOULD_BLOCK if writing to the output stream would
 *   block the calling thread (non-blocking mode only)
 * @throws <other-error> on failure
 */
/* unsigned long write (in string aBuf, in unsigned long aCount); */
nsresult VFTCALL downOutputStream_Write(void *pvThis, const char *aBuf, PRUint32 aCount, PRUint32 *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIOutputStream);
    dprintf(("%s: aBuf=%p aCount=%d _retval=%p\n", pszFunction, aBuf, aCount, _retval));
    rc = pMozI->Write(aBuf, aCount, _retval);
    if (VALID_PTR(_retval))
        dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Writes data into the stream from an input stream.
 *
 * @param aFromStream the stream containing the data to be written
 * @param aCount the maximum number of bytes to be written
 *
 * @return number of bytes written (may be less than aCount)
 *
 * @throws NS_BASE_STREAM_WOULD_BLOCK if writing to the output stream would
 *    block the calling thread (non-blocking mode only)
 * @throws <other-error> on failure
 *
 * NOTE: This method is defined by this interface in order to allow the
 * output stream to efficiently copy the data from the input stream into
 * its internal buffer (if any). If this method was provided as an external
 * facility, a separate char* buffer would need to be used in order to call
 * the output stream's other Write method.
 */
/* unsigned long writeFrom (in nsIInputStream aFromStream, in unsigned long aCount); */
nsresult VFTCALL downOutputStream_WriteFrom(void *pvThis, nsIInputStream *aFromStream, PRUint32 aCount, PRUint32 *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIOutputStream);
    dprintf(("%s: aFromStream=%p aCount=%d _retval=%p\n", pszFunction, aFromStream, aCount, _retval));

    nsIInputStream *pUpFromStream = aFromStream;
    rc = upCreateWrapper((void**)&pUpFromStream, kInputStreamIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        rc = pMozI->WriteFrom(pUpFromStream, aCount, _retval);
        if (VALID_PTR(_retval))
            dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


/**
 * Special wrapper function which get called from a little stub we create
 * in UpOutputStream::WriteSegments. The stub pushed the entry point of
 * the original function before calling us.
 */
nsresult __cdecl downOutputStream_nsReadSegmentWrapper(nsReadSegmentFun pfnOrg, void *pvRet,
     nsIOutputStream *aOutStream, void *aClosure, char *aToSegment,
     PRUint32 aFromOffset, PRUint32 aCount, PRUint32 *aReadCount)
{
    DEBUG_FUNCTIONNAME();
    nsresult rc;
    dprintf(("%s: pfnOrg=%p pvRet=%p aOutStream=%p aClosure=%p aToSegment=%p aFromOffset=%d aCount=%d aReadCount=%p\n",
             pszFunction, pfnOrg, pvRet, aOutStream, aClosure, aToSegment, aFromOffset, aCount, aReadCount));

    nsIOutputStream * pupOutStream = aOutStream;
    rc = upCreateWrapper((void**)&pupOutStream, kOutputStreamIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        rc = pfnOrg(pupOutStream, aClosure, aToSegment, aFromOffset, aCount, aReadCount);
        if (VALID_PTR(aReadCount))
            dprintf(("%s: *aReadCount=%d\n", pszFunction, *aReadCount));
    }

    dprintf(("%s: rc=%d\n", pszFunction, rc));
    return rc;
}


/**
 * Low-level write method that has access to the stream's underlying buffer.
 * The reader function may be called multiple times for segmented buffers.
 * WriteSegments is expected to keep calling the reader until either there
 * is nothing left to write or the reader returns an error.  WriteSegments
 * should not call the reader with zero bytes to provide.
 *
 * @param aReader the "provider" of the data to be written
 * @param aClosure opaque parameter passed to reader
 * @param aCount the maximum number of bytes to be written
 *
 * @return number of bytes written (may be less than aCount)
 *
 * @throws NS_BASE_STREAM_WOULD_BLOCK if writing to the output stream would
 *    block the calling thread (non-blocking mode only)
 * @throws <other-error> on failure
 *
 * NOTE: this function may be unimplemented if a stream has no underlying
 * buffer (e.g., socket output stream).
 */
/* [noscript] unsigned long writeSegments (in nsReadSegmentFun aReader, in voidPtr aClosure, in unsigned long aCount); */
nsresult VFTCALL downOutputStream_WriteSegments(void *pvThis, nsReadSegmentFun aReader, void * aClosure, PRUint32 aCount, PRUint32 *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIOutputStream);
    dprintf(("%s: aReader=%p aClosure=%p aCount=%d _retval=%p", pszFunction, aReader, aClosure, aCount, _retval));

    int i;
    char achWrapper[32];

    /* push aReader */
    achWrapper[0] = 0x68;
    *((unsigned*)&achWrapper[1]) = (unsigned)aReader;
    i = 5;

#ifdef __IBMCPP__

    /* optlink - mov [ebp + 08h], eax */
    achWrapper[i++] = 0x89;
    achWrapper[i++] = 0x45;
    achWrapper[i++] = 0x08;
    /* optlink - mov [ebp + 0ch], edx */
    achWrapper[i++] = 0x89;
    achWrapper[i++] = 0x55;
    achWrapper[i++] = 0x0c;
    /* optlink - mov [ebp + 10h], ecx */
    achWrapper[i++] = 0x89;
    achWrapper[i++] = 0x4d;
    achWrapper[i++] = 0x10;
#endif
    /* call UpOutputStream_nsReadSegmentWrapper */
    achWrapper[i] = 0xe8;
    *((unsigned*)&achWrapper[i+1]) = (unsigned)downOutputStream_nsReadSegmentWrapper - (unsigned)&achWrapper[i+5];
    i += 5;

    /* add esp, 4 */
    achWrapper[i++] = 0x83;
    achWrapper[i++] = 0xc4;
    achWrapper[i++] = 0x04;

    /* _Optlink - ret */
    achWrapper[i++] = 0xc3;
    achWrapper[i++] = 0xcc;
    achWrapper[i] = 0xcc;

    /* do call - this better not be asynchronous stuff. */
    rc = pMozI->WriteSegments((nsReadSegmentFun)((void*)&achWrapper[0]), aClosure, aCount, _retval);
    if (VALID_PTR(_retval))
        dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * @return true if stream is non-blocking
 *
 * NOTE: writing to a blocking output stream will block the calling thread
 * until all given data can be consumed by the stream.
 */
/* boolean isNonBlocking (); */
nsresult VFTCALL downOutputStream_IsNonBlocking(void *pvThis, PRBool *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIOutputStream);
    dprintf(("%s: _retval=%p\n", pszFunction, _retval));
    rc = pMozI->IsNonBlocking(_retval);
    if (VALID_PTR(_retval))
        dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsIOutputStream, downVFTnsIOutputStream)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downOutputStream_Close,                                 VFTDELTA_VAL()
    downOutputStream_Flush,                                 VFTDELTA_VAL()
    downOutputStream_Write,                                 VFTDELTA_VAL()
    downOutputStream_WriteFrom,                             VFTDELTA_VAL()
    downOutputStream_WriteSegments,                         VFTDELTA_VAL()
    downOutputStream_IsNonBlocking,                         VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();




//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIInterfaceRequestor
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//


/**
 * Retrieves the specified interface pointer.
 *
 * @param uuid The IID of the interface being requested.
 * @param result [out] The interface pointer to be filled in if
 *               the interface is accessible.
 * @return NS_OK - interface was successfully returned.
 *         NS_NOINTERFACE - interface not accessible.
 *         NS_ERROR* - method failure.
 */
/* void getInterface (in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
nsresult VFTCALL downInterfaceRequestor_GetInterface(void *pvThis, const nsIID & uuid, void * *result)
{
    DOWN_ENTER_RC(pvThis, nsIInterfaceRequestor);
    DPRINTF_NSID(uuid);

    /*
     * Is this a supported interface?
     */
    const void *pvVFT = downIsSupportedInterface(uuid);
    if (pvVFT)
    {
        rc = pMozI->GetInterface(uuid, result);
        rc = downCreateWrapper(result, pvVFT, rc);
    }
    else
    {
        dprintf(("%s: Unsupported interface!!!", pszFunction));
        rc = NS_NOINTERFACE;
        if (result)
            *result = nsnull;
        ReleaseInt3(0xbaddbeef, 34, uuid.m0);
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}



MAKE_SAFE_VFT(VFTnsIInterfaceRequestor, downVFTnsIInterfaceRequestor)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downInterfaceRequestor_GetInterface,                    VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();




//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIRequest
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * The name of the request.  Often this is the URI of the request.
 */
/* readonly attribute AUTF8String name; */
nsresult VFTCALL downRequest_GetName(void *pvThis, nsACString & aName)
{
    DOWN_ENTER_RC(pvThis, nsIRequest);
    dprintf(("%s: &aName=%p", pszFunction, &aName));
    /** @todo Wrap nsACString */
    dprintf(("%s: nsACString wrapping isn't supported.", pszFunction));
    ReleaseInt3(0xbaddbeef, 35, 0);

    rc = pMozI->GetName(aName);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * @return TRUE if the request has yet to reach completion.
 * @return FALSE if the request has reached completion (e.g., after
 *   OnStopRequest has fired).
 * Suspended requests are still considered pending.
 */
/* boolean isPending (); */
nsresult VFTCALL downRequest_IsPending(void *pvThis, PRBool *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIRequest);
    dprintf(("%s: _retval=%p\n", pszFunction, _retval));
    rc = pMozI->IsPending(_retval);
    if (VALID_PTR(_retval))
        dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * The error status associated with the request.
 */
/* readonly attribute nsresult status; */
nsresult VFTCALL downRequest_GetStatus(void *pvThis, nsresult *aStatus)
{
    DOWN_ENTER_RC(pvThis, nsIRequest);
    dprintf(("%s: aStatus=%p\n", pszFunction, aStatus));
    rc = pMozI->GetStatus(aStatus);
    if (VALID_PTR(aStatus))
        dprintf(("%s: *aStatus=%#x\n", pszFunction, *aStatus));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Cancels the current request.  This will close any open input or
 * output streams and terminate any async requests.  Users should
 * normally pass NS_BINDING_ABORTED, although other errors may also
 * be passed.  The error passed in will become the value of the
 * status attribute.
 *
 * @param aStatus the reason for canceling this request.
 *
 * NOTE: most nsIRequest implementations expect aStatus to be a
 * failure code; however, some implementations may allow aStatus to
 * be a success code such as NS_OK.  In general, aStatus should be
 * a failure code.
 */
/* void cancel (in nsresult aStatus); */
nsresult VFTCALL downRequest_Cancel(void *pvThis, nsresult aStatus)
{
    DOWN_ENTER_RC(pvThis, nsIRequest);
    dprintf(("%s: aStatus=%#x\n", pszFunction, aStatus));
    rc = pMozI->Cancel(aStatus);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Suspends the current request.  This may have the effect of closing
 * any underlying transport (in order to free up resources), although
 * any open streams remain logically opened and will continue delivering
 * data when the transport is resumed.
 *
 * NOTE: some implementations are unable to immediately suspend, and
 * may continue to deliver events already posted to an event queue. In
 * general, callers should be capable of handling events even after
 * suspending a request.
 */
/* void suspend (); */
nsresult VFTCALL downRequest_Suspend(void *pvThis)
{
    DOWN_ENTER_RC(pvThis, nsIRequest);
    rc = pMozI->Suspend();
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Resumes the current request.  This may have the effect of re-opening
 * any underlying transport and will resume the delivery of data to
 * any open streams.
 */
/* void resume (); */
nsresult VFTCALL downRequest_Resume(void *pvThis)
{
    DOWN_ENTER_RC(pvThis, nsIRequest);
    rc = pMozI->Resume();
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * The load group of this request.  While pending, the request is a
 * member of the load group.  It is the responsibility of the request
 * to implement this policy.
 */
/* attribute nsILoadGroup loadGroup; */
nsresult VFTCALL downRequest_GetLoadGroup(void *pvThis, nsILoadGroup * *aLoadGroup)
{
    DOWN_ENTER_RC(pvThis, nsIRequest);
    dprintf(("%s: aLoadGroup=%p\n", pszFunction, aLoadGroup));

    rc = pMozI->GetLoadGroup(aLoadGroup);
    rc = downCreateWrapper((void**)aLoadGroup, kLoadGroupIID, rc);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

nsresult VFTCALL downRequest_SetLoadGroup(void *pvThis, nsILoadGroup * aLoadGroup)
{
    DOWN_ENTER_RC(pvThis, nsIRequest);
    dprintf(("%s: aLoadGroup=%p\n", pszFunction, aLoadGroup));

    nsILoadGroup * pUpLoadGroup = aLoadGroup;
    rc = upCreateWrapper((void**)&pUpLoadGroup, kLoadGroupIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->SetLoadGroup(pUpLoadGroup);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * The load flags of this request.  Bits 0-15 are reserved.
 *
 * When added to a load group, this request's load flags are merged with
 * the load flags of the load group.
 */
/* attribute nsLoadFlags loadFlags; */
nsresult VFTCALL downRequest_GetLoadFlags(void *pvThis, nsLoadFlags *aLoadFlags)
{
    DOWN_ENTER_RC(pvThis, nsIRequest);
    dprintf(("%s: aLoadFlags=%p\n", pszFunction, aLoadFlags));
    rc = pMozI->GetLoadFlags(aLoadFlags);
    if (VALID_PTR(aLoadFlags))
        dprintf(("%s: *aLoadFlags=%#x\n", pszFunction, *aLoadFlags));
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

nsresult VFTCALL downRequest_SetLoadFlags(void *pvThis, nsLoadFlags aLoadFlags)
{
    DOWN_ENTER_RC(pvThis, nsIRequest);
    dprintf(("%s: aLoadFlags=%p\n", pszFunction, aLoadFlags));
    rc = pMozI->SetLoadFlags(aLoadFlags);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsIRequest, downVFTnsIRequest)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downRequest_GetName,                                    VFTDELTA_VAL()
    downRequest_IsPending,                                  VFTDELTA_VAL()
    downRequest_GetStatus,                                  VFTDELTA_VAL()
    downRequest_Cancel,                                     VFTDELTA_VAL()
    downRequest_Suspend,                                    VFTDELTA_VAL()
    downRequest_Resume,                                     VFTDELTA_VAL()
    downRequest_GetLoadGroup,                               VFTDELTA_VAL()
    downRequest_SetLoadGroup,                               VFTDELTA_VAL()
    downRequest_GetLoadFlags,                               VFTDELTA_VAL()
    downRequest_SetLoadFlags,                               VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsILoadGroup
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/**
 * The group observer is notified when requests are added to and removed
 * from this load group.  The groupObserver is weak referenced.
 */
/* attribute nsIRequestObserver groupObserver; */
nsresult VFTCALL downLoadGroup_GetGroupObserver(void *pvThis, nsIRequestObserver * *aGroupObserver)
{
    DOWN_ENTER_RC(pvThis, nsILoadGroup);
    dprintf(("%s: aGroupObserver=%p\n", pszFunction, aGroupObserver));

    rc = pMozI->GetGroupObserver(aGroupObserver);
    rc = downCreateWrapper((void**)aGroupObserver, kRequestObserverIID, rc);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

nsresult VFTCALL downLoadGroup_SetGroupObserver(void *pvThis, nsIRequestObserver * aGroupObserver)
{
    DOWN_ENTER_RC(pvThis, nsILoadGroup);
    dprintf(("%s: aGroupObserver=%p\n", pszFunction, aGroupObserver));

    nsIRequestObserver * pUpGroupObserver = aGroupObserver;
    rc = upCreateWrapper((void**)&pUpGroupObserver, kRequestObserverIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->SetGroupObserver(pUpGroupObserver);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Accesses the default load request for the group.  Each time a number
 * of requests are added to a group, the defaultLoadRequest may be set
 * to indicate that all of the requests are related to a base request.
 *
 * The load group inherits its load flags from the default load request.
 * If the default load request is NULL, then the group's load flags are
 * not changed.
 */
/* attribute nsIRequest defaultLoadRequest; */
nsresult VFTCALL downLoadGroup_GetDefaultLoadRequest(void *pvThis, nsIRequest * *aDefaultLoadRequest)
{
    DOWN_ENTER_RC(pvThis, nsILoadGroup);
    dprintf(("%s: aDefaultLoadRequest=%p\n", pszFunction, aDefaultLoadRequest));

    rc = pMozI->GetDefaultLoadRequest(aDefaultLoadRequest);
    rc = downCreateWrapper((void**)aDefaultLoadRequest, kRequestIID, rc);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;

}

nsresult VFTCALL downLoadGroup_SetDefaultLoadRequest(void *pvThis, nsIRequest * aDefaultLoadRequest)
{
    DOWN_ENTER_RC(pvThis, nsILoadGroup);
    dprintf(("%s: aDefaultLoadRequest=%p\n", pszFunction, aDefaultLoadRequest));

    nsIRequest * pUpDefaultLoadRequest = aDefaultLoadRequest;
    rc = upCreateWrapper((void**)&pUpDefaultLoadRequest, kRequestIID, NS_OK);
    if (NS_SUCCEEDED(rc))
        rc = pMozI->SetDefaultLoadRequest(pUpDefaultLoadRequest);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Adds a new request to the group.  This will cause the default load
 * flags to be applied to the request.  If this is a foreground
 * request then the groupObserver's onStartRequest will be called.
 *
 * If the request is the default load request or if the default load
 * request is null, then the load group will inherit its load flags from
 * the request.
 */
/* void addRequest (in nsIRequest aRequest, in nsISupports aContext); */
nsresult VFTCALL downLoadGroup_AddRequest(void *pvThis, nsIRequest *aRequest, nsISupports *aContext)
{
    DOWN_ENTER_RC(pvThis, nsILoadGroup);
    dprintf(("%s: aRequest=%p aContext=%p\n", pszFunction, aRequest, aContext));

    nsIRequest * pUpRequest = aRequest;
    rc = upCreateWrapper((void**)&pUpRequest, kRequestIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports * pUpContext = aContext;
        rc = upCreateWrapper((void**)&pUpContext, kSupportsIID, rc);
        if (NS_SUCCEEDED(rc))
            rc = pMozI->AddRequest(pUpRequest, pUpContext);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Removes a request from the group.  If this is a foreground request
 * then the groupObserver's onStopRequest will be called.
 */
/* void removeRequest (in nsIRequest aRequest, in nsISupports aContext, in nsresult aStatus); */
nsresult VFTCALL downLoadGroup_RemoveRequest(void *pvThis, nsIRequest *aRequest, nsISupports *aContext, nsresult aStatus)
{
    DOWN_ENTER_RC(pvThis, nsILoadGroup);
    dprintf(("%s: aRequest=%p aContext=%p aStatus=%#x\n", pszFunction, aRequest, aContext, aStatus));

    nsIRequest * pUpRequest = aRequest;
    rc = upCreateWrapper((void**)&pUpRequest, kRequestIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports * pUpContext = aContext;
        rc = upCreateWrapper((void**)&pUpContext, kSupportsIID, rc);
        if (NS_SUCCEEDED(rc))
            rc = pMozI->RemoveRequest(pUpRequest, pUpContext, aStatus);
    }

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Returns the requests contained directly in this group.
 * Enumerator element type: nsIRequest.
 */
/* readonly attribute nsISimpleEnumerator requests; */
nsresult VFTCALL downLoadGroup_GetRequests(void *pvThis, nsISimpleEnumerator * *aRequests)
{
    DOWN_ENTER_RC(pvThis, nsILoadGroup);
    dprintf(("%s: aRequests=%p\n", pszFunction, aRequests));

    rc = pMozI->GetRequests(aRequests);
    rc = downCreateWrapper((void**)aRequests, kSimpleEnumeratorIID, rc);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Returns the count of "active" requests (ie. requests without the
 * LOAD_BACKGROUND bit set).
 */
/* readonly attribute unsigned long activeCount; */
nsresult VFTCALL downLoadGroup_GetActiveCount(void *pvThis, PRUint32 *aActiveCount)
{
    DOWN_ENTER_RC(pvThis, nsILoadGroup);
    dprintf(("%s: aActiveCount=%p\n", pszFunction, aActiveCount));

    rc = pMozI->GetActiveCount(aActiveCount);
    if (VALID_PTR(aActiveCount))
        dprintf(("%s: *aActiveCount=%d\n", pszFunction, aActiveCount));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Notification callbacks for the load group.
 */
/* attribute nsIInterfaceRequestor notificationCallbacks; */
nsresult VFTCALL downLoadGroup_GetNotificationCallbacks(void *pvThis, nsIInterfaceRequestor * *aNotificationCallbacks)
{
    DOWN_ENTER_RC(pvThis, nsILoadGroup);
    dprintf(("%s: aNotificationCallbacks=%p\n", pszFunction, aNotificationCallbacks));

    rc = pMozI->GetNotificationCallbacks(aNotificationCallbacks);
    rc = downCreateWrapper((void**)aNotificationCallbacks, kInterfaceRequestorIID, rc);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

nsresult VFTCALL downLoadGroup_SetNotificationCallbacks(void *pvThis, nsIInterfaceRequestor * aNotificationCallbacks)
{
    DOWN_ENTER_RC(pvThis, nsILoadGroup);
    dprintf(("%s: aNotificationCallbacks=%p\n", pszFunction, aNotificationCallbacks));

    nsIInterfaceRequestor * pUpNotificationCallbacks = aNotificationCallbacks;
    rc = upCreateWrapper((void**)&pUpNotificationCallbacks, kInterfaceRequestorIID, rc);
    rc = pMozI->SetNotificationCallbacks(pUpNotificationCallbacks);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsILoadGroup, downVFTnsILoadGroup)
{
 {
   {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
   },
    /* */
    downRequest_GetName,                                    VFTDELTA_VAL()
    downRequest_IsPending,                                  VFTDELTA_VAL()
    downRequest_GetStatus,                                  VFTDELTA_VAL()
    downRequest_Cancel,                                     VFTDELTA_VAL()
    downRequest_Suspend,                                    VFTDELTA_VAL()
    downRequest_Resume,                                     VFTDELTA_VAL()
    downRequest_GetLoadGroup,                               VFTDELTA_VAL()
    downRequest_SetLoadGroup,                               VFTDELTA_VAL()
    downRequest_GetLoadFlags,                               VFTDELTA_VAL()
    downRequest_SetLoadFlags,                               VFTDELTA_VAL()
 },
    downLoadGroup_GetGroupObserver,                         VFTDELTA_VAL()
    downLoadGroup_SetGroupObserver,                         VFTDELTA_VAL()
    downLoadGroup_GetDefaultLoadRequest,                    VFTDELTA_VAL()
    downLoadGroup_SetDefaultLoadRequest,                    VFTDELTA_VAL()
    downLoadGroup_AddRequest,                               VFTDELTA_VAL()
    downLoadGroup_RemoveRequest,                            VFTDELTA_VAL()
    downLoadGroup_GetRequests,                              VFTDELTA_VAL()
    downLoadGroup_GetActiveCount,                           VFTDELTA_VAL()
    downLoadGroup_GetNotificationCallbacks,                 VFTDELTA_VAL()
    downLoadGroup_SetNotificationCallbacks,                 VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();




//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIPluginStreamInfo
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/* readonly attribute nsMIMEType contentType; */
nsresult VFTCALL downPluginStreamInfo_GetContentType(void *pvThis, nsMIMEType *aContentType)
{
    DOWN_ENTER_RC(pvThis, nsIPluginStreamInfo);
    dprintf(("%s: aContentType=%p\n", pszFunction, aContentType));

    rc = pMozI->GetContentType(aContentType);
    if (VALID_PTR(aContentType))
        DPRINTF_STR(*aContentType);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/* void isSeekable (out boolean aSeekable); */
nsresult VFTCALL downPluginStreamInfo_IsSeekable(void *pvThis, PRBool *aSeekable)
{
    DOWN_ENTER_RC(pvThis, nsIPluginStreamInfo);
    dprintf(("%s: aSeekable=%p\n", pszFunction, aSeekable));

    rc = pMozI->IsSeekable(aSeekable);
    if (VALID_PTR(aSeekable))
        dprintf(("%s: *aSeekable=%d\n", pszFunction, *aSeekable));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/* readonly attribute unsigned long length; */
nsresult VFTCALL downPluginStreamInfo_GetLength(void *pvThis, PRUint32 *aLength)
{
    DOWN_ENTER_RC(pvThis, nsIPluginStreamInfo);
    dprintf(("%s: aLength=%p\n", pszFunction, aLength));

    rc = pMozI->GetLength(aLength);
    if (VALID_PTR(aLength))
        dprintf(("%s: *aLength=%d\n", pszFunction, *aLength));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/* readonly attribute unsigned long lastModified; */
nsresult VFTCALL downPluginStreamInfo_GetLastModified(void *pvThis, PRUint32 *aLastModified)
{
    DOWN_ENTER_RC(pvThis, nsIPluginStreamInfo);
    dprintf(("%s: aLastModified=%p\n", pszFunction, aLastModified));

    rc = pMozI->GetLastModified(aLastModified);
    if (VALID_PTR(aLastModified))
        dprintf(("%s: *aLastModified=%d\n", pszFunction, *aLastModified));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/* void getURL (out constCharPtr aURL); */
nsresult VFTCALL downPluginStreamInfo_GetURL(void *pvThis, const char * *aURL)
{
    DOWN_ENTER_RC(pvThis, nsIPluginStreamInfo);
    dprintf(("%s: aURL=%p\n", pszFunction, aURL));

    rc = pMozI->GetURL(aURL);
    if (VALID_PTR(aURL))
        DPRINTF_STR(*aURL);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/* void requestRead (in nsByteRangePtr aRangeList); */
nsresult VFTCALL downPluginStreamInfo_RequestRead(void *pvThis, nsByteRange * aRangeList)
{
    DOWN_ENTER_RC(pvThis, nsIPluginStreamInfo);
    dprintf(("%s: aRangeList=%p\n", pszFunction, aRangeList));
    rc = pMozI->RequestRead(aRangeList);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/* attribute long streamOffset; */
nsresult VFTCALL downPluginStreamInfo_GetStreamOffset(void *pvThis, PRInt32 *aStreamOffset)
{
    DOWN_ENTER_RC(pvThis, nsIPluginStreamInfo);
    dprintf(("%s: aStreamOffset=%p\n", pszFunction, aStreamOffset));

    rc = pMozI->GetStreamOffset(aStreamOffset);
    if (VALID_PTR(aStreamOffset))
        dprintf(("%s: *aStreamOffset=%p\n", pszFunction, *aStreamOffset));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

nsresult VFTCALL downPluginStreamInfo_SetStreamOffset(void *pvThis, PRInt32 aStreamOffset)
{
    DOWN_ENTER_RC(pvThis, nsIPluginStreamInfo);
    dprintf(("%s: aStreamOffset=%d\n", pszFunction, aStreamOffset));
    rc = pMozI->SetStreamOffset(aStreamOffset);
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}



MAKE_SAFE_VFT(VFTnsIPluginStreamInfo, downVFTnsIPluginStreamInfo)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downPluginStreamInfo_GetContentType,                    VFTDELTA_VAL()
    downPluginStreamInfo_IsSeekable,                        VFTDELTA_VAL()
    downPluginStreamInfo_GetLength,                         VFTDELTA_VAL()
    downPluginStreamInfo_GetLastModified,                   VFTDELTA_VAL()
    downPluginStreamInfo_GetURL,                            VFTDELTA_VAL()
    downPluginStreamInfo_RequestRead,                       VFTDELTA_VAL()
    downPluginStreamInfo_GetStreamOffset,                   VFTDELTA_VAL()
    downPluginStreamInfo_SetStreamOffset,                   VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIRequestObserver
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//



/**
 * Called to signify the beginning of an asynchronous request.
 *
 * @param aRequest request being observed
 * @param aContext user defined context
 *
 * An exception thrown from onStartRequest has the side-effect of
 * causing the request to be canceled.
 */
/* void onStartRequest (in nsIRequest aRequest, in nsISupports aContext); */
nsresult VFTCALL downRequestObserver_OnStartRequest(void *pvThis, nsIRequest *aRequest, nsISupports *aContext)
{
    DOWN_ENTER_RC(pvThis, nsIRequestObserver);
    dprintf(("%s: aRequest=%p aContext=%p\n", pszFunction, aRequest, aContext));

    nsIRequest *pUpRequest = aRequest;
    rc = upCreateWrapper((void**)&pUpRequest, kRequestIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports *pUpContext = aContext;
        rc = upCreateWrapper((void**)&pUpContext, kSupportsIID, NS_OK);
        if (NS_SUCCEEDED(rc))
            rc = pMozI->OnStartRequest(pUpRequest, pUpContext);
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Called to signify the end of an asynchronous request.  This
 * call is always preceded by a call to onStartRequest.
 *
 * @param aRequest request being observed
 * @param aContext user defined context
 * @param aStatusCode reason for stopping (NS_OK if completed successfully)
 *
 * An exception thrown from onStopRequest is generally ignored.
 */
/* void onStopRequest (in nsIRequest aRequest, in nsISupports aContext, in nsresult aStatusCode); */
nsresult VFTCALL downRequestObserver_OnStopRequest(void *pvThis, nsIRequest *aRequest, nsISupports *aContext, nsresult aStatusCode)
{
    DOWN_ENTER_RC(pvThis, nsIRequestObserver);
    dprintf(("%s: aRequest=%p aContext=%p aStatusCode=%d\n", pszFunction, aRequest, aContext, aStatusCode));

    nsIRequest *pUpRequest = aRequest;
    rc = upCreateWrapper((void**)&pUpRequest, kRequestIID, NS_OK);
    if (NS_SUCCEEDED(rc))
    {
        nsISupports *pUpContext = aContext;
        rc = upCreateWrapper((void**)&pUpContext, kSupportsIID, NS_OK);
        if (NS_SUCCEEDED(rc))
            rc = pMozI->OnStopRequest(pUpRequest, pUpContext, aStatusCode);
    }
    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsIRequestObserver, downVFTnsIRequestObserver)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downRequestObserver_OnStartRequest,                     VFTDELTA_VAL()
    downRequestObserver_OnStopRequest,                      VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();


//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsISimpleEnumerator
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//


/**
 * Called to determine whether or not the enumerator has
 * any elements that can be returned via getNext(). This method
 * is generally used to determine whether or not to initiate or
 * continue iteration over the enumerator, though it can be
 * called without subsequent getNext() calls. Does not affect
 * internal state of enumerator.
 *
 * @see getNext()
 * @return PR_TRUE if there are remaining elements in the enumerator.
 *         PR_FALSE if there are no more elements in the enumerator.
 */
/* boolean hasMoreElements (); */
nsresult VFTCALL downSimpleEnumerator_HasMoreElements(void *pvThis, PRBool *_retval)
{
    DOWN_ENTER_RC(pvThis, nsISimpleEnumerator);
    dprintf(("%s: _retval=%p\n", pszFunction, _retval));

    rc = pMozI->HasMoreElements(_retval);
    if (VALID_PTR(_retval))
        dprintf(("%s: *_retval=%d\n", pszFunction, *_retval));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * Called to retrieve the next element in the enumerator. The "next"
 * element is the first element upon the first call. Must be
 * pre-ceeded by a call to hasMoreElements() which returns PR_TRUE.
 * This method is generally called within a loop to iterate over
 * the elements in the enumerator.
 *
 * @see hasMoreElements()
 * @return NS_OK if the call succeeded in returning a non-null
 *               value through the out parameter.
 *         NS_ERROR_FAILURE if there are no more elements
 *                          to enumerate.
 * @return the next element in the enumeration.
 */
/* nsISupports getNext (); */
nsresult VFTCALL downSimpleEnumerator_GetNext(void *pvThis, nsISupports **_retval)
{
    DOWN_ENTER_RC(pvThis, nsISimpleEnumerator);
    dprintf(("%s: _retval=%p\n", pszFunction, _retval));

    rc = pMozI->GetNext(_retval);
    rc = downCreateWrapper((void**)_retval, kSupportsIID, rc);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}



MAKE_SAFE_VFT(VFTnsISimpleEnumerator, downVFTnsISimpleEnumerator)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downSimpleEnumerator_HasMoreElements,                   VFTDELTA_VAL()
    downSimpleEnumerator_GetNext,                           VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();





//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: FlashIObject7
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//


/**
 * Notify the observer that the URL has started to load.  This method is
 * called only once, at the beginning of a URL load.<BR><BR>
 *
 * @param aPluginInfo  - plugin stream info
 * @return             - the return value is currently ignored, in the future
 * it may be used to cancel the URL load..
 */
/* void onStartBinding (in nsIPluginStreamInfo aPluginInfo); */
nsresult VFTCALL downFlashIObject7_Evaluate(void *pvThis, const char *aString, FlashIObject7 **aFlashObject)
{
    DOWN_ENTER_RC(pvThis, FlashIObject7);
    dprintf(("%s: aString=%p aFlashObject=%p", pszFunction, aString, aFlashObject));
    DPRINTF_STR(aString);

    rc = pMozI->Evaluate(aString, aFlashObject);
    rc = downCreateWrapper((void**)aFlashObject, kFlashIObject7IID, NS_OK);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}




MAKE_SAFE_VFT(VFTFlashIObject7, downVFTFlashIObject7)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downFlashIObject7_Evaluate,                             VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();




//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIHTTPHeaderListener
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//


/**
 * Called for each HTTP Response header.
 * NOTE: You must copy the values of the params.
 */
/* void newResponseHeader (in string headerName, in string headerValue); */
nsresult VFTCALL downHTTPHeaderListener_NewResponseHeader(void *pvThis, const char *headerName, const char *headerValue)
{
    DOWN_ENTER_RC(pvThis, nsIHTTPHeaderListener);
    DPRINTF_STR(headerName);
    DPRINTF_STR(headerValue);

    rc = pMozI->NewResponseHeader(headerName, headerValue);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}


MAKE_SAFE_VFT(VFTnsIHTTPHeaderListener, downVFTnsIHTTPHeaderListener)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downHTTPHeaderListener_NewResponseHeader,               VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN: nsIMemory
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//


/**
 * Allocates a block of memory of a particular size. If the memory
 * cannot be allocated (because of an out-of-memory condition), null
 * is returned.
 *
 * @param size - the size of the block to allocate
 * @result the block of memory
 */
/* [noscript, notxpcom] voidPtr alloc (in size_t size); */
void * VFTCALL downMemory_Alloc(void *pvThis, size_t size)
{
    DOWN_ENTER_NORET(pvThis, nsIMemory);
    dprintf(("%s: size=%d\n", pszFunction, size));

    void *pv = pMozI->Alloc(size);

    DOWN_LEAVE_INT(pvThis, (unsigned)pv);
    return pv;
}

/**
 * Reallocates a block of memory to a new size.
 *
 * @param ptr - the block of memory to reallocate
 * @param size - the new size
 * @result the reallocated block of memory
 *
 * If ptr is null, this function behaves like malloc.
 * If s is the size of the block to which ptr points, the first
 * min(s, size) bytes of ptr's block are copied to the new block.
 * If the allocation succeeds, ptr is freed and a pointer to the
 * new block returned.  If the allocation fails, ptr is not freed
 * and null is returned. The returned value may be the same as ptr.
 */
/* [noscript, notxpcom] voidPtr realloc (in voidPtr ptr, in size_t newSize); */
void * VFTCALL downMemory_Realloc(void *pvThis, void * ptr, size_t newSize)
{
    DOWN_ENTER_NORET(pvThis, nsIMemory);
    dprintf(("%s: ptr=%p newSize=%d\n", pszFunction, ptr, newSize));

    void *pv = pMozI->Realloc(ptr, newSize);

    DOWN_LEAVE_INT(pvThis, (unsigned)pv);
    return pv;
}

/**
 * Frees a block of memory. Null is a permissible value, in which case
 * nothing happens.
 *
 * @param ptr - the block of memory to free
 */
/* [noscript, notxpcom] void free (in voidPtr ptr); */
void VFTCALL downMemory_Free(void *pvThis, void * ptr)
{
    DOWN_ENTER_NORET(pvThis, nsIMemory);
    dprintf(("%s: ptr=%p\n", pszFunction, ptr));

    pMozI->Free(ptr);

    DOWN_LEAVE(pvThis);
    return;
}

/**
 * Attempts to shrink the heap.
 * @param immediate - if true, heap minimization will occur
 *   immediately if the call was made on the main thread. If
 *   false, the flush will be scheduled to happen when the app is
 *   idle.
 * @return NS_ERROR_FAILURE if 'immediate' is set an the call
 *   was not on the application's main thread.
 */
/* void heapMinimize (in boolean immediate); */
nsresult VFTCALL  downMemory_HeapMinimize(void *pvThis, PRBool immediate)
{
    DOWN_ENTER_RC(pvThis, nsIMemory);
    dprintf(("%s: immediate=%d\n", pszFunction, immediate));

    rc = pMozI->HeapMinimize(immediate);

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

/**
 * This predicate can be used to determine if we're in a low-memory
 * situation (what constitutes low-memory is platform dependent). This
 * can be used to trigger the memory pressure observers.
 */
/* boolean isLowMemory (); */
nsresult VFTCALL  downMemory_IsLowMemory(void *pvThis, PRBool *_retval)
{
    DOWN_ENTER_RC(pvThis, nsIMemory);
    dprintf(("%s: _retval=%p\n", pszFunction, _retval));

    rc = pMozI->IsLowMemory(_retval);
    if (VALID_PTR(_retval))
        dprintf(("%s: *_retval=%d", pszFunction, *_retval));

    DOWN_LEAVE_INT(pvThis, rc);
    return rc;
}

MAKE_SAFE_VFT(VFTnsIMemory, downVFTnsIMemory)
{
 {
    VFTFIRST_VAL()
    downQueryInterface,                                     VFTDELTA_VAL()
    downAddRef,                                             VFTDELTA_VAL()
    downRelease,                                            VFTDELTA_VAL()
 },
    downMemory_Alloc,                                       VFTDELTA_VAL()
    downMemory_Realloc,                                     VFTDELTA_VAL()
    downMemory_Free,                                        VFTDELTA_VAL()
    downMemory_HeapMinimize,                                VFTDELTA_VAL()
    downMemory_IsLowMemory,                                 VFTDELTA_VAL()
}
SAFE_VFT_ZEROS();



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// DOWN Helpers
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//

/** Supported Interface for downQueryInterface(). */
static struct SupportedInterface_Down
{
    /** Interface ID. */
    const nsIID *   pIID;
    /** Virtual Function Table. */
    const void *    pvVFT;
}   aDownInterfaces[] =
{
    { &kServiceManagerIID,          &downVFTnsIServiceManager },
    { &kServiceManagerObsoleteIID,  &downVFTnsIServiceManagerObsolete },
    { &kSupportsIID,                &downVFTnsISupports },
    { &kPluginManagerIID,           &downVFTnsIPluginManager },
    { &kPluginManager2IID,          &downVFTnsIPluginManager2 },
    { &kPluginInstancePeerIID,      &downVFTnsIPluginInstancePeer },
    { &kPluginInstancePeer2IID,     &downVFTnsIPluginInstancePeer2 },
    { &kPluginTagInfoIID,           &downVFTnsIPluginTagInfo },
    { &kPluginTagInfo2IID,          &downVFTnsIPluginTagInfo2 },
    { &kCookieStorageIID,           &downVFTnsICookieStorage },
    { &kJVMThreadManagerIID,        &downVFTnsIJVMThreadManager },
    { &kJVMManagerIID,              &downVFTnsIJVMManager },
    { &kLiveconnectIID,             &downVFTnsILiveconnect },
    { &kSecureLiveconnectIID,       &downVFTnsISecureLiveconnect },
    { &kComponentManagerIID,        &downVFTnsIComponentManager },
    { &kComponentManagerObsoleteIID,&downVFTnsIComponentManagerObsolete },
    { &kSecurityContextIID,         &downVFTnsISecurityContext },
    { &kEnumeratorIID,              &downVFTnsIEnumerator },
    { &kFactoryIID,                 &downVFTnsIFactory },
    { &kInputStreamIID,             &downVFTnsIInputStream },
    { &kOutputStreamIID,            &downVFTnsIOutputStream },
    { &kInterfaceRequestorIID,      &downVFTnsIInterfaceRequestor },
    { &kRequestIID,                 &downVFTnsIRequest },
    { &kLoadGroupIID,               &downVFTnsILoadGroup },
    { &kPluginStreamInfoIID,        &downVFTnsIPluginStreamInfo },
    { &kRequestObserverIID,         &downVFTnsIRequestObserver },
    { &kSimpleEnumeratorIID,        &downVFTnsISimpleEnumerator },
    { &kFlashIObject7IID,           &downVFTFlashIObject7 },
    { &kHTTPHeaderListenerIID,      &downVFTnsIHTTPHeaderListener },
    { &kMemoryIID,                  &downVFTnsIMemory },
};

/**
 * Checks if the specified Interface ID is supported by the 'down'
 * wrappers.
 *
 * @returns Pointer to the virtual function table for the wrapper if supported.
 * @returns NULL if not supported.
 * @param   aIID    Reference to the Interface ID.
 *
 */
const void * downIsSupportedInterface(REFNSIID aIID)
{
    for (unsigned iInterface = 0; iInterface < sizeof(aDownInterfaces) / sizeof(aDownInterfaces[0]); iInterface++)
        if (aDownInterfaces[iInterface].pIID->Equals(aIID))
            return aDownInterfaces[iInterface].pvVFT;
    return NULL;
}

/**
 * Creates a 'down' wrapper - a wrapper object which is presented down to the
 * Win32 plugin.
 *
 * @returns rcOrg on success.
 * @returns Appropriate error message on failure.
 *
 * @param   pvVFT       Pointer to the wrapper VFT.
 * @param   ppvResult   Where the caller have requested the interface pointer
 *                      to be stored.
 *                      On entry this is the pointer to create wrapper for.
 *                      On exit this will point to the created wrapper.
 * @param   rc          The RC returned by the original method, this is
 *                      returned on success.
 */
nsresult downCreateWrapper(void **ppvResult, const void *pvVFT, nsresult rc)
{
    DEBUG_FUNCTIONNAME();
    dprintf(("%s: pvvResult=%x, pvVFT=%x, rc=%x", pszFunction, pvVFT, ppvResult, rc));
    #ifdef DEBUG
    for (unsigned iInterface = 0; iInterface < sizeof(aDownInterfaces) / sizeof(aDownInterfaces[0]); iInterface++)
        if (aDownInterfaces[iInterface].pvVFT == pvVFT)
        {
            DPRINTF_NSID(*aDownInterfaces[iInterface].pIID);
            break;
        }
    #endif

    if (VALID_PTR(ppvResult))
    {
        if (VALID_PTR(*ppvResult))
        {
            if (pvVFT)
            {
                if (NS_SUCCEEDED(rc))
                {
                    void *pvThis = *ppvResult;

                    /*
                     * First check if the object returned actually is an up wrapper.
                     */
                    void *pvNoWrapper = UpWrapperBase::findUpWrapper(pvThis);
                    if (pvNoWrapper)
                    {
                        dprintf(("%s: COOL! pvThis(%x) is an up wrapper, no wrapping needed. returns real obj=%x",
                                 pszFunction,  pvThis, pvNoWrapper));
                        *ppvResult = pvNoWrapper;
                        return rc;
                    }

                    #if 1 //did this to minimize heap calls
                    /*
                     * Check if exists before trying to create one.
                     */
                    DOWN_LOCK();
                    for (PDOWNTHIS pDown = (PDOWNTHIS)gpDownHead; pDown; pDown = (PDOWNTHIS)pDown->pNext)
                        if (pDown->pvThis == pvThis)
                        {
                            if (pDown->pvVFT == pvVFT)
                            {
                                DOWN_UNLOCK();
                                dprintf(("%s: Found existing wrapper %x for %x.", pszFunction, pDown, pvThis));
                                *ppvResult = pDown;
                                return rc;
                            }
                        }
                    DOWN_UNLOCK();
                    #endif

                    /*
                     * Create the wrapper.
                     *  See upCreateWrapper for explanation of why this is done this way.
                     */
                    PDOWNTHIS pWrapper = new DOWNTHIS;
                    if (pWrapper)
                    {
                        pWrapper->initialize(pvThis, pvVFT);

                        /*
                         * Check if there is any existing down wrapper for this object/pvVFT.
                         */
                        DOWN_LOCK();
                        for (PDOWNTHIS pDown = (PDOWNTHIS)gpDownHead; pDown; pDown = (PDOWNTHIS)pDown->pNext)
                            if (pDown->pvThis == pvThis)
                            {
                                if (pDown->pvVFT == pvVFT)
                                {
                                    DOWN_UNLOCK();
                                    delete pWrapper;
                                    dprintf(("%s: Found existing wrapper %x for %x. (2)", pszFunction, pDown, pvThis));
                                    *ppvResult = pDown;
                                    return rc;
                                }
                            }
                        /*
                         * Ok, no existing wrapper, insert the new one.
                         */
                        pWrapper->pNext = gpDownHead;
                        gpDownHead = pWrapper;
                        DOWN_UNLOCK();

                        dprintf(("%s: Created wrapper %x for %x.", pszFunction, pWrapper, pvThis));
                        *ppvResult = pWrapper;
                        return rc;
                    }
                    dprintf(("new failed!!"));
                    rc = NS_ERROR_OUT_OF_MEMORY;
                }
                else
                    dprintf(("%s: Passed in rc means failure, (rc=%x)", pszFunction, rc));
            }
            else
            {
                dprintf(("%s: No VFT, that can only mean that it's an Unsupported interface!!!", pszFunction));
                rc = NS_ERROR_NOT_IMPLEMENTED;
                ReleaseInt3(0xbaddbeef, 6, 0);
            }
            *ppvResult = nsnull;
        }
        else if (*ppvResult || rc != NS_OK) /* don't complain about the obvious.. we use this combination. */
            dprintf(("%s: *ppvResult (=%p) is invalid (rc=%x)", pszFunction, *ppvResult, rc));
    }
    else
        dprintf(("%s: ppvResult (=%p) is invalid (rc=%x)", pszFunction, ppvResult, rc));

    return rc;
}

/**
 * Create down wrapper by Interface Id.
 */
nsresult downCreateWrapper(void **ppvResult, REFNSIID aIID, nsresult rc)
{
    return downCreateWrapper(ppvResult, downIsSupportedInterface(aIID), rc);
}


/**
 * Create a JNIEnv wrapper for sending down to the plugin.
 * @returns rc on success
 * @returns rc or othere error code on failure.
 * @param   ppJNIEnv    Where to get and store the JNIEnv wrapper.
 * @param   rc          Current rc.
 */
int downCreateJNIEnvWrapper(JNIEnv **ppJNIEnv, int rc)
{
    DEBUG_FUNCTIONNAME();
    dprintf(("%s: ppJNIEnv=%x, rc=%x", pszFunction, ppJNIEnv, rc));

    if (VALID_PTR(ppJNIEnv))
    {
        if (VALID_PTR(*ppJNIEnv))
        {
            if (NS_SUCCEEDED(rc))
            {
                    /*
                     * Success!
                     */
                    return rc;
            }
            else
                dprintf(("%s: The query method failed with rc=%x", pszFunction, rc));
            *ppJNIEnv = nsnull;
        }
        else if (*ppJNIEnv || rc != NS_OK) /* don't complain about the obvious.. we use this combination. */
            dprintf(("%s: *ppJNIEnv (=%p) is invalid (rc=%x)", pszFunction, *ppJNIEnv, rc));
    }
    else
        dprintf(("%s: ppJNIEnv (=%p) is invalid (rc=%x)", pszFunction, ppJNIEnv, rc));

    return rc;
}



/**
 * Lock the down wrapper list.
 */
void            downLock(void)
{
    DEBUG_FUNCTIONNAME();

    if (!ghmtxDown)
    {
        int rc = DosCreateMutexSem(NULL, &ghmtxDown, 0, TRUE);
        if (rc)
        {
            dprintf(("%s: DosCreateMutexSem failed with rc=%d.", pszFunction, rc));
            ReleaseInt3(0xdeadbee1, 0xd000, rc);
        }
    }
    else
    {
        int rc = DosRequestMutexSem(ghmtxDown, SEM_INDEFINITE_WAIT);
        if (rc)
        {
            dprintf(("%s: DosRequestMutexSem failed with rc=%d.", pszFunction, rc));
            ReleaseInt3(0xdeadbee1, 0xd001, rc);
        }
        //@todo handle cases with the holder dies.
    }
}

/**
 * UnLock the down wrapper list.
 */
void            downUnLock(void)
{
    DEBUG_FUNCTIONNAME();

    int rc = DosReleaseMutexSem(ghmtxDown);
    if (rc)
    {
        dprintf(("%s: DosRequestMutexSem failed with rc=%d.", pszFunction, rc));
        ReleaseInt3(0xdeadbee1, 0xd002, rc);
    }
    //@todo handle cases with the holder dies.
}



//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// Misc Helpers
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//


/**
 * Looks up the understable name corresponding to the specified nsID.
 * @returns name string if found.
 * @returns "<unknown>" if not found.
 * @param   aIIDorCID   IID or CID to lookup.
 */
const char *    getIIDCIDName(REFNSIID aIIDorCID)
{
    for (unsigned i = 0; i < sizeof(aIDNameLookup) / sizeof(aIDNameLookup[0]); i++)
        if (aIDNameLookup[i].pID->Equals(aIIDorCID))
            return aIDNameLookup[i].pszName;

    return "<unknown>";
}


/**
 * Get NSID from string.
 *
 * @returns pointer to NSID.
 * @returns NULL if not found.
 * @param   aIIDorCID   IID or CID to lookup.
 */
const nsID *    getIIDCIDFromName(const char *pszStrID)
{
    for (unsigned i = 0; i < sizeof(aIDStrIDLookup) / sizeof(aIDStrIDLookup[0]); i++)
        if (!stricmp(aIDStrIDLookup[i].pszStrID, pszStrID))
            return aIDStrIDLookup[i].pID;

    return NULL;
}



/**
 * This is a hack to fix problems occuring when a plugin send the browser a non
 * UTF-8 when a UTF-8 string is expected.
 *
 * This is only experimental stuff.
 *
 * @param   pszString   String to validate and hack.
 * @param   pszFunction Function we're called from.
 */
void    verifyAndFixUTF8String(const char *pszString, const char *pszFunction)
{
    /* Fix for stupid Mozilla+IBM:
     *      The interface doesn't say UTF8, but mozilla expects it.
     *      The IBM guys didn't check the mozilla sources and sends it
     *      a string in the current code page.
     * We'll work around this by spacing out all characaters which
     * aren't valid UTF8.
     */
    unsigned char *pszString2 = *(unsigned char**)((void*)&pszString);
    for (unsigned char *pch = pszString2; *pch; pch++)
    {
        unsigned ch = *pch;
        if (ch > 0x7f)
        {
            /*
            U-00000000 - U-0000007F: 0xxxxxxx
            U-00000080 - U-000007FF: 110xxxxx 10xxxxxx
            U-00000800 - U-0000FFFF: 1110xxxx 10xxxxxx 10xxxxxx
            U-00010000 - U-001FFFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            U-00200000 - U-03FFFFFF: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
            U-04000000 - U-7FFFFFFF: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
            */
            unsigned val;
            unsigned min;
            unsigned c;
            unsigned i;
            if ((ch & 0xe0) == 0xc0)
            {
                c = 1;
                min = 0x80;
                val = ch & ~0xc0;
            }
            else if ((ch & 0xf0) == 0xe0)
            {
                c = 2;
                min = 0x800;
                val = ch & ~0xe0;
            }
            else if ((ch & 0xf8) == 0xf0)
            {
                c = 3;
                min = 0x1000;
                val = ch & ~0xf0;
            }
            else if ((ch & 0xfc) == 0xf8)
            {
                c = 4;
                min = 0x20000;
                val = ch & ~0xf8;
            }
            else if ((ch & 0xfe) == 0xfc)
            {
                c = 5;
                min = 0x400000;
                val = ch & ~0xfc;
            }
            else
                goto invalid;

            for (i = 1; i <= c; i++)
            {
                unsigned chPart = pch[i];
                if ((chPart & 0xc0) != 0x80)
                    goto invalid;
                val <<= 6;
                val |= chPart & 0x3f;
            }
            if (val < min)
                goto invalid;
            pch += c;
            continue;

        invalid:
            *pch = ' ';
        }
    }
}

//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//
// Exported Generic Plugin Wrapper interfaces.
//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//



nsresult npXPCOMGenericGetFactory(nsIServiceManagerObsolete *aServMgr,
                                  REFNSIID aClass,
                                  const char *aClassName,
                                  const char *aContractID,
                                  PRLibrary * aLibrary,
                                  nsIPlugin **aResult)
{
    DEBUG_FUNCTIONNAME();
    typedef nsresult (* _Optlink nsLegacyFactorProc)(PDOWNTHIS aServMgr, const nsCID &aClass, const char *aClassName, const char *aContractID, nsIPlugin **aResult);
    nsresult            rc;
    nsLegacyFactorProc  pfnGetFactory;

    dprintf(("%s: enter", pszFunction));
    DPRINTF_STR(aClassName);
    DPRINTF_NSID(aClass);
    DPRINTF_CONTRACTID(aContractID);

    /*
     * find the NSGetFactory() export.
     */
    pfnGetFactory = (nsLegacyFactorProc)PR_FindSymbol(aLibrary, "NSGetFactory");
    if (!pfnGetFactory)
    {
        dprintf(("%s: Could not find NSGetFactory.", pszFunction));
        return NS_ERROR_FAILURE;
    }


    /*
     * Make wrapper for the aServMgr argument.
     */
    PDOWNTHIS   pDownServMgr = (PDOWNTHIS)aServMgr;
    rc = downCreateWrapper((void**)&pDownServMgr, downIsSupportedInterface(kSupportsIID), NS_OK);
    if (NS_FAILED(rc))
    {
        dprintf(("%s: NSGetFactory failed.", pszFunction));
        *aResult = nsnull;
        return rc;
    }


    /*
     * Call the legacy NSGetFactory.
     */
    rc = pfnGetFactory(pDownServMgr, aClass, aClassName, aContractID, aResult);
    if (NS_FAILED(rc))
    {
        dprintf(("%s: NSGetFactory failed.", pszFunction));
        *aResult = nsnull;
        return rc;
    }


    /*
     * Make wrapper for the plugin factory interface (aResult).
     */
    dprintf(("%s: pfnNSGetFactory succeeded!", pszFunction));
    rc = upCreateWrapper((void**)aResult, kPluginIID, rc);

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
nsresult npXPCOMGenericMaybeWrap(REFNSIID aIID, nsISupports *aIn, nsISupports **aOut)
{
    DEBUG_FUNCTIONNAME();

    dprintf(("%s: Enter. aIn=%p aOut=%p", pszFunction, aIn, aOut));
    DPRINTF_NSID(aIID);

    /*
     * Is this an VAC interface?
     *
     * We'll verify this by checking the AddRef(), Release() and QueryInterface()
     * entries of the VFT. The first two entries in the VFT is usually NULL I think,
     * but we don't relie on them because we don't know. We'll just check that every
     * second entry starting [2] is code, and every second entry starting [3] is an
     * not pointing to valid code.
     *
     * Code is range 0x10000 to 0xc0000000 (Damed be anyone who implements module loading
     * above 3GB).
     *
     */
    if (!VALID_PTR(aIn))
    {
        dprintf(("%s: Invalid aIn pointer %p!!!", pszFunction, aIn));
        return NS_ERROR_FAILURE;
    }
    VFTnsISupports * pVFT = (VFTnsISupports *)(*(void**)aIn);
    if (    VALID_PTR(pVFT->QueryInterface)
        &&  !VALID_PTR(pVFT->uDeltaQueryInterface)
        &&  VALID_PTR(pVFT->AddRef)
        &&  !VALID_PTR(pVFT->uDeltaAddRef)
        &&  VALID_PTR(pVFT->Release)
        &&  !VALID_PTR(pVFT->uDeltaRelease)
        )
    {
        dprintf(("%s: Detected VAC VFT.", pszFunction));

        /*
         * Is the interface supported?
         * @todo: It seems that our flash 5 plugin doesn't do this right.
         *        So, I'm not going to support this interface (nsIFlash) till we
         *        know it's right or can work around the problem in a good manner.
         */
        if (1) //if (upIsSupportedInterface(aIID)) //@todo: remove hack!
        {
            *aOut = aIn;
            nsresult rc = upCreateWrapper((void**)aOut, /*aIID*/ kSupportsIID, NS_OK); /* @todo: fix aIID hack */
            if (NS_SUCCEEDED(rc))
            {
                dprintf(("%s: Successfully created wrapper.", pszFunction));
            }
            else
            {
                dprintf(("%s: failed to create wrapper for known interface!!!", pszFunction));
                ReleaseInt3(0xdeadbee2, 0xdead0001, rc);
                return rc;
            }
        }
        else
        {
            dprintf(("%s: Unsupported Interface !!!", pszFunction));
            *aOut = nsnull;
            ReleaseInt3(0xdeadbee2, aIID.m0, aIID.m1);
            return NS_ERROR_NO_INTERFACE;
        }
    }
    else
    {
        dprintf(("%s: Not a VAC VFT assuming native VFT which doesn't need wrapping!", pszFunction));
        *aOut = aIn;
    }

    return NS_OK;
}



/**
 * Initiates the semaphores we use.
 * This function cannot fail.
 */
void    npXPCOMInitSems(void)
{
    DOWN_LOCK();
    DOWN_UNLOCK();
    UpWrapperBase::upLock();
    UpWrapperBase::upUnLock();
}
