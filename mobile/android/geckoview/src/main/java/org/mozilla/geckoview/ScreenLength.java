/* License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * ScreenLength is a class that represents a length on the screen using different units. The default
 * unit is a pixel. However lengths may be also represented by a dimension of the visual viewport or
 * of the full scroll size of the root document.
 */
public class ScreenLength {
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({PIXEL, VISUAL_VIEWPORT_WIDTH, VISUAL_VIEWPORT_HEIGHT, DOCUMENT_WIDTH, DOCUMENT_HEIGHT})
  public @interface ScreenLengthType {}

  /** Pixel units. */
  public static final int PIXEL = 0;

  /**
   * Units are in visual viewport width. If the visual viewport is 100 pixels wide, then a value of
   * 2.0 would represent a length of 200 pixels.
   *
   * @see <a href="https://developer.mozilla.org/en-US/docs/Glossary/Visual_Viewport">MDN Visual
   *     Viewport</a>
   */
  public static final int VISUAL_VIEWPORT_WIDTH = 1;

  /**
   * Units are in visual viewport height. If the visual viewport is 100 pixels high, then a value of
   * 2.0 would represent a length of 200 pixels.
   *
   * @see <a href="https://developer.mozilla.org/en-US/docs/Glossary/Visual_Viewport">MDN Visual
   *     Viewport</a>
   */
  public static final int VISUAL_VIEWPORT_HEIGHT = 2;

  /**
   * Units represent the entire scrollable documents width. If the document is 1000 pixels wide then
   * a value of 1.0 would represent 1000 pixels.
   */
  public static final int DOCUMENT_WIDTH = 3;

  /**
   * Units represent the entire scrollable documents height. If the document is 1000 pixels tall
   * then a value of 1.0 would represent 1000 pixels.
   */
  public static final int DOCUMENT_HEIGHT = 4;

  /**
   * Create a ScreenLength of zero pixels length. Type is {@link #PIXEL}.
   *
   * @return ScreenLength of zero length.
   */
  @NonNull
  @AnyThread
  public static ScreenLength zero() {
    return new ScreenLength(0.0, PIXEL);
  }

  /**
   * Create a ScreenLength of zero pixels length. Type is {@link #PIXEL}. Can be used to scroll to
   * the top of a page when used with PanZoomController.scrollTo()
   *
   * @return ScreenLength of zero length.
   */
  @NonNull
  @AnyThread
  public static ScreenLength top() {
    return zero();
  }

  /**
   * Create a ScreenLength of the documents height. Type is {@link #DOCUMENT_HEIGHT}. Can be used to
   * scroll to the bottom of a page when used with {@link PanZoomController#scrollTo(ScreenLength,
   * ScreenLength)}
   *
   * @return ScreenLength of document height.
   */
  @NonNull
  @AnyThread
  public static ScreenLength bottom() {
    return new ScreenLength(1.0, DOCUMENT_HEIGHT);
  }

  /**
   * Create a ScreenLength of a specific length. Type is {@link #PIXEL}.
   *
   * @param value Pixel length.
   * @return ScreenLength of document height.
   */
  @NonNull
  @AnyThread
  public static ScreenLength fromPixels(final double value) {
    return new ScreenLength(value, PIXEL);
  }

  /**
   * Create a ScreenLength that uses the visual viewport width as units. Type is {@link
   * #VISUAL_VIEWPORT_WIDTH}. Can be used with {@link PanZoomController#scrollBy(ScreenLength,
   * ScreenLength)} to scroll a value of the width of visual viewport content.
   *
   * @param value Factor used to calculate length. A value of 2.0 would indicate a length twice as
   *     long as the length of the visual viewports width.
   * @return ScreenLength of specifying a length of value * visual viewport width.
   */
  @NonNull
  @AnyThread
  public static ScreenLength fromVisualViewportWidth(final double value) {
    return new ScreenLength(value, VISUAL_VIEWPORT_WIDTH);
  }

  /**
   * Create a ScreenLength that uses the visual viewport width as units. Type is {@link
   * #VISUAL_VIEWPORT_HEIGHT}. Can be used with {@link PanZoomController#scrollBy(ScreenLength,
   * ScreenLength)} to scroll a value of the height of visual viewport content.
   *
   * @param value Factor used to calculate length. A value of 2.0 would indicate a length twice as
   *     long as the length of the visual viewports height.
   * @return ScreenLength of specifying a length of value * visual viewport width.
   */
  @NonNull
  @AnyThread
  public static ScreenLength fromVisualViewportHeight(final double value) {
    return new ScreenLength(value, VISUAL_VIEWPORT_HEIGHT);
  }

  private final double mValue;
  @ScreenLengthType private final int mType;

  /* package */ ScreenLength(final double value, @ScreenLengthType final int type) {
    mValue = value;
    mType = type;
  }

  /**
   * Returns the scalar value used to calculate length. The units of the returned valued are defined
   * by what is returned by {@link #getType()}
   *
   * @return Scalar value of the length.
   */
  @AnyThread
  public double getValue() {
    return mValue;
  }

  /**
   * Returns the unit type of the length The length can be one of the following: {@link #PIXEL},
   * {@link #VISUAL_VIEWPORT_WIDTH}, {@link #VISUAL_VIEWPORT_HEIGHT}, {@link #DOCUMENT_WIDTH},
   * {@link #DOCUMENT_HEIGHT}
   *
   * @return Unit type of the length.
   */
  @AnyThread
  @ScreenLengthType
  public int getType() {
    return mType;
  }
}
