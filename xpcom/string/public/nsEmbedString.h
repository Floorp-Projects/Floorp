/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is a small implementation of the nsAString and nsACString.
 *
 * The Initial Developer of the Original Code is
 *   Peter Annema <jaggernaut@netscape.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
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

#ifndef nsEmbedString_h___
#define nsEmbedString_h___

#include "nsStringAPI.h"

class nsEmbedString : public nsStringContainer
  {
    public:
      typedef PRUnichar        char_type;
      typedef nsEmbedString    self_type;
      typedef nsAString        abstract_string_type;
      typedef PRUint32         size_type;
      typedef PRUint32         index_type;
    
      nsEmbedString()
        {
          NS_StringContainerInit(*this);
        }

      nsEmbedString(const self_type& aString)
        {
          NS_StringContainerInit(*this);
          NS_StringCopy(*this, aString);
        }

      explicit
      nsEmbedString(const abstract_string_type& aReadable)
        {
          NS_StringContainerInit(*this);
          NS_StringCopy(*this, aReadable);
        }

      explicit
      nsEmbedString(const char_type* aData, size_type aLength = PR_UINT32_MAX)
        {
          NS_StringContainerInit(*this);
          NS_StringSetData(*this, aData, aLength);
        }
      
      ~nsEmbedString()
        {
          NS_StringContainerFinish(*this);
        }

      const char_type* get() const
        {
          const char_type* data;
          NS_StringGetData(*this, &data);
          return data;
        }

      size_type Length() const
        {
          const char_type* data;
          return NS_StringGetData(*this, &data);
        }

      void Assign(const self_type& aString)
        {
          NS_StringCopy(*this, aString);
        }
      void Assign(const abstract_string_type& aReadable)
        {
          NS_StringCopy(*this, aReadable);
        }
      void Assign(const char_type* aData, size_type aLength = PR_UINT32_MAX)
        {
          NS_StringSetData(*this, aData, aLength);
        }
      void Assign(char_type aChar)
        {
          NS_StringSetData(*this, &aChar, 1);
        }
      
      self_type& operator=(const self_type& aString)              { Assign(aString);   return *this; }
      self_type& operator=(const abstract_string_type& aReadable) { Assign(aReadable); return *this; }
      self_type& operator=(const char_type* aPtr)                 { Assign(aPtr);      return *this; }
      self_type& operator=(char_type aChar)                       { Assign(aChar);     return *this; }

      void Replace( index_type cutStart, size_type cutLength, const char_type* data, size_type length = size_type(-1) )
        {
          NS_StringSetDataRange(*this, cutStart, cutLength, data, length);
        }
      void Replace( index_type cutStart, size_type cutLength, char_type c )
        {
          Replace(cutStart, cutLength, &c, 1);
        }
      void Replace( index_type cutStart, size_type cutLength, const abstract_string_type& readable )
        {
          const char_type* data;
          PRUint32 dataLen = NS_StringGetData(readable, &data);
          NS_StringSetDataRange(*this, cutStart, cutLength, data, dataLen);
        }

      void Append( char_type c )                                                                 { Replace(size_type(-1), 0, c); }
      void Append( const char_type* data, size_type length = size_type(-1) )                     { Replace(size_type(-1), 0, data, length); }
      void Append( const abstract_string_type& readable )                                        { Replace(size_type(-1), 0, readable); }

      self_type& operator+=( char_type c )                                                       { Append(c);        return *this; }
      self_type& operator+=( const char_type* data )                                             { Append(data);     return *this; }
      self_type& operator+=( const abstract_string_type& readable )                              { Append(readable); return *this; }

      void Insert( char_type c, index_type pos )                                                 { Replace(pos, 0, c); }
      void Insert( const char_type* data, index_type pos, size_type length = size_type(-1) )     { Replace(pos, 0, data, length); }
      void Insert( const abstract_string_type& readable, index_type pos )                        { Replace(pos, 0, readable); }

      void Cut( index_type cutStart, size_type cutLength )                                       { Replace(cutStart, cutLength, nsnull, 0); }
  };

class nsEmbedCString : public nsCStringContainer
  {
    public:
      typedef char             char_type;
      typedef nsEmbedCString   self_type;
      typedef nsACString       abstract_string_type;
      typedef PRUint32         size_type;
      typedef PRUint32         index_type;
    
      nsEmbedCString()
        {
          NS_CStringContainerInit(*this);
        }

      nsEmbedCString(const self_type& aString)
        {
          NS_CStringContainerInit(*this);
          NS_CStringCopy(*this, aString);
        }

      explicit
      nsEmbedCString(const abstract_string_type& aReadable)
        {
          NS_CStringContainerInit(*this);
          NS_CStringCopy(*this, aReadable);
        }

      explicit
      nsEmbedCString(const char_type* aData, size_type aLength = PR_UINT32_MAX)
        {
          NS_CStringContainerInit(*this);
          NS_CStringSetData(*this, aData, aLength);
        }
      
      ~nsEmbedCString()
        {
          NS_CStringContainerFinish(*this);
        }

      const char_type* get() const
        {
          const char_type* data;
          NS_CStringGetData(*this, &data);
          return data;
        }

      size_type Length() const
        {
          const char_type* data;
          return NS_CStringGetData(*this, &data);
        }

      void Assign(const self_type& aString)
        {
          NS_CStringCopy(*this, aString);
        }
      void Assign(const abstract_string_type& aReadable)
        {
          NS_CStringCopy(*this, aReadable);
        }
      void Assign(const char_type* aData, size_type aLength = PR_UINT32_MAX)
        {
          NS_CStringSetData(*this, aData, aLength);
        }
      void Assign(char_type aChar)
        {
          NS_CStringSetData(*this, &aChar, 1);
        }
      
      self_type& operator=(const self_type& aString)              { Assign(aString);   return *this; }
      self_type& operator=(const abstract_string_type& aReadable) { Assign(aReadable); return *this; }
      self_type& operator=(const char_type* aPtr)                 { Assign(aPtr);      return *this; }
      self_type& operator=(char_type aChar)                       { Assign(aChar);     return *this; }

      void Replace( index_type cutStart, size_type cutLength, const char_type* data, size_type length = size_type(-1) )
        {
          NS_CStringSetDataRange(*this, cutStart, cutLength, data, length);
        }
      void Replace( index_type cutStart, size_type cutLength, char_type c )
        {
          Replace(cutStart, cutLength, &c, 1);
        }
      void Replace( index_type cutStart, size_type cutLength, const abstract_string_type& readable )
        {
          const char_type* data;
          PRUint32 dataLen = NS_CStringGetData(readable, &data);
          NS_CStringSetDataRange(*this, cutStart, cutLength, data, dataLen);
        }

      void Append( char_type c )                                                                 { Replace(size_type(-1), 0, c); }
      void Append( const char_type* data, size_type length = size_type(-1) )                     { Replace(size_type(-1), 0, data, length); }
      void Append( const abstract_string_type& readable )                                        { Replace(size_type(-1), 0, readable); }

      self_type& operator+=( char_type c )                                                       { Append(c);        return *this; }
      self_type& operator+=( const char_type* data )                                             { Append(data);     return *this; }
      self_type& operator+=( const abstract_string_type& readable )                              { Append(readable); return *this; }

      void Insert( char_type c, index_type pos )                                                 { Replace(pos, 0, c); }
      void Insert( const char_type* data, index_type pos, size_type length = size_type(-1) )     { Replace(pos, 0, data, length); }
      void Insert( const abstract_string_type& readable, index_type pos )                        { Replace(pos, 0, readable); }

      void Cut( index_type cutStart, size_type cutLength )                                       { Replace(cutStart, cutLength, nsnull, 0); }
  };

#endif // !defined(nsEmbedString_h___)
