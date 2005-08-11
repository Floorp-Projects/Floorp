/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is C++ hashtable templates.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg.
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

#ifndef nsTHashtable_h__
#define nsTHashtable_h__

#include "nscore.h"
#include "pldhash.h"

#ifdef _MSC_VER
#pragma warning( disable: 4231 ) // 'extern template' nonstandard C++ extension
#endif

/**
 * a base class for templated hashtables.
 *
 * Clients will rarely need to use this class directly. Check the derived
 * classes first, to see if they will meet your needs.
 *
 * @param EntryType  the templated entry-type class that is managed by the
 *   hashtable. <code>EntryType</code> must extend the following declaration,
 *   and <strong>must not declare any virtual functions or derive from classes
 *   with virtual functions.</strong>  Any vtable pointer would break the
 *   PLDHashTable code.
 *<pre>   class EntryType : PLDHashEntryHdr
 *   {
 *   public: or friend nsTHashtable<EntryType>;
 *     // KeyType is what we use when Get()ing or Put()ing this entry
 *     // this should either be a simple datatype (PRUint32, nsISupports*) or
 *     // a const reference (const nsAString&)
 *     typedef something KeyType;
 *     // KeyTypePointer is the pointer-version of KeyType, because pldhash.h
 *     // requires keys to cast to <code>const void*</code>
 *     typedef const something* KeyTypePointer;
 *
 *     EntryType(KeyTypePointer aKey);
 *
 *     // the copy constructor must be defined, even if AllowMemMove() == true
 *     // or you will cause link errors!
 *     EntryType(const EntryType& aEnt);
 *
 *     // the destructor must be defined... or you will cause link errors!
 *     ~EntryType();
 *
 *     // return the key of this entry
 *     const KeyTypePointer GetKeyPointer() const;
 *
 *     // KeyEquals(): does this entry match this key?
 *     PRBool KeyEquals(KeyTypePointer aKey) const;
 *
 *     // KeyToPointer(): Convert KeyType to KeyTypePointer
 *     static KeyTypePointer KeyToPointer(KeyType aKey);
 *
 *     // HashKey(): calculate the hash number
 *     static PLDHashNumber HashKey(KeyTypePointer aKey);
 *
 *     // AllowMemMove(): can we move this class with memmove(), or do we have
 *     // to use the copy constructor?
 *     static PRBool AllowMemMove();
 *   }</pre>
 *
 * @see nsInterfaceHashtable
 * @see nsDataHashtable
 * @see nsClassHashtable
 * @author "Benjamin Smedberg <bsmedberg@covad.net>"
 */

template<class EntryType>
class nsTHashtable
{
public:
  /**
   * A dummy constructor; you must call Init() before using this class.
   */
  nsTHashtable();

  /**
   * destructor, cleans up and deallocates
   */
  ~nsTHashtable();

  /**
   * Initialize the class.  This function must be called before any other
   * class operations.  This can fail due to OOM conditions.
   * @param initSize the initial number of buckets in the hashtable, default 16
   * @return PR_TRUE if the class was initialized properly.
   */
  PRBool Init(PRUint32 initSize = PL_DHASH_MIN_SIZE);

  /**
   * KeyType is typedef'ed for ease of use.
   */
  typedef typename EntryType::KeyType KeyType;

  /**
   * KeyTypePointer is typedef'ed for ease of use.
   */
  typedef typename EntryType::KeyTypePointer KeyTypePointer;

  /**
   * Get the entry associated with a key.
   * @param     aKey the key to retrieve
   * @return    pointer to the entry class, if the key exists; nsnull if the
   *            key doesn't exist
   */
  EntryType* GetEntry(KeyTypePointer aKey) const;

  /**
   * Get the entry associated with a key, or create a new entry,
   * @param     aKey the key to retrieve
   * @return    pointer to the entry class retreived; nsnull only if memory
                can't be allocated
   */
  EntryType* PutEntry(KeyTypePointer aKey);

  /**
   * Remove the entry associated with a key.
   * @param     aKey of the entry to remove
   */
  void RemoveEntry(KeyTypePointer aKey);

  /**
   * client must provide an <code>Enumerator</code> function for
   *   EnumerateEntries
   * @param     aEntry the entry being enumerated
   * @param     userArg passed unchanged from <code>EnumerateEntries</code>
   * @return    combination of flags
   *            @link PLDHashOperator::PL_DHASH_NEXT PL_DHASH_NEXT @endlink ,
   *            @link PLDHashOperator::PL_DHASH_STOP PL_DHASH_STOP @endlink ,
   *            @link PLDHashOperator::PL_DHASH_REMOVE PL_DHASH_REMOVE @endlink
   */
  typedef PLDHashOperator (*Enumerator)(EntryType* aEntry, void* userArg);

  /**
   * Enumerate all the entries of the function.
   * @param     enumFunc the <code>Enumerator</code> function to call
   * @param     userArg a pointer to pass to the
   *            <code>Enumerator</code> function
   * @return    the number of entries actually enumerated
   */
  PRUint32 EnumerateEntries(Enumerator enumFunc, void* userArg);

  /**
   * remove all entries, return hashtable to "pristine" state ;)
   */
  void Clear();

protected:
  PLDHashTable mTable;

  static PLDHashTableOps sOps;

  static const void* s_GetKey(PLDHashTable    *table,
                              PLDHashEntryHdr *entry);

  static PLDHashNumber s_HashKey(PLDHashTable *table,
                                 const void   *key);
  
  static PRBool s_MatchEntry(PLDHashTable           *table,
                             const PLDHashEntryHdr  *entry,
                             const void             *key);
  
  static void s_CopyEntry(PLDHashTable          *table,
                          const PLDHashEntryHdr *from,
                          PLDHashEntryHdr       *to);
  
  static void s_ClearEntry(PLDHashTable *table,
                           PLDHashEntryHdr *entry);

  static void s_InitEntry(PLDHashTable     *table,
                          PLDHashEntryHdr  *entry,
                          const void       *key);

  /**
   * passed internally during enumeration.  Allocated on the stack.
   *
   * @param userFunc the Enumerator function passed to
   *   EnumerateEntries by the client
   * @param userArg the userArg passed unaltered
   */
  struct s_EnumArgs
  {
    Enumerator userFunc;
    void* userArg;
  };
  
  static PLDHashOperator s_EnumStub(PLDHashTable    *table,
                                    PLDHashEntryHdr *entry,
                                    PRUint32         number,
                                    void            *arg);
};

// helper function for Reset()
PLDHashOperator PL_DHashStubEnumRemove(PLDHashTable    *table,
                                       PLDHashEntryHdr *entry,
                                       PRUint32         ordinal,
                                       void            *userArg);

/**
 * if we can't declare external linkage for templates, we need to include the
 * implementation header.
 */
#ifndef HAVE_CPP_EXTERN_INSTANTIATION
#include "nsTHashtableImpl.h"
#endif

#endif // nsTHashtable_h__
