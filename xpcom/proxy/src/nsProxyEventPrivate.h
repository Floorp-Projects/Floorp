/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsProxyEventPrivate_h__
#define nsProxyEventPrivate_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIEventTarget.h"
#include "nsIInterfaceInfo.h"
#include "nsIProxyObjectManager.h"

#include "nsXPTCUtils.h"

#include "mozilla/Mutex.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"

#include "nsClassHashtable.h"
#include "nsHashtable.h"

#include "prlog.h"

class nsProxyEventObject;

/**
 * To make types clearer, we distinguish between a canonical nsISupports* and
 * a proxied interface pointer which represents an arbitrary interface known
 * at runtime.
 */
typedef nsISupports nsISomeInterface;

#define NS_PROXYOBJECT_CLASS_IID \
{ 0xeea90d45, 0xb059, 0x11d2,                       \
  { 0x91, 0x5e, 0xc1, 0x2b, 0x69, 0x6c, 0x93, 0x33 } }

// This IID is used to filter runnables during synchronous event handling.
// The returned pointer is type nsProxyObjectCallInfo

#define NS_PROXYEVENT_FILTER_IID \
{ 0xec373590, 0x9164, 0x11d3,    \
{0x8c, 0x73, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74} }

/**
 * An object representing an IID and its associated interfaceinfo. Instances
 * of this class are obtained via nsProxyObjectManager::GetClass.
 */
class nsProxyEventClass
{
public:
    nsIInterfaceInfo*        GetInterfaceInfo() const {return mInfo;}
    const nsIID&             GetProxiedIID()    const {return mIID; }

    nsProxyEventClass(REFNSIID aIID, nsIInterfaceInfo* aInfo);
    ~nsProxyEventClass();

    nsIID                      mIID;
    nsCOMPtr<nsIInterfaceInfo> mInfo;
    uint32*                    mDescriptors;
};

/**
 * A class which provides the XPCOM identity for a proxied object.
 * Instances of this class are obtained from the POM, and are uniquely
 * hashed on a proxytype/eventtarget/realobject key.
 */
class nsProxyObject : public nsISupports
{
public:
    NS_DECL_ISUPPORTS

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_PROXYOBJECT_CLASS_IID)

    nsProxyObject(nsIEventTarget *destQueue, PRInt32 proxyType,
                  nsISupports *realObject);
 
    nsISupports*        GetRealObject() const { return mRealObject; }
    nsIEventTarget*     GetTarget() const { return mTarget; }
    PRInt32             GetProxyType() const { return mProxyType; }

    // these are the equivalents of AddRef/Release, but must be called
    // while holding the global POM lock
    nsrefcnt LockedAddRef();
    nsrefcnt LockedRelease();

    // LockedFind should be called holding the POM lock. It will
    // temporarily unlock the lock during execution.
    nsresult LockedFind(REFNSIID iid, void **aResult);

    void LockedRemove(nsProxyEventObject* aObject);

    friend class nsProxyObjectManager;
private:
    ~nsProxyObject();

    PRInt32                   mProxyType;
    nsCOMPtr<nsIEventTarget>  mTarget;           /* event target */
    nsCOMPtr<nsISupports>     mRealObject;       /* the non-proxy object that this object is proxying
                                                    This is a strong ref. */
    nsProxyEventObject       *mFirst;

    class nsProxyObjectDestructorEvent : public nsRunnable
    {
        nsProxyObjectDestructorEvent(nsProxyObject *doomed) :
            mDoomed(doomed)
        {}

        NS_DECL_NSIRUNNABLE

        friend class nsProxyObject;
    private:
        nsProxyObject *mDoomed;
    };

    friend class nsProxyObjectDestructorEvent;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsProxyObject, NS_PROXYOBJECT_CLASS_IID)

/**
 * Object representing a single interface implemented on a proxied object.
 * This object is maintained in a singly-linked list from the associated
 * "parent" nsProxyObject.
 */
class nsProxyEventObject : protected nsAutoXPTCStub
{
public:

    NS_DECL_ISUPPORTS

    // call this method and return result
    NS_IMETHOD CallMethod(PRUint16 methodIndex,
                          const XPTMethodDescriptor* info,
                          nsXPTCMiniVariant* params);

    nsProxyEventClass*    GetClass()            const { return mClass; }
    nsISomeInterface*     GetProxiedInterface() const { return mRealInterface; }
    nsIEventTarget*       GetTarget()           const { return mProxyObject->GetTarget(); }
    PRInt32               GetProxyType()        const { return mProxyObject->GetProxyType(); } 

    nsresult convertMiniVariantToVariant(const XPTMethodDescriptor *methodInfo,
                                         nsXPTCMiniVariant *params,
                                         nsXPTCVariant **fullParam,
                                         uint8 *outParamCount);

    nsProxyEventObject(nsProxyObject *aParent,
                       nsProxyEventClass *aClass,
                       already_AddRefed<nsISomeInterface> aRealInterface,
                       nsresult *rv);

    // AddRef, but you must be holding the global POM lock
    nsrefcnt LockedAddRef();
    friend class nsProxyObject;

private:
    ~nsProxyEventObject();

    // Member ordering is important: See note in the destructor.
    nsProxyEventClass          *mClass;
    nsCOMPtr<nsProxyObject>     mProxyObject;
    nsCOMPtr<nsISomeInterface>  mRealInterface;

    // Weak reference, maintained by the parent nsProxyObject
    nsProxyEventObject         *mNext;
};

#define NS_PROXYEVENT_IID                             \
{ /* 9a24dc5e-2b42-4a5a-aeca-37b8c8fd8ccd */          \
    0x9a24dc5e,                                       \
    0x2b42,                                           \
    0x4a5a,                                           \
    {0xae, 0xca, 0x37, 0xb8, 0xc8, 0xfd, 0x8c, 0xcd}  \
}

/**
 * A class representing a particular proxied method call.
 */
class nsProxyObjectCallInfo : public nsRunnable
{
public:

    NS_DECL_NSIRUNNABLE

    NS_IMETHOD QueryInterface(REFNSIID aIID, void **aResult);

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_PROXYEVENT_IID)
    
    nsProxyObjectCallInfo(nsProxyEventObject* owner,
                          const XPTMethodDescriptor *methodInfo,
                          PRUint32 methodIndex, 
                          nsXPTCVariant* parameterList, 
                          PRUint32 parameterCount);

    ~nsProxyObjectCallInfo();

    PRUint32            GetMethodIndex() const { return mMethodIndex; }
    nsXPTCVariant*      GetParameterList() const { return mParameterList; }
    PRUint32            GetParameterCount() const { return mParameterCount; }
    nsresult            GetResult() const { return mResult; }
    
    bool                GetCompleted();
    void                SetCompleted();
    void                PostCompleted();

    void                SetResult(nsresult rv) { mResult = rv; }
    
    nsIEventTarget*     GetCallersTarget();
    void                SetCallersTarget(nsIEventTarget* target);
    bool                IsSync() const
    {
        return !!(mOwner->GetProxyType() & NS_PROXY_SYNC);
    }

private:
    
    nsresult         mResult;                    /* this is the return result of the called function */
    const XPTMethodDescriptor *mMethodInfo;
    PRUint32         mMethodIndex;               /* which method to be called? */
    nsXPTCVariant   *mParameterList;             /* marshalled in parameter buffer */
    PRUint32         mParameterCount;            /* number of params */
    PRInt32          mCompleted;                 /* is true when the method has been called. */
       
    nsCOMPtr<nsIEventTarget> mCallersTarget;     /* this is the dispatch target that we must post a message back to 
                                                    when we are done invoking the method (only NS_PROXY_SYNC). */

    nsRefPtr<nsProxyEventObject>   mOwner;       /* this is the strong referenced nsProxyObject */
   
    void RefCountInInterfacePointers(bool addRef);
    void CopyStrings(bool copy);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsProxyObjectCallInfo, NS_PROXYEVENT_IID)

////////////////////////////////////////////////////////////////////////////////
// nsProxyObjectManager
////////////////////////////////////////////////////////////////////////////////

class nsProxyObjectManager: public nsIProxyObjectManager
{
    typedef mozilla::Mutex Mutex;

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROXYOBJECTMANAGER
        
    static nsresult Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);
    
    nsProxyObjectManager();
    
    static nsProxyObjectManager *GetInstance();
    static bool IsManagerShutdown();

    static void Shutdown();

    nsresult GetClass(REFNSIID aIID, nsProxyEventClass **aResult);

    void LockedRemove(nsProxyObject* aProxy);

    Mutex& GetLock() { return mProxyCreationLock; }

#ifdef PR_LOGGING
    static PRLogModuleInfo *sLog;
#endif

private:
    ~nsProxyObjectManager();

    static nsProxyObjectManager* gInstance;
    nsHashtable  mProxyObjectMap;
    nsClassHashtable<nsIDHashKey, nsProxyEventClass> mProxyClassMap;
    Mutex mProxyCreationLock;
};

#define NS_XPCOMPROXY_CLASSNAME "nsProxyObjectManager"
#define NS_PROXYEVENT_MANAGER_CID                 \
{ 0xeea90d41,                                     \
  0xb059,                                         \
  0x11d2,                                         \
 {0x91, 0x5e, 0xc1, 0x2b, 0x69, 0x6c, 0x93, 0x33} \
}

#define PROXY_LOG(args) PR_LOG(nsProxyObjectManager::sLog, PR_LOG_DEBUG, args)

#endif  // nsProxyEventPrivate_h__
