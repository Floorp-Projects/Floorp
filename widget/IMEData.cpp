/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IMEData.h"
#include <sstream>

#include "gfxFontUtils.h"

#include "mozilla/WritingModes.h"

#include "nsPrintfCString.h"
#include "nsString.h"

namespace mozilla {

PrintStringDetail::PrintStringDetail(const nsAString& aString,
                                     uint32_t aMaxLength /* = UINT32_MAX */) {
  Assign("\"");
  const uint32_t kFirstHalf =
      aString.Length() <= aMaxLength ? UINT32_MAX : (aMaxLength + 1) / 2;
  const uint32_t kSecondHalf =
      aString.Length() <= aMaxLength ? 0 : aMaxLength / 2;
  for (uint32_t i = 0; i < aString.Length(); i++) {
    if (i > 0) {
      Append(" ");
    }
    char32_t ch = aString.CharAt(i);
    if (NS_IS_HIGH_SURROGATE(ch) && i + 1 < aString.Length() &&
        NS_IS_LOW_SURROGATE(aString.CharAt(i + 1))) {
      ch = SURROGATE_TO_UCS4(ch, aString.CharAt(i + 1));
    }
    Append(PrintCharData(ch));
    if (i + 1 == kFirstHalf) {
      Append(" ...");
      i = aString.Length() - kSecondHalf - 1;
      if (NS_IS_LOW_SURROGATE(aString.CharAt(i)) &&
          NS_IS_HIGH_SURROGATE(aString.CharAt(i - 1))) {
        if (i - 1 <= kFirstHalf) {
          i++;
        } else {
          i--;
        }
      }
    } else if (!IS_IN_BMP(ch)) {
      i++;
    }
  }
  Append("\"");
}

// static
nsCString PrintStringDetail::PrintCharData(char32_t aChar) {
  switch (aChar) {
    case 0x0000:
      return "NULL (0x0000)"_ns;
    case 0x0008:
      return "BACKSPACE (0x0008)"_ns;
    case 0x0009:
      return "CHARACTER TABULATION (0x0009)"_ns;
    case 0x000A:
      return "LINE FEED (0x000A)"_ns;
    case 0x000B:
      return "LINE TABULATION (0x000B)"_ns;
    case 0x000C:
      return "FORM FEED (0x000C)"_ns;
    case 0x000D:
      return "CARRIAGE RETURN (0x000D)"_ns;
    case 0x0018:
      return "CANCEL (0x0018)"_ns;
    case 0x001B:
      return "ESCAPE (0x001B)"_ns;
    case 0x0020:
      return "SPACE (0x0020)"_ns;
    case 0x007F:
      return "DELETE (0x007F)"_ns;
    case 0x00A0:
      return "NO-BREAK SPACE (0x00A0)"_ns;
    case 0x00AD:
      return "SOFT HYPHEN (0x00AD)"_ns;
    case 0x2000:
      return "EN QUAD (0x2000)"_ns;
    case 0x2001:
      return "EM QUAD (0x2001)"_ns;
    case 0x2002:
      return "EN SPACE (0x2002)"_ns;
    case 0x2003:
      return "EM SPACE (0x2003)"_ns;
    case 0x2004:
      return "THREE-PER-EM SPACE (0x2004)"_ns;
    case 0x2005:
      return "FOUR-PER-EM SPACE (0x2005)"_ns;
    case 0x2006:
      return "SIX-PER-EM SPACE (0x2006)"_ns;
    case 0x2007:
      return "FIGURE SPACE (0x2007)"_ns;
    case 0x2008:
      return "PUNCTUATION SPACE (0x2008)"_ns;
    case 0x2009:
      return "THIN SPACE (0x2009)"_ns;
    case 0x200A:
      return "HAIR SPACE (0x200A)"_ns;
    case 0x200B:
      return "ZERO WIDTH SPACE (0x200B)"_ns;
    case 0x200C:
      return "ZERO WIDTH NON-JOINER (0x200C)"_ns;
    case 0x200D:
      return "ZERO WIDTH JOINER (0x200D)"_ns;
    case 0x200E:
      return "LEFT-TO-RIGHT MARK (0x200E)"_ns;
    case 0x200F:
      return "RIGHT-TO-LEFT MARK (0x200F)"_ns;
    case 0x2029:
      return "PARAGRAPH SEPARATOR (0x2029)"_ns;
    case 0x202A:
      return "LEFT-TO-RIGHT EMBEDDING (0x202A)"_ns;
    case 0x202B:
      return "RIGHT-TO-LEFT EMBEDDING (0x202B)"_ns;
    case 0x202D:
      return "LEFT-TO-RIGHT OVERRIDE (0x202D)"_ns;
    case 0x202E:
      return "RIGHT-TO-LEFT OVERRIDE (0x202E)"_ns;
    case 0x202F:
      return "NARROW NO-BREAK SPACE (0x202F)"_ns;
    case 0x205F:
      return "MEDIUM MATHEMATICAL SPACE (0x205F)"_ns;
    case 0x2060:
      return "WORD JOINER (0x2060)"_ns;
    case 0x2066:
      return "LEFT-TO-RIGHT ISOLATE (0x2066)"_ns;
    case 0x2067:
      return "RIGHT-TO-LEFT ISOLATE (0x2067)"_ns;
    case 0x3000:
      return "IDEOGRAPHIC SPACE (0x3000)"_ns;
    case 0xFEFF:
      return "ZERO WIDTH NO-BREAK SPACE (0xFEFF)"_ns;
    default: {
      if (aChar < ' ' || (aChar >= 0x80 && aChar < 0xA0)) {
        return nsPrintfCString("Control (0x%04X)", aChar);
      }
      if (NS_IS_HIGH_SURROGATE(aChar)) {
        return nsPrintfCString("High Surrogate (0x%04X)", aChar);
      }
      if (NS_IS_LOW_SURROGATE(aChar)) {
        return nsPrintfCString("Low Surrogate (0x%04X)", aChar);
      }
      if (gfxFontUtils::IsVarSelector(aChar)) {
        return IS_IN_BMP(aChar)
                   ? nsPrintfCString("Variant Selector (0x%04X)", aChar)
                   : nsPrintfCString("Variant Selector (0x%08X)", aChar);
      }
      nsAutoString utf16Str;
      AppendUCS4ToUTF16(aChar, utf16Str);
      return IS_IN_BMP(aChar)
                 ? nsPrintfCString("'%s' (0x%04X)",
                                   NS_ConvertUTF16toUTF8(utf16Str).get(), aChar)
                 : nsPrintfCString("'%s' (0x%08X)",
                                   NS_ConvertUTF16toUTF8(utf16Str).get(),
                                   aChar);
    }
  }
}

namespace widget {

std::ostream& operator<<(std::ostream& aStream, const IMEEnabled& aEnabled) {
  switch (aEnabled) {
    case IMEEnabled::Disabled:
      return aStream << "DISABLED";
    case IMEEnabled::Enabled:
      return aStream << "ENABLED";
    case IMEEnabled::Password:
      return aStream << "PASSWORD";
    case IMEEnabled::Unknown:
      return aStream << "illegal value";
  }
  MOZ_ASSERT_UNREACHABLE("Add a case to handle your new IMEEnabled value");
  return aStream;
}

std::ostream& operator<<(std::ostream& aStream, const IMEState::Open& aOpen) {
  switch (aOpen) {
    case IMEState::DONT_CHANGE_OPEN_STATE:
      aStream << "DONT_CHANGE_OPEN_STATE";
      break;
    case IMEState::OPEN:
      aStream << "OPEN";
      break;
    case IMEState::CLOSED:
      aStream << "CLOSED";
      break;
    default:
      aStream << "illegal value";
      break;
  }
  return aStream;
}

std::ostream& operator<<(std::ostream& aStream, const IMEState& aState) {
  aStream << "{ mEnabled=" << aState.mEnabled << ", mOpen=" << aState.mOpen
          << " }";
  return aStream;
}

std::ostream& operator<<(std::ostream& aStream,
                         const InputContext::Origin& aOrigin) {
  switch (aOrigin) {
    case InputContext::ORIGIN_MAIN:
      aStream << "ORIGIN_MAIN";
      break;
    case InputContext::ORIGIN_CONTENT:
      aStream << "ORIGIN_CONTENT";
      break;
    default:
      aStream << "illegal value";
      break;
  }
  return aStream;
}

std::ostream& operator<<(std::ostream& aStream, const InputContext& aContext) {
  aStream << "{ mIMEState=" << aContext.mIMEState
          << ", mOrigin=" << aContext.mOrigin << ", mHTMLInputType=\""
          << aContext.mHTMLInputType << "\", mHTMLInputInputmode=\""
          << aContext.mHTMLInputInputmode << "\", mActionHint=\""
          << aContext.mActionHint << "\", mAutocapitalize=\""
          << aContext.mAutocapitalize << "\", mMayBeIMEUnaware="
          << (aContext.mMayBeIMEUnaware ? "true" : "false")
          << ", mIsPrivateBrowsing="
          << (aContext.mInPrivateBrowsing ? "true" : "false") << " }";
  return aStream;
}

std::ostream& operator<<(std::ostream& aStream,
                         const InputContextAction::Cause& aCause) {
  switch (aCause) {
    case InputContextAction::CAUSE_UNKNOWN:
      aStream << "CAUSE_UNKNOWN";
      break;
    case InputContextAction::CAUSE_UNKNOWN_CHROME:
      aStream << "CAUSE_UNKNOWN_CHROME";
      break;
    case InputContextAction::CAUSE_KEY:
      aStream << "CAUSE_KEY";
      break;
    case InputContextAction::CAUSE_MOUSE:
      aStream << "CAUSE_MOUSE";
      break;
    case InputContextAction::CAUSE_TOUCH:
      aStream << "CAUSE_TOUCH";
      break;
    case InputContextAction::CAUSE_LONGPRESS:
      aStream << "CAUSE_LONGPRESS";
      break;
    case InputContextAction::CAUSE_UNKNOWN_DURING_NON_KEYBOARD_INPUT:
      aStream << "CAUSE_UNKNOWN_DURING_NON_KEYBOARD_INPUT";
      break;
    case InputContextAction::CAUSE_UNKNOWN_DURING_KEYBOARD_INPUT:
      aStream << "CAUSE_UNKNOWN_DURING_KEYBOARD_INPUT";
      break;
    default:
      aStream << "illegal value";
      break;
  }
  return aStream;
}

std::ostream& operator<<(std::ostream& aStream,
                         const InputContextAction::FocusChange& aFocusChange) {
  switch (aFocusChange) {
    case InputContextAction::FOCUS_NOT_CHANGED:
      aStream << "FOCUS_NOT_CHANGED";
      break;
    case InputContextAction::GOT_FOCUS:
      aStream << "GOT_FOCUS";
      break;
    case InputContextAction::LOST_FOCUS:
      aStream << "LOST_FOCUS";
      break;
    case InputContextAction::MENU_GOT_PSEUDO_FOCUS:
      aStream << "MENU_GOT_PSEUDO_FOCUS";
      break;
    case InputContextAction::MENU_LOST_PSEUDO_FOCUS:
      aStream << "MENU_LOST_PSEUDO_FOCUS";
      break;
    case InputContextAction::WIDGET_CREATED:
      aStream << "WIDGET_CREATED";
      break;
    default:
      aStream << "illegal value";
      break;
  }
  return aStream;
}

std::ostream& operator<<(
    std::ostream& aStream,
    const IMENotification::SelectionChangeDataBase& aData) {
  if (!aData.IsValid()) {
    aStream << "{ IsValid()=false }";
    return aStream;
  }
  aStream << "{ mOffset=" << aData.mOffset;
  if (aData.mString->Length() > 20) {
    aStream << ", mString.Length()=" << aData.mString->Length();
  } else {
    aStream << ", mString=\"" << NS_ConvertUTF16toUTF8(*aData.mString)
            << "\" (Length()=" << aData.mString->Length() << ")";
  }

  aStream << ", GetWritingMode()=" << aData.GetWritingMode()
          << ", mReversed=" << (aData.mReversed ? "true" : "false")
          << ", mCausedByComposition="
          << (aData.mCausedByComposition ? "true" : "false")
          << ", mCausedBySelectionEvent="
          << (aData.mCausedBySelectionEvent ? "true" : "false")
          << ", mOccurredDuringComposition="
          << (aData.mOccurredDuringComposition ? "true" : "false") << " }";
  return aStream;
}

std::ostream& operator<<(std::ostream& aStream,
                         const IMENotification::TextChangeDataBase& aData) {
  if (!aData.IsValid()) {
    aStream << "{ IsValid()=false }";
    return aStream;
  }
  aStream << "{ mStartOffset=" << aData.mStartOffset
          << ", mRemoveEndOffset=" << aData.mRemovedEndOffset
          << ", mAddedEndOffset=" << aData.mAddedEndOffset
          << ", mCausedOnlyByComposition="
          << (aData.mCausedOnlyByComposition ? "true" : "false")
          << ", mIncludingChangesDuringComposition="
          << (aData.mIncludingChangesDuringComposition ? "true" : "false")
          << ", mIncludingChangesWithoutComposition="
          << (aData.mIncludingChangesWithoutComposition ? "true" : "false")
          << " }";
  return aStream;
}

}  // namespace widget
}  // namespace mozilla
