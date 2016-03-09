/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.tests.helpers.GeckoHelper;
import org.mozilla.gecko.tests.helpers.JavascriptBridge;
import org.mozilla.gecko.tests.helpers.NavigationHelper;

/**
 * Extended to write tests using JavascriptBridge, which allows Java and JS to communicate back-and-forth.
 * If you don't need back-and-forth communication, consider {@link JavascriptTest}.
 *
 * To write a test:
 *   * Extend this class
 *   * Add your javascript file to the base robocop directory (see where `testJavascriptBridge.js` is located)
 *   * In the main test method, call {@link #blockForReadyAndLoadJS(String)} with your javascript file name
 * (don't include the path) or if you're loading a non-harness url, be sure to call {@link GeckoHelper#blockForReady()}
 *   * You can access js calls via the {@link #getJS()} method
 *     - Read {@link JavascriptBridge} javadoc for more information about using the API.
 */
public class JavascriptBridgeTest extends UITest {

    private JavascriptBridge js;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        js = new JavascriptBridge(this);
    }

    @Override
    public void tearDown() throws Exception {
        js.disconnect();
        super.tearDown();
    }

    public JavascriptBridge getJS() {
        return js;
    }

    protected void blockForReadyAndLoadJS(final String jsFilename) {
        NavigationHelper.enterAndLoadUrl(mStringHelper.getHarnessUrlForJavascript(jsFilename));
    }
}
