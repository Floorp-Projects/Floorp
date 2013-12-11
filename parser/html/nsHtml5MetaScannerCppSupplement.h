/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#include "nsEncoderDecoderUtils.h"
#include "nsTraceRefcnt.h"

#include "mozilla/dom/EncodingUtils.h"

using mozilla::dom::EncodingUtils;

void
nsHtml5MetaScanner::sniff(nsHtml5ByteReadable* bytes, nsACString& charset)
{
  readable = bytes;
  stateLoop(stateSave);
  readable = nullptr;
  charset.Assign(mCharset);
}

bool
nsHtml5MetaScanner::tryCharset(nsString* charset)
{
  // This code needs to stay in sync with
  // nsHtml5StreamParser::internalEncodingDeclaration. Unfortunately, the
  // trickery with member fields here leads to some copy-paste reuse. :-(
  nsAutoCString label;
  CopyUTF16toUTF8(*charset, label);
  nsAutoCString encoding;
  if (!EncodingUtils::FindEncodingForLabel(label, encoding)) {
    return false;
  }
  if (encoding.EqualsLiteral("UTF-16BE") ||
      encoding.EqualsLiteral("UTF-16LE")) {
    mCharset.Assign("UTF-8");
    return true;
  }
  mCharset.Assign(encoding);
  return true;
}
