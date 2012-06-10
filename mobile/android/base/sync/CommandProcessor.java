/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

import org.json.simple.JSONArray;
import org.mozilla.gecko.R;

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

  public void displayURI(List<String> args, Context context) {
    // These two args are guaranteed to exist by trusting the client sender.
    String uri = args.get(0);
    String clientId = args.get(1);

    Logger.info(LOG_TAG, "Received a URI for display: " + uri + " from " + clientId);

    String title = null;
    if (args.size() == 3) {
      title = args.get(2);
    }

    // Get NotificationManager.
    String ns = Context.NOTIFICATION_SERVICE;
    NotificationManager mNotificationManager = (NotificationManager)context.getSystemService(ns);

    // Create a Notficiation.
    int icon = R.drawable.sync_ic_launcher;
    String notificationTitle = context.getString(R.string.sync_new_tab);
    if (title != null) {
      notificationTitle = notificationTitle.concat(": " + title);
    }
    long when = System.currentTimeMillis();
    Notification notification = new Notification(icon, notificationTitle, when);
    notification.flags = Notification.FLAG_AUTO_CANCEL;

    // Set pending intent associated with the notification.
    Intent notificationIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(uri));
    PendingIntent contentIntent = PendingIntent.getActivity(context, 0, notificationIntent, 0);
    notification.setLatestEventInfo(context, notificationTitle, uri, contentIntent);

    // Send notification.
    mNotificationManager.notify(currentId.getAndIncrement(), notification);
  }
}
