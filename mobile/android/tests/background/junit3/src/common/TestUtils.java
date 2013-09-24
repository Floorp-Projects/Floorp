/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.common;

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
}
