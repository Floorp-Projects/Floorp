/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIComponentManager.idl
 */

#ifndef __gen_nsIComponentManager_h__
#define __gen_nsIComponentManager_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIEnumerator.h"

class nsIFile; /* forward declaration */


/* starting interface:    nsIComponentManager */
#define NS_ICOMPONENTMANAGER_IID_STR "8458a740-d5dc-11d2-92fb-00e09805570f"

#define NS_ICOMPONENTMANAGER_IID \
  {0x8458a740, 0xd5dc, 0x11d2, \
    { 0x92, 0xfb, 0x00, 0xe0, 0x98, 0x05, 0x57, 0x0f }}

class nsIComponentManager : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICOMPONENTMANAGER_IID)

  /* nsIFactory findFactory (in nsCIDRef aClass); */
  NS_IMETHOD FindFactory(const nsCID & aClass, nsIFactory **_retval) = 0;

  /* [noscript] voidStar getClassObject (in nsCIDRef aClass, in nsIIDRef aIID); */
  NS_IMETHOD GetClassObject(const nsCID & aClass, const nsIID & aIID, void * *_retval) = 0;

  /* [noscript] void progIDToClassID (in string aProgID, out nsCID aClass); */
  NS_IMETHOD ProgIDToClassID(const char *aProgID, nsCID *aClass) = 0;

  /* string CLSIDToProgID (in nsCIDRef aClass, out string aClassName); */
  NS_IMETHOD CLSIDToProgID(const nsCID & aClass, char **aClassName, char **_retval) = 0;

  /* [noscript] voidStar createInstance (in nsCIDRef aClass, in nsISupports aDelegate, in nsIIDRef aIID); */
  NS_IMETHOD CreateInstance(const nsCID & aClass, nsISupports *aDelegate, const nsIID & aIID, void * *_retval) = 0;

  /* [noscript] voidStar createInstanceByProgID (in string aProgID, in nsISupports aDelegate, in nsIIDRef IID); */
  NS_IMETHOD CreateInstanceByProgID(const char *aProgID, nsISupports *aDelegate, const nsIID & IID, void * *_retval) = 0;

  /**
     * Produce a rel: or abs: registry location for a given spec.
     */
  /* string registryLocationForSpec (in nsIFile aSpec); */
  NS_IMETHOD RegistryLocationForSpec(nsIFile *aSpec, char **_retval) = 0;

  /**
     * Create a full FileSpec for the rel:/abs: location provided.
     */
  /* nsIFile specForRegistryLocation (in string aLocation); */
  NS_IMETHOD SpecForRegistryLocation(const char *aLocation, nsIFile **_retval) = 0;

  /* void registerFactory (in nsCIDRef aClass, in string aClassName, in string aProgID, in nsIFactory aFactory, in boolean aReplace); */
  NS_IMETHOD RegisterFactory(const nsCID & aClass, const char *aClassName, const char *aProgID, nsIFactory *aFactory, PRBool aReplace) = 0;

  /* void registerComponentLoader (in string aType, in string aProgID, in boolean aReplace); */
  NS_IMETHOD RegisterComponentLoader(const char *aType, const char *aProgID, PRBool aReplace) = 0;

  /* void registerComponent (in nsCIDRef aClass, in string aClassName, in string aProgID, in string aLocation, in boolean aReplace, in boolean aPersist); */
  NS_IMETHOD RegisterComponent(const nsCID & aClass, const char *aClassName, const char *aProgID, const char *aLocation, PRBool aReplace, PRBool aPersist) = 0;

  /* void registerComponentWithType (in nsCIDRef aClass, in string aClassName, in string aProgID, in nsIFile aSpec, in string aLocation, in boolean aReplace, in boolean aPersist, in string aType); */
  NS_IMETHOD RegisterComponentWithType(const nsCID & aClass, const char *aClassName, const char *aProgID, nsIFile *aSpec, const char *aLocation, PRBool aReplace, PRBool aPersist, const char *aType) = 0;

  /* void registerComponentSpec (in nsCIDRef aClass, in string aClassName, in string aProgID, in nsIFile aLibrary, in boolean aReplace, in boolean aPersist); */
  NS_IMETHOD RegisterComponentSpec(const nsCID & aClass, const char *aClassName, const char *aProgID, nsIFile *aLibrary, PRBool aReplace, PRBool aPersist) = 0;

  /* void registerComponentLib (in nsCIDRef aClass, in string aClassName, in string aProgID, in string adllName, in boolean aReplace, in boolean aPersist); */
  NS_IMETHOD RegisterComponentLib(const nsCID & aClass, const char *aClassName, const char *aProgID, const char *adllName, PRBool aReplace, PRBool aPersist) = 0;

  /* void unregisterFactory (in nsCIDRef aClass, in nsIFactory aFactory); */
  NS_IMETHOD UnregisterFactory(const nsCID & aClass, nsIFactory *aFactory) = 0;

  /* void unregisterComponent (in nsCIDRef aClass, in string aLibrary); */
  NS_IMETHOD UnregisterComponent(const nsCID & aClass, const char *aLibrary) = 0;

  /* void unregisterComponentSpec (in nsCIDRef aClass, in nsIFile aLibrarySpec); */
  NS_IMETHOD UnregisterComponentSpec(const nsCID & aClass, nsIFile *aLibrarySpec) = 0;

  /* void freeLibraries (); */
  NS_IMETHOD FreeLibraries(void) = 0;

  enum { NS_Startup = 0 };

  enum { NS_Script = 1 };

  enum { NS_Timer = 2 };

  enum { NS_Shutdown = 3 };

  /* void autoRegister (in long when, in nsIFile directory); */
  NS_IMETHOD AutoRegister(PRInt32 when, nsIFile *directory) = 0;

  /* void autoRegisterComponent (in long when, in nsIFile component); */
  NS_IMETHOD AutoRegisterComponent(PRInt32 when, nsIFile *component) = 0;

  /* void autoUnregisterComponent (in long when, in nsIFile component); */
  NS_IMETHOD AutoUnregisterComponent(PRInt32 when, nsIFile *component) = 0;

  /* boolean isRegistered (in nsCIDRef aClass); */
  NS_IMETHOD IsRegistered(const nsCID & aClass, PRBool *_retval) = 0;

  /* nsIEnumerator enumerateCLSIDs (); */
  NS_IMETHOD EnumerateCLSIDs(nsIEnumerator **_retval) = 0;

  /* nsIEnumerator enumerateProgIDs (); */
  NS_IMETHOD EnumerateProgIDs(nsIEnumerator **_retval) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSICOMPONENTMANAGER \
  NS_IMETHOD FindFactory(const nsCID & aClass, nsIFactory **_retval); \
  NS_IMETHOD GetClassObject(const nsCID & aClass, const nsIID & aIID, void * *_retval); \
  NS_IMETHOD ProgIDToClassID(const char *aProgID, nsCID *aClass); \
  NS_IMETHOD CLSIDToProgID(const nsCID & aClass, char **aClassName, char **_retval); \
  NS_IMETHOD CreateInstance(const nsCID & aClass, nsISupports *aDelegate, const nsIID & aIID, void * *_retval); \
  NS_IMETHOD CreateInstanceByProgID(const char *aProgID, nsISupports *aDelegate, const nsIID & IID, void * *_retval); \
  NS_IMETHOD RegistryLocationForSpec(nsIFile *aSpec, char **_retval); \
  NS_IMETHOD SpecForRegistryLocation(const char *aLocation, nsIFile **_retval); \
  NS_IMETHOD RegisterFactory(const nsCID & aClass, const char *aClassName, const char *aProgID, nsIFactory *aFactory, PRBool aReplace); \
  NS_IMETHOD RegisterComponentLoader(const char *aType, const char *aProgID, PRBool aReplace); \
  NS_IMETHOD RegisterComponent(const nsCID & aClass, const char *aClassName, const char *aProgID, const char *aLocation, PRBool aReplace, PRBool aPersist); \
  NS_IMETHOD RegisterComponentWithType(const nsCID & aClass, const char *aClassName, const char *aProgID, nsIFile *aSpec, const char *aLocation, PRBool aReplace, PRBool aPersist, const char *aType); \
  NS_IMETHOD RegisterComponentSpec(const nsCID & aClass, const char *aClassName, const char *aProgID, nsIFile *aLibrary, PRBool aReplace, PRBool aPersist); \
  NS_IMETHOD RegisterComponentLib(const nsCID & aClass, const char *aClassName, const char *aProgID, const char *adllName, PRBool aReplace, PRBool aPersist); \
  NS_IMETHOD UnregisterFactory(const nsCID & aClass, nsIFactory *aFactory); \
  NS_IMETHOD UnregisterComponent(const nsCID & aClass, const char *aLibrary); \
  NS_IMETHOD UnregisterComponentSpec(const nsCID & aClass, nsIFile *aLibrarySpec); \
  NS_IMETHOD FreeLibraries(void); \
  NS_IMETHOD AutoRegister(PRInt32 when, nsIFile *directory); \
  NS_IMETHOD AutoRegisterComponent(PRInt32 when, nsIFile *component); \
  NS_IMETHOD AutoUnregisterComponent(PRInt32 when, nsIFile *component); \
  NS_IMETHOD IsRegistered(const nsCID & aClass, PRBool *_retval); \
  NS_IMETHOD EnumerateCLSIDs(nsIEnumerator **_retval); \
  NS_IMETHOD EnumerateProgIDs(nsIEnumerator **_retval); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSICOMPONENTMANAGER(_to) \
  NS_IMETHOD FindFactory(const nsCID & aClass, nsIFactory **_retval) { return _to ## FindFactory(aClass, _retval); } \
  NS_IMETHOD GetClassObject(const nsCID & aClass, const nsIID & aIID, void * *_retval) { return _to ## GetClassObject(aClass, aIID, _retval); } \
  NS_IMETHOD ProgIDToClassID(const char *aProgID, nsCID *aClass) { return _to ## ProgIDToClassID(aProgID, aClass); } \
  NS_IMETHOD CLSIDToProgID(const nsCID & aClass, char **aClassName, char **_retval) { return _to ## CLSIDToProgID(aClass, aClassName, _retval); } \
  NS_IMETHOD CreateInstance(const nsCID & aClass, nsISupports *aDelegate, const nsIID & aIID, void * *_retval) { return _to ## CreateInstance(aClass, aDelegate, aIID, _retval); } \
  NS_IMETHOD CreateInstanceByProgID(const char *aProgID, nsISupports *aDelegate, const nsIID & IID, void * *_retval) { return _to ## CreateInstanceByProgID(aProgID, aDelegate, IID, _retval); } \
  NS_IMETHOD RegistryLocationForSpec(nsIFile *aSpec, char **_retval) { return _to ## RegistryLocationForSpec(aSpec, _retval); } \
  NS_IMETHOD SpecForRegistryLocation(const char *aLocation, nsIFile **_retval) { return _to ## SpecForRegistryLocation(aLocation, _retval); } \
  NS_IMETHOD RegisterFactory(const nsCID & aClass, const char *aClassName, const char *aProgID, nsIFactory *aFactory, PRBool aReplace) { return _to ## RegisterFactory(aClass, aClassName, aProgID, aFactory, aReplace); } \
  NS_IMETHOD RegisterComponentLoader(const char *aType, const char *aProgID, PRBool aReplace) { return _to ## RegisterComponentLoader(aType, aProgID, aReplace); } \
  NS_IMETHOD RegisterComponent(const nsCID & aClass, const char *aClassName, const char *aProgID, const char *aLocation, PRBool aReplace, PRBool aPersist) { return _to ## RegisterComponent(aClass, aClassName, aProgID, aLocation, aReplace, aPersist); } \
  NS_IMETHOD RegisterComponentWithType(const nsCID & aClass, const char *aClassName, const char *aProgID, nsIFile *aSpec, const char *aLocation, PRBool aReplace, PRBool aPersist, const char *aType) { return _to ## RegisterComponentWithType(aClass, aClassName, aProgID, aSpec, aLocation, aReplace, aPersist, aType); } \
  NS_IMETHOD RegisterComponentSpec(const nsCID & aClass, const char *aClassName, const char *aProgID, nsIFile *aLibrary, PRBool aReplace, PRBool aPersist) { return _to ## RegisterComponentSpec(aClass, aClassName, aProgID, aLibrary, aReplace, aPersist); } \
  NS_IMETHOD RegisterComponentLib(const nsCID & aClass, const char *aClassName, const char *aProgID, const char *adllName, PRBool aReplace, PRBool aPersist) { return _to ## RegisterComponentLib(aClass, aClassName, aProgID, adllName, aReplace, aPersist); } \
  NS_IMETHOD UnregisterFactory(const nsCID & aClass, nsIFactory *aFactory) { return _to ## UnregisterFactory(aClass, aFactory); } \
  NS_IMETHOD UnregisterComponent(const nsCID & aClass, const char *aLibrary) { return _to ## UnregisterComponent(aClass, aLibrary); } \
  NS_IMETHOD UnregisterComponentSpec(const nsCID & aClass, nsIFile *aLibrarySpec) { return _to ## UnregisterComponentSpec(aClass, aLibrarySpec); } \
  NS_IMETHOD FreeLibraries(void) { return _to ## FreeLibraries(); } \
  NS_IMETHOD AutoRegister(PRInt32 when, nsIFile *directory) { return _to ## AutoRegister(when, directory); } \
  NS_IMETHOD AutoRegisterComponent(PRInt32 when, nsIFile *component) { return _to ## AutoRegisterComponent(when, component); } \
  NS_IMETHOD AutoUnregisterComponent(PRInt32 when, nsIFile *component) { return _to ## AutoUnregisterComponent(when, component); } \
  NS_IMETHOD IsRegistered(const nsCID & aClass, PRBool *_retval) { return _to ## IsRegistered(aClass, _retval); } \
  NS_IMETHOD EnumerateCLSIDs(nsIEnumerator **_retval) { return _to ## EnumerateCLSIDs(_retval); } \
  NS_IMETHOD EnumerateProgIDs(nsIEnumerator **_retval) { return _to ## EnumerateProgIDs(_retval); } 

#if 0
/* Use the code below as a template for the implementation class for this interface. */

/* Header file */
class nsComponentManager : public nsIComponentManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOMPONENTMANAGER

  nsComponentManager();
  virtual ~nsComponentManager();
  /* additional members */
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsComponentManager, nsIComponentManager)

nsComponentManager::nsComponentManager()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsComponentManager::~nsComponentManager()
{
  /* destructor code */
}

/* nsIFactory findFactory (in nsCIDRef aClass); */
NS_IMETHODIMP nsComponentManager::FindFactory(const nsCID & aClass, nsIFactory **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] voidStar getClassObject (in nsCIDRef aClass, in nsIIDRef aIID); */
NS_IMETHODIMP nsComponentManager::GetClassObject(const nsCID & aClass, const nsIID & aIID, void * *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void progIDToClassID (in string aProgID, out nsCID aClass); */
NS_IMETHODIMP nsComponentManager::ProgIDToClassID(const char *aProgID, nsCID *aClass)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string CLSIDToProgID (in nsCIDRef aClass, out string aClassName); */
NS_IMETHODIMP nsComponentManager::CLSIDToProgID(const nsCID & aClass, char **aClassName, char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] voidStar createInstance (in nsCIDRef aClass, in nsISupports aDelegate, in nsIIDRef aIID); */
NS_IMETHODIMP nsComponentManager::CreateInstance(const nsCID & aClass, nsISupports *aDelegate, const nsIID & aIID, void * *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] voidStar createInstanceByProgID (in string aProgID, in nsISupports aDelegate, in nsIIDRef IID); */
NS_IMETHODIMP nsComponentManager::CreateInstanceByProgID(const char *aProgID, nsISupports *aDelegate, const nsIID & IID, void * *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* string registryLocationForSpec (in nsIFile aSpec); */
NS_IMETHODIMP nsComponentManager::RegistryLocationForSpec(nsIFile *aSpec, char **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIFile specForRegistryLocation (in string aLocation); */
NS_IMETHODIMP nsComponentManager::SpecForRegistryLocation(const char *aLocation, nsIFile **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void registerFactory (in nsCIDRef aClass, in string aClassName, in string aProgID, in nsIFactory aFactory, in boolean aReplace); */
NS_IMETHODIMP nsComponentManager::RegisterFactory(const nsCID & aClass, const char *aClassName, const char *aProgID, nsIFactory *aFactory, PRBool aReplace)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void registerComponentLoader (in string aType, in string aProgID, in boolean aReplace); */
NS_IMETHODIMP nsComponentManager::RegisterComponentLoader(const char *aType, const char *aProgID, PRBool aReplace)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void registerComponent (in nsCIDRef aClass, in string aClassName, in string aProgID, in string aLocation, in boolean aReplace, in boolean aPersist); */
NS_IMETHODIMP nsComponentManager::RegisterComponent(const nsCID & aClass, const char *aClassName, const char *aProgID, const char *aLocation, PRBool aReplace, PRBool aPersist)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void registerComponentWithType (in nsCIDRef aClass, in string aClassName, in string aProgID, in nsIFile aSpec, in string aLocation, in boolean aReplace, in boolean aPersist, in string aType); */
NS_IMETHODIMP nsComponentManager::RegisterComponentWithType(const nsCID & aClass, const char *aClassName, const char *aProgID, nsIFile *aSpec, const char *aLocation, PRBool aReplace, PRBool aPersist, const char *aType)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void registerComponentSpec (in nsCIDRef aClass, in string aClassName, in string aProgID, in nsIFile aLibrary, in boolean aReplace, in boolean aPersist); */
NS_IMETHODIMP nsComponentManager::RegisterComponentSpec(const nsCID & aClass, const char *aClassName, const char *aProgID, nsIFile *aLibrary, PRBool aReplace, PRBool aPersist)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void registerComponentLib (in nsCIDRef aClass, in string aClassName, in string aProgID, in string adllName, in boolean aReplace, in boolean aPersist); */
NS_IMETHODIMP nsComponentManager::RegisterComponentLib(const nsCID & aClass, const char *aClassName, const char *aProgID, const char *adllName, PRBool aReplace, PRBool aPersist)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void unregisterFactory (in nsCIDRef aClass, in nsIFactory aFactory); */
NS_IMETHODIMP nsComponentManager::UnregisterFactory(const nsCID & aClass, nsIFactory *aFactory)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void unregisterComponent (in nsCIDRef aClass, in string aLibrary); */
NS_IMETHODIMP nsComponentManager::UnregisterComponent(const nsCID & aClass, const char *aLibrary)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void unregisterComponentSpec (in nsCIDRef aClass, in nsIFile aLibrarySpec); */
NS_IMETHODIMP nsComponentManager::UnregisterComponentSpec(const nsCID & aClass, nsIFile *aLibrarySpec)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void freeLibraries (); */
NS_IMETHODIMP nsComponentManager::FreeLibraries()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void autoRegister (in long when, in nsIFile directory); */
NS_IMETHODIMP nsComponentManager::AutoRegister(PRInt32 when, nsIFile *directory)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void autoRegisterComponent (in long when, in nsIFile component); */
NS_IMETHODIMP nsComponentManager::AutoRegisterComponent(PRInt32 when, nsIFile *component)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void autoUnregisterComponent (in long when, in nsIFile component); */
NS_IMETHODIMP nsComponentManager::AutoUnregisterComponent(PRInt32 when, nsIFile *component)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean isRegistered (in nsCIDRef aClass); */
NS_IMETHODIMP nsComponentManager::IsRegistered(const nsCID & aClass, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIEnumerator enumerateCLSIDs (); */
NS_IMETHODIMP nsComponentManager::EnumerateCLSIDs(nsIEnumerator **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIEnumerator enumerateProgIDs (); */
NS_IMETHODIMP nsComponentManager::EnumerateProgIDs(nsIEnumerator **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* End of implementation class template. */
#endif

/* include after the class def'n, because it needs to see it. */
#include "nsComponentManagerUtils.h"

#endif /* __gen_nsIComponentManager_h__ */
