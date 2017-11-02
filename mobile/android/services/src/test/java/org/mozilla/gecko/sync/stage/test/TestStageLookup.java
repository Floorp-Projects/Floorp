/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.stage.test;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;

import java.util.HashSet;
import java.util.Set;

import static org.junit.Assert.assertEquals;

@RunWith(TestRunner.class)
public class TestStageLookup {

  @Test
  public void testStageLookupByName() {
    Set<Stage> namedStages = new HashSet<Stage>(Stage.getNamedStages());
    Set<Stage> expected = new HashSet<Stage>();
    expected.add(Stage.syncClientsEngine);
    expected.add(Stage.syncBookmarks);
    expected.add(Stage.syncTabs);
    expected.add(Stage.syncFormHistory);
    expected.add(Stage.syncFullHistory);
    expected.add(Stage.syncRecentHistory);
    expected.add(Stage.syncPasswords);

    assertEquals(expected, namedStages);
    assertEquals(Stage.syncClientsEngine, Stage.byName("clients"));
    assertEquals(Stage.syncTabs,          Stage.byName("tabs"));
    assertEquals(Stage.syncBookmarks,     Stage.byName("bookmarks"));
    assertEquals(Stage.syncFormHistory,   Stage.byName("forms"));
    assertEquals(Stage.syncFullHistory,   Stage.byName("history"));
    assertEquals(Stage.syncRecentHistory, Stage.byName("recentHistory"));
    assertEquals(Stage.syncPasswords,     Stage.byName("passwords"));

    assertEquals(null, Stage.byName("foobar"));
    assertEquals(null, Stage.byName(null));
  }
}
