Last updated 10.02.2001

The current version of the Netscape Plugin API is designed to help
the developers to start creating plugins for Mozilla based browsers.

This SDK is based on the API developed originally for Netscape browsers
starting with Netscape 2.x. Some additions have been made at the time
of release Netscape 3.x and Netscape 4.x. The present SDK reflects major
changes related to Mozilla code base: LiveConnect for plugin scriptability
is no longer supported, existing plugins should be modified slightly to 
become scriptable again; the browser services are now accessible from
the plugin through the access to the service manager.

The SDK is intended to help in creating full-blown plugins to work with
Mozilla code base without actually having the whole Mozilla source tree
present and built.

The Common folder contains stub implementations of the NPAPI methods, there
is no need to modify files in this folder, just include them into your project.
This is not necessary though, some samples or plugin projects may use
there own implementations, the files in this folder are just an illustration
of one possible way to do that.

The Samples section at this point contains the following plugin samples:

1. Basic plugin

Shows the bare bones of the plugin dll. It does not do anything special,
'Hello, World' type of thing. Demonstrates how the plugin dll is invoked
and how NPAPI methods are called. Can be used as a starting template for 
writing your own plugin.

2. Simple plugin 

This plugin example illustrates specific for Mozilla code base features.
It is scriptable via JavaScript and uses services provided by the browser.
Some xpcom interfaces are implemented here so the Mozilla browser is aware
of its capabilities. The plugin does not draw in the native window but 
rather JavaScript box is used to display the result of its work. Therefore,
there are no separate projects for different platforms in this sample.

3. Scriptable plugin

Yet another example of plugin scriptability. This plugin implements two
native methods callable from the JavaScript and uses native window drawings.

Scriptable samples require generation of .xpt files which should reside
in the Mozilla Components directory. To make sure Mozilla is aware of 
the presence of the new .xpt file one may look at xpi.dat. To force
Mozilla to re-scan the Components directory xpi.dat should be removed
before Mozilla is started.