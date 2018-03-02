/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.test;

import org.mozilla.geckoview.GeckoSession;

import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class NavigationTests extends BaseGeckoViewTest {
    @Test
    public void testLoadUri() {
        loadTestPath("hello.html", new Runnable() {
            @Override public void run() {
                done();
            }
        });

        waitUntilDone();
    }

    @Test
    public void testGoBack() {
        final String startPath = "hello.html";
        loadTestPath(startPath, new Runnable() {
            @Override public void run() {
                loadTestPath("hello2.html", new Runnable() {
                    @Override public void run() {
                        mSession.setNavigationDelegate(new GeckoSession.NavigationDelegate() {
                            @Override
                            public void onLocationChange(GeckoSession session, String url) {
                                assertTrue("URL should end with " + startPath + ", got " + url, url.endsWith(startPath));
                                done();
                            }

                            @Override
                            public void onCanGoBack(GeckoSession session, boolean canGoBack) {
                                assertFalse("Should not be able to go back", canGoBack);
                            }

                            @Override
                            public void onCanGoForward(GeckoSession session, boolean canGoForward) {
                                assertTrue("Should be able to go forward", canGoForward);
                            }

                            @Override
                            public boolean onLoadUri(GeckoSession session, String uri, TargetWindow where) {
                                return false;
                            }

                            @Override
                            public void onNewSession(GeckoSession session, String uri, GeckoSession.Response<GeckoSession> response) {
                                response.respond(null);
                            }
                        });

                        mSession.goBack();
                    }
                });
            }
        });

        waitUntilDone();
    }

    @Test
    public void testReload() {
        loadTestPath("hello.html", new Runnable() {
            @Override public void run() {
                mSession.setProgressDelegate(new GeckoSession.ProgressDelegate() {
                    @Override
                    public void onPageStart(GeckoSession session, String url) {
                    }

                    @Override
                    public void onPageStop(GeckoSession session, boolean success) {
                        assertTrue(success);
                        done();
                    }

                    @Override
                    public void onSecurityChange(GeckoSession session, SecurityInformation securityInfo) {

                    }
                });

                mSession.reload();
            }
        });

        waitUntilDone();
    }

    @Test
    public void testExpiredCert() {
        mSession.setProgressDelegate(new GeckoSession.ProgressDelegate() {
            private boolean mNotBlank;

            @Override
            public void onPageStart(GeckoSession session, String url) {
                mNotBlank = !url.equals("about:blank");
            }

            @Override
            public void onPageStop(GeckoSession session, boolean success) {
                if (mNotBlank) {
                    assertFalse("Expected unsuccessful page load", success);
                    done();
                }
            }

            @Override
            public void onSecurityChange(GeckoSession session, SecurityInformation securityInfo) {
                assertFalse(securityInfo.isSecure);
                assertEquals(securityInfo.securityMode, SecurityInformation.SECURITY_MODE_UNKNOWN);
            }
        });

        mSession.loadUri("https://expired.badssl.com/");
        waitUntilDone();
    }

    @Test
    public void testValidTLS() {
        mSession.setProgressDelegate(new GeckoSession.ProgressDelegate() {
            private boolean mNotBlank;

            @Override
            public void onPageStart(GeckoSession session, String url) {
                mNotBlank = !url.equals("about:blank");
            }

            @Override
            public void onPageStop(GeckoSession session, boolean success) {
                if (mNotBlank) {
                    assertTrue("Expected successful page load", success);
                    done();
                }
            }

            @Override
            public void onSecurityChange(GeckoSession session, SecurityInformation securityInfo) {
                assertTrue(securityInfo.isSecure);
                assertEquals(securityInfo.securityMode, SecurityInformation.SECURITY_MODE_IDENTIFIED);
            }
        });

        mSession.loadUri("https://mozilla-modern.badssl.com/");
        waitUntilDone();
    }

    @Test
    public void testOnNewSession() {
        mSession.setNavigationDelegate(new GeckoSession.NavigationDelegate() {
            @Override
            public void onLocationChange(GeckoSession session, String url) {
            }

            @Override
            public void onCanGoBack(GeckoSession session, boolean canGoBack) {

            }

            @Override
            public void onCanGoForward(GeckoSession session, boolean canGoForward) {

            }

            @Override
            public boolean onLoadUri(GeckoSession session, String uri, TargetWindow where) {
                return false;
            }

            @Override
            public void onNewSession(GeckoSession session, String uri, GeckoSession.Response<GeckoSession> response) {
                final GeckoSession newSession = new GeckoSession(session.getSettings());
                newSession.setContentDelegate(new GeckoSession.ContentDelegate() {
                    @Override
                    public void onTitleChange(GeckoSession session, String title) {

                    }

                    @Override
                    public void onFocusRequest(GeckoSession session) {

                    }

                    @Override
                    public void onCloseRequest(GeckoSession session) {
                        session.closeWindow();
                        done();
                    }

                    @Override
                    public void onFullScreen(GeckoSession session, boolean fullScreen) {

                    }

                    @Override
                    public void onContextMenu(GeckoSession session, int screenX, int screenY, String uri, String elementSrc) {

                    }
                });

                newSession.openWindow(InstrumentationRegistry.getTargetContext());
                response.respond(newSession);
            }
        });

        mSession.setProgressDelegate(new GeckoSession.ProgressDelegate() {
            @Override
            public void onPageStart(GeckoSession session, String url) {

            }

            @Override
            public void onPageStop(GeckoSession session, boolean success) {
                // Send a click to open the window
                sendClick(100, 100);
            }

            @Override
            public void onSecurityChange(GeckoSession session, SecurityInformation securityInfo) {

            }
        });


        mSession.loadUri(buildAssetUrl("newSession.html"));

        waitUntilDone();
    }

    @Test(expected = IllegalArgumentException.class)
    public void testOnNewSessionNoExisting() {
        // This makes sure that we get an exception if you try to return
        // an existing GeckoSession instance from the NavigationDelegate.onNewSession()
        // implementation.

        mSession.setNavigationDelegate(new GeckoSession.NavigationDelegate() {
            @Override
            public void onLocationChange(GeckoSession session, String url) {
            }

            @Override
            public void onCanGoBack(GeckoSession session, boolean canGoBack) {

            }

            @Override
            public void onCanGoForward(GeckoSession session, boolean canGoForward) {

            }

            @Override
            public boolean onLoadUri(GeckoSession session, String uri, TargetWindow where) {
                return false;
            }

            @Override
            public void onNewSession(GeckoSession session, String uri, GeckoSession.Response<GeckoSession> response) {
                // This is where the throw should occur
                response.respond(mSession);
            }
        });

        mSession.setProgressDelegate(new GeckoSession.ProgressDelegate() {
            @Override
            public void onPageStart(GeckoSession session, String url) {

            }

            @Override
            public void onPageStop(GeckoSession session, boolean success) {
                sendClick(100, 100);
            }

            @Override
            public void onSecurityChange(GeckoSession session, SecurityInformation securityInfo) {

            }
        });


        mSession.loadUri(buildAssetUrl("newSession.html"));
        waitUntilDone();
    }
}
