/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.common;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.mozilla.gecko.background.helpers.AndroidSyncTestCase;
import org.mozilla.gecko.sync.Utils;

import android.os.Bundle;

public class TestUtils extends AndroidSyncTestCase {
  protected static void assertStages(String[] all, String[] sync, String[] skip, String[] expected) {
    final Set<String> sAll = new HashSet<String>();
    for (String s : all) {
      sAll.add(s);
    }
    List<String> sSync = null;
    if (sync != null) {
      sSync = new ArrayList<String>();
      for (String s : sync) {
        sSync.add(s);
      }
    }
    List<String> sSkip = null;
    if (skip != null) {
      sSkip = new ArrayList<String>();
      for (String s : skip) {
        sSkip.add(s);
      }
    }
    List<String> stages = new ArrayList<String>(Utils.getStagesToSync(sAll, sSync, sSkip));
    Collections.sort(stages);
    List<String> exp = new ArrayList<String>();
    for (String e : expected) {
      exp.add(e);
    }
    assertEquals(exp, stages);
  }

  public void testGetStagesToSync() {
    final String[] all = new String[] { "other1", "other2", "skip1", "skip2", "sync1", "sync2" };
    assertStages(all, null, null, all);
    assertStages(all, new String[] { "sync1" }, null, new String[] { "sync1" });
    assertStages(all, null, new String[] { "skip1", "skip2" }, new String[] { "other1", "other2", "sync1", "sync2" });
    assertStages(all, new String[] { "sync1", "sync2" }, new String[] { "skip1", "skip2" }, new String[] { "sync1", "sync2" });
  }

  protected static void assertStagesFromBundle(String[] all, String[] sync, String[] skip, String[] expected) {
    final Set<String> sAll = new HashSet<String>();
    for (String s : all) {
      sAll.add(s);
    }
    final Bundle bundle = new Bundle();
    Utils.putStageNamesToSync(bundle, sync, skip);

    Collection<String> ss = Utils.getStagesToSyncFromBundle(sAll, bundle);
    List<String> stages = new ArrayList<String>(ss);
    Collections.sort(stages);
    List<String> exp = new ArrayList<String>();
    for (String e : expected) {
      exp.add(e);
    }
    assertEquals(exp, stages);
  }

  public void testGetStagesToSyncFromBundle() {
    final String[] all = new String[] { "other1", "other2", "skip1", "skip2", "sync1", "sync2" };
    assertStagesFromBundle(all, null, null, all);
    assertStagesFromBundle(all, new String[] { "sync1" }, null, new String[] { "sync1" });
    assertStagesFromBundle(all, null, new String[] { "skip1", "skip2" }, new String[] { "other1", "other2", "sync1", "sync2" });
    assertStagesFromBundle(all, new String[] { "sync1", "sync2" }, new String[] { "skip1", "skip2" }, new String[] { "sync1", "sync2" });
  }

  public static void deleteDirectoryRecursively(final File dir) throws IOException {
    if (!dir.isDirectory()) {
      throw new IllegalStateException("Given directory, " + dir + ", is not a directory!");
    }

    for (File f : dir.listFiles()) {
      if (f.isDirectory()) {
        deleteDirectoryRecursively(f);
      } else if (!f.delete()) {
        // Since this method is for testing, we assume we should be able to do this.
        throw new IOException("Could not delete file, " + f.getAbsolutePath() + ". Permissions?");
      }
    }

    if (!dir.delete()) {
      throw new IOException("Could not delete dir, " + dir.getAbsolutePath() + ".");
    }
  }

  public void testDeleteDirectoryRecursively() throws Exception {
    final String TEST_DIR = getApplicationContext().getCacheDir().getAbsolutePath() +
        "-testDeleteDirectory-" + System.currentTimeMillis();

    // Non-existent directory.
    final File nonexistent = new File("nonexistentDirectory"); // Hopefully. ;)
    assertFalse(nonexistent.exists());
    try {
      deleteDirectoryRecursively(nonexistent);
      fail("deleteDirectoryRecursively on a nonexistent directory should throw Exception");
    } catch (IllegalStateException e) { }

    // Empty dir.
    File dir = mkdir(TEST_DIR);
    deleteDirectoryRecursively(dir);
    assertFalse(dir.exists());

    // Filled dir.
    dir = mkdir(TEST_DIR);
    populateDir(dir);
    deleteDirectoryRecursively(dir);
    assertFalse(dir.exists());

    // Filled dir with empty dir.
    dir = mkdir(TEST_DIR);
    populateDir(dir);
    File subDir = new File(TEST_DIR + File.separator + "subDir");
    assertTrue(subDir.mkdir());
    deleteDirectoryRecursively(dir);
    assertFalse(subDir.exists()); // For short-circuiting errors.
    assertFalse(dir.exists());

    // Filled dir with filled dir.
    dir = mkdir(TEST_DIR);
    populateDir(dir);
    subDir = new File(TEST_DIR + File.separator + "subDir");
    assertTrue(subDir.mkdir());
    populateDir(subDir);
    deleteDirectoryRecursively(dir);
    assertFalse(subDir.exists()); // For short-circuiting errors.
    assertFalse(dir.exists());
  }

  private File mkdir(final String name) {
    final File dir = new File(name);
    assertTrue(dir.mkdir());
    return dir;
  }

  private void populateDir(final File dir) throws IOException {
    assertTrue(dir.isDirectory());
    final String dirPath = dir.getAbsolutePath();
    for (int i = 0; i < 3; i++) {
      final File f = new File(dirPath + File.separator + i);
      assertTrue(f.createNewFile()); // Throws IOException if file could not be created.
    }
  }
}
