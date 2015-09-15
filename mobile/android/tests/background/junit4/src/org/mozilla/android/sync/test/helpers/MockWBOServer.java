package org.mozilla.android.sync.test.helpers;

import java.util.HashMap;

import org.simpleframework.http.Path;
import org.simpleframework.http.Request;
import org.simpleframework.http.Response;

/**
 * A trivial server that collects and returns WBOs.
 *
 * @author rnewman
 *
 */
public class MockWBOServer extends MockServer {
  public HashMap<String, HashMap<String, String> > collections;

  public MockWBOServer() {
    collections = new HashMap<String, HashMap<String, String> >();
  }

  @Override
  public void handle(Request request, Response response) {
    Path path = request.getPath();
    path.getPath(0);
    // TODO
  }
}
