/* vim:set ts=2 sw=2 et cindent: */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation.  All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#ifndef nsStringAPI_h__
#define nsStringAPI_h__

/**
 * nsStringAPI.h
 *
 * This file describes a minimal API for working with XPCOM's abstract
 * string classes.  It divorces the consumer from having any run-time
 * dependency on the implementation details of the abstract string types.
 */

#include "nscore.h"
#define NS_STRINGAPI(x) extern "C" NS_COM x

/* ------------------------------------------------------------------------- */

/**
 * These are dummy class definitions for the abstract string types used in
 * XPIDL generated header files.  Do not count on the structure of these
 * classes, and do not try to mix these definitions with the internal 
 * definition of these classes used within the mozilla codebase.
 */

#ifndef nsAString_external
#define nsAString_external nsAString
#endif

class nsAString_external
{
private:
  void *v;
};

#ifndef nsACString_external
#define nsACString_external nsACString
#endif

class nsACString_external
{
private:
  void *v;
};

/* ------------------------------------------------------------------------- */

/**
 * nsStringContainer
 *
 * This is an opaque data type that is large enough to hold the canonical 
 * implementation of nsAString.  The binary structure of this class is an
 * implementation detail.
 *
 * The string data stored in a string container is always single fragment
 * and null-terminated.
 *
 * Typically, string containers are allocated on the stack for temporary
 * use.  However, they can also be malloc'd if necessary.  In either case,
 * a string container is not useful until it has been initialized with a
 * call to NS_StringContainerInit.  The following example shows how to use
 * a string container to call a function that takes a |nsAString &| out-param.
 *
 *   NS_METHOD GetBlah(nsAString &aBlah);
 *
 *   void MyCode()
 *   {
 *     nsStringContainer sc;
 *     if (NS_StringContainerInit(sc))
 *     {
 *       nsresult rv = GetBlah(sc);
 *       if (NS_SUCCEEDED(rv))
 *       {
 *         const PRUnichar *data = NS_StringGetDataPtr(sc);
 *         //
 *         // |data| now points to the result of the GetBlah function
 *         //
 *       }
 *       NS_StringContainerFinish(sc);
 *     }
 *   }
 *
 * The following example show how to use a string container to pass a string
 * parameter to a function taking a |const nsAString &| in-param.
 *
 *   NS_METHOD SetBlah(const nsAString &aBlah);
 *
 *   void MyCode()
 *   {
 *     nsStringContainer sc;
 *     if (NS_StringContainerInit(sc))
 *     {
 *       const PRUnichar kData[] = {'x','y','z','\0'};
 *       NS_StringSetData(sc, kData, sizeof(kData)-1);
 *
 *       SetBlah(sc);
 *
 *       NS_StringContainerFinish(sc);
 *     }
 *   }
 */
class nsStringContainer : public nsAString_external
{
private:
  void     *d1;
  PRUint32  d2;
  void     *d3;

public:
  nsStringContainer() {} // MSVC6 needs this
};

/**
 * NS_StringContainerInit
 *
 * @param aContainer    string container reference
 * @return              true if string container successfully initialized
 *
 * This function may allocate additional memory for aContainer.  When
 * aContainer is no longer needed, NS_StringContainerFinish should be called.
 */
NS_STRINGAPI(PRBool)
NS_StringContainerInit(nsStringContainer &aContainer);

/**
 * NS_StringContainerFinish
 * 
 * @param aContainer    string container reference
 *
 * This function frees any memory owned by aContainer.
 */
NS_STRINGAPI(void)
NS_StringContainerFinish(nsStringContainer &aContainer);

/* ------------------------------------------------------------------------- */

/**
 * NS_StringGetData
 *
 * This function returns a const character pointer to the string's internal
 * buffer, the length of the string, and a boolean value indicating whether
 * or not the buffer is null-terminated.
 *
 * @param aStr          abstract string reference
 * @param aData         out param that will hold the address of aStr's
 *                      internal buffer
 * @param aTerminated   if non-null, this out param will be set to indicate
 *                      whether or not aStr's internal buffer is null-
 *                      terminated
 * @return              length of aStr's internal buffer
 */
NS_STRINGAPI(PRUint32)
NS_StringGetData
  (const nsAString &aStr, const PRUnichar **aData,
   PRBool *aTerminated = nsnull);

/**
 * NS_StringSetData
 *
 * This function copies aData into aStr.
 *
 * @param aStr          abstract string reference
 * @param aData         character buffer
 * @param aDataLength   number of characters to copy from source string (pass
 *                      PR_UINT32_MAX to copy until end of aData, designated by
 *                      a null character)
 *
 * This function does not necessarily null-terminate aStr after copying data
 * from aData.  The behavior depends on the implementation of the abstract 
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 */
NS_STRINGAPI(void)
NS_StringSetData
  (nsAString &aStr, const PRUnichar *aData,
   PRUint32 aDataLength = PR_UINT32_MAX);

/**
 * NS_StringSetDataRange
 *
 * This function copies aData into a section of aStr.  As a result it can be
 * used to insert new characters into the string.
 *
 * @param aStr          abstract string reference
 * @param aCutOffset    starting index where the string's existing data
 *                      is to be overwritten (pass PR_UINT32_MAX to cause
 *                      aData to be appended to the end of aStr, in which
 *                      case the value of aCutLength is ignored).
 * @param aCutLength    number of characters to overwrite starting at
 *                      aCutOffset (pass PR_UINT32_MAX to overwrite until the
 *                      end of aStr).
 * @param aData         character buffer (pass null to cause this function
 *                      to simply remove the "cut" range)
 * @param aDataLength   number of characters to copy from source string (pass
 *                      PR_UINT32_MAX to copy until end of aData, designated by
 *                      a null character)
 *
 * This function does not necessarily null-terminate aStr after copying data
 * from aData.  The behavior depends on the implementation of the abstract 
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 */
NS_STRINGAPI(void)
NS_StringSetDataRange
  (nsAString &aStr, PRUint32 aCutOffset, PRUint32 aCutLength,
   const PRUnichar *aData, PRUint32 aDataLength = PR_UINT32_MAX);

/**
 * NS_StringCopy
 *
 * This function makes aDestStr have the same value as aSrcStr.  It is
 * provided as an optimization.
 *
 * @param aDestStr      abstract string reference to be modified
 * @param aSrcStr       abstract string reference containing source string
 *
 * This function does not necessarily null-terminate aDestStr after copying
 * data from aSrcStr.  The behavior depends on the implementation of the
 * abstract string, aDestStr.  If aDestStr is a reference to a
 * nsStringContainer, then its data will be null-terminated by this function.
 */
NS_STRINGAPI(void)
NS_StringCopy
  (nsAString &aDestStr, const nsAString &aSrcStr);

/**
 * NS_StringAppendData
 *
 * This function appends data to the existing value of aStr.
 *
 * @param aStr          abstract string reference to be modified
 * @param aData         character buffer
 * @param aDataLength   number of characters to append (pass PR_UINT32_MAX to
 *                      append until a null-character is encountered)
 *
 * This function does not necessarily null-terminate aStr upon completion.
 * The behavior depends on the implementation of the abstract string, aStr.
 * If aStr is a reference to a nsStringContainer, then its data will be null-
 * terminated by this function.
 */
inline void
NS_StringAppendData(nsAString &aStr, const PRUnichar *aData,
                    PRUint32 aDataLength = PR_UINT32_MAX)
{
  NS_StringSetDataRange(aStr, PR_UINT32_MAX, 0, aData, aDataLength);
}

/**
 * NS_StringInsertData
 *
 * This function inserts data into the existing value of aStr at the specified
 * offset.
 *
 * @param aStr          abstract string reference to be modified
 * @param aOffset       specifies where in the string to insert aData
 * @param aData         character buffer
 * @param aDataLength   number of characters to append (pass PR_UINT32_MAX to
 *                      append until a null-character is encountered)
 *
 * This function does not necessarily null-terminate aStr upon completion.
 * The behavior depends on the implementation of the abstract string, aStr.
 * If aStr is a reference to a nsStringContainer, then its data will be null-
 * terminated by this function.
 */
inline void
NS_StringInsertData(nsAString &aStr, PRUint32 aOffset, const PRUnichar *aData,
                    PRUint32 aDataLength = PR_UINT32_MAX)
{
  NS_StringSetDataRange(aStr, aOffset, 0, aData, aDataLength);
}
      
/**
 * NS_StringCutData
 *
 * This function shortens the existing value of aStr, by removing characters
 * at the specified offset.
 *
 * @param aStr          abstract string reference to be modified
 * @param aCutOffset    specifies where in the string to insert aData
 * @param aCutLength    number of characters to remove
 */
inline void
NS_StringCutData(nsAString &aStr, PRUint32 aCutOffset, PRUint32 aCutLength)
{
  NS_StringSetDataRange(aStr, aCutOffset, aCutLength, nsnull, 0);
}

/* ------------------------------------------------------------------------- */

/**
 * nsCStringContainer
 *
 * This is an opaque data type that is large enough to hold the canonical 
 * implementation of nsACString.  The binary structure of this class is an
 * implementation detail.
 *
 * The string data stored in a string container is always single fragment
 * and null-terminated.
 *
 * @see nsStringContainer for use cases and further documentation.
 */
class nsCStringContainer : public nsACString_external
{
private:
  void    *d1;
  PRUint32 d2;
  void    *d3;

public:
  nsCStringContainer() {} // MSVC6 needs this
};

/**
 * NS_CStringContainerInit
 *
 * @param aContainer    string container reference
 * @return              true if string container successfully initialized
 *
 * This function may allocate additional memory for aContainer.  When
 * aContainer is no longer needed, NS_CStringContainerFinish should be called.
 */
NS_STRINGAPI(PRBool)
NS_CStringContainerInit(nsCStringContainer &aContainer);

/**
 * NS_CStringContainerFinish
 * 
 * @param aContainer    string container reference
 *
 * This function frees any memory owned by aContainer.
 */
NS_STRINGAPI(void)
NS_CStringContainerFinish(nsCStringContainer &aContainer);

/* ------------------------------------------------------------------------- */

/**
 * NS_CStringGetData
 *
 * This function returns a const character pointer to the string's internal
 * buffer, the length of the string, and a boolean value indicating whether
 * or not the buffer is null-terminated.
 *
 * @param aStr          abstract string reference
 * @param aData         out param that will hold the address of aStr's
 *                      internal buffer
 * @param aTerminated   if non-null, this out param will be set to indicate
 *                      whether or not aStr's internal buffer is null-
 *                      terminated
 * @return              length of aStr's internal buffer
 */
NS_STRINGAPI(PRUint32)
NS_CStringGetData
  (const nsACString &aStr, const char **aData,
   PRBool *aTerminated = nsnull);

/**
 * NS_CStringSetData
 *
 * This function copies aData into aStr.
 *
 * @param aStr          abstract string reference
 * @param aData         character buffer
 * @param aDataLength   number of characters to copy from source string (pass
 *                      PR_UINT32_MAX to copy until end of aData, designated by
 *                      a null character)
 *
 * This function does not necessarily null-terminate aStr after copying data
 * from aData.  The behavior depends on the implementation of the abstract 
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 */
NS_STRINGAPI(void)
NS_CStringSetData
  (nsACString &aStr, const char *aData,
   PRUint32 aDataLength = PR_UINT32_MAX);

/**
 * NS_CStringSetDataRange
 *
 * This function copies aData into a section of aStr.  As a result it can be
 * used to insert new characters into the string.
 *
 * @param aStr          abstract string reference
 * @param aCutOffset    starting index where the string's existing data
 *                      is to be overwritten (pass PR_UINT32_MAX to cause
 *                      aData to be appended to the end of aStr, in which
 *                      case the value of aCutLength is ignored).
 * @param aCutLength    number of characters to overwrite starting at
 *                      aCutOffset (pass PR_UINT32_MAX to overwrite until the
 *                      end of aStr).
 * @param aData         character buffer (pass null to cause this function
 *                      to simply remove the "cut" range)
 * @param aDataLength   number of characters to copy from source string (pass
 *                      PR_UINT32_MAX to copy until end of aData, designated by
 *                      a null character)
 *
 * This function does not necessarily null-terminate aStr after copying data
 * from aData.  The behavior depends on the implementation of the abstract 
 * string, aStr.  If aStr is a reference to a nsStringContainer, then its data
 * will be null-terminated by this function.
 */
NS_STRINGAPI(void)
NS_CStringSetDataRange
  (nsACString &aStr, PRUint32 aCutOffset, PRUint32 aCutLength,
   const char *aData, PRUint32 aDataLength = PR_UINT32_MAX);

/**
 * NS_CStringCopy
 *
 * This function makes aDestStr have the same value as aSrcStr.  It is
 * provided as an optimization.
 *
 * @param aDestStr      abstract string reference to be modified
 * @param aSrcStr       abstract string reference containing source string
 *
 * This function does not necessarily null-terminate aDestStr after copying
 * data from aSrcStr.  The behavior depends on the implementation of the
 * abstract string, aDestStr.  If aDestStr is a reference to a
 * nsStringContainer, then its data will be null-terminated by this function.
 */
NS_STRINGAPI(void)
NS_CStringCopy
  (nsACString &aDestStr, const nsACString &aSrcStr);

/**
 * NS_CStringAppendData
 *
 * This function appends data to the existing value of aStr.
 *
 * @param aStr          abstract string reference to be modified
 * @param aData         character buffer
 * @param aDataLength   number of characters to append (pass PR_UINT32_MAX to
 *                      append until a null-character is encountered)
 *
 * This function does not necessarily null-terminate aStr upon completion.
 * The behavior depends on the implementation of the abstract string, aStr.
 * If aStr is a reference to a nsStringContainer, then its data will be null-
 * terminated by this function.
 */
inline void
NS_CStringAppendData(nsACString &aStr, const char *aData,
                    PRUint32 aDataLength = PR_UINT32_MAX)
{
  NS_CStringSetDataRange(aStr, PR_UINT32_MAX, 0, aData, aDataLength);
}

/**
 * NS_CStringInsertData
 *
 * This function inserts data into the existing value of aStr at the specified
 * offset.
 *
 * @param aStr          abstract string reference to be modified
 * @param aOffset       specifies where in the string to insert aData
 * @param aData         character buffer
 * @param aDataLength   number of characters to append (pass PR_UINT32_MAX to
 *                      append until a null-character is encountered)
 *
 * This function does not necessarily null-terminate aStr upon completion.
 * The behavior depends on the implementation of the abstract string, aStr.
 * If aStr is a reference to a nsStringContainer, then its data will be null-
 * terminated by this function.
 */
inline void
NS_CStringInsertData(nsACString &aStr, PRUint32 aOffset, const char *aData,
                    PRUint32 aDataLength = PR_UINT32_MAX)
{
  NS_CStringSetDataRange(aStr, aOffset, 0, aData, aDataLength);
}
      
/**
 * NS_CStringCutData
 *
 * This function shortens the existing value of aStr, by removing characters
 * at the specified offset.
 *
 * @param aStr          abstract string reference to be modified
 * @param aCutOffset    specifies where in the string to insert aData
 * @param aCutLength    number of characters to remove
 */
inline void
NS_CStringCutData(nsACString &aStr, PRUint32 aCutOffset, PRUint32 aCutLength)
{
  NS_CStringSetDataRange(aStr, aCutOffset, aCutLength, nsnull, 0);
}

#endif // nsStringAPI_h__
