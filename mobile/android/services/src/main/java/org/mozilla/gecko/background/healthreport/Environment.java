/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

/**
 * This captures all of the details that define an 'environment' for FHR's purposes.
 * Whenever this format changes, it'll be changing with a build ID, so no migration
 * of values is needed.
 *
 * Unless you remove the build descriptors from the set, of course.
 *
 * Or store these in a database.
 *
 * Instances of this class should be considered "effectively immutable": control their
 * scope such that clear creation/sharing boundaries exist. Once you've populated and
 * registered an <code>Environment</code>, don't do so again; start from scratch.
 *
 */
public abstract class Environment extends EnvironmentV2 {
  // Version 2 adds osLocale, appLocale, acceptLangSet, and distribution.
  // Version 3 adds device characteristics.
  public static final int CURRENT_VERSION = 3;

  public static enum UIType {
    // Corresponds to the typical phone interface.
    DEFAULT("default"),

    // Corresponds to a device for which Fennec is displaying the large tablet UI.
    LARGE_TABLET("largetablet"),

    // Corresponds to a device for which Fennec is displaying the small tablet UI.
    SMALL_TABLET("smalltablet");

    private final String label;

    private UIType(final String label) {
      this.label = label;
    }

    public String toString() {
      return this.label;
    }

    public static UIType fromLabel(final String label) {
      for (UIType type : UIType.values()) {
        if (type.label.equals(label)) {
          return type;
        }
      }

      throw new IllegalArgumentException("Bad enum value: " + label);
    }
  }

  public UIType uiType = UIType.DEFAULT;

  /**
   * Mask of Configuration#uiMode. E.g., UI_MODE_TYPE_CAR.
   */
  public int uiMode = 0;     // UI_MODE_TYPE_UNDEFINED = 0

  /**
   * Computed physical dimensions in millimeters.
   */
  public int screenXInMM;
  public int screenYInMM;

  /**
   * One of the Configuration#SCREENLAYOUT_SIZE_* constants.
   */
  public int screenLayout = 0;  // SCREENLAYOUT_SIZE_UNDEFINED = 0

  public boolean hasHardwareKeyboard;

  public Environment() {
    this(Environment.HashAppender.class);
  }

  public Environment(Class<? extends EnvironmentAppender> appenderClass) {
    super(appenderClass);
    version = CURRENT_VERSION;
  }

  @Override
  protected void appendHash(EnvironmentAppender appender) {
    super.appendHash(appender);

    // v3.
    appender.append(hasHardwareKeyboard ? 1 : 0);
    appender.append(uiType.toString());
    appender.append(uiMode);
    appender.append(screenLayout);
    appender.append(screenXInMM);
    appender.append(screenYInMM);
  }
}
