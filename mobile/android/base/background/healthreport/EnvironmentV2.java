/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

public abstract class EnvironmentV2 extends EnvironmentV1 {
  private static final int VERSION = 2;

  public String osLocale;
  public String appLocale;
  public int acceptLangSet;
  public String distribution;

  public EnvironmentV2(Class<? extends EnvironmentAppender> appenderClass) {
    super(appenderClass);
    version = VERSION;
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