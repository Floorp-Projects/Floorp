/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.mozilla.thirdparty.com.google.android.exoplayer2.text.webvtt;

import android.graphics.Typeface;
import android.text.Layout;
import android.text.TextUtils;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.Util;
import java.lang.annotation.Documented;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import org.checkerframework.checker.nullness.qual.EnsuresNonNull;

/**
 * Style object of a Css style block in a Webvtt file.
 *
 * @see <a href="https://w3c.github.io/webvtt/#applying-css-properties">W3C specification - Apply
 *     CSS properties</a>
 */
public final class WebvttCssStyle {

  public static final int UNSPECIFIED = -1;

  /**
   * Style flag enum. Possible flag values are {@link #UNSPECIFIED}, {@link #STYLE_NORMAL}, {@link
   * #STYLE_BOLD}, {@link #STYLE_ITALIC} and {@link #STYLE_BOLD_ITALIC}.
   */
  @Documented
  @Retention(RetentionPolicy.SOURCE)
  @IntDef(
      flag = true,
      value = {UNSPECIFIED, STYLE_NORMAL, STYLE_BOLD, STYLE_ITALIC, STYLE_BOLD_ITALIC})
  public @interface StyleFlags {}

  public static final int STYLE_NORMAL = Typeface.NORMAL;
  public static final int STYLE_BOLD = Typeface.BOLD;
  public static final int STYLE_ITALIC = Typeface.ITALIC;
  public static final int STYLE_BOLD_ITALIC = Typeface.BOLD_ITALIC;

  /**
   * Font size unit enum. One of {@link #UNSPECIFIED}, {@link #FONT_SIZE_UNIT_PIXEL}, {@link
   * #FONT_SIZE_UNIT_EM} or {@link #FONT_SIZE_UNIT_PERCENT}.
   */
  @Documented
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({UNSPECIFIED, FONT_SIZE_UNIT_PIXEL, FONT_SIZE_UNIT_EM, FONT_SIZE_UNIT_PERCENT})
  public @interface FontSizeUnit {}

  public static final int FONT_SIZE_UNIT_PIXEL = 1;
  public static final int FONT_SIZE_UNIT_EM = 2;
  public static final int FONT_SIZE_UNIT_PERCENT = 3;

  @Documented
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({UNSPECIFIED, OFF, ON})
  private @interface OptionalBoolean {}

  private static final int OFF = 0;
  private static final int ON = 1;

  // Selector properties.
  private String targetId;
  private String targetTag;
  private List<String> targetClasses;
  private String targetVoice;

  // Style properties.
  @Nullable private String fontFamily;
  private int fontColor;
  private boolean hasFontColor;
  private int backgroundColor;
  private boolean hasBackgroundColor;
  @OptionalBoolean private int linethrough;
  @OptionalBoolean private int underline;
  @OptionalBoolean private int bold;
  @OptionalBoolean private int italic;
  @FontSizeUnit private int fontSizeUnit;
  private float fontSize;
  @Nullable private Layout.Alignment textAlign;

  // Calling reset() is forbidden because `this` isn't initialized. This can be safely suppressed
  // because reset() only assigns fields, it doesn't read any.
  @SuppressWarnings("nullness:method.invocation.invalid")
  public WebvttCssStyle() {
    reset();
  }

  @EnsuresNonNull({"targetId", "targetTag", "targetClasses", "targetVoice"})
  public void reset() {
    targetId = "";
    targetTag = "";
    targetClasses = Collections.emptyList();
    targetVoice = "";
    fontFamily = null;
    hasFontColor = false;
    hasBackgroundColor = false;
    linethrough = UNSPECIFIED;
    underline = UNSPECIFIED;
    bold = UNSPECIFIED;
    italic = UNSPECIFIED;
    fontSizeUnit = UNSPECIFIED;
    textAlign = null;
  }

  public void setTargetId(String targetId) {
    this.targetId  = targetId;
  }

  public void setTargetTagName(String targetTag) {
    this.targetTag = targetTag;
  }

  public void setTargetClasses(String[] targetClasses) {
    this.targetClasses = Arrays.asList(targetClasses);
  }

  public void setTargetVoice(String targetVoice) {
    this.targetVoice = targetVoice;
  }

  /**
   * Returns a value in a score system compliant with the CSS Specificity rules.
   *
   * @see <a href="https://www.w3.org/TR/CSS2/cascade.html">CSS Cascading</a>
   *     <p>The score works as follows:
   *     <ul>
   *       <li>Id match adds 0x40000000 to the score.
   *       <li>Each class and voice match adds 4 to the score.
   *       <li>Tag matching adds 2 to the score.
   *       <li>Universal selector matching scores 1.
   *     </ul>
   *
   * @param id The id of the cue if present, {@code null} otherwise.
   * @param tag Name of the tag, {@code null} if it refers to the entire cue.
   * @param classes An array containing the classes the tag belongs to. Must not be null.
   * @param voice Annotated voice if present, {@code null} otherwise.
   * @return The score of the match, zero if there is no match.
   */
  public int getSpecificityScore(
      @Nullable String id, @Nullable String tag, String[] classes, @Nullable String voice) {
    if (targetId.isEmpty() && targetTag.isEmpty() && targetClasses.isEmpty()
        && targetVoice.isEmpty()) {
      // The selector is universal. It matches with the minimum score if and only if the given
      // element is a whole cue.
      return TextUtils.isEmpty(tag) ? 1 : 0;
    }
    int score = 0;
    score = updateScoreForMatch(score, targetId, id, 0x40000000);
    score = updateScoreForMatch(score, targetTag, tag, 2);
    score = updateScoreForMatch(score, targetVoice, voice, 4);
    if (score == -1 || !Arrays.asList(classes).containsAll(targetClasses)) {
      return 0;
    } else {
      score += targetClasses.size() * 4;
    }
    return score;
  }

  /**
   * Returns the style or {@link #UNSPECIFIED} when no style information is given.
   *
   * @return {@link #UNSPECIFIED}, {@link #STYLE_NORMAL}, {@link #STYLE_BOLD}, {@link #STYLE_BOLD}
   *     or {@link #STYLE_BOLD_ITALIC}.
   */
  @StyleFlags public int getStyle() {
    if (bold == UNSPECIFIED && italic == UNSPECIFIED) {
      return UNSPECIFIED;
    }
    return (bold == ON ? STYLE_BOLD : STYLE_NORMAL)
        | (italic == ON ? STYLE_ITALIC : STYLE_NORMAL);
  }

  public boolean isLinethrough() {
    return linethrough == ON;
  }

  public WebvttCssStyle setLinethrough(boolean linethrough) {
    this.linethrough = linethrough ? ON : OFF;
    return this;
  }

  public boolean isUnderline() {
    return underline == ON;
  }

  public WebvttCssStyle setUnderline(boolean underline) {
    this.underline = underline ? ON : OFF;
    return this;
  }
  public WebvttCssStyle setBold(boolean bold) {
    this.bold = bold ? ON : OFF;
    return this;
  }

  public WebvttCssStyle setItalic(boolean italic) {
    this.italic = italic ? ON : OFF;
    return this;
  }

  @Nullable
  public String getFontFamily() {
    return fontFamily;
  }

  public WebvttCssStyle setFontFamily(@Nullable String fontFamily) {
    this.fontFamily = Util.toLowerInvariant(fontFamily);
    return this;
  }

  public int getFontColor() {
    if (!hasFontColor) {
      throw new IllegalStateException("Font color not defined");
    }
    return fontColor;
  }

  public WebvttCssStyle setFontColor(int color) {
    this.fontColor = color;
    hasFontColor = true;
    return this;
  }

  public boolean hasFontColor() {
    return hasFontColor;
  }

  public int getBackgroundColor() {
    if (!hasBackgroundColor) {
      throw new IllegalStateException("Background color not defined.");
    }
    return backgroundColor;
  }

  public WebvttCssStyle setBackgroundColor(int backgroundColor) {
    this.backgroundColor = backgroundColor;
    hasBackgroundColor = true;
    return this;
  }

  public boolean hasBackgroundColor() {
    return hasBackgroundColor;
  }

  @Nullable
  public Layout.Alignment getTextAlign() {
    return textAlign;
  }

  public WebvttCssStyle setTextAlign(@Nullable Layout.Alignment textAlign) {
    this.textAlign = textAlign;
    return this;
  }

  public WebvttCssStyle setFontSize(float fontSize) {
    this.fontSize = fontSize;
    return this;
  }

  public WebvttCssStyle setFontSizeUnit(short unit) {
    this.fontSizeUnit = unit;
    return this;
  }

  @FontSizeUnit public int getFontSizeUnit() {
    return fontSizeUnit;
  }

  public float getFontSize() {
    return fontSize;
  }

  public void cascadeFrom(WebvttCssStyle style) {
    if (style.hasFontColor) {
      setFontColor(style.fontColor);
    }
    if (style.bold != UNSPECIFIED) {
      bold = style.bold;
    }
    if (style.italic != UNSPECIFIED) {
      italic = style.italic;
    }
    if (style.fontFamily != null) {
      fontFamily = style.fontFamily;
    }
    if (linethrough == UNSPECIFIED) {
      linethrough = style.linethrough;
    }
    if (underline == UNSPECIFIED) {
      underline = style.underline;
    }
    if (textAlign == null) {
      textAlign = style.textAlign;
    }
    if (fontSizeUnit == UNSPECIFIED) {
      fontSizeUnit = style.fontSizeUnit;
      fontSize = style.fontSize;
    }
    if (style.hasBackgroundColor) {
      setBackgroundColor(style.backgroundColor);
    }
  }

  private static int updateScoreForMatch(
      int currentScore, String target, @Nullable String actual, int score) {
    if (target.isEmpty() || currentScore == -1) {
      return currentScore;
    }
    return target.equals(actual) ? currentScore + score : -1;
  }

}
