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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/**
 * nsDoubleHashtable.h is OBSOLETE. Use nsTHashtable or a derivative instead.
 */

#ifndef __nsDoubleHashtable_h__
#define __nsDoubleHashtable_h__

#include "pldhash.h"
#include "nscore.h"
#include "nsString.h"
#include "nsHashKeys.h"

/*
 * This file provides several major things to make PLDHashTable easier to use:
 * - hash class macros for you to define a hashtable
 * - default key classes to use as superclasses for your entries
 * - empty maps for string, cstring, int and void
 */

/*
 * USAGE
 *
 * To use nsDoubleHashtable macros
 * (1) Define an entry class
 * (2) Create the hash class
 * (3) Use the hash class
 *
 * EXAMPLE
 *
 * As an example, let's create a dictionary, a mapping from a string (the word)
 * to the pronunciation and definition of those words.
 *
 * (1) Define an entry class
 *
 * What we want here is an entry class that contains the word, the
 * pronunciation string, and the definition string.  Since we have a string key
 * we can use the standard PLDHashStringEntry class as our base, it will handle
 * the key stuff for us automatically.
 *
 * #include "nsDoubleHashtable.h"
 *
 * // Do NOT ADD VIRTUAL METHODS INTO YOUR ENTRY.  Everything will break.
 * // This is because of the 4-byte pointer C++ magically prepends onto your
 * // entry class.  It interacts very unhappily with PLDHashTable.
 * class DictionaryEntry : public PLDHashStringEntry {
 * public:
 *   DictionaryEntry(const void* aKey) : PLDHashStringEntry(aKey) { }
 *   ~DictionaryEntry() { }
 *   nsString mPronunciation;
 *   nsString mDefinition;
 * }
 *
 * (2) Create the hash class
 *
 * The final hash class you will use in step 3 is defined by 2 macros.
 *
 * DECL_DHASH_WRAPPER(Dictionary, DictionaryEntry, const nsAString&)
 * DHASH_WRAPPER(Dictionary, DictionaryEntry, const nsAString&)
 * 
 * (3) Use the hash class
 *
 * Here is a simple main() that might look up a string:
 *
 * int main(void) {
 *   Dictionary d;
 *   nsresult rv = d.Init(10);
 *   if (NS_FAILED(rv)) return 1;
 *
 *   // Put an entry
 *   DictionaryEntry* a = d.AddEntry(NS_LITERAL_STRING("doomed"));
 *   if (!a) return 1;
 *   a->mDefinition.AssignLiteral("The state you get in when a Mozilla release is pending");
 *   a->mPronunciation.AssignLiteral("doom-d");
 *
 *   // Get the entry
 *   DictionaryEntry* b = d.GetEntry(NS_LITERAL_STRING("doomed"));
 *   printf("doomed: %s\n", NS_ConvertUTF16toUTF8(b->mDefinition).get());
 *
 *   // Entries will be automagically cleaned up when the Dictionary object goes away
 *   return 0;
 * }
 *
 *
 * BONUS POINTS
 *
 * You may wish to extend this class and add helper functions like
 * nsDependentString* GetDefinition(nsAString& aWord).  For example:
 *
 * class MyDictionary : public Dictionary {
 * public:
 *   MyDictionary() { }
 *   // Make SURE you have a virtual destructor
 *   virtual ~myDictionary() { }
 *   nsDependentString* GetDefinition(const nsAString& aWord) {
 *     DictionaryEntry* e = GetEntry(aWord);
 *     if (e) {
 *       // We're returning an nsDependentString here, callers need to delete it
 *       // and it doesn't last long, but at least it doesn't create a copy
 *       return new nsDependentString(e->mDefinition.get());
 *     } else {
 *       return nsnull;
 *     }
 *   }
 *   nsresult PutDefinition(const nsAString& aWord,
 *                          const nsAString& aDefinition,
 *                          const nsAString& aPronunciation) {
 *     DictionaryEntry* e = AddEntry(aWord);
 *     if (!e) {
 *       return NS_ERROR_OUT_OF_MEMORY;
 *     }
 *     e->mDefinition = aDefinition;
 *     e->mPronunciation = aPronunciation;
 *     return NS_OK;
 *   }
 * }
 */

/*
 * ENTRY CLASS DEFINITION
 *
 * The simplifications of PLDHashTable all hinge upon the idea of an entry
 * class, which is a class you define, where you store the key and values that
 * you will place in each hash entry.  You must define six methods for an entry
 * (the standard key classes, which you can extend from, define all of these
 * for you except the constructor and destructor):
 *
 * CONSTRUCTOR(const void* aKey)
 * When your entry is constructed it will only be given a pointer to the key.
 *
 * DESTRUCTOR
 * Called when the entry is destroyed (of course).
 *
 * const void* GetKey()
 * Must return a pointer to the key
 *
 * PRBool MatchEntry(const void* aKey) - return true or false depending on
 *        whether the key pointed to by aKey matches this entry
 *
 * static PLDHashNumber HashKey(const void* aKey) - get a hashcode based on the
 *        key (must be the same every time for the same key, but does not have
 *        to be unique)
 *
 * For a small hash that just does key->value, you will often just extend a
 * standard key class and put a value member into it, and have a destructor and
 * constructor--nothing else necessary.
 *
 * See the default key entry classes as example entry classes.
 *
 * NOTES:
 * - Do NOT ADD VIRTUAL METHODS INTO YOUR ENTRY.  Everything will break.
 *   This is because of the 4-byte pointer C++ magically prepends onto your
 *   entry class.  It interacts very unhappily with PLDHashTable.
 */

/*
 * PRIVATE HASHTABLE MACROS
 *
 * These internal macros can be used to declare the callbacks for an entry
 * class, but the wrapper class macros call these for you so don't call them.
 */

//
// DHASH_CALLBACKS
//
// Define the hashtable callback functions.  Do this in one place only, as you
// will have redundant symbols otherwise.
//
// ENTRY_CLASS: the classname of the entry
//
#define DHASH_CALLBACKS(ENTRY_CLASS)                                          \
PR_STATIC_CALLBACK(PLDHashNumber)                                             \
ENTRY_CLASS##HashKey(PLDHashTable* table, const void* key)                    \
{                                                                             \
  return ENTRY_CLASS::HashKey(key);                                           \
}                                                                             \
PR_STATIC_CALLBACK(PRBool)                                                    \
ENTRY_CLASS##MatchEntry(PLDHashTable *table, const PLDHashEntryHdr *entry,    \
                        const void *key)                                      \
{                                                                             \
  const ENTRY_CLASS* e = NS_STATIC_CAST(const ENTRY_CLASS*, entry);           \
  return e->MatchEntry(key);                                                  \
}                                                                             \
PR_STATIC_CALLBACK(void)                                                      \
ENTRY_CLASS##ClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)          \
{                                                                             \
  ENTRY_CLASS* e = NS_STATIC_CAST(ENTRY_CLASS *, entry);                      \
  e->~ENTRY_CLASS();                                                          \
}                                                                             \
PR_STATIC_CALLBACK(PRBool)                                                    \
ENTRY_CLASS##InitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,           \
                       const void *key)                                       \
{                                                                             \
  new (entry) ENTRY_CLASS(key);                                               \
  return PR_TRUE;                                                             \
}

//
// DHASH_INIT
//
// Initialize hashtable to a certain class.
//
// HASHTABLE: the name of the PLDHashTable variable
// ENTRY_CLASS: the classname of the entry
// NUM_INITIAL_ENTRIES: the number of entry slots the hashtable should start
//                      with
// RV: an nsresult variable to hold the outcome of the initialization.
//     Will be NS_ERROR_OUT_OF_MEMORY if failed, NS_OK otherwise.
//
#define DHASH_INIT(HASHTABLE,ENTRY_CLASS,NUM_INITIAL_ENTRIES,RV)              \
PR_BEGIN_MACRO                                                                \
  static PLDHashTableOps hash_table_ops =                                     \
  {                                                                           \
    PL_DHashAllocTable,                                                       \
    PL_DHashFreeTable,                                                        \
    ENTRY_CLASS##HashKey,                                                     \
    ENTRY_CLASS##MatchEntry,                                                  \
    PL_DHashMoveEntryStub,                                                    \
    ENTRY_CLASS##ClearEntry,                                                  \
    PL_DHashFinalizeStub,                                                     \
    ENTRY_CLASS##InitEntry                                                    \
  };                                                                          \
  PRBool isLive = PL_DHashTableInit(&(HASHTABLE),                             \
                                    &hash_table_ops, nsnull,                  \
                                    sizeof(ENTRY_CLASS),                      \
                                    (NUM_INITIAL_ENTRIES));                   \
  if (!isLive) {                                                              \
    (HASHTABLE).ops = nsnull;                                                 \
    RV = NS_ERROR_OUT_OF_MEMORY;                                              \
  } else {                                                                    \
    RV = NS_OK;                                                               \
  }                                                                           \
PR_END_MACRO


/*
 * WRAPPER CLASS
 *
 * This class handles initialization and destruction of the hashtable
 * (you must call Init() yourself).  It defines these functions:
 *
 * Init(aNumInitialEntries)
 * Initialize the hashtable.  This must be called once, it is only separate
 * from the constructor so that you can get the return value.  You should pass
 * in the number of entries you think the hashtable will typically hold (this
 * will be the amount of space allocated initially so that it will not have to
 * grow).
 *
 * ENTRY_CLASS* GetEntry(aKey):
 * Get the entry referenced by aKey and return a pointer to it.  THIS IS A
 * TEMPORARY POINTER and is only guaranteed to exist until the next time you do
 * an operation on the hashtable.  But you can safely use it until then.
 *
 * Returns nsnull if the entry is not found.
 *
 * ENTRY_CLASS* AddEntry(aKey):
 * Create a new, empty entry and return a pointer to it for you to fill values
 * into.  THIS IS A TEMPORARY POINTER and is only guaranteed to exist until the
 * next time you do an operation on the hashtable.  But you can safely fill it
 * in.
 *
 * Returns nsnull if the entry cannot be created (usually a low memory
 * constraint).
 *
 * void Remove(aKey)
 * Remove the entry referenced by aKey.  If the entry does not exist, nothing
 * will happen.
 *
 *
 * DECL_DHASH_WRAPPER(CLASSNAME,ENTRY_CLASS,KEY_TYPE)
 *
 * Declare the hash class but do not define the functions.
 *
 * CLASSNAME: the name of the class to declare.
 * ENTRY_CLASS: the class of the entry struct.
 * KEY_TYPE: the name of the key type for GetEntry and AddEntry.
 *
 *
 * DHASH_WRAPPER(CLASSNAME,ENTRY_CLASS,KEY_TYPE)
 *
 * Define the functions for the hash class.
 *
 * CLASSNAME: the name of the class to declare.
 * ENTRY_CLASS: the class of the entry struct.
 * KEY_TYPE: the name of the key type for GetEntry and AddEntry.
 *
 *
 * CAVEATS:
 * - You may have only *one* wrapper class per entry class.
 */

#define DECL_DHASH_WRAPPER(CLASSNAME,ENTRY_CLASS,KEY_TYPE)                    \
class DHASH_EXPORT CLASSNAME {                                                \
public:                                                                       \
  CLASSNAME();                                                                \
  ~CLASSNAME();                                                               \
  nsresult Init(PRUint32 aNumInitialEntries);                                 \
  ENTRY_CLASS* GetEntry(const KEY_TYPE aKey);                                 \
  ENTRY_CLASS* AddEntry(const KEY_TYPE aKey);                                 \
  void Remove(const KEY_TYPE aKey);                                           \
  PLDHashTable mHashTable;                                                    \
};

#define DHASH_WRAPPER(CLASSNAME,ENTRY_CLASS,KEY_TYPE)                         \
DHASH_CALLBACKS(ENTRY_CLASS)                                                  \
CLASSNAME::CLASSNAME() {                                                      \
  mHashTable.ops = nsnull;                                                    \
}                                                                             \
CLASSNAME::~CLASSNAME() {                                                     \
  if (mHashTable.ops) {                                                       \
    PL_DHashTableFinish(&mHashTable);                                         \
  }                                                                           \
}                                                                             \
nsresult CLASSNAME::Init(PRUint32 aNumInitialEntries) {                       \
  if (!mHashTable.ops) {                                                      \
    nsresult rv;                                                              \
    DHASH_INIT(mHashTable,ENTRY_CLASS,aNumInitialEntries,rv);                 \
    return rv;                                                                \
  }                                                                           \
  return NS_OK;                                                               \
}                                                                             \
ENTRY_CLASS* CLASSNAME::GetEntry(const KEY_TYPE aKey) {                       \
  ENTRY_CLASS* e = NS_STATIC_CAST(ENTRY_CLASS*,                               \
                                  PL_DHashTableOperate(&mHashTable, &aKey,    \
                                                       PL_DHASH_LOOKUP));     \
  return PL_DHASH_ENTRY_IS_BUSY(e) ? e : nsnull;                              \
}                                                                             \
ENTRY_CLASS* CLASSNAME::AddEntry(const KEY_TYPE aKey) {                       \
  return NS_STATIC_CAST(ENTRY_CLASS*,                                         \
                        PL_DHashTableOperate(&mHashTable, &aKey,              \
                                             PL_DHASH_ADD));                  \
}                                                                             \
void CLASSNAME::Remove(const KEY_TYPE aKey) {                                 \
  PL_DHashTableOperate(&mHashTable, &aKey, PL_DHASH_REMOVE);                  \
}

/*
 * STANDARD KEY ENTRY CLASSES
 *
 * We have declared some standard key classes for you to make life a little
 * easier.  These include string, int and void* keys.  You can extend these
 * and add value data members to make a working hash entry class with your
 * values.
 *
 * PLDHashStringEntry:  nsAString
 * PLDHashCStringEntry: nsACString
 * PLDHashInt32Entry:   PRInt32
 * PLDHashVoidEntry:    void*
 *
 * As a short example, if you want to make a class that maps int to string,
 * you could do:
 *
 * class MyIntStringEntry : public PLDHashInt32Entry
 * {
 * public:
 *   MyIntStringEntry(const void* aKey) : PLDHashInt32Entry(aKey) { }
 *   ~MyIntStringEntry() { };
 *   nsString mMyStr;
 * };
 *
 * XXX It could be advisable (unless COW strings ever happens) to have a
 * PLDHashDependentStringEntry
 */

//
// String-key entry
//
class NS_COM PLDHashStringEntry : public PLDHashEntryHdr
{
public:
  PLDHashStringEntry(const void* aKey) :
    mKey(*NS_STATIC_CAST(const nsAString*, aKey)) { }
  ~PLDHashStringEntry() { }

  const void* GetKey() const {
    return NS_STATIC_CAST(const nsAString*, &mKey);
  }
  static PLDHashNumber HashKey(const void* key) {
    return HashString(*NS_STATIC_CAST(const nsAString*, key));
  }
  PRBool MatchEntry(const void* key) const {
    return NS_STATIC_CAST(const nsAString*, key)->Equals(mKey);
  }

  const nsString mKey;
};

//
// CString-key entry
//
class NS_COM PLDHashCStringEntry : public PLDHashEntryHdr
{
public:
  PLDHashCStringEntry(const void* aKey) :
    mKey(*NS_STATIC_CAST(const nsACString*, aKey)) { }
  ~PLDHashCStringEntry() { }

  const void* GetKey() const {
    return NS_STATIC_CAST(const nsACString*, &mKey);
  }
  static PLDHashNumber HashKey(const void* key) {
    return HashString(*NS_STATIC_CAST(const nsACString*, key));
  }
  PRBool MatchEntry(const void* key) const {
    return NS_STATIC_CAST(const nsACString*, key)->Equals(mKey);
  }

  const nsCString mKey;
};

//
// Int-key entry
//
class NS_COM PLDHashInt32Entry : public PLDHashEntryHdr
{
public:
  PLDHashInt32Entry(const void* aKey) :
    mKey(*(NS_STATIC_CAST(const PRInt32*, aKey))) { }
  ~PLDHashInt32Entry() { }

  const void* GetKey() const {
    return NS_STATIC_CAST(const PRInt32*, &mKey);
  }
  static PLDHashNumber HashKey(const void* key) {
    return *NS_STATIC_CAST(const PRInt32*, key);
  }
  PRBool MatchEntry(const void* key) const {
    return *(NS_STATIC_CAST(const PRInt32*, key)) == mKey;
  }

  const PRInt32 mKey;
};


//
// Void-key entry
//
class NS_COM PLDHashVoidEntry : public PLDHashEntryHdr
{
public:
  PLDHashVoidEntry(const void* aKey) :
    mKey(*(const void**)aKey) { }
  ~PLDHashVoidEntry() { }

  const void* GetKey() const {
    return (const void**)&mKey;
  }
  static PLDHashNumber HashKey(const void* key) {
    return PLDHashNumber(NS_PTR_TO_INT32(*(const void**)key)) >> 2;
  }
  PRBool MatchEntry(const void* key) const {
    return *(const void**)key == mKey;
  }

  const void* mKey;
};


#define DHASH_EXPORT


#endif
