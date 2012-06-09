/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.R;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.android.ClientsDatabaseAccessor;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

public class CommandProcessor {
  private static final String LOG_TAG = "Command";
  private static AtomicInteger currentId = new AtomicInteger();
  protected ConcurrentHashMap<String, CommandRunner> commands = new ConcurrentHashMap<String, CommandRunner>();

  private final static CommandProcessor processor = new CommandProcessor();

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

    public synchronized List<String> getArgsList() {
      if (argsList == null) {
        ArrayList<String> argsList = new ArrayList<String>(args.size());

        for (int i = 0; i < args.size(); i++) {
          argsList.add(args.get(i).toString());
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

    executableCommand.executeCommand(command.getArgsList());
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
    Logger.info(LOG_TAG, "Sending URI to client: " + uri + " -> " + clientID + " (" + title + ")");

    JSONArray args = new JSONArray();
    args.add(uri);
    args.add(sender);
    args.add(title);

    Command displayURICommand = new Command("displayURI", args);
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

    String title = null;
    if (args.size() == 3) {
      title = args.get(2);
    }

    final String ns = Context.NOTIFICATION_SERVICE;
    final NotificationManager notificationManager = (NotificationManager) context.getSystemService(ns);

    // Create a Notificiation.
    final int icon = R.drawable.sync_ic_launcher;
    String notificationTitle = context.getString(R.string.sync_new_tab);
    if (title != null) {
      notificationTitle = notificationTitle.concat(": " + title);
    }

    final long when = System.currentTimeMillis();
    Notification notification = new Notification(icon, notificationTitle, when);
    notification.flags = Notification.FLAG_AUTO_CANCEL;

    // Set pending intent associated with the notification.
    Intent notificationIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(uri));
    PendingIntent contentIntent = PendingIntent.getActivity(context, 0, notificationIntent, 0);
    notification.setLatestEventInfo(context, notificationTitle, uri, contentIntent);

    // Send notification.
    notificationManager.notify(currentId.getAndIncrement(), notification);
  }
}
