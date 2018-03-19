/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko;

import android.view.Menu;
import android.view.MenuItem;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mozilla.gecko.util.GeckoBundle;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

@RunWith(RobolectricTestRunner.class)
public class TestAddonUICache {
    private AddonUICache mAddonUICache;
    private @Mock Menu mMockMenu;
    private static final MessageGenerator addonMessage = new AddonMenuMessageGenerator();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockMenu.add(anyInt(), anyInt(), anyInt(), anyString())).thenReturn(mock(MenuItem.class));

        mAddonUICache = AddonUICache.getInstance();
        mAddonUICache.init();
    }

    @After
    public void tearDown() {
        mAddonUICache.reset();
    }

    @Test
    public void testMenuMessageDirectForward() {
        mAddonUICache.onCreateOptionsMenu(mMockMenu);
        verify(mMockMenu, never()).add(anyInt(), anyInt(), anyInt(), anyString());

        Map<String, GeckoBundle> sentMessages = sendAddonMessages("Menu:Add",
                addonMessage);

        ArgumentCaptor<String> menuLabel = ArgumentCaptor.forClass(String.class);
        verify(mMockMenu, times(4)).add(anyInt(), anyInt(), anyInt(), menuLabel.capture());

        List<String> expectedLabels = new ArrayList<>(sentMessages.keySet());
        Assert.assertEquals(expectedLabels, menuLabel.getAllValues());
    }

    @Test
    public void testMenuMessageStoreForward() {
        Map<String, GeckoBundle> sentMessages = sendAddonMessages("Menu:Add",
                addonMessage);

        mAddonUICache.onCreateOptionsMenu(mMockMenu);
        ArgumentCaptor<String> menuLabel = ArgumentCaptor.forClass(String.class);
        verify(mMockMenu, times(4)).add(anyInt(), anyInt(), anyInt(), menuLabel.capture());

        List<String> expectedLabels = new ArrayList<>(sentMessages.keySet());
        Assert.assertEquals(expectedLabels, menuLabel.getAllValues());
    }

    @Test
    public void testMenuMessageStoreRemoveForward() {
        Map<String, GeckoBundle> sentMessages = sendAddonMessages("Menu:Add",
                addonMessage);
        sendAddonMessages("Menu:Remove", new ArrayList<>(sentMessages.values()));

        mAddonUICache.onCreateOptionsMenu(mMockMenu);
        verify(mMockMenu, never()).add(anyInt(), anyInt(), anyInt(), anyString());
    }

    private Map<String, GeckoBundle> sendAddonMessages(String event, MessageGenerator generator) {
        Map<String, GeckoBundle> sentMessages = new LinkedHashMap<>();
        for (int i = 0; i < 4; i++) {
            String label = "Menu " + i;
            GeckoBundle message = generator.getMessage(label);
            mAddonUICache.handleMessage(event, message, null);
            sentMessages.put(label, message);
        }
        return sentMessages;
    }

    private void sendAddonMessages(String event, List<GeckoBundle> messages) {
        for (GeckoBundle message : messages) {
            mAddonUICache.handleMessage(event, message, null);
        }
    }

    private interface MessageGenerator {
        GeckoBundle getMessage(String label);
    }

    private static class AddonMenuMessageGenerator implements MessageGenerator {
        @Override
        public GeckoBundle getMessage(String label) {
            GeckoBundle message = new GeckoBundle(2);
            message.putString("name", label);
            message.putString("uuid", "{" + UUID.randomUUID().toString() + "}");
            return message;
        }
    }
}
