/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.android.ClientsDatabaseAccessor;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Process commands received from Sync clients.
 * <p>
 * We need a command processor at two different times:
 * <ol>
 * <li>We execute commands during the "clients" engine stage of a Sync. Each
 * command takes a <code>GlobalSession</code> instance as a parameter.</li>
 * <li>We queue commands to be executed or propagated to other Sync clients
 * during an activity completely unrelated to a sync</li>
 * </ol>
 * To provide a processor for both these time frames, we maintain a static
 * long-lived singleton.
 */
public class CommandProcessor {
  private static final String LOG_TAG = "Command";
  private static final AtomicInteger currentId = new AtomicInteger();
  protected ConcurrentHashMap<String, CommandRunner> commands = new ConcurrentHashMap<String, CommandRunner>();

  private final static CommandProcessor processor = new CommandProcessor();

  /**
   * Get the global singleton command processor.
   *
   * @return the singleton processor.
   */
  public static CommandProcessor getProcessor() {
    return processor;
  }

  public static class Command {
    public final String commandType;
    public final JSONArray args;
    private List<String> argsList;

    public Command(String commandType, JSONArray args) {
      this.commandType = commandType;
      this.args = args;
    }

    /**
     * Get list of arguments as strings.  Individual arguments may be null.
     *
     * @return list of strings.
     */
    public synchronized List<String> getArgsList() {
      if (argsList == null) {
        ArrayList<String> argsList = new ArrayList<String>(args.size());

        for (int i = 0; i < args.size(); i++) {
          final Object arg = args.get(i);
          if (arg == null) {
            argsList.add(null);
            continue;
          }
          argsList.add(arg.toString());
        }
        this.argsList = argsList;
      }
      return this.argsList;
    }

    @SuppressWarnings("unchecked")
    public JSONObject asJSONObject() {
      JSONObject out = new JSONObject();
      out.put("command", this.commandType);
      out.put("args", this.args);
      return out;
    }
  }

  /**
   * Register a command.
   * <p>
   * Any existing registration is overwritten.
   *
   * @param commandType
   *          the name of the command, i.e., "displayURI".
   * @param command
   *          the <code>CommandRunner</code> instance that should handle the
   *          command.
   */
  public void registerCommand(String commandType, CommandRunner command) {
    commands.put(commandType, command);
  }

  /**
   * Process a command in the context of the given global session.
   *
   * @param session
   *          the <code>GlobalSession</code> instance currently executing.
   * @param unparsedCommand
   *          command as a <code>ExtendedJSONObject</code> instance.
   */
  public void processCommand(final GlobalSession session, ExtendedJSONObject unparsedCommand) {
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

    executableCommand.executeCommand(session, command.getArgsList());
  }

  /**
   * Parse a JSON command into a ParsedCommand object for easier handling.
   *
   * @param unparsedCommand - command as ExtendedJSONObject
   * @return - null if command is invalid, else return ParsedCommand with
   *           no null attributes.
   */
  protected static Command parseCommand(ExtendedJSONObject unparsedCommand) {
    String type = (String) unparsedCommand.get("command");
    if (type == null) {
      return null;
    }

    try {
      JSONArray unparsedArgs = unparsedCommand.getArray("args");
      if (unparsedArgs == null) {
        return null;
      }

      return new Command(type, unparsedArgs);
    } catch (NonArrayJSONException e) {
      Logger.debug(LOG_TAG, "Unable to parse args array. Invalid command");
      return null;
    }
  }

  @SuppressWarnings("unchecked")
  public void sendURIToClientForDisplay(String uri, String clientID, String title, String sender, Context context) {
    Logger.info(LOG_TAG, "Sending URI to client " + clientID + ".");
    if (Logger.LOG_PERSONAL_INFORMATION) {
      Logger.pii(LOG_TAG, "URI is " + uri + "; title is '" + title + "'.");
    }

    final JSONArray args = new JSONArray();
    args.add(uri);
    args.add(sender);
    args.add(title);

    final Command displayURICommand = new Command("displayURI", args);
    this.sendCommand(clientID, displayURICommand, context);
  }

  /**
   * Validates and sends a command to a client or all clients.
   *
   * Calling this does not actually sync the command data to the server. If the
   * client already has the command/args pair, it won't receive a duplicate
   * command.
   *
   * @param clientID
   *        Client ID to send command to. If null, send to all remote
   *        clients.
   * @param command
   *        Command to invoke on remote clients
   */
  public void sendCommand(String clientID, Command command, Context context) {
    Logger.debug(LOG_TAG, "In sendCommand.");

    CommandRunner commandData = commands.get(command.commandType);

    // Don't send commands that we don't know about.
    if (commandData == null) {
      Logger.error(LOG_TAG, "Unknown command to send: " + command);
      return;
    }

    // Don't send a command with the wrong number of arguments.
    if (!commandData.argumentsAreValid(command.getArgsList())) {
      Logger.error(LOG_TAG, "Expected " + commandData.argCount + " args for '" +
                   command + "', but got " + command.args);
      return;
    }

    if (clientID != null) {
      this.sendCommandToClient(clientID, command, context);
      return;
    }

    ClientsDatabaseAccessor db = new ClientsDatabaseAccessor(context);
    try {
      Map<String, ClientRecord> clientMap = db.fetchAllClients();
      for (ClientRecord client : clientMap.values()) {
        this.sendCommandToClient(client.guid, command, context);
      }
    } catch (NullCursorException e) {
      Logger.error(LOG_TAG, "NullCursorException when fetching all GUIDs");
    } finally {
      db.close();
    }
  }

  protected void sendCommandToClient(String clientID, Command command, Context context) {
    Logger.info(LOG_TAG, "Sending " + command.commandType + " to " + clientID);

    ClientsDatabaseAccessor db = new ClientsDatabaseAccessor(context);
    try {
      db.store(clientID, command);
    } catch (NullCursorException e) {
      Logger.error(LOG_TAG, "NullCursorException: Unable to send command.");
    } finally {
      db.close();
    }
  }

  public static void displayURI(final List<String> args, final Context context) {
    // We trust the client sender that these exist.
    final String uri = args.get(0);
    final String clientId = args.get(1);
    Logger.pii(LOG_TAG, "Received a URI for display: " + uri + " from " + clientId);

    if (uri == null) {
      Logger.pii(LOG_TAG, "URI is null – ignoring");
      return;
    }

    String title = null;
    if (args.size() == 3) {
      title = args.get(2);
    }

    final Intent sendTabNotificationIntent = new Intent();
    sendTabNotificationIntent.setClassName(context, BrowserContract.TAB_RECEIVED_SERVICE_CLASS_NAME);
    sendTabNotificationIntent.setData(Uri.parse(uri));
    sendTabNotificationIntent.putExtra(Intent.EXTRA_TITLE, title);
    sendTabNotificationIntent.putExtra(BrowserContract.EXTRA_CLIENT_GUID, clientId);
    final ComponentName componentName = context.startService(sendTabNotificationIntent);
  }
}
