/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.json.simple.parser.ParseException;
import org.junit.Test;
import org.mozilla.gecko.sync.CommandProcessor;
import org.mozilla.gecko.sync.CommandRunner;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.NonObjectJSONException;

public class TestCommandProcessor extends CommandProcessor {

  public static final String commandType = "displayURI";
  public static final String commandWithNoArgs = "{\"command\":\"displayURI\"}";
  public static final String commandWithNoType = "{\"args\":[\"https://bugzilla.mozilla.org/show_bug.cgi?id=731341\",\"PKsljsuqYbGg\"]}";
  public static final String wellFormedCommand = "{\"args\":[\"https://bugzilla.mozilla.org/show_bug.cgi?id=731341\",\"PKsljsuqYbGg\"],\"command\":\"displayURI\"}";
  public static final String wellFormedCommandWithNullArgs = "{\"args\":[\"https://bugzilla.mozilla.org/show_bug.cgi?id=731341\",null,\"PKsljsuqYbGg\",null],\"command\":\"displayURI\"}";

  private boolean commandExecuted;

  // Session is not used in these tests.
  protected final GlobalSession session = null;

  public class MockCommandRunner extends CommandRunner {
    public MockCommandRunner(int argCount) {
      super(argCount);
    }

    @Override
    public void executeCommand(final GlobalSession session, List<String> args) {
      commandExecuted = true;
    }
  }

  @Test
  public void testRegisterCommand() throws NonObjectJSONException, IOException, ParseException {
    assertNull(commands.get(commandType));
    this.registerCommand(commandType, new MockCommandRunner(1));
    assertNotNull(commands.get(commandType));
  }

  @Test
  public void testProcessRegisteredCommand() throws NonObjectJSONException, IOException, ParseException {
    commandExecuted = false;
    ExtendedJSONObject unparsedCommand = new ExtendedJSONObject(wellFormedCommand);
    this.registerCommand(commandType, new MockCommandRunner(1));
    this.processCommand(session, unparsedCommand);
    assertTrue(commandExecuted);
  }

  @Test
  public void testProcessUnregisteredCommand() throws NonObjectJSONException, IOException, ParseException {
    commandExecuted = false;
    ExtendedJSONObject unparsedCommand = new ExtendedJSONObject(wellFormedCommand);
    this.processCommand(session, unparsedCommand);
    assertFalse(commandExecuted);
  }

  @Test
  public void testProcessInvalidCommand() throws NonObjectJSONException, IOException, ParseException {
    ExtendedJSONObject unparsedCommand = new ExtendedJSONObject(commandWithNoType);
    this.registerCommand(commandType, new MockCommandRunner(1));
    this.processCommand(session, unparsedCommand);
    assertFalse(commandExecuted);
  }

  @Test
  public void testParseCommandNoType() throws NonObjectJSONException, IOException, ParseException {
    ExtendedJSONObject unparsedCommand = new ExtendedJSONObject(commandWithNoType);
    assertNull(CommandProcessor.parseCommand(unparsedCommand));
  }

  @Test
  public void testParseCommandNoArgs() throws NonObjectJSONException, IOException, ParseException {
    ExtendedJSONObject unparsedCommand = new ExtendedJSONObject(commandWithNoArgs);
    assertNull(CommandProcessor.parseCommand(unparsedCommand));
  }

  @Test
  public void testParseWellFormedCommand() throws NonObjectJSONException, IOException, ParseException {
    ExtendedJSONObject unparsedCommand = new ExtendedJSONObject(wellFormedCommand);
    Command parsedCommand = CommandProcessor.parseCommand(unparsedCommand);
    assertNotNull(parsedCommand);
    assertEquals(2, parsedCommand.args.size());
    assertEquals(commandType, parsedCommand.commandType);
  }

  @Test
  public void testParseCommandNullArg() throws NonObjectJSONException, IOException, ParseException {
    ExtendedJSONObject unparsedCommand = new ExtendedJSONObject(wellFormedCommandWithNullArgs);
    Command parsedCommand = CommandProcessor.parseCommand(unparsedCommand);
    assertNotNull(parsedCommand);
    assertEquals(4, parsedCommand.args.size());
    assertEquals(commandType, parsedCommand.commandType);
    final List<String> expectedArgs = new ArrayList<String>();
    expectedArgs.add("https://bugzilla.mozilla.org/show_bug.cgi?id=731341");
    expectedArgs.add(null);
    expectedArgs.add("PKsljsuqYbGg");
    expectedArgs.add(null);
    assertEquals(expectedArgs, parsedCommand.getArgsList());
  }
}
