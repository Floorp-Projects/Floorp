/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;

import org.json.simple.JSONArray;

public class CommandProcessor {
  private static final String LOG_TAG = "Command";
  protected ConcurrentHashMap<String, CommandRunner> commands = new ConcurrentHashMap<String, CommandRunner>();

  private final static CommandProcessor processor = new CommandProcessor();

  public static CommandProcessor getProcessor() {
    return processor;
  }

  public static class Command {
    public final String commandType;
    public final List<String> args;

    public Command(String commandType, List<String> args) {
      this.commandType = commandType;
      this.args = args;
    }
  }

  public void registerCommand(String commandType, CommandRunner command) {
    commands.put(commandType, command);
  }

  public void processCommand(ExtendedJSONObject unparsedCommand) {
    Command command = parseCommand(unparsedCommand);
    if (command == null) {
      Logger.debug(LOG_TAG, "Invalid command: " + unparsedCommand + " will not be processed.");
      return;
    }

    CommandRunner executableCommand = commands.get(command.commandType);
    if (executableCommand == null) {
      Logger.debug(LOG_TAG, "Command \"" + command.commandType + "\" not registered and will not be processed.");
      return;
    }

    executableCommand.executeCommand(command.args);
  }

  /**
   * Parse a JSON command into a ParsedCommand object for easier handling.
   *
   * @param unparsedCommand - command as ExtendedJSONObject
   * @return - null if command is invalid, else return ParsedCommand with
   *           no null attributes.
   */
  protected Command parseCommand(ExtendedJSONObject unparsedCommand) {
    String type = (String) unparsedCommand.get("command");
    if (type == null) {
      return null;
    }

    try {
      JSONArray unparsedArgs = unparsedCommand.getArray("args");
      if (unparsedArgs == null) {
        return null;
      }
      ArrayList<String> args = new ArrayList<String>(unparsedArgs.size());

      for (int i = 0; i < unparsedArgs.size(); i++) {
        args.add(unparsedArgs.get(i).toString());
      }

      return new Command(type, args);
    } catch (NonArrayJSONException e) {
      Logger.debug(LOG_TAG, "Unable to parse args array. Invalid command");
      return null;
    }
  }
}
