/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.dlc.catalog;

import org.json.JSONException;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

@RunWith(TestRunner.class)
public class TestDownloadContentBuilder {
    /**
     * Verify that the values passed to the builder are all set on the DownloadContent object.
     */
    @Test
    public void testBuilder() {
        DownloadContent content = createTestContent();

        Assert.assertEquals("Some-ID", content.getId());
        Assert.assertEquals("/somewhere/something", content.getLocation());
        Assert.assertEquals("some.file", content.getFilename());
        Assert.assertEquals("Some-checksum", content.getChecksum());
        Assert.assertEquals("Some-download-checksum", content.getDownloadChecksum());
        Assert.assertEquals(4223, content.getLastModified());
        Assert.assertEquals("Some-type", content.getType());
        Assert.assertEquals("Some-kind", content.getKind());
        Assert.assertEquals(27, content.getSize());
        Assert.assertEquals(DownloadContent.STATE_SCHEDULED, content.getState());
    }

    /**
     * Verify that a DownloadContent object exported to JSON and re-imported from JSON does not change.
     */
    public void testJSONSerializationAndDeserialization() throws JSONException {
        DownloadContent content = DownloadContentBuilder.fromJSON(DownloadContentBuilder.toJSON(createTestContent()));

        Assert.assertEquals("Some-ID", content.getId());
        Assert.assertEquals("/somewhere/something", content.getLocation());
        Assert.assertEquals("some.file", content.getFilename());
        Assert.assertEquals("Some-checksum", content.getChecksum());
        Assert.assertEquals("Some-download-checksum", content.getDownloadChecksum());
        Assert.assertEquals(4223, content.getLastModified());
        Assert.assertEquals("Some-type", content.getType());
        Assert.assertEquals("Some-kind", content.getKind());
        Assert.assertEquals(27, content.getSize());
        Assert.assertEquals(DownloadContent.STATE_SCHEDULED, content.getState());
    }

    /**
     * Create a DownloadContent object with arbitrary data.
     */
    private DownloadContent createTestContent() {
        return new DownloadContentBuilder()
                .setId("Some-ID")
                .setLocation("/somewhere/something")
                .setFilename("some.file")
                .setChecksum("Some-checksum")
                .setDownloadChecksum("Some-download-checksum")
                .setLastModified(4223)
                .setType("Some-type")
                .setKind("Some-kind")
                .setSize(27)
                .setState(DownloadContent.STATE_SCHEDULED)
                .build();
    }
}
