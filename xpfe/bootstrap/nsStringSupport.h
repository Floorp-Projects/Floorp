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
 * Portions created by IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
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

#ifndef nsStringSupport_h__
#define nsStringSupport_h__

/**
 * This file attempts to implement the subset of string classes and methods
 * used by xpfe/bootstrap in terms of the frozen string API.
 *
 * This code exists to allow xpfe/bootstrap to be optionally compiled as a
 * standalone embedder of the GRE.  However, since the module can also be built
 * statically or as a non-GRE application, it is necessary to support compiling
 * it directly against the internal xpcom string classes.  Hence, this layer of
 * indirection ;-)
 */

#include "nsEmbedString.h"
#include "nsMemory.h"
#include "prprf.h"
#include "plstr.h"

// rename class names that are redefined here
#define nsCString                     nsCString_local
#define nsCAutoString                 nsCAutoString_local
#define nsDependentCString            nsDependentCString_local
#define nsXPIDLCString                nsXPIDLCString_local
#define nsCGetterCopies               nsCGetterCopies_local
#define nsString                      nsString_local
#define nsAutoString                  nsAutoString_local
#define nsDependentString             nsDependentString_local
#define nsXPIDLString                 nsXPIDLString_local
#define nsGetterCopies                nsGetterCopies_local
#define NS_ConvertUCS2toUTF8          NS_ConvertUCS2toUTF8_local
#define NS_LossyConvertUCS2toASCII    NS_LossyConvertUCS2toASCII_local
#define getter_Copies                 getter_Copies_local

#define kNotFound -1

class nsCString : public nsEmbedCString
  {
    public:
      nsCString() {}
      nsCString(const char *s)
        {
          Assign(s);
        }
      void AppendInt(PRInt32 value)
        {
          char buf[32];
          PR_snprintf(buf, sizeof(buf), "%d", value);
          Append(buf);
        }
      PRBool IsEmpty()
        {
          return Length() == 0;
        }
      PRInt32 FindChar(char c, PRUint32 offset = 0)
        {
          NS_ASSERTION(offset <= Length(), "invalid offset");
          const char *data = get() + offset;
          for (const char *p = data; *p; ++p)
            if (*p == c)
              return p - data;
          return kNotFound;
        }
      PRInt32 Find(const char *needle, PRBool ignoreCase = PR_FALSE)
        {
          const char *data = get(), *p;
          if (ignoreCase)
            p = PL_strcasestr(data, needle);
          else
            p = PL_strstr(data, needle);
          return p ? p - data : kNotFound;
        }
      PRBool Equals(const char *s)
        {
          return strcmp(get(), s) == 0;
        }
  };

class nsDependentCString : public nsCString
  {
    public:
      nsDependentCString(const char *data)
        {
          Assign(data); // XXX forced to copy
        }
      nsDependentCString(const char *data, PRUint32 len)
        {
          Assign(data, len); // XXX forced to copy
        }
  };

class nsCAutoString : public nsCString
  {
    public:
      nsCAutoString() {}
      nsCAutoString(const char *data)
        {
          Assign(data); 
        }
      nsCAutoString(const nsCString &s)
        {
          Assign(s);
        }
  };

class nsString : public nsEmbedString
  {
    public:
      nsString() {}
      PRBool IsEmpty()
        {
          return Length() == 0;
        }
      PRInt32 FindChar(PRUnichar c, PRUint32 offset = 0)
        {
          NS_ASSERTION(offset <= Length(), "invalid offset");
          const PRUnichar *data = get() + offset;
          for (const PRUnichar *p = data; *p; ++p)
            if (*p == c)
              return p - data;
          return kNotFound;
        }
  };

class nsDependentString : public nsString
  {
    public:
      nsDependentString(const PRUnichar *data)
        {
          Assign(data); // XXX forced to copy
        }
  };

class nsAutoString : public nsString
  {
    public:
      void AssignWithConversion(const char *data)
        {
          AssignLiteral(data);
        }
      void AssignLiteral(const char *data)
        {
          NS_CStringToUTF16(nsEmbedCString(data), NS_CSTRING_ENCODING_ASCII, *this);
        }
      nsAutoString &operator=(const PRUnichar *s)
        {
          Assign(s);
          return *this;
        }
  };

class nsXPIDLCString : public nsCString
  {
    public:
      nsXPIDLCString() : mVoided(PR_TRUE) {}

      const char *get() const
        { 
          return mVoided ? nsnull : nsEmbedCString::get();
        }
      operator const char*() const
        {
          return get();
        }

      void Adopt(char *data)
        {
          if (data)
            {
              // XXX unfortunately, we don't have a better way to do this.
              Assign(data);
              nsMemory::Free(data);
              mVoided = PR_FALSE;
            }
          else
            {
              Cut(0, PR_UINT32_MAX);
              mVoided = PR_TRUE;
            }
        }
        
    private:
      PRBool mVoided;
  };

class nsCGetterCopies
  {
    public:
      typedef char char_type;

      nsCGetterCopies(nsXPIDLCString& str)
        : mString(str), mData(nsnull) {}

      ~nsCGetterCopies()
        {
          mString.Adopt(mData); // OK if mData is null
        }

      operator char_type**()
        {
          return &mData;
        }

    private:
      nsXPIDLCString& mString;
      char_type*      mData;
  };

inline
nsCGetterCopies
getter_Copies( nsXPIDLCString& aString )
  {
    return nsCGetterCopies(aString);
  }

class nsXPIDLString : public nsString
  {
    public:
      nsXPIDLString() : mVoided(PR_TRUE) {}

      const PRUnichar *get() const
        { 
          return mVoided ? nsnull : nsEmbedString::get();
        }
      operator const PRUnichar*() const
        {
          return get();
        }

      void Adopt(PRUnichar *data)
        {
          if (data)
            {
              // XXX unfortunately, we don't have a better way to do this.
              Assign(data);
              nsMemory::Free(data);
              mVoided = PR_FALSE;
            }
          else
            {
              Cut(0, PR_UINT32_MAX);
              mVoided = PR_TRUE;
            }
        }
        
    private:
      PRBool mVoided;
  };

class nsGetterCopies
  {
    public:
      typedef PRUnichar char_type;

      nsGetterCopies(nsXPIDLString& str)
        : mString(str), mData(nsnull) {}

      ~nsGetterCopies()
        {
          mString.Adopt(mData); // OK if mData is null
        }

      operator char_type**()
        {
          return &mData;
        }

    private:
      nsXPIDLString& mString;
      char_type*     mData;
  };

inline
nsGetterCopies
getter_Copies( nsXPIDLString& aString )
  {
    return nsGetterCopies(aString);
  }

class NS_ConvertUCS2toUTF8 : public nsCString
  {
    public:
      NS_ConvertUCS2toUTF8(const nsAString &str)
        {
          NS_UTF16ToCString(str, NS_CSTRING_ENCODING_UTF8, *this);
        }
  };

class NS_LossyConvertUCS2toASCII : public nsCString
  {
    public:
      NS_LossyConvertUCS2toASCII(const nsAString &str)
        {
          NS_UTF16ToCString(str, NS_CSTRING_ENCODING_ASCII, *this);
        }
  };

#define NS_LITERAL_CSTRING(s) nsDependentCString(s)

#ifdef HAVE_CPP_2BYTE_WCHAR_T
#define NS_LITERAL_STRING(s) nsDependentString(L##s)
#else
#error "need implementation of NS_LITERAL_STRING"
#endif

#define EmptyCString() nsCString()
#define EmptyString() nsString()

#endif // !nsStringSupport_h__
