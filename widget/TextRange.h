/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextRage_h_
#define mozilla_TextRage_h_

#include <stdint.h>

#include "mozilla/EventForwards.h"

#include "nsColor.h"
#include "nsISelectionController.h"
#include "nsITextInputProcessor.h"
#include "nsTArray.h"

namespace mozilla {

/******************************************************************************
 * mozilla::TextRangeStyle
 ******************************************************************************/

struct TextRangeStyle {
  typedef uint8_t LineStyleType;
  // FYI: Modify IME_RANGE_LINE_* too when you modify LineStyle.
  enum class LineStyle : LineStyleType {
    None,
    Solid,
    Dotted,
    Dashed,
    Double,
    Wavy,
  };
  inline static LineStyle ToLineStyle(RawTextRangeType aRawLineStyle) {
    switch (static_cast<LineStyle>(aRawLineStyle)) {
      case LineStyle::None:
      case LineStyle::Solid:
      case LineStyle::Dotted:
      case LineStyle::Dashed:
      case LineStyle::Double:
      case LineStyle::Wavy:
        return static_cast<LineStyle>(aRawLineStyle);
    }
    MOZ_ASSERT_UNREACHABLE("aRawLineStyle value is invalid");
    return LineStyle::None;
  }

  enum {
    DEFINED_NONE = 0x00,
    DEFINED_LINESTYLE = 0x01,
    DEFINED_FOREGROUND_COLOR = 0x02,
    DEFINED_BACKGROUND_COLOR = 0x04,
    DEFINED_UNDERLINE_COLOR = 0x08
  };

  // Initialize all members, because TextRange instances may be compared by
  // memcomp.
  //
  // FIXME(emilio): I don't think that'd be sound, as it has padding which the
  // compiler is not guaranteed to initialize.
  TextRangeStyle() { Clear(); }

  void Clear() {
    mDefinedStyles = DEFINED_NONE;
    mLineStyle = LineStyle::None;
    mIsBoldLine = false;
    mForegroundColor = mBackgroundColor = mUnderlineColor = NS_RGBA(0, 0, 0, 0);
  }

  bool IsDefined() const { return mDefinedStyles != DEFINED_NONE; }

  bool IsLineStyleDefined() const {
    return (mDefinedStyles & DEFINED_LINESTYLE) != 0;
  }

  bool IsForegroundColorDefined() const {
    return (mDefinedStyles & DEFINED_FOREGROUND_COLOR) != 0;
  }

  bool IsBackgroundColorDefined() const {
    return (mDefinedStyles & DEFINED_BACKGROUND_COLOR) != 0;
  }

  bool IsUnderlineColorDefined() const {
    return (mDefinedStyles & DEFINED_UNDERLINE_COLOR) != 0;
  }

  bool IsNoChangeStyle() const {
    return !IsForegroundColorDefined() && !IsBackgroundColorDefined() &&
           IsLineStyleDefined() && mLineStyle == LineStyle::None;
  }

  bool Equals(const TextRangeStyle& aOther) const {
    if (mDefinedStyles != aOther.mDefinedStyles) return false;
    if (IsLineStyleDefined() && (mLineStyle != aOther.mLineStyle ||
                                 !mIsBoldLine != !aOther.mIsBoldLine))
      return false;
    if (IsForegroundColorDefined() &&
        (mForegroundColor != aOther.mForegroundColor))
      return false;
    if (IsBackgroundColorDefined() &&
        (mBackgroundColor != aOther.mBackgroundColor))
      return false;
    if (IsUnderlineColorDefined() &&
        (mUnderlineColor != aOther.mUnderlineColor))
      return false;
    return true;
  }

  bool operator!=(const TextRangeStyle& aOther) const {
    return !Equals(aOther);
  }

  bool operator==(const TextRangeStyle& aOther) const { return Equals(aOther); }

  uint8_t mDefinedStyles;
  LineStyle mLineStyle;  // DEFINED_LINESTYLE

  bool mIsBoldLine;  // DEFINED_LINESTYLE

  nscolor mForegroundColor;  // DEFINED_FOREGROUND_COLOR
  nscolor mBackgroundColor;  // DEFINED_BACKGROUND_COLOR
  nscolor mUnderlineColor;   // DEFINED_UNDERLINE_COLOR
};

/******************************************************************************
 * mozilla::TextRange
 ******************************************************************************/

enum class TextRangeType : RawTextRangeType {
  eUninitialized = 0x00,
  eCaret = 0x01,
  eRawClause = nsITextInputProcessor::ATTR_RAW_CLAUSE,
  eSelectedRawClause = nsITextInputProcessor::ATTR_SELECTED_RAW_CLAUSE,
  eConvertedClause = nsITextInputProcessor::ATTR_CONVERTED_CLAUSE,
  eSelectedClause = nsITextInputProcessor::ATTR_SELECTED_CLAUSE
};

bool IsValidRawTextRangeValue(RawTextRangeType aRawTextRangeValue);
RawTextRangeType ToRawTextRangeType(TextRangeType aTextRangeType);
TextRangeType ToTextRangeType(RawTextRangeType aRawTextRangeType);
const char* ToChar(TextRangeType aTextRangeType);
SelectionType ToSelectionType(TextRangeType aTextRangeType);

struct TextRange {
  TextRange()
      : mStartOffset(0),
        mEndOffset(0),
        mRangeType(TextRangeType::eUninitialized) {}

  uint32_t mStartOffset;
  // XXX Storing end offset makes the initializing code very complicated.
  //     We should replace it with mLength.
  uint32_t mEndOffset;

  TextRangeStyle mRangeStyle;

  TextRangeType mRangeType;

  uint32_t Length() const { return mEndOffset - mStartOffset; }

  bool IsClause() const { return mRangeType != TextRangeType::eCaret; }

  bool Equals(const TextRange& aOther) const {
    return mStartOffset == aOther.mStartOffset &&
           mEndOffset == aOther.mEndOffset && mRangeType == aOther.mRangeType &&
           mRangeStyle == aOther.mRangeStyle;
  }

  void RemoveCharacter(uint32_t aOffset) {
    if (mStartOffset > aOffset) {
      --mStartOffset;
      --mEndOffset;
    } else if (mEndOffset > aOffset) {
      --mEndOffset;
    }
  }
};

/******************************************************************************
 * mozilla::TextRangeArray
 ******************************************************************************/
class TextRangeArray final : public AutoTArray<TextRange, 10> {
  friend class WidgetCompositionEvent;

  ~TextRangeArray() = default;

  NS_INLINE_DECL_REFCOUNTING(TextRangeArray)

  const TextRange* GetTargetClause() const {
    for (uint32_t i = 0; i < Length(); ++i) {
      const TextRange& range = ElementAt(i);
      if (range.mRangeType == TextRangeType::eSelectedRawClause ||
          range.mRangeType == TextRangeType::eSelectedClause) {
        return &range;
      }
    }
    return nullptr;
  }

  // Returns target clause offset.  If there are selected clauses, this returns
  // the first selected clause offset.  Otherwise, 0.
  uint32_t TargetClauseOffset() const {
    const TextRange* range = GetTargetClause();
    return range ? range->mStartOffset : 0;
  }

  // Returns target clause length.  If there are selected clauses, this returns
  // the first selected clause length.  Otherwise, UINT32_MAX.
  uint32_t TargetClauseLength() const {
    const TextRange* range = GetTargetClause();
    return range ? range->Length() : UINT32_MAX;
  }

 public:
  bool IsComposing() const {
    for (uint32_t i = 0; i < Length(); ++i) {
      if (ElementAt(i).IsClause()) {
        return true;
      }
    }
    return false;
  }

  bool Equals(const TextRangeArray& aOther) const {
    size_t len = Length();
    if (len != aOther.Length()) {
      return false;
    }
    for (size_t i = 0; i < len; i++) {
      if (!ElementAt(i).Equals(aOther.ElementAt(i))) {
        return false;
      }
    }
    return true;
  }

  void RemoveCharacter(uint32_t aOffset) {
    for (size_t i = 0, len = Length(); i < len; i++) {
      ElementAt(i).RemoveCharacter(aOffset);
    }
  }

  bool HasCaret() const {
    for (const TextRange& range : *this) {
      if (range.mRangeType == TextRangeType::eCaret) {
        return true;
      }
    }
    return false;
  }

  bool HasClauses() const {
    for (const TextRange& range : *this) {
      if (range.IsClause()) {
        return true;
      }
    }
    return false;
  }

  uint32_t GetCaretPosition() const {
    for (const TextRange& range : *this) {
      if (range.mRangeType == TextRangeType::eCaret) {
        return range.mStartOffset;
      }
    }
    return UINT32_MAX;
  }

  const TextRange* GetFirstClause() const {
    for (const TextRange& range : *this) {
      // Look for the range of a clause whose start offset is 0 because the
      // first clause's start offset is always 0.
      if (range.IsClause() && !range.mStartOffset) {
        return &range;
      }
    }
    MOZ_ASSERT(!HasClauses());
    return nullptr;
  }
};

}  // namespace mozilla

#endif  // mozilla_TextRage_h_
