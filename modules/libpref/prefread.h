/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef prefread_h__
#define prefread_h__

#include "prefapi.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback function used to notify consumer of preference name value pairs.
// The pref name and value must be copied by the implementor of the callback
// if they are needed beyond the scope of the callback function.
//
// |aClosure| is user data passed to PREF_InitParseState.
// |aPref| is the preference name.
// |aValue| is the preference value.
// |aType| is the preference type (PREF_STRING, PREF_INT, or PREF_BOOL).
// |aIsDefault| indicates if it's a default preference.
// |aIsStickyDefault| indicates if it's a sticky default preference.
typedef void (*PrefReader)(void* aClosure,
                           const char* aPref,
                           PrefValue aValue,
                           PrefType aType,
                           bool aIsDefault,
                           bool aIsStickyDefault);

// Report any errors or warnings we encounter during parsing.
typedef void (*PrefParseErrorReporter)(const char* aMessage,
                                       int aLine,
                                       bool aError);

typedef struct PrefParseState
{
  PrefReader mReader;
  PrefParseErrorReporter mReporter;
  void* mClosure;
  int mState;            // PREF_PARSE_...
  int mNextState;        // sometimes used...
  const char* mStrMatch; // string to match
  int mStrIndex;         // next char of smatch to check;
                         // also, counter in \u parsing
  char16_t mUtf16[2];    // parsing UTF16 (\u) escape
  int mEscLen;           // length in mEscTmp
  char mEscTmp[6];       // raw escape to put back if err
  char mQuoteChar;       // char delimiter for quotations
  char* mLb;             // line buffer (only allocation)
  char* mLbCur;          // line buffer cursor
  char* mLbEnd;          // line buffer end
  char* mVb;             // value buffer (ptr into mLb)
  PrefType mVtype;       // PREF_{STRING,INT,BOOL}
  bool mIsDefault;       // true if (default) pref
  bool mIsStickyDefault; // true if (sticky) pref
} PrefParseState;

// Initialize a PrefParseState instance.
//
// |aPS| is the PrefParseState instance.
// |aReader| is the PrefReader callback function, which will be called once for
// each preference name value pair extracted.
// |aReporter| is the PrefParseErrorReporter callback function, which will be
// called if we encounter any errors (stop) or warnings (continue) during
// parsing.
// |aClosure| is extra data passed to |aReader|.
void
PREF_InitParseState(PrefParseState* aPS,
                    PrefReader aReader,
                    PrefParseErrorReporter aReporter,
                    void* aClosure);

// Release any memory in use by the PrefParseState instance.
void
PREF_FinalizeParseState(PrefParseState* aPS);

// Parse a buffer containing some portion of a preference file.  This function
// may be called repeatedly as new data is made available. The PrefReader
// callback function passed PREF_InitParseState will be called as preference
// name value pairs are extracted from the data. Returns false if buffer
// contains malformed content.
bool
PREF_ParseBuf(PrefParseState* aPS, const char* aBuf, int aBufLen);

#ifdef __cplusplus
}
#endif
#endif /* prefread_h__ */
