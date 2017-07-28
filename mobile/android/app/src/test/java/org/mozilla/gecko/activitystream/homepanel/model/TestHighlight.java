/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.activitystream.homepanel.model;

import junit.framework.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import static android.R.id.input;

@RunWith(TestRunner.class)
public class TestHighlight {

    @Test
    public void testInitFastImageURL() throws Exception {
        final Map<String, String> jsonToExpected = new HashMap<>();
        jsonToExpected.put(
                "{\"image_url\":\"https:\\/\\/upload.wikimedia.org\\/wikipedia\\/commons\\/f\\/f1\\/Brauysegen_im_Bett.gif\"}",
                "https:\\/\\/upload.wikimedia.org\\/wikipedia\\/commons\\/f\\/f1\\/Brauysegen_im_Bett.gif");
        jsonToExpected.put(
                "{\"image_url\":\"https:\\/\\/www.apple.com\\/v\\/music\\/e\\/images\\/shared\\/og_image_family.png?201706051846\",\"provider\":\"Apple\",\"description_length\":135}",
                "https:\\/\\/www.apple.com\\/v\\/music\\/e\\/images\\/shared\\/og_image_family.png?201706051846");
        jsonToExpected.put(
                "{\"image_url\":\"https:\\/\\/i.kinja-img.com\\/gawker-media\\/image\\/upload\\/s--XYjAnDao--\\/c_fill,fl_progressive,g_center,h_200,q_80,w_200\\/ghxlwgdztvqerb4zptdx.png\",\"provider\":\"Kotaku\",\"description_length\":17}",
                "https:\\/\\/i.kinja-img.com\\/gawker-media\\/image\\/upload\\/s--XYjAnDao--\\/c_fill,fl_progressive,g_center,h_200,q_80,w_200\\/ghxlwgdztvqerb4zptdx.png");

        for (final Map.Entry<String, String> entry : jsonToExpected.entrySet()) {
            final String input = entry.getKey();
            final String expected = entry.getValue();
            Assert.assertEquals("For input: " + input, expected, Highlight.initFastImageURL(input));
        }
    }

    @Test
    public void testInitFastImageURLReturnsNullWithoutImageURL() {
        final List<String> inputs = Arrays.asList(
                "{\"description_length\":130}",
                "{\"provider\":\"CNN\",\"description_length\":117}"
        );

        for (final String input : inputs) {
            Assert.assertNull("For input: " + input, Highlight.initFastImageURL(input));
        }
    }

    @Test
    public void testInitFastImageURLNullInput() {
        Assert.assertNull(Highlight.initFastImageURL(null));
    }

    @Test
    public void testInitFastDescriptionLength() {
        final Map<String, Integer> jsonToExpected = new HashMap<>();
        jsonToExpected.put("{\"description_length\":130}", 130);
        jsonToExpected.put(
                "{\"image_url\":\"https:\\/\\/www.apple.com\\/v\\/music\\/e\\/images\\/shared\\/og_image_family.png?201706051846\",\"provider\":\"Apple\",\"description_length\":135}",
                135);
        jsonToExpected.put(
                "{\"image_url\":\"https:\\/\\/i.kinja-img.com\\/gawker-media\\/image\\/upload\\/s--XYjAnDao--\\/c_fill,fl_progressive,g_center,h_200,q_80,w_200\\/ghxlwgdztvqerb4zptdx.png\",\"provider\":\"Kotaku\",\"description_length\":17}",
                17);

        for (final Map.Entry<String, Integer> entry : jsonToExpected.entrySet()) {
            final String input = entry.getKey();
            final int expected = entry.getValue();
            Assert.assertEquals("For input: " + input, expected, Highlight.initFastDescriptionLength(input));
        }
    }

    @Test
    public void testInitFastDescriptionLengthMissingDescriptionLengthKey() {
        Assert.assertEquals(0, Highlight.initFastDescriptionLength(
                "{\"image_url\":\"https:\\/\\/upload.wikimedia.org\\/wikipedia\\/commons\\/f\\/f1\\/Brauysegen_im_Bett.gif\"}"));
    }

    @Test
    public void testInitFastDescriptionLengthInvalidValue() {
        Assert.assertEquals(0, Highlight.initFastDescriptionLength("{\"description_length\":abc}"));
    }

    @Test
    public void testInitFastDescriptionLengthNullInput() {
        Assert.assertEquals(0, Highlight.initFastDescriptionLength(null));
    }
}