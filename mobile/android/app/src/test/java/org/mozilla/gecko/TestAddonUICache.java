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
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mozilla.gecko.util.GeckoBundle;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.mozilla.gecko.toolbar.PageActionLayout.PageAction;
import static org.mozilla.gecko.toolbar.PageActionLayout.PageActionLayoutDelegate;

@RunWith(RobolectricTestRunner.class)
public class TestAddonUICache {
    private AddonUICache mAddonUICache;
    private @Mock Menu mMockMenu;
    private @Mock PageActionLayoutDelegate mMockPalDelegate;
    private @Captor ArgumentCaptor<List<PageAction>> palCaptor;
    private static final MessageGenerator addonMessage = new AddonMenuMessageGenerator();
    private static final MessageGenerator pageActionMessage = new PageActionMessageGenerator();

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

    @Test
    public void testResolvedPageActionListSaving() {
        final List<PageAction> pageActionList = new ArrayList<>();
        mAddonUICache.removePageActionLayoutDelegate(pageActionList);

        mAddonUICache.setPageActionLayoutDelegate(mMockPalDelegate);
        verify(mMockPalDelegate).setCachedPageActions(palCaptor.capture());
        Assert.assertEquals(pageActionList, palCaptor.getValue());
    }

    @Test
    public void testPageActionMessageDirectForward() {
        mAddonUICache.setPageActionLayoutDelegate(mMockPalDelegate);
        verify(mMockPalDelegate, never()).addPageAction(any(GeckoBundle.class));
        verify(mMockPalDelegate, never()).removePageAction(any(GeckoBundle.class));

        Map<String, GeckoBundle> sentMessages = sendAddonMessages("PageActions:Add",
                pageActionMessage);

        ArgumentCaptor<GeckoBundle> messages = ArgumentCaptor.forClass(GeckoBundle.class);
        verify(mMockPalDelegate, times(4)).addPageAction(messages.capture());

        List<GeckoBundle> expectedMessages = new ArrayList<>(sentMessages.values());
        Assert.assertEquals(expectedMessages, messages.getAllValues());

        sendAddonMessages("PageActions:Remove", new ArrayList<>(sentMessages.values()));
        messages = ArgumentCaptor.forClass(GeckoBundle.class);
        verify(mMockPalDelegate, times(4)).removePageAction(messages.capture());

        Assert.assertEquals(expectedMessages, messages.getAllValues());
    }

    @Test
    public void testPageActionMessageStoreForward() {
        Map<String, GeckoBundle> sentMessages = sendAddonMessages("PageActions:Add",
                pageActionMessage);

        mAddonUICache.setPageActionLayoutDelegate(mMockPalDelegate);
        ArgumentCaptor<GeckoBundle> messages = ArgumentCaptor.forClass(GeckoBundle.class);
        verify(mMockPalDelegate, times(4)).addPageAction(messages.capture());

        List<GeckoBundle> expectedMessages = new ArrayList<>(sentMessages.values());
        Assert.assertEquals(expectedMessages, messages.getAllValues());

        verify(mMockPalDelegate, never()).removePageAction(any(GeckoBundle.class));
    }

    @Test
    public void testPageActionMessageStoreRemoveForward() {
        Map<String, GeckoBundle> sentMessages = sendAddonMessages("PageActions:Add",
                pageActionMessage);
        sendAddonMessages("PageActions:Remove", new ArrayList<>(sentMessages.values()));

        mAddonUICache.setPageActionLayoutDelegate(mMockPalDelegate);
        verify(mMockPalDelegate, never()).addPageAction(any(GeckoBundle.class));
        verify(mMockPalDelegate, never()).removePageAction(any(GeckoBundle.class));
    }

    @Test
    public void testResolvedPageActionListSavingRemoval() {
        final List<PageAction> pageActionList = new ArrayList<>();
        final GeckoBundle palMessage = pageActionMessage.getMessage("Frob widget");
        final PageAction pageAction = new PageAction(palMessage.getString("id"),
                palMessage.getString("title"), null, null, false);
        pageActionList.add(pageAction);
        mAddonUICache.removePageActionLayoutDelegate(pageActionList);

        sendAddonMessage("PageActions:Remove", palMessage);

        mAddonUICache.setPageActionLayoutDelegate(mMockPalDelegate);
        verify(mMockPalDelegate).setCachedPageActions(palCaptor.capture());
        Assert.assertEquals(pageActionList, palCaptor.getValue());
        Assert.assertTrue(pageActionList.isEmpty());
    }

    private Map<String, GeckoBundle> sendAddonMessages(String event, MessageGenerator generator) {
        Map<String, GeckoBundle> sentMessages = new LinkedHashMap<>();
        for (int i = 0; i < 4; i++) {
            String label = "Menu " + i;
            GeckoBundle message = generator.getMessage(label);
            sendAddonMessage(event, message);
            sentMessages.put(label, message);
        }
        return sentMessages;
    }

    private void sendAddonMessages(String event, List<GeckoBundle> messages) {
        for (GeckoBundle message : messages) {
            sendAddonMessage(event, message);
        }
    }

    private void sendAddonMessage(String event, GeckoBundle message) {
        mAddonUICache.handleMessage(event, message, null);
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

    private static class PageActionMessageGenerator implements MessageGenerator {
        @Override
        public GeckoBundle getMessage(String label) {
            GeckoBundle message = new GeckoBundle(2);
            message.putString("title", label);
            message.putString("id", "{" + UUID.randomUUID().toString() + "}");
            return message;
        }
    }
}

