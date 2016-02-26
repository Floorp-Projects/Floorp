/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import android.content.ContentResolver;
import org.mozilla.gecko.annotation.RobocopTarget;

public interface UrlAnnotations {
    @RobocopTarget void insertAnnotation(ContentResolver cr, String url, String key, String value);
}
