/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.uploaders;

import android.net.Uri;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.net.URI;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class RecordUploadRunnableTest {
    @Test
    public void testBuildPostURI() throws Exception {
        BatchMeta batchMeta = new BatchMeta(new Object(), 1, 1, null);
        URI postURI = RecordUploadRunnable.buildPostURI(
                false, batchMeta, Uri.parse("http://example.com/"));
        assertEquals("http://example.com/?batch=true", postURI.toString());

        postURI = RecordUploadRunnable.buildPostURI(
                true, batchMeta, Uri.parse("http://example.com/"));
        assertEquals("http://example.com/?batch=true&commit=true", postURI.toString());

        batchMeta.setToken("MTIzNA", false);
        postURI = RecordUploadRunnable.buildPostURI(
                false, batchMeta, Uri.parse("http://example.com/"));
        assertEquals("http://example.com/?batch=MTIzNA", postURI.toString());

        postURI = RecordUploadRunnable.buildPostURI(
                true, batchMeta, Uri.parse("http://example.com/"));
        assertEquals("http://example.com/?batch=MTIzNA&commit=true", postURI.toString());
    }
}