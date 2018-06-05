/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package android.view.inputmethod;

/**
 * This dummy class is used when running tests on Android versions prior to 21,
 * when the CursorAnchorInfo class was first introduced. Without this class,
 * tests will crash with ClassNotFoundException when the test rule uses reflection
 * to access the TextInputDelegate interface.
 */
public class CursorAnchorInfo {
}
