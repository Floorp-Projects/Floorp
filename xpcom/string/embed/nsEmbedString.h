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

#ifndef nsEmbedString_h__
#define nsEmbedString_h__

#include "nsAString.h"

class nsEmbedString : public nsAString
{
  public:
    typedef nsEmbedString self_type;
  
    nsEmbedString();
    nsEmbedString(const self_type& aString);
    explicit nsEmbedString(const abstract_string_type&);
    explicit nsEmbedString(const char_type*);
    nsEmbedString(const char_type*, size_type);
    
    virtual ~nsEmbedString();
    
    virtual const char_type* get()  const { return mStr; }
    virtual     size_type  Length() const { return mLength; }
    
    void SetLength(size_type aLength);
    
    void SetCapacity(size_type aNewCapacity);
    
    nsEmbedString& operator=(const self_type& aString)              { Assign(aString); return *this; }
    nsEmbedString& operator=(const abstract_string_type& aReadable) { Assign(aReadable); return *this; }
    nsEmbedString& operator=(const char_type* aPtr)                 { Assign(aPtr); return *this; }
    nsEmbedString& operator=(char_type aChar)                       { Assign(aChar); return *this; }
    
  protected:
    void Init();
    void Destroy();

    void Free();
    PRBool EnsureCapacity(size_type);
    PRBool GrowCapacity(size_type);
    virtual PRBool OwnsBuffer() const;
    void AddNullTerminator() { mStr[mLength] = 0; }
    PRBool Realloc(size_type);

    virtual const buffer_handle_type* GetFlatBufferHandle() const;

    virtual const char_type* GetReadableFragment(const_fragment_type&, nsFragmentRequest, index_type) const;
    virtual       char_type* GetWritableFragment(fragment_type&, nsFragmentRequest, index_type);

  private:
    // NOT TO BE IMPLEMENTED
    //  these signatures help clients not accidentally call the wrong thing helped by C++ automatic integral promotion
    void operator=(incompatible_char_type);
    
  protected:
    char_type* mStr;
    size_type  mLength;
    size_type  mCapacity;
};

class nsEmbedCString : public nsACString
{
  public:
    typedef nsEmbedCString self_type;
  
    nsEmbedCString();
    nsEmbedCString(const self_type& aString);
    explicit nsEmbedCString(const abstract_string_type&);
    explicit nsEmbedCString(const char_type*);
    nsEmbedCString(const char_type*, size_type);
    
    virtual ~nsEmbedCString();
    
    virtual const char_type* get()  const { return mStr; }
    virtual     size_type  Length() const { return mLength; }
    
    void SetLength(size_type aLength);
    
    void SetCapacity(size_type aNewCapacity);
    
    nsEmbedCString& operator=(const self_type& aString)              { Assign(aString); return *this; }
    nsEmbedCString& operator=(const abstract_string_type& aReadable) { Assign(aReadable); return *this; }
    nsEmbedCString& operator=(const char_type* aPtr)                 { Assign(aPtr); return *this; }
    nsEmbedCString& operator=(char_type aChar)                       { Assign(aChar); return *this; }
    
  protected:
    void Init();
    void Destroy();
    
    void Free();
    PRBool EnsureCapacity(size_type);
    PRBool GrowCapacity(size_type);
    virtual PRBool OwnsBuffer() const;
    void AddNullTerminator() { mStr[mLength] = 0; }
    PRBool Realloc(size_type);
    
    virtual const buffer_handle_type* GetFlatBufferHandle() const;
    
    virtual const char_type* GetReadableFragment(const_fragment_type&, nsFragmentRequest, index_type) const;
    virtual       char_type* GetWritableFragment(fragment_type&, nsFragmentRequest, index_type);
    
  private:
    // NOT TO BE IMPLEMENTED
    //  these signatures help clients not accidentally call the wrong thing helped by C++ automatic integral promotion
    void operator=(incompatible_char_type);
    
  protected:
    char_type* mStr;
    size_type  mLength;
    size_type  mCapacity;
};

#endif // !nsEmbedString_h__
