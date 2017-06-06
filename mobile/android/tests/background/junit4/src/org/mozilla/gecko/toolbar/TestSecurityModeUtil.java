/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.toolbar;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.SiteIdentity;
import org.mozilla.gecko.SiteIdentity.MixedMode;
import org.mozilla.gecko.SiteIdentity.SecurityMode;
import org.mozilla.gecko.SiteIdentity.TrackingMode;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mozilla.gecko.toolbar.SecurityModeUtil.IconType;
import static org.mozilla.gecko.toolbar.SecurityModeUtil.resolve;

@RunWith(TestRunner.class)
public class TestSecurityModeUtil {

    private SiteIdentity identity;

    @Before
    public void setUp() {
        identity = spy(new SiteIdentity());
    }

    /**
     * To test resolve function if there is not any SiteIdentity
     */
    @Test
    public void testNoSiteIdentity() {
        assertEquals(IconType.UNKNOWN, resolve(null));
        assertEquals(IconType.SEARCH, resolve(null, "about:home"));
        assertEquals(IconType.UNKNOWN, resolve(null, "about:firefox"));
        assertEquals(IconType.UNKNOWN, resolve(null, "https://mozilla.com"));
    }

    /**
     * To test resolve function for SecurityException.
     * SecurityException detection is prior than SecurityMode, TrackingMode and MixedMode.
     * If SecurityException exists, it should keep returning WARNING until url is "about:home".
     */
    @Test
    public void testSecurityException() {
        // SecurityException exists for each of below cases
        doReturn(true).when(identity).isSecurityException();
        doReturn(SecurityMode.UNKNOWN).when(identity).getSecurityMode();

        // about:home always show Search Icon
        assertEquals(IconType.SEARCH, resolve(identity, "about:home"));

        // other cases, return WARNING
        assertEquals(IconType.WARNING, resolve(identity));
        assertEquals(IconType.WARNING, resolve(identity, "https://mozilla.com"));

        // even about:* pages, they can still load external sites.
        assertEquals(IconType.WARNING, resolve(identity, "about:firefox"));
        // even specify ChromeUI, still respect SecurityException
        doReturn(SecurityMode.CHROMEUI).when(identity).getSecurityMode();
        assertEquals(IconType.WARNING, resolve(identity, "about:firefox"));

        // TrackingMode does not matter
        doReturn(TrackingMode.TRACKING_CONTENT_BLOCKED).when(identity).getTrackingMode();
        assertEquals(IconType.WARNING, resolve(identity));
        doReturn(TrackingMode.TRACKING_CONTENT_LOADED).when(identity).getTrackingMode();
        assertEquals(IconType.WARNING, resolve(identity));

        // MixedModeDisplay does not matter
        doReturn(MixedMode.LOADED).when(identity).getMixedModeDisplay();
        assertEquals(IconType.WARNING, resolve(identity));

        doReturn(MixedMode.BLOCKED).when(identity).getMixedModeDisplay();
        assertEquals(IconType.WARNING, resolve(identity));

        // MixedModeActive does not matter
        doReturn(MixedMode.LOADED).when(identity).getMixedModeActive();
        assertEquals(IconType.WARNING, resolve(identity));

        doReturn(MixedMode.BLOCKED).when(identity).getMixedModeActive();
        assertEquals(IconType.WARNING, resolve(identity));

    }

    /**
     * To test resolve function for TrackingContentLoaded, without SecurityException.
     * TrackingMode detection is prior than SecurityMode and MixedMode.
     * If TrackingMode is TRACKING_CONTENT_LOADED, it should keep returning TRACKING_CONTENT_LOADED
     * icon until url is "about:home".
     */
    @Test
    public void testTrackingContentLoaded() {
        doReturn(false).when(identity).isSecurityException();

        // enable TRACKING_CONTENT_LOADED
        doReturn(TrackingMode.TRACKING_CONTENT_LOADED).when(identity).getTrackingMode();

        // about:home always show Search Icon
        doReturn(SecurityMode.UNKNOWN).when(identity).getSecurityMode();
        assertEquals(IconType.SEARCH, resolve(identity, "about:home"));

        // otherwise, return icon for TRACKING_CONTENT_LOADED
        assertEquals(IconType.TRACKING_CONTENT_LOADED, resolve(identity));
        assertEquals(IconType.TRACKING_CONTENT_LOADED, resolve(identity, "https://mozilla.com"));

        // even for SecurityMode.CHROMEUI, TrackingMode still prior than SecurityMode.
        doReturn(SecurityMode.CHROMEUI).when(identity).getSecurityMode();
        assertEquals(IconType.TRACKING_CONTENT_LOADED, resolve(identity));

        // even for SecurityMode.VERIFIED, TrackingMode still prior than SecurityMode.
        doReturn(SecurityMode.VERIFIED).when(identity).getSecurityMode();
        assertEquals(IconType.TRACKING_CONTENT_LOADED, resolve(identity));

        // even for SecurityMode.IDENTIFIED, TrackingMode still prior than SecurityMode.
        doReturn(SecurityMode.IDENTIFIED).when(identity).getSecurityMode();
        assertEquals(IconType.TRACKING_CONTENT_LOADED, resolve(identity));
    }

    /**
     * To test resolve function for TrackingContentBlocked, without SecurityException.
     * TrackingMode detection is prior than SecurityMode and MixedMode.
     * If TrackingMode is TRACKING_CONTENT_BLOCKED, it should keep returning TRACKING_CONTENT_BLOCKED
     * icon until url is "about:home".
     */
    @Test
    public void testTrackingContentBlocked() {
        doReturn(false).when(identity).isSecurityException();

        // enable TRACKING_CONTENT_BLOCKED
        doReturn(TrackingMode.TRACKING_CONTENT_BLOCKED).when(identity).getTrackingMode();

        // about:home always show Search Icon
        doReturn(SecurityMode.UNKNOWN).when(identity).getSecurityMode();
        assertEquals(IconType.SEARCH, resolve(identity, "about:home"));

        // otherwise, return icon for TRACKING_CONTENT_BLOCKED
        assertEquals(IconType.TRACKING_CONTENT_BLOCKED, resolve(identity));

        // even for SecurityMode.CHROMEUI, TrackingMode still prior than SecurityMode.
        doReturn(SecurityMode.CHROMEUI).when(identity).getSecurityMode();
        assertEquals(IconType.TRACKING_CONTENT_BLOCKED, resolve(identity));

        // even for SecurityMode.VERIFIED, TrackingMode still prior than SecurityMode.
        doReturn(SecurityMode.VERIFIED).when(identity).getSecurityMode();
        assertEquals(IconType.TRACKING_CONTENT_BLOCKED, resolve(identity));

        // even for SecurityMode.IDENTIFIED, TrackingMode still prior than SecurityMode.
        doReturn(SecurityMode.IDENTIFIED).when(identity).getSecurityMode();
        assertEquals(IconType.TRACKING_CONTENT_BLOCKED, resolve(identity));
    }

    /**
     * To test resolve function for MixedMode, without SecurityException nor TrackingMode.
     * MixedMode detection is prior than SecurityMode. And in MixedMode, MixedActiveContent
     * is prior than MixedDisplayContent.
     */
    @Test
    public void testMixedMode() {
        doReturn(false).when(identity).isSecurityException();
        doReturn(TrackingMode.UNKNOWN).when(identity).getTrackingMode();

        // MixedActiveContent loaded, MixedDisplayContent loaded
        doReturn(MixedMode.LOADED).when(identity).getMixedModeActive();
        doReturn(MixedMode.LOADED).when(identity).getMixedModeDisplay();

        // about:home always show Search Icon
        doReturn(SecurityMode.UNKNOWN).when(identity).getSecurityMode();
        assertEquals(IconType.SEARCH, resolve(identity, "about:home"));
        // otherwise, return icon for MixedModeActive-loaded
        assertEquals(IconType.LOCK_INSECURE, resolve(identity));

        // MixedActiveContent loaded, MixedDisplayContent blocked
        // It should be same as ActiveContent-loaded-DisplayContent-loaded
        doReturn(MixedMode.LOADED).when(identity).getMixedModeActive();
        doReturn(MixedMode.BLOCKED).when(identity).getMixedModeDisplay();
        assertEquals(IconType.LOCK_INSECURE, resolve(identity));

        // MixedActiveContent blocked, MixedDisplayContent loaded
        doReturn(MixedMode.BLOCKED).when(identity).getMixedModeActive();
        doReturn(MixedMode.LOADED).when(identity).getMixedModeDisplay();
        assertEquals(IconType.WARNING, resolve(identity));
    }

    /**
     * To test resolve function for SecurityMode, without SecurityException nor TrackingMode nor
     * MixedMode.
     */
    @Test
    public void testSecurityMode() {
        doReturn(false).when(identity).isSecurityException();
        doReturn(TrackingMode.UNKNOWN).when(identity).getTrackingMode();
        doReturn(MixedMode.BLOCKED).when(identity).getMixedModeActive();
        doReturn(MixedMode.BLOCKED).when(identity).getMixedModeDisplay();

        // about:home always show Search Icon
        doReturn(SecurityMode.UNKNOWN).when(identity).getSecurityMode();
        assertEquals(IconType.SEARCH, resolve(identity, "about:home"));

        // for SecurityMode.CHROMEUI, We should show a global icon
        doReturn(SecurityMode.CHROMEUI).when(identity).getSecurityMode();
        assertEquals(IconType.DEFAULT, resolve(identity));

        // for SecurityMode.VERIFIED, We should show a VERIFIED icon
        doReturn(SecurityMode.VERIFIED).when(identity).getSecurityMode();
        assertEquals(IconType.LOCK_SECURE, resolve(identity));

        // for SecurityMode.IDENTIFIED, We should show a IDENTIFIED icon
        doReturn(SecurityMode.IDENTIFIED).when(identity).getSecurityMode();
        assertEquals(IconType.LOCK_SECURE, resolve(identity));
    }

    @Test
    public void testIsTrackingProtectionEnabled() {
        // no identity, the tracking protection should be regard as disabled
        Assert.assertFalse(SecurityModeUtil.isTrackingProtectionEnabled(null));

        doReturn(TrackingMode.UNKNOWN).when(identity).getTrackingMode();
        Assert.assertFalse(SecurityModeUtil.isTrackingProtectionEnabled(identity));

        doReturn(TrackingMode.TRACKING_CONTENT_LOADED).when(identity).getTrackingMode();
        Assert.assertFalse(SecurityModeUtil.isTrackingProtectionEnabled(identity));

        doReturn(TrackingMode.TRACKING_CONTENT_BLOCKED).when(identity).getTrackingMode();
        Assert.assertTrue(SecurityModeUtil.isTrackingProtectionEnabled(identity));
    }

}
