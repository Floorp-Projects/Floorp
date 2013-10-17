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
public abstract class Environment extends EnvironmentV1 {
  // Version 2 adds osLocale, appLocale, acceptLangSet, and distribution.
  public static final int CURRENT_VERSION = 2;

  public String osLocale;                // The Android OS "Locale" value.
  public String appLocale;
  public int acceptLangSet;
  public String distribution;            // ID + version. Typically empty.

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

    // v2.
    appender.append(osLocale);
    appender.append(appLocale);
    appender.append(acceptLangSet);
    appender.append(distribution);
  }
}
