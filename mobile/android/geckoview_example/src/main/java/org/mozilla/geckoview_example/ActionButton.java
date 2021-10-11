/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview_example;

import android.graphics.Bitmap;

public class ActionButton {
  final Bitmap icon;
  final String text;
  final Integer textColor;
  final Integer backgroundColor;

  public ActionButton(
      final Bitmap icon,
      final String text,
      final Integer textColor,
      final Integer backgroundColor) {
    this.icon = icon;
    this.text = text;
    this.textColor = textColor;
    this.backgroundColor = backgroundColor;
  }
}
