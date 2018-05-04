/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This package contains the public interfaces for the library.
 *
 * <ul>
 * <li>
 *     {@link org.mozilla.geckoview.GeckoRuntime} is the entry point for starting and initializing
 *     Gecko. You can use this to preload Gecko before you need to load a page or to configure features
 *     such as crash reporting.
 * </li>
 *
 * <li>
 *     {@link org.mozilla.geckoview.GeckoSession} is where most interesting work happens, such as
 *     loading pages. It relies on {@link org.mozilla.geckoview.GeckoRuntime}
 *     to talk to Gecko.
 * </li>
 *
 * <li>
 *     {@link org.mozilla.geckoview.GeckoView} is the embeddable {@link android.view.View}. This is
 *     the most common way of getting a {@link org.mozilla.geckoview.GeckoSession} onto the screen.
 * </li>
 * </ul>
 *
 * <p>
 * <strong>Permissions</strong>
 * <p>
 * This library does not request any dangerous permissions in the manifest, though it's possible
 * that some web features may require them. For instance, WebRTC video calls would need the
 * {@link android.Manifest.permission#CAMERA} and {@link android.Manifest.permission#RECORD_AUDIO}
 * permissions. Declaring these are at the application's discretion. If you want full web
 * functionality, the following permissions should be declared:
 *
 * <ul>
 *     <li>{@link android.Manifest.permission#ACCESS_COARSE_LOCATION}</li>
 *     <li>{@link android.Manifest.permission#ACCESS_FINE_LOCATION}</li>
 *     <li>{@link android.Manifest.permission#READ_EXTERNAL_STORAGE}</li>
 *     <li>{@link android.Manifest.permission#WRITE_EXTERNAL_STORAGE}</li>
 *     <li>{@link android.Manifest.permission#CAMERA}</li>
 *     <li>{@link android.Manifest.permission#RECORD_AUDIO}</li>
 * </ul>
 *
 */
package org.mozilla.geckoview;
