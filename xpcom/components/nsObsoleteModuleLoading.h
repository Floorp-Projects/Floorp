
#ifndef OBSOLETE_MODULE_LOADING
/*
 * Prototypes for dynamic library export functions. Your DLL/DSO needs to export
 * these methods to play in the component world.
 *
 * THIS IS OBSOLETE. Look at nsIModule.idl
 */

extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory);
extern "C" NS_EXPORT PRBool   NSCanUnload(nsISupports* aServMgr);
extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, const char *fullpath);
extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, const char *fullpath);

typedef nsresult (*nsFactoryProc)(nsISupports* aServMgr,
                                  const nsCID &aClass,
                                  const char *aClassName,
                                  const char *aContractID,
                                  nsIFactory **aFactory);
typedef PRBool   (*nsCanUnloadProc)(nsISupports* aServMgr);
typedef nsresult (*nsRegisterProc)(nsISupports* aServMgr, const char *path);
typedef nsresult (*nsUnregisterProc)(nsISupports* aServMgr, const char *path);
#endif /* OBSOLETE_MODULE_LOADING */
