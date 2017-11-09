/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.test;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

@RunWith(TestRunner.class)
public class TestRepositorySessionBundle {
  @Test
  public void testSetGetTimestamp() {
    RepositorySessionBundle bundle = new RepositorySessionBundle(-1);
    assertEquals(-1, bundle.getTimestamp());

    bundle.setTimestamp(10);
    assertEquals(10, bundle.getTimestamp());
  }

  @Test
  public void testBumpTimestamp() {
    RepositorySessionBundle bundle = new RepositorySessionBundle(50);
    assertEquals(50, bundle.getTimestamp());

    bundle.bumpTimestamp(20);
    assertEquals(50, bundle.getTimestamp());

    bundle.bumpTimestamp(80);
    assertEquals(80, bundle.getTimestamp());
  }

  @Test
  public void testSerialize() throws Exception {
    RepositorySessionBundle bundle = new RepositorySessionBundle(50);

    String json = bundle.toJSONString();
    assertNotNull(json);

    RepositorySessionBundle read = new RepositorySessionBundle(json);
    assertEquals(50, read.getTimestamp());
  }
}
