README for the plugin event sample.

INTENT:
-------
The intent of this plugin is to demonstrate how to implement a
"real", native control as a plugin.

This sample shows how a plugin can sit "nicely" with the Mozilla event 
framework - for example, getting and losing focus, and allowing keyboard, 
mouse and resize events to be either consumed or passed on at the 
plugin's discretion.

In this sample, we create a standard "edit" control using the
platform's native edit widget.  Obviously if you really needed a simple 
edit control, you would use a standard HTML or XUL widget - but a native
edit control was chosen for this demonstration as it is generally a simple
native widget, and allows us to concentrate on the event interaction.

WINDOWS NOTES:
--------------
A standard Windows edit control is created and subclassed.  All keyboard and 
mouse messages are passed back to Mozilla itself rather than passed to the
edit control.  These messages then wind themselves back around the event 
framework, and end up as calls to nsIPlugin::HandleEvent() calls.

If we let the keyboard messages go directly to the edit control,
we have no way of allowing the keystrokes we _dont_ want to be passed back. If
we let the mouse messages go directly to the control, we don't interact correctly
with focus.

PROBLEMS:
* Plugins on Windows don't have event messages.
* Plugins on Windows dont have key-pressed messages (only keyup and down)
* Focus interaction is not correct (probably need a focus event)


LINUX NOTES:
-----------
A standard Gtk edit control is used.  

PROBLEMS:
* Focus has problems.  If you click in the plugin window, then click in the
  HTML edit widget, you get cursors in both places.  Typing is still directed
  at the plugin, even though focus was just set to the HTML widget.  Using the
  TAB key to change focus yields better results.

MAC NOTES:
----------
No Mac support at all.
