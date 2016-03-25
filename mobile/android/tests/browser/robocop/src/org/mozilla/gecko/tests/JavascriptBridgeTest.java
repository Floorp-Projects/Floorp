/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import static org.mozilla.gecko.tests.helpers.AssertionHelper.*;

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

    private static final long WAIT_GET_FROM_JS_MILLIS = 20000;

    private JavascriptBridge js;

    // Feel free to implement additional return types.
    private boolean isAsyncValueSet;
    private String asyncValueStr;

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

    /**
     * Used to retrieve values from js when it's required to call async methods (e.g. promises).
     * This method will block until the value is retrieved else timeout.
     *
     * This method is not thread-safe.
     *
     * Ideally, we could just have Javascript call Java when the callback completes but Java won't
     * listen for messages unless we call into JS again (bug 1253467).
     *
     * To use this method:
     *   * Call this method with a name argument, henceforth known as `varName`. Note that it will be capitalized
     * in all function names.
     *   * Create a js function, `"getAsync" + varName` (e.g. if `varName == "clientId`, the function is
     * `getAsyncClientId`) of no args. This function should call the async get method and assign a global variable to
     * the return value.
     *   * Create a js function, `"pollGetAsync" + varName` (e.g. `pollGetAsyncClientId`) of no args. It should call
     * `java.asyncCall('blockingFromJsResponseString', ...` with two args: a boolean if the async value has been set yet
     * and a String with the global return value (`null` or `undefined` are acceptable if the value has not been set).
     */
    public String getBlockingFromJsString(final String varName) {
        isAsyncValueSet = false;
        final String fnSuffix = capitalize(varName);
        getJS().syncCall("getAsync" + fnSuffix); // Initiate async callback

        final long timeoutMillis = System.currentTimeMillis() + WAIT_GET_FROM_JS_MILLIS;
        do {
            // Avoid sleeping! The async callback may have already completed so
            // we test for completion here, rather than in the loop predicate.
            getJS().syncCall("pollGetAsync" + fnSuffix);
            if (isAsyncValueSet) {
                break;
            }

            if (System.currentTimeMillis() > timeoutMillis) {
                fFail("Retrieving " + varName + " from JS has timed out");
            }
            try {
                Thread.sleep(500, 0); // Give time for JS to complete its operation. (emulator one core?)
            } catch (final InterruptedException e) { }
        } while (true);

        return asyncValueStr;
    }

    public void blockingFromJsResponseString(final boolean isValueSet, final String value) {
        this.isAsyncValueSet = isValueSet;
        this.asyncValueStr = value;
    }

    private String capitalize(final String str) {
        return str.substring(0, 1).toUpperCase() + str.substring(1);
    }
}
