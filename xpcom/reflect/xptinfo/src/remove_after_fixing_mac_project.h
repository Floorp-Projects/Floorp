
#ifndef NS_DECL_NSIINTERFACEINFO
#define NS_DECL_NSIINTERFACEINFO \
  NS_IMETHOD GetName(char * *aName); \
  NS_IMETHOD GetIID(nsIID * *aIID); \
  NS_IMETHOD IsScriptable(PRBool *_retval); \
  NS_IMETHOD GetParent(nsIInterfaceInfo * *aParent); \
  NS_IMETHOD GetMethodCount(PRUint16 *aMethodCount); \
  NS_IMETHOD GetConstantCount(PRUint16 *aConstantCount); \
  NS_IMETHOD GetMethodInfo(PRUint16 index, const nsXPTMethodInfo * *info); \
  NS_IMETHOD GetMethodInfoForName(const char *methodName, PRUint16 *index, const nsXPTMethodInfo * *info); \
  NS_IMETHOD GetConstant(PRUint16 index, const nsXPTConstant * *constant); \
  NS_IMETHOD GetInfoForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, nsIInterfaceInfo **_retval); \
  NS_IMETHOD GetIIDForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, nsIID * *_retval); \
  NS_IMETHOD GetTypeForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint16 dimension, nsXPTType *_retval); \
  NS_IMETHOD GetSizeIsArgNumberForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint16 dimension, PRUint8 *_retval); \
  NS_IMETHOD GetLengthIsArgNumberForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint16 dimension, PRUint8 *_retval); \
  NS_IMETHOD GetInterfaceIsArgNumberForParam(PRUint16 methodIndex, const nsXPTParamInfo * param, PRUint8 *_retval);
#endif

#ifndef NS_DECL_NSIINTERFACEINFOMANAGER
#define NS_DECL_NSIINTERFACEINFOMANAGER \
  NS_IMETHOD GetInfoForIID(const nsIID * iid, nsIInterfaceInfo **_retval); \
  NS_IMETHOD GetInfoForName(const char *name, nsIInterfaceInfo **_retval); \
  NS_IMETHOD GetIIDForName(const char *name, nsIID * *_retval); \
  NS_IMETHOD GetNameForIID(const nsIID * iid, char **_retval); \
  NS_IMETHOD EnumerateInterfaces(nsIEnumerator **_retval); \
  NS_IMETHOD AutoRegisterInterfaces(void); 
#endif

