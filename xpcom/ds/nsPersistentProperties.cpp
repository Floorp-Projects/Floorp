/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArrayEnumerator.h"
#include "nsID.h"
#include "nsCOMArray.h"
#include "nsUnicharInputStream.h"
#include "nsPrintfCString.h"
#include "nsAutoPtr.h"

#include "nsPersistentProperties.h"
#include "nsIProperties.h"

#include "mozilla/ArenaAllocatorExtensions.h"

using mozilla::ArenaStrdup;

struct PropertyTableEntry : public PLDHashEntryHdr
{
  // both of these are arena-allocated
  const char* mKey;
  const char16_t* mValue;
};

static const struct PLDHashTableOps property_HashTableOps = {
  PLDHashTable::HashStringKey,
  PLDHashTable::MatchStringKey,
  PLDHashTable::MoveEntryStub,
  PLDHashTable::ClearEntryStub,
  nullptr,
};

//
// parser stuff
//
enum EParserState
{
  eParserState_AwaitingKey,
  eParserState_Key,
  eParserState_AwaitingValue,
  eParserState_Value,
  eParserState_Comment
};

enum EParserSpecial
{
  eParserSpecial_None,          // not parsing a special character
  eParserSpecial_Escaped,       // awaiting a special character
  eParserSpecial_Unicode        // parsing a \Uxxx value
};

class MOZ_STACK_CLASS nsPropertiesParser
{
public:
  explicit nsPropertiesParser(nsIPersistentProperties* aProps)
    : mHaveMultiLine(false)
    , mState(eParserState_AwaitingKey)
    , mProps(aProps)
  {
  }

  void FinishValueState(nsAString& aOldValue)
  {
    static const char trimThese[] = " \t";
    mKey.Trim(trimThese, false, true);

    // This is really ugly hack but it should be fast
    char16_t backup_char;
    uint32_t minLength = mMinLength;
    if (minLength) {
      backup_char = mValue[minLength - 1];
      mValue.SetCharAt('x', minLength - 1);
    }
    mValue.Trim(trimThese, false, true);
    if (minLength) {
      mValue.SetCharAt(backup_char, minLength - 1);
    }

    mProps->SetStringProperty(NS_ConvertUTF16toUTF8(mKey), mValue, aOldValue);
    mSpecialState = eParserSpecial_None;
    WaitForKey();
  }

  EParserState GetState() { return mState; }

  static nsresult SegmentWriter(nsIUnicharInputStream* aStream,
                                void* aClosure,
                                const char16_t* aFromSegment,
                                uint32_t aToOffset,
                                uint32_t aCount,
                                uint32_t* aWriteCount);

  nsresult ParseBuffer(const char16_t* aBuffer, uint32_t aBufferLength);

private:
  bool ParseValueCharacter(
    char16_t aChar,               // character that is just being parsed
    const char16_t* aCur,         // pointer to character aChar in the buffer
    const char16_t*& aTokenStart, // string copying is done in blocks as big as
                                  // possible, aTokenStart points to the beginning
                                  // of this block
    nsAString& aOldValue);        // when duplicate property is found, new value
                                  // is stored into hashtable and the old one is
                                  // placed in this variable

  void WaitForKey()
  {
    mState = eParserState_AwaitingKey;
  }

  void EnterKeyState()
  {
    mKey.Truncate();
    mState = eParserState_Key;
  }

  void WaitForValue()
  {
    mState = eParserState_AwaitingValue;
  }

  void EnterValueState()
  {
    mValue.Truncate();
    mMinLength = 0;
    mState = eParserState_Value;
    mSpecialState = eParserSpecial_None;
  }

  void EnterCommentState()
  {
    mState = eParserState_Comment;
  }

  nsAutoString mKey;
  nsAutoString mValue;

  uint32_t  mUnicodeValuesRead; // should be 4!
  char16_t mUnicodeValue;      // currently parsed unicode value
  bool      mHaveMultiLine;     // is TRUE when last processed characters form
                                // any of following sequences:
                                //  - "\\\r"
                                //  - "\\\n"
                                //  - "\\\r\n"
                                //  - any sequence above followed by any
                                //    combination of ' ' and '\t'
  bool      mMultiLineCanSkipN; // TRUE if "\\\r" was detected
  uint32_t  mMinLength;         // limit right trimming at the end to not trim
                                // escaped whitespaces
  EParserState mState;
  // if we see a '\' then we enter this special state
  EParserSpecial mSpecialState;
  nsCOMPtr<nsIPersistentProperties> mProps;
};

inline bool
IsWhiteSpace(char16_t aChar)
{
  return (aChar == ' ') || (aChar == '\t') ||
         (aChar == '\r') || (aChar == '\n');
}

inline bool
IsEOL(char16_t aChar)
{
  return (aChar == '\r') || (aChar == '\n');
}


bool
nsPropertiesParser::ParseValueCharacter(char16_t aChar, const char16_t* aCur,
                                        const char16_t*& aTokenStart,
                                        nsAString& aOldValue)
{
  switch (mSpecialState) {
    // the normal state - look for special characters
    case eParserSpecial_None:
      switch (aChar) {
        case '\\':
          if (mHaveMultiLine) {
            // there is nothing to append to mValue yet
            mHaveMultiLine = false;
          } else {
            mValue += Substring(aTokenStart, aCur);
          }

          mSpecialState = eParserSpecial_Escaped;
          break;

        case '\n':
          // if we detected multiline and got only "\\\r" ignore next "\n" if any
          if (mHaveMultiLine && mMultiLineCanSkipN) {
            // but don't allow another '\n' to be skipped
            mMultiLineCanSkipN = false;
            // Now there is nothing to append to the mValue since we are skipping
            // whitespaces at the beginning of the new line of the multiline
            // property. Set aTokenStart properly to ensure that nothing is appended
            // if we find regular line-end or the end of the buffer.
            aTokenStart = aCur + 1;
            break;
          }
          MOZ_FALLTHROUGH;

        case '\r':
          // we're done! We have a key and value
          mValue += Substring(aTokenStart, aCur);
          FinishValueState(aOldValue);
          mHaveMultiLine = false;
          break;

        default:
          // there is nothing to do with normal characters,
          // but handle multilines correctly
          if (mHaveMultiLine) {
            if (aChar == ' ' || aChar == '\t') {
              // don't allow another '\n' to be skipped
              mMultiLineCanSkipN = false;
              // Now there is nothing to append to the mValue since we are skipping
              // whitespaces at the beginning of the new line of the multiline
              // property. Set aTokenStart properly to ensure that nothing is appended
              // if we find regular line-end or the end of the buffer.
              aTokenStart = aCur + 1;
              break;
            }
            mHaveMultiLine = false;
            aTokenStart = aCur;
          }
          break; // from switch on (aChar)
      }
      break; // from switch on (mSpecialState)

    // saw a \ character, so parse the character after that
    case eParserSpecial_Escaped:
      // probably want to start parsing at the next token
      // other characters, like 'u' might override this
      aTokenStart = aCur + 1;
      mSpecialState = eParserSpecial_None;

      switch (aChar) {
        // the easy characters - \t, \n, and so forth
        case 't':
          mValue += char16_t('\t');
          mMinLength = mValue.Length();
          break;
        case 'n':
          mValue += char16_t('\n');
          mMinLength = mValue.Length();
          break;
        case 'r':
          mValue += char16_t('\r');
          mMinLength = mValue.Length();
          break;
        case '\\':
          mValue += char16_t('\\');
          break;

        // switch to unicode mode!
        case 'u':
        case 'U':
          mSpecialState = eParserSpecial_Unicode;
          mUnicodeValuesRead = 0;
          mUnicodeValue = 0;
          break;

        // a \ immediately followed by a newline means we're going multiline
        case '\r':
        case '\n':
          mHaveMultiLine = true;
          mMultiLineCanSkipN = (aChar == '\r');
          mSpecialState = eParserSpecial_None;
          break;

        default:
          // don't recognize the character, so just append it
          mValue += aChar;
          break;
      }
      break;

    // we're in the middle of parsing a 4-character unicode value
    // like \u5f39
    case eParserSpecial_Unicode:
      if ('0' <= aChar && aChar <= '9') {
        mUnicodeValue =
          (mUnicodeValue << 4) | (aChar - '0');
      } else if ('a' <= aChar && aChar <= 'f') {
        mUnicodeValue =
          (mUnicodeValue << 4) | (aChar - 'a' + 0x0a);
      } else if ('A' <= aChar && aChar <= 'F') {
        mUnicodeValue =
          (mUnicodeValue << 4) | (aChar - 'A' + 0x0a);
      } else {
        // non-hex character. Append what we have, and move on.
        mValue += mUnicodeValue;
        mMinLength = mValue.Length();
        mSpecialState = eParserSpecial_None;

        // leave aTokenStart at this unknown character, so it gets appended
        aTokenStart = aCur;

        // ensure parsing this non-hex character again
        return false;
      }

      if (++mUnicodeValuesRead >= 4) {
        aTokenStart = aCur + 1;
        mSpecialState = eParserSpecial_None;
        mValue += mUnicodeValue;
        mMinLength = mValue.Length();
      }

      break;
  }

  return true;
}

nsresult
nsPropertiesParser::SegmentWriter(nsIUnicharInputStream* aStream,
                                  void* aClosure,
                                  const char16_t* aFromSegment,
                                  uint32_t aToOffset,
                                  uint32_t aCount,
                                  uint32_t* aWriteCount)
{
  nsPropertiesParser* parser = static_cast<nsPropertiesParser*>(aClosure);
  parser->ParseBuffer(aFromSegment, aCount);

  *aWriteCount = aCount;
  return NS_OK;
}

nsresult
nsPropertiesParser::ParseBuffer(const char16_t* aBuffer,
                                uint32_t aBufferLength)
{
  const char16_t* cur = aBuffer;
  const char16_t* end = aBuffer + aBufferLength;

  // points to the start/end of the current key or value
  const char16_t* tokenStart = nullptr;

  // if we're in the middle of parsing a key or value, make sure
  // the current token points to the beginning of the current buffer
  if (mState == eParserState_Key ||
      mState == eParserState_Value) {
    tokenStart = aBuffer;
  }

  nsAutoString oldValue;

  while (cur != end) {

    char16_t c = *cur;

    switch (mState) {
      case eParserState_AwaitingKey:
        if (c == '#' || c == '!') {
          EnterCommentState();
        }

        else if (!IsWhiteSpace(c)) {
          // not a comment, not whitespace, we must have found a key!
          EnterKeyState();
          tokenStart = cur;
        }
        break;

      case eParserState_Key:
        if (c == '=' || c == ':') {
          mKey += Substring(tokenStart, cur);
          WaitForValue();
        }
        break;

      case eParserState_AwaitingValue:
        if (IsEOL(c)) {
          // no value at all! mimic the normal value-ending
          EnterValueState();
          FinishValueState(oldValue);
        }

        // ignore white space leading up to the value
        else if (!IsWhiteSpace(c)) {
          tokenStart = cur;
          EnterValueState();

          // make sure to handle this first character
          if (ParseValueCharacter(c, cur, tokenStart, oldValue)) {
            cur++;
          }
          // If the character isn't consumed, don't do cur++ and parse
          // the character again. This can happen f.e. for char 'X' in sequence
          // "\u00X". This character can be control character and must be
          // processed again.
          continue;
        }
        break;

      case eParserState_Value:
        if (ParseValueCharacter(c, cur, tokenStart, oldValue)) {
          cur++;
        }
        // See few lines above for reason of doing this
        continue;

      case eParserState_Comment:
        // stay in this state till we hit EOL
        if (c == '\r' || c == '\n') {
          WaitForKey();
        }
        break;
    }

    // finally, advance to the next character
    cur++;
  }

  // if we're still parsing the value and are in eParserSpecial_None, then
  // append whatever we have..
  if (mState == eParserState_Value && tokenStart &&
      mSpecialState == eParserSpecial_None) {
    mValue += Substring(tokenStart, cur);
  }
  // if we're still parsing the key, then append whatever we have..
  else if (mState == eParserState_Key && tokenStart) {
    mKey += Substring(tokenStart, cur);
  }

  return NS_OK;
}

nsPersistentProperties::nsPersistentProperties()
  : mIn(nullptr)
  , mTable(&property_HashTableOps, sizeof(PropertyTableEntry), 16)
  , mArena()
{
}

nsPersistentProperties::~nsPersistentProperties()
{
}

size_t
nsPersistentProperties::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // The memory used by mTable is accounted for in mArena.
  size_t n = 0;
  n += mArena.SizeOfExcludingThis(aMallocSizeOf);
  n += mTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return aMallocSizeOf(this) + n;
}

nsresult
nsPersistentProperties::Create(nsISupports* aOuter, REFNSIID aIID,
                               void** aResult)
{
  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }
  RefPtr<nsPersistentProperties> props = new nsPersistentProperties();
  return props->QueryInterface(aIID, aResult);
}

NS_IMPL_ISUPPORTS(nsPersistentProperties, nsIPersistentProperties, nsIProperties)

NS_IMETHODIMP
nsPersistentProperties::Load(nsIInputStream* aIn)
{
  nsresult rv = NS_NewUnicharInputStream(aIn, getter_AddRefs(mIn));

  if (rv != NS_OK) {
    NS_WARNING("Error creating UnicharInputStream");
    return NS_ERROR_FAILURE;
  }

  nsPropertiesParser parser(this);

  uint32_t nProcessed;
  // If this 4096 is changed to some other value, make sure to adjust
  // the bug121341.properties test file accordingly.
  while (NS_SUCCEEDED(rv = mIn->ReadSegments(nsPropertiesParser::SegmentWriter,
                                             &parser, 4096, &nProcessed)) &&
         nProcessed != 0);
  mIn = nullptr;
  if (NS_FAILED(rv)) {
    return rv;
  }

  // We may have an unprocessed value at this point
  // if the last line did not have a proper line ending.
  if (parser.GetState() == eParserState_Value) {
    nsAutoString oldValue;
    parser.FinishValueState(oldValue);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPersistentProperties::SetStringProperty(const nsACString& aKey,
                                          const nsAString& aNewValue,
                                          nsAString& aOldValue)
{
  const nsCString& flatKey = PromiseFlatCString(aKey);
  auto entry = static_cast<PropertyTableEntry*>
                          (mTable.Add(flatKey.get()));

  if (entry->mKey) {
    aOldValue = entry->mValue;
    NS_WARNING(nsPrintfCString("the property %s already exists",
                               flatKey.get()).get());
  } else {
    aOldValue.Truncate();
  }

  entry->mKey = ArenaStrdup(flatKey, mArena);
  entry->mValue = ArenaStrdup(aNewValue, mArena);

  return NS_OK;
}

NS_IMETHODIMP
nsPersistentProperties::Save(nsIOutputStream* aOut, const nsACString& aHeader)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPersistentProperties::GetStringProperty(const nsACString& aKey,
                                          nsAString& aValue)
{
  const nsCString& flatKey = PromiseFlatCString(aKey);

  auto entry = static_cast<PropertyTableEntry*>(mTable.Search(flatKey.get()));
  if (!entry) {
    return NS_ERROR_FAILURE;
  }

  aValue = entry->mValue;
  return NS_OK;
}

NS_IMETHODIMP
nsPersistentProperties::Enumerate(nsISimpleEnumerator** aResult)
{
  nsCOMArray<nsIPropertyElement> props;

  // We know the necessary size; we can avoid growing it while adding elements
  props.SetCapacity(mTable.EntryCount());

  // Step through hash entries populating a transient array
  for (auto iter = mTable.Iter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<PropertyTableEntry*>(iter.Get());

    RefPtr<nsPropertyElement> element =
      new nsPropertyElement(nsDependentCString(entry->mKey),
                            nsDependentString(entry->mValue));

    if (!props.AppendObject(element)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_NewArrayEnumerator(aResult, props);
}

////////////////////////////////////////////////////////////////////////////////
// XXX Some day we'll unify the nsIPersistentProperties interface with
// nsIProperties, but until now...

NS_IMETHODIMP
nsPersistentProperties::Get(const char* aProp, const nsIID& aUUID,
                            void** aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPersistentProperties::Set(const char* aProp, nsISupports* value)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsPersistentProperties::Undefine(const char* aProp)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPersistentProperties::Has(const char* aProp, bool* aResult)
{
  *aResult = !!mTable.Search(aProp);
  return NS_OK;
}

NS_IMETHODIMP
nsPersistentProperties::GetKeys(uint32_t* aCount, char*** aKeys)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// PropertyElement
////////////////////////////////////////////////////////////////////////////////

nsresult
nsPropertyElement::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }
  RefPtr<nsPropertyElement> propElem = new nsPropertyElement();
  return propElem->QueryInterface(aIID, aResult);
}

NS_IMPL_ISUPPORTS(nsPropertyElement, nsIPropertyElement)

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
