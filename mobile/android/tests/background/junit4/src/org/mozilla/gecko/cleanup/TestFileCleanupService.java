/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.gecko.cleanup;

import android.content.Intent;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.*;

/**
 * Tests the methods of {@link FileCleanupService}.
 */
@RunWith(TestRunner.class)
public class TestFileCleanupService {
    @Rule
    public final TemporaryFolder tempFolder = new TemporaryFolder();

    private void assertAllFilesExist(final List<File> fileList) {
        for (final File file : fileList) {
            assertTrue("File exists", file.exists());
        }
    }

    private void assertAllFilesDoNotExist(final List<File> fileList) {
        for (final File file : fileList) {
            assertFalse("File does not exist", file.exists());
        }
    }

    private void onHandleIntent(final ArrayList<String> filePaths) {
        final FileCleanupService service = new FileCleanupService();
        final Intent intent = new Intent(FileCleanupService.ACTION_DELETE_FILES);
        intent.putExtra(FileCleanupService.EXTRA_FILE_PATHS_TO_DELETE, filePaths);
        service.onHandleIntent(intent);
    }

    @Test
    public void testOnHandleIntentDeleteSpecifiedFiles() throws Exception {
        final int fileListCount = 3;
        final ArrayList<File> filesToDelete = generateFileList(fileListCount);

        final ArrayList<String> pathsToDelete = new ArrayList<>(fileListCount);
        for (final File file : filesToDelete) {
            pathsToDelete.add(file.getAbsolutePath());
        }

        assertAllFilesExist(filesToDelete);
        onHandleIntent(pathsToDelete);
        assertAllFilesDoNotExist(filesToDelete);
    }

    @Test
    public void testOnHandleIntentDoesNotDeleteUnrelatedFiles() throws Exception {
        final ArrayList<File> filesShouldNotBeDeleted = generateFileList(3);
        assertAllFilesExist(filesShouldNotBeDeleted);
        onHandleIntent(new ArrayList<String>());
        assertAllFilesExist(filesShouldNotBeDeleted);
    }

    @Test
    public void testOnHandleIntentDeletesEmptyDirectory() throws Exception {
        final File dir = tempFolder.newFolder();
        final ArrayList<String> filesToDelete = new ArrayList<>(1);
        filesToDelete.add(dir.getAbsolutePath());

        assertTrue("Empty directory exists", dir.exists());
        onHandleIntent(filesToDelete);
        assertFalse("Empty directory deleted by service", dir.exists());
    }

    @Test
    public void testOnHandleIntentDoesNotDeleteNonEmptyDirectory() throws Exception {
        final File dir = tempFolder.newFolder();
        final ArrayList<String> filesCannotDelete = new ArrayList<>(1);
        filesCannotDelete.add(dir.getAbsolutePath());
        assertTrue("Directory exists", dir.exists());

        final File fileInDir = new File(dir, "file_in_dir");
        assertTrue("File in dir created", fileInDir.createNewFile());

        onHandleIntent(filesCannotDelete);
        assertTrue("Non-empty directory not deleted", dir.exists());
        assertTrue("File in directory not deleted", fileInDir.exists());
    }

    private ArrayList<File> generateFileList(final int size) throws IOException {
        final ArrayList<File> fileList = new ArrayList<>(size);
        for (int i = 0; i < size; ++i) {
            fileList.add(tempFolder.newFile());
        }
        return fileList;
    }
}
