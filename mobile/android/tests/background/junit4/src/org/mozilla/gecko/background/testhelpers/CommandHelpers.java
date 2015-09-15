/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import org.json.simple.JSONArray;
import org.mozilla.gecko.sync.CommandProcessor.Command;

public class CommandHelpers {

  @SuppressWarnings("unchecked")
  public static Command getCommand1() {
    JSONArray args = new JSONArray();
    args.add("argsA");
    return new Command("displayURI", args);
  }

  @SuppressWarnings("unchecked")
  public static Command getCommand2() {
    JSONArray args = new JSONArray();
    args.add("argsB");
    return new Command("displayURI", args);
  }

  @SuppressWarnings("unchecked")
  public static Command getCommand3() {
    JSONArray args = new JSONArray();
    args.add("argsC");
    return new Command("displayURI", args);
  }

  @SuppressWarnings("unchecked")
  public static Command getCommand4() {
    JSONArray args = new JSONArray();
    args.add("URI of Page");
    args.add("Sender ID");
    args.add("Title of Page");
    return new Command("displayURI", args);
  }
}
