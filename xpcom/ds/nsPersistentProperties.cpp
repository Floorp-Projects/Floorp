/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsID.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsIInputStream.h"
#include "nsIUnicharInputStream.h"
#include "pratom.h"
#include "nsEnumeratorUtils.h"
#include "nsReadableUtils.h"
#include "nsPrintfCString.h"
#include "nsDependentString.h"

#define PL_ARENA_CONST_ALIGN_MASK 3
#include "nsPersistentProperties.h"
#include "nsIProperties.h"
#include "nsProperties.h"

#define PROP_BUFFER_SIZE 2048

struct propertyTableEntry : public PLDHashEntryHdr
{
    // both of these are arena-allocated
    const char *mKey;
    const PRUnichar *mValue;
};

static PRUnichar*
ArenaStrdup(const nsAFlatString& aString, PLArenaPool* aArena)
{
    void *mem;
    // add one to include the null terminator
    PRInt32 len = (aString.Length()+1) * sizeof(PRUnichar);
    PL_ARENA_ALLOCATE(mem, aArena, len);
    NS_ASSERTION(mem, "Couldn't allocate space!\n");
    if (mem) {
        memcpy(mem, aString.get(), len);
    }
    return NS_STATIC_CAST(PRUnichar*, mem);
}

static char*
ArenaStrdup(const nsAFlatCString& aString, PLArenaPool* aArena)
{
    void *mem;
    // add one to include the null terminator
    PRInt32 len = (aString.Length()+1) * sizeof(char);
    PL_ARENA_ALLOCATE(mem, aArena, len);
    NS_ASSERTION(mem, "Couldn't allocate space!\n");
    if (mem)
        memcpy(mem, aString.get(), len);
    return NS_STATIC_CAST(char*, mem);
}

PR_STATIC_CALLBACK(PRBool)
matchPropertyKeys(PLDHashTable*, const PLDHashEntryHdr* aHdr,
                  const void *key)
{
  const propertyTableEntry* entry =
    NS_STATIC_CAST(const propertyTableEntry*,aHdr);
  const char *keyValue = NS_STATIC_CAST(const char*,key);
    
  return (strcmp(entry->mKey, keyValue)==0);
}

PR_STATIC_CALLBACK(const void*)
getPropertyKey(PLDHashTable*, PLDHashEntryHdr* aHdr)
{
  propertyTableEntry* entry =
    NS_STATIC_CAST(propertyTableEntry*, aHdr);

  return entry->mKey;
}

struct PLDHashTableOps property_HashTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    getPropertyKey,
    PL_DHashStringKey,
    matchPropertyKeys,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    nsnull,
};

//
// parser stuff
// 
enum {
    eParserState_AwaitingKey,
    eParserState_Key,
    eParserState_AwaitingValue,
    eParserState_Value,
    eParserState_Comment
};

enum {
  eParserSpecial_None,          // not parsing a special character
  eParserSpecial_MultiLine,     // Have a multiline sequence
  eParserSpecial_Escaped,       // awaiting a special character
  eParserSpecial_Unicode        // parsing a \Uxxx value
};

class nsParserState
{
public:
  nsParserState(nsIPersistentProperties* aProps) :
    mState(eParserState_AwaitingKey), mHaveMultiLine(PR_FALSE),
    mProps(aProps) {}

  void WaitForKey() {
    mState = eParserState_AwaitingKey;
  }

  void EnterKeyState() {
    mKey.Truncate();
    mState = eParserState_Key;
  }

  void WaitForValue() {
    mState = eParserState_AwaitingValue;
  }

  void EnterValueState() {
    mValue.Truncate();
    mState = eParserState_Value;
    mSpecialState = eParserSpecial_None;
  }
  
  void FinishValueState(nsAString& aOldValue) {
    static const char trimThese[] = " \t";
    mKey.Trim(trimThese, PR_FALSE, PR_TRUE);
    mValue.Trim(trimThese, PR_FALSE, PR_TRUE);
    mProps->SetStringProperty(NS_ConvertUCS2toUTF8(mKey), mValue, aOldValue);
    mSpecialState = eParserSpecial_None;
    WaitForKey();
  }

  void EnterCommentState() {
    mState = eParserState_Comment;
  }

  nsAutoString mKey;
  nsAutoString mValue;

  PRUint32 GetState() { return mState; }
  
  // if we see a '\' then we enter this special state
  PRUint32 mSpecialState;

  PRUint32  mUnicodeValuesRead; // should be 4!
  PRUnichar mUnicodeValue;      // currently parsed unicode value
  PRBool    mHaveMultiLine;     // if this key is multi-line

private:
  PRUint32 mState;
  nsCOMPtr<nsIPersistentProperties> mProps; // weak
};

nsPersistentProperties::nsPersistentProperties()
{
  mIn = nsnull;
  mSubclass = NS_STATIC_CAST(nsIPersistentProperties*, this);
  PL_DHashTableInit(&mTable, &property_HashTableOps, nsnull,
                    sizeof(propertyTableEntry), 20);

  PL_INIT_ARENA_POOL(&mArena, "PersistentPropertyArena", 2048);
  
}

nsPersistentProperties::~nsPersistentProperties()
{
    PL_FinishArenaPool(&mArena);
    PL_DHashTableFinish(&mTable);
}

NS_METHOD
nsPersistentProperties::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsPersistentProperties* props = new nsPersistentProperties();
    if (props == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(props);
    nsresult rv = props->QueryInterface(aIID, aResult);
    NS_RELEASE(props);
    return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsPersistentProperties, nsIPersistentProperties, nsIProperties)


// for future support of segmented reads - see bug 189528
#if 0
NS_METHOD
nsPersistentProperties::SegmentWriter(nsIInputStream* aStream,
                                      void* aClosure,
                                      const char* aFromSegment,
                                      PRUint32 aToOffset,
                                      PRUint32 aCount,
                                      PRUint32* aWriteCount)
{
  nsParserState *parseState = 
    NS_STATIC_CAST(nsParserState*, aClosure);

  // this won't work until ParseBuffer starts taking raw UTF8, rather
  // than unicode
  ParseBuffer(*parserState, aFromSegment, aCount);
  
  return rv;
}
#endif

#define IS_WHITE_SPACE(c) \
  (((c) == ' ') || ((c) == '\t') || ((c) == '\r') || ((c) == '\n'))

#define IS_EOL(c) \
  (((c) == '\r') || ((c) == '\n'))

static void
ParseValueCharacter(nsParserState& aState, PRUnichar c,
                    const PRUnichar* cur, const PRUnichar* &tokenStart,
                    nsAString& oldValue)
{
  switch (aState.mSpecialState) {

    // the normal state - look for special characters
  case eParserSpecial_None:
    switch (c) {
    case '\\':
      // handle multilines - since this is the beginning of a line,
      // there's no value to append
      if (aState.mHaveMultiLine)
        aState.mHaveMultiLine = PR_FALSE;
      else
        aState.mValue += Substring(tokenStart, cur);
      
      aState.mSpecialState = eParserSpecial_Escaped;
      break;
      
    case '\n':
    case '\r':
      // ignore sequential line endings
      if (aState.mHaveMultiLine)
        break;
      
      // we're done! We have a key and value
      aState.mValue += Substring(tokenStart, cur);
      aState.FinishValueState(oldValue);
      break;

    default:
      // there is nothing to do with normal characters,
      // but handle multilines correctly
      if (aState.mHaveMultiLine) {
        aState.mHaveMultiLine = PR_FALSE;
        tokenStart = cur;
      }
      break;
    }
    break;


    // saw a \ character, so parse the character after that
  case eParserSpecial_Escaped:
    // probably want to start parsing at the next token
    // other characters, like 'u' might override this
    tokenStart = cur+1;
    aState.mSpecialState = eParserSpecial_None;
    
    switch (c) {
      
      // the easy characters - \t, \n, and so forth
    case 't':
      aState.mValue += PRUnichar('\t');
      break;
    case 'n':
      aState.mValue += PRUnichar('\n');
      break;
    case 'r':
      aState.mValue += PRUnichar('\r');
      break;
    case '\\':
      aState.mValue += PRUnichar('\\');
      break;
      
      // switch to unicode mode!
    case 'u':
    case 'U':
      aState.mSpecialState = eParserSpecial_Unicode;
      aState.mUnicodeValuesRead = 0;
      aState.mUnicodeValue = 0;
      break;
      
      // a \ immediately followed by a newline means we're going multiline
    case '\r':
    case '\n':
      aState.mHaveMultiLine = PR_TRUE;
      aState.mSpecialState = eParserSpecial_None;
      aState.mHaveMultiLine=PR_TRUE;
      break;

    default:
      // don't recognize the character, so just append it
      aState.mValue += c;
      break;
    }
    break;
    
    // we're in the middle of parsing a 4-character unicode value
    // like \u5f39
  case eParserSpecial_Unicode:

    if(('0' <= c) && (c <= '9'))
      aState.mUnicodeValue =
        (aState.mUnicodeValue << 4) | (c - '0');
    else if(('a' <= c) && (c <= 'f'))
      aState.mUnicodeValue =
        (aState.mUnicodeValue << 4) | (c - 'a' + 0x0a);
    else if(('A' <= c) && (c <= 'F'))
      aState.mUnicodeValue =
        (aState.mUnicodeValue << 4) | (c - 'A' + 0x0a);
    else {
      // non-hex character. Append what we have, and move on.
      aState.mValue += aState.mUnicodeValue;
      aState.mSpecialState = eParserSpecial_None;

      // leave tokenStart at this unknown character, so it gets appended
      tokenStart = cur;

      // break out early from switch() so we don't process it anymore
      break;
    }

    if (++aState.mUnicodeValuesRead >= 4) {
      tokenStart = cur+1;
      aState.mSpecialState = eParserSpecial_None;
      aState.mValue += aState.mUnicodeValue;
    }

    break;
  }
}

static nsresult
ParseBuffer(nsParserState& aState,
            const PRUnichar* aBuffer,
            PRUint32 aBufferLength)
{
  const PRUnichar* cur = aBuffer;
  const PRUnichar* end = aBuffer + aBufferLength;
  
  // points the start/end of the current key or value
  const PRUnichar* tokenStart = nsnull;

  // if we're in the middle of parsing a key or value, make sure
  // the current token points to the beginning of the current buffer
  if (aState.GetState() == eParserState_Key ||
      aState.GetState() == eParserState_Value) {
    tokenStart = aBuffer;
  }

  nsAutoString oldValue;
  
  while (cur != end) {
      
    PRUnichar c = *cur;
    
    switch (aState.GetState()) {
    case eParserState_AwaitingKey:
      if (c == '#' || c == '!')
        aState.EnterCommentState();
      
      else if (!IS_WHITE_SPACE(c)) {
        // not a comment, not whitespace, we must have found a key!
        aState.EnterKeyState();
        tokenStart = cur;
      }
      break;

    case eParserState_Key:
      if (c == '=' || c == ':') {
        aState.mKey += Substring(tokenStart, cur);
        aState.WaitForValue();
      }
      break;
      
    case eParserState_AwaitingValue:
      if (IS_EOL(c)) {
        // no value at all! mimic the normal value-ending4
        aState.EnterValueState();
        aState.mValue.Truncate();
        aState.FinishValueState(oldValue);
      }
      
      // ignore white space leading up to the value
      else if (!IS_WHITE_SPACE(c)) {
        tokenStart = cur;
        aState.EnterValueState();
        
        // make sure to handle this first character
        ParseValueCharacter(aState, c, cur, tokenStart, oldValue);
      }
      break;

    case eParserState_Value:
      ParseValueCharacter(aState, c, cur, tokenStart, oldValue);
      break;
      
    case eParserState_Comment:
      // stay in this state till we hit EOL
      if (c == '\r' || c== '\n')
        aState.WaitForKey();
      break;
    }

    // finally, advance to the next character
    cur++;
  }

  // if we're still parsing the value, then append whatever we have..
  if (aState.GetState() == eParserState_Value && tokenStart)
    aState.mValue += Substring(tokenStart, cur);
  // if we're still parsing the value, then append whatever we have..
  else if (aState.GetState() == eParserState_Key && tokenStart)
    aState.mKey += Substring(tokenStart, cur);
  
  return NS_OK;
}

NS_IMETHODIMP
nsPersistentProperties::Load(nsIInputStream *aIn)
{
  nsresult ret = NS_NewUTF8ConverterStream(getter_AddRefs(mIn), aIn, 0);
  if (ret != NS_OK) {
    NS_WARNING("NS_NewUTF8ConverterStream failed");
    return NS_ERROR_FAILURE;
  }

  PRUnichar buf[PROP_BUFFER_SIZE];

  PRUint32 nRead;

  nsParserState parserState(mSubclass);

// for future support of segmented reads - see bug 189528
#if 0
  ret = mIn->ReadSegments(SegmentWriter, this, 0);
#endif
  
  ret = mIn->Read(buf, 0, PROP_BUFFER_SIZE, &nRead);
  
  while (NS_SUCCEEDED(ret) && nRead > 0) {
    
    ParseBuffer(parserState, buf, nRead);
    
    ret = mIn->Read(buf, 0, PROP_BUFFER_SIZE, &nRead);
  }

  return NS_OK;
}
                         

NS_IMETHODIMP
nsPersistentProperties::SetStringProperty(const nsACString& aKey,
                                          const nsAString& aNewValue,
                                          nsAString& aOldValue)
{
  const nsAFlatCString&  flatKey = PromiseFlatCString(aKey);
  propertyTableEntry *entry =
      NS_STATIC_CAST(propertyTableEntry*,
                     PL_DHashTableOperate(&mTable, flatKey.get(), PL_DHASH_ADD));

  if (entry->mKey) {
      aOldValue = entry->mValue;
      NS_WARNING(nsPrintfCString(aKey.Length() + 30,
                                 "the property %s already exists\n",
                                 flatKey.get()).get());
  }

  entry->mKey = ArenaStrdup(flatKey, &mArena);
  entry->mValue = ArenaStrdup(PromiseFlatString(aNewValue), &mArena);
  
  return NS_OK;
}

NS_IMETHODIMP
nsPersistentProperties::Save(nsIOutputStream* aOut, const nsACString& aHeader)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPersistentProperties::Subclass(nsIPersistentProperties* aSubclass)
{
  if (aSubclass) {
    mSubclass = aSubclass;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPersistentProperties::GetStringProperty(const nsACString& aKey,
                                          nsAString& aValue)
{
  const nsAFlatCString&  flatKey = PromiseFlatCString(aKey);

  propertyTableEntry *entry =
    NS_STATIC_CAST(propertyTableEntry*,
                   PL_DHashTableOperate(&mTable, flatKey.get(), PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_FREE(entry))
    return NS_ERROR_FAILURE;

  aValue = entry->mValue;
  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
AddElemToArray(PLDHashTable* table, PLDHashEntryHdr *hdr,
               PRUint32 i, void *arg)
{
  nsISupportsArray  *propArray = (nsISupportsArray *) arg;
  propertyTableEntry* entry =
      NS_STATIC_CAST(propertyTableEntry*, hdr);
  
  nsPropertyElement *element =
    new nsPropertyElement(nsDependentCString(entry->mKey),
                          nsDependentString(entry->mValue));
  if (!element)
     return PL_DHASH_STOP;

  NS_ADDREF(element);
  propArray->InsertElementAt(element, i);

  return PL_DHASH_NEXT;
}


NS_IMETHODIMP
nsPersistentProperties::Enumerate(nsISimpleEnumerator** aResult)
{
  nsCOMPtr<nsIBidirectionalEnumerator> iterator;

  nsISupportsArray* propArray;
  nsresult rv = NS_NewISupportsArray(&propArray);
  if (rv != NS_OK)
    return rv;

  // Step through hash entries populating a transient array
  PRUint32 n =
      PL_DHashTableEnumerate(&mTable, AddElemToArray, (void *)propArray);
  if ( n < mTable.entryCount )
      return NS_ERROR_OUT_OF_MEMORY;

  return NS_NewArrayEnumerator(aResult, propArray); 
}


PRInt32
nsPersistentProperties::Read()
{
  PRUnichar  c;
  PRUint32  nRead;
  nsresult  ret;

  ret = mIn->Read(&c, 0, 1, &nRead);
  if (ret == NS_OK && nRead == 1) {
    return c;
  }

  return -1;
}

PRInt32
nsPersistentProperties::SkipWhiteSpace(PRInt32 c)
{
  while ((c >= 0) && IS_WHITE_SPACE(c)) {
    c = Read();
  }

  return c;
}

PRInt32
nsPersistentProperties::SkipLine(PRInt32 c)
{
  while ((c >= 0) && (c != '\r') && (c != '\n')) {
    c = Read();
  }
  if (c == '\r') {
    c = Read();
  }
  if (c == '\n') {
    c = Read();
  }

  return c;
}

////////////////////////////////////////////////////////////////////////////////
// XXX Some day we'll unify the nsIPersistentProperties interface with 
// nsIProperties, but until now...

NS_IMETHODIMP 
nsPersistentProperties::Get(const char* prop, const nsIID & uuid, void* *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsPersistentProperties::Set(const char* prop, nsISupports* value)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP 
nsPersistentProperties::Undefine(const char* prop)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsPersistentProperties::Has(const char* prop, PRBool *result)
{
  propertyTableEntry *entry =
    NS_STATIC_CAST(propertyTableEntry*,
                   PL_DHashTableOperate(&mTable, prop, PL_DHASH_LOOKUP));

  *result = (entry && PL_DHASH_ENTRY_IS_BUSY(entry));

  return NS_OK;
}

NS_IMETHODIMP 
nsPersistentProperties::GetKeys(PRUint32 *count, char ***keys)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// PropertyElement
////////////////////////////////////////////////////////////////////////////////


NS_METHOD
nsPropertyElement::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsPropertyElement* propElem = new nsPropertyElement();
    if (propElem == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(propElem);
    nsresult rv = propElem->QueryInterface(aIID, aResult);
    NS_RELEASE(propElem);
    return rv;
}

NS_IMPL_ISUPPORTS1(nsPropertyElement, nsIPropertyElement)

NS_IMETHODIMP
nsPropertyElement::GetKey(nsACString& aReturnKey)
{
  aReturnKey = mKey;
  return NS_OK;
}

NS_IMETHODIMP
nsPropertyElement::GetValue(nsAString& aReturnValue)
{
  aReturnValue = mValue;
  return NS_OK;
}

NS_IMETHODIMP
nsPropertyElement::SetKey(const nsACString& aKey)
{
  mKey = aKey;
  return NS_OK;
}

NS_IMETHODIMP
nsPropertyElement::SetValue(const nsAString& aValue)
{
  mValue = aValue;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

