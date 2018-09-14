/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.dataprotect

import android.util.Base64

val B64_FLAGS = Base64.URL_SAFE or Base64.NO_PADDING or Base64.NO_PADDING
val KEYSTORE_LABEL = "samples-dataprotect"