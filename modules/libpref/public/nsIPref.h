/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIPref.idl
 */

#ifndef __gen_nsIPref_h__
#define __gen_nsIPref_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */
#include "jsapi.h"
typedef int (*PrefChangedFunc) (const char *, void *);
 
#define NS_PREF_CID                                    \
  { /* {dc26e0e0-ca94-11d1-a9a4-00805f8a7ac4} */       \
    0xdc26e0e0,                                        \
    0xca94,                                            \
    0x11d1,                                            \
    { 0xa9, 0xa4, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 } \
  }
class nsFileSpec; 
 
#define NS_PREF_VALUE_CHANGED 1 


/* starting interface:    nsIPref */

/* {a22ad7b0-ca86-11d1-a9a4-00805f8a7ac4} */
#define NS_IPREF_IID_STR "a22ad7b0-ca86-11d1-a9a4-00805f8a7ac4"
#define NS_IPREF_IID \
  {0xa22ad7b0, 0xca86, 0x11d1, \
    { 0xa9, 0xa4, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xc4 }}

class nsIPref : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPREF_IID)

  /* void StartUp (); */
  NS_IMETHOD StartUp() = 0;

  /* void StartUpWith (in nsFileSpec spec); */
  NS_IMETHOD StartUpWith(nsFileSpec * spec) = 0;

  /* void ShutDown (); */
  NS_IMETHOD ShutDown() = 0;

  /* void ReadUserJSFile (in nsFileSpec filename); */
  NS_IMETHOD ReadUserJSFile(nsFileSpec * filename) = 0;

  /* void ReadLIJSFile (in nsFileSpec filename); */
  NS_IMETHOD ReadLIJSFile(nsFileSpec * filename) = 0;

  /* void EvaluateConfigScript (in string js_buffer, in PRUint32 length, in boolean bGlobalContext, in boolean bCallbacks); */
  NS_IMETHOD EvaluateConfigScript(const char *js_buffer, PRUint32 length, PRBool bGlobalContext, PRBool bCallbacks) = 0;

  /* void EvaluateConfigScriptFile (in string js_buffer, in PRUint32 length, in nsFileSpec filename, in boolean bGlobalContext, in boolean bCallbacks); */
  NS_IMETHOD EvaluateConfigScriptFile(const char *js_buffer, PRUint32 length, nsFileSpec * filename, PRBool bGlobalContext, PRBool bCallbacks) = 0;

  /* void SavePrefFileAs (in nsFileSpec filename); */
  NS_IMETHOD SavePrefFileAs(nsFileSpec * filename) = 0;

  /* void SaveLIPrefFile (in nsFileSpec filename); */
  NS_IMETHOD SaveLIPrefFile(nsFileSpec * filename) = 0;

  /* [noscript] readonly attribute JSContext configContext; */
  NS_IMETHOD GetConfigContext(JSContext * *aConfigContext) = 0;

  /* [noscript] readonly attribute JSObject globalConfigObject; */
  NS_IMETHOD GetGlobalConfigObject(JSObject * *aGlobalConfigObject) = 0;

  /* [noscript] readonly attribute JSObject prefConfigObject; */
  NS_IMETHOD GetPrefConfigObject(JSObject * *aPrefConfigObject) = 0;

  /* long GetIntPref (in string pref); */
  NS_IMETHOD GetIntPref(const char *pref, PRInt32 *_retval) = 0;

  /* boolean GetBoolPref (in string pref); */
  NS_IMETHOD GetBoolPref(const char *pref, PRBool *_retval) = 0;

  /* void GetBinaryPref (in string pref, in voidStar buf, inout long buf_length); */
  NS_IMETHOD GetBinaryPref(const char *pref, void * buf, PRInt32 *buf_length) = 0;

  /* void GetColorPref (in string pref, out octet red, out octet green, out octet blue); */
  NS_IMETHOD GetColorPref(const char *pref, PRUint8 *red, PRUint8 *green, PRUint8 *blue) = 0;

  /* unsigned long GetColorPrefDWord (in string pref); */
  NS_IMETHOD GetColorPrefDWord(const char *pref, PRUint32 *_retval) = 0;

  /* void GetRectPref (in string pref, out short left, out short top, out short right, out short bottom); */
  NS_IMETHOD GetRectPref(const char *pref, PRInt16 *left, PRInt16 *top, PRInt16 *right, PRInt16 *bottom) = 0;

  /* void SetCharPref (in string pref, in string value); */
  NS_IMETHOD SetCharPref(const char *pref, const char *value) = 0;

  /* void SetIntPref (in string pref, in long value); */
  NS_IMETHOD SetIntPref(const char *pref, PRInt32 value) = 0;

  /* void SetBoolPref (in string pref, in boolean value); */
  NS_IMETHOD SetBoolPref(const char *pref, PRBool value) = 0;

  /* void SetBinaryPref (in string pref, in voidStar value, in unsigned long size); */
  NS_IMETHOD SetBinaryPref(const char *pref, void * value, PRUint32 size) = 0;

  /* void SetColorPref (in string pref, in octet red, in octet green, in octet blue); */
  NS_IMETHOD SetColorPref(const char *pref, PRUint8 red, PRUint8 green, PRUint8 blue) = 0;

  /* void SetColorPrefDWord (in string pref, in unsigned long colorref); */
  NS_IMETHOD SetColorPrefDWord(const char *pref, PRUint32 colorref) = 0;

  /* void SetRectPref (in string pref, in short left, in short top, in short right, in short bottom); */
  NS_IMETHOD SetRectPref(const char *pref, PRInt16 left, PRInt16 top, PRInt16 right, PRInt16 bottom) = 0;

  /* void ClearUserPref (in string pref_name); */
  NS_IMETHOD ClearUserPref(const char *pref_name) = 0;

  /* long GetDefaultIntPref (in string pref); */
  NS_IMETHOD GetDefaultIntPref(const char *pref, PRInt32 *_retval) = 0;

  /* boolean GetDefaultBoolPref (in string pref); */
  NS_IMETHOD GetDefaultBoolPref(const char *pref, PRBool *_retval) = 0;

  /* void GetDefaultBinaryPref (in string pref, in voidStar value, out long length); */
  NS_IMETHOD GetDefaultBinaryPref(const char *pref, void * value, PRInt32 *length) = 0;

  /* void GetDefaultColorPref (in string pref, out octet red, out octet green, out octet blue); */
  NS_IMETHOD GetDefaultColorPref(const char *pref, PRUint8 *red, PRUint8 *green, PRUint8 *blue) = 0;

  /* unsigned long GetDefaultColorPrefDWord (in string pref); */
  NS_IMETHOD GetDefaultColorPrefDWord(const char *pref, PRUint32 *_retval) = 0;

  /* void GetDefaultRectPref (in string pref, out short left, out short top, out short right, out short bottom); */
  NS_IMETHOD GetDefaultRectPref(const char *pref, PRInt16 *left, PRInt16 *top, PRInt16 *right, PRInt16 *bottom) = 0;

  /* void SetDefaultCharPref (in string pref, in string value); */
  NS_IMETHOD SetDefaultCharPref(const char *pref, const char *value) = 0;

  /* void SetDefaultIntPref (in string pref, in long value); */
  NS_IMETHOD SetDefaultIntPref(const char *pref, PRInt32 value) = 0;

  /* void SetDefaultBoolPref (in string pref, in boolean value); */
  NS_IMETHOD SetDefaultBoolPref(const char *pref, PRBool value) = 0;

  /* void SetDefaultBinaryPref (in string pref, in voidStar value, in unsigned long size); */
  NS_IMETHOD SetDefaultBinaryPref(const char *pref, void * value, PRUint32 size) = 0;

  /* void SetDefaultColorPref (in string pref, in octet red, in octet green, in octet blue); */
  NS_IMETHOD SetDefaultColorPref(const char *pref, PRUint8 red, PRUint8 green, PRUint8 blue) = 0;

  /* void SetDefaultRectPref (in string pref, in short left, in short top, in short right, in short bottom); */
  NS_IMETHOD SetDefaultRectPref(const char *pref, PRInt16 left, PRInt16 top, PRInt16 right, PRInt16 bottom) = 0;

  /* string CopyCharPref (in string pref); */
  NS_IMETHOD CopyCharPref(const char *pref, char **_retval) = 0;

  /* voidStar CopyBinaryPref (in string pref, out long size); */
  NS_IMETHOD CopyBinaryPref(const char *pref, PRInt32 *size, void * *_retval) = 0;

  /* string CopyDefaultCharPref (in string pref); */
  NS_IMETHOD CopyDefaultCharPref(const char *pref, char **_retval) = 0;

  /* voidStar CopyDefaultBinaryPref (in string pref, out long size); */
  NS_IMETHOD CopyDefaultBinaryPref(const char *pref, PRInt32 *size, void * *_retval) = 0;

  /* nsFileSpec GetFilePref (in string pref); */
  NS_IMETHOD GetFilePref(const char *pref, nsFileSpec * *_retval) = 0;

  /* void SetFilePref (in string pref, in nsFileSpec value, in boolean setDefault); */
  NS_IMETHOD SetFilePref(const char *pref, nsFileSpec * value, PRBool setDefault) = 0;

  /* boolean PrefIsLocked (in string pref); */
  NS_IMETHOD PrefIsLocked(const char *pref, PRBool *_retval) = 0;

  /* void SavePrefFile (); */
  NS_IMETHOD SavePrefFile() = 0;

  /* void RegisterCallback (in string domain, in PrefChangedFunc callback, in voidStar closure); */
  NS_IMETHOD RegisterCallback(const char *domain, PrefChangedFunc callback, void * closure) = 0;

  /* void UnregisterCallback (in string domain, in PrefChangedFunc callback, in voidStar closure); */
  NS_IMETHOD UnregisterCallback(const char *domain, PrefChangedFunc callback, void * closure) = 0;

  /* void CopyPrefsTree (in string srcRoot, in string destRoot); */
  NS_IMETHOD CopyPrefsTree(const char *srcRoot, const char *destRoot) = 0;

  /* void DeleteBranch (in string branchName); */
  NS_IMETHOD DeleteBranch(const char *branchName) = 0;
};

#endif /* __gen_nsIPref_h__ */
