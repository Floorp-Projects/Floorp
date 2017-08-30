/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.topstories;

import junit.framework.Assert;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.activitystream.homepanel.model.TopStory;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.util.List;

@RunWith(TestRunner.class)
public class TestPocketStoriesLoader {
    private static final String KEY_STATUS = "status";
    private static final String KEY_LIST = "list";

    private static final String KEY_URL = "url";
    private static final String KEY_DEDUPE_URL = "dedupe_url";
    private static final String KEY_TITLE = "title";
    private static final String KEY_IMAGE_SRC = "image_src";

    private static final String POCKET_URL = "POCKET_URL";
    private static final String PAGE_URL = "PAGE_URL";
    private static final String TITLE = "TITLE";
    private static final String IMAGE_URL = "IMAGE_URL";

    private String makeBasicPocketResponse(JSONObject[] storyItems) {
        final JSONObject basicResponse = new JSONObject();
        try {
            final JSONArray array = new JSONArray();
            for (JSONObject item : storyItems) {
                array.put(item);
            }

            basicResponse.put(KEY_STATUS, 1); // arbitrarily chosen
            basicResponse.put(KEY_LIST, array);

        } catch (JSONException e) {
            Assert.fail("Problem creating Pocket response JSON object");
        }
        return basicResponse.toString();
    }

    private JSONObject makeBasicStoryItem() {
        final JSONObject storyItem = new JSONObject();
        try {
            storyItem.put(KEY_URL, POCKET_URL);
            storyItem.put(KEY_DEDUPE_URL, PAGE_URL);
            storyItem.put(KEY_TITLE, TITLE);
            storyItem.put(KEY_IMAGE_SRC, IMAGE_URL);
        } catch (JSONException e) {
            Assert.fail("Problem creating Pocket story JSON object");
        }
        return storyItem;
    }

    @Test
    public void testJSONStringToTopStoriesWithBasicObject() {
        final String basicResponseString = makeBasicPocketResponse(new JSONObject[] { makeBasicStoryItem() });
        final List<TopStory> stories = PocketStoriesLoader.jsonStringToTopStories(basicResponseString);
        Assert.assertEquals(1, stories.size());

        final TopStory story = stories.get(0);
        Assert.assertEquals(PAGE_URL, story.getUrl());
        Assert.assertEquals(TITLE, story.getTitle());
        Assert.assertEquals(IMAGE_URL, story.getImageUrl());
    }

    @Test
    public void testJSONStringToTopStoriesWithMissingTitle() {
        final JSONObject storyItem = makeBasicStoryItem();
        storyItem.remove(KEY_TITLE);
        final String malformedResponseString = makeBasicPocketResponse(new JSONObject[] { storyItem });
        final List<TopStory> stories = PocketStoriesLoader.jsonStringToTopStories(malformedResponseString);
        Assert.assertEquals(0, stories.size()); // Should skip malformed item.
    }

    // Pulled 8/28 Pocket response, with some trimming for content/brevity.
    final String VALID_POCKET_RESPONSE = "{\"status\":1,\"list\":[" +
            "{\"id\":2910,\"url\":\"https:\\/\\/pocket.co\\/pocket-shorted-url\"," +
            "\"dedupe_url\":\"http:\\/\\/www.bbc.co.uk\\/actual-url\"," +
            "\"title\":\"A Title Here\"," +
            "\"excerpt\":\"Middle-aged people are being urged...\"," +
            "\"domain\":\"bbc.co.uk\"," +
            "\"image_src\":\"https:\\/\\/img.cloudfront.net\"," +
            "\"published_timestamp\":\"1503550800\",\"sort_id\":0}," +

            "{\"id\":2909,\"url\":\"https:\\/\\/pocket.co\\/00000\"," +
            "\"dedupe_url\":\"http:\\/\\/fortune.com\"," +
            "\"title\":\"The Mattress Industry\",\"excerpt\":\"In any random commute...\"," +
            "\"domain\":\"fortune.com\",\"image_src\":\"https:\\/\\/d33ypg4xwx0n86.cloudfront.net\"," +
            "\"published_timestamp\":\"1503464400\",\"sort_id\":1}," +

            "{\"id\":2908,\"url\":\"https:\\/\\/pocket.co\\/00000\"," +
            "\"dedupe_url\":\"https:\\/\\/medium.com\",\"title\":\"Mediocrity is a Virus.\"," +
            "\"excerpt\":\"Little things become big things.\",\"domain\":\"medium.com\"," +
            "\"image_src\":\"https:\\/\\/d33ypg4xwx0n86.cloudfront.net\",\"published_timestamp\":\"1503578016\",\"sort_id\":2}]}";

    @Test
    public void testJSONStringToTopStoriesWithPocketServerResponse() {
        final List<TopStory> stories = PocketStoriesLoader.jsonStringToTopStories(VALID_POCKET_RESPONSE);
        Assert.assertEquals(3, stories.size());
        final TopStory story = stories.get(0);

        Assert.assertEquals("http://www.bbc.co.uk/actual-url", story.getUrl());
        Assert.assertEquals("A Title Here", story.getTitle());
        Assert.assertEquals("https://img.cloudfront.net", story.getImageUrl());
    }
}