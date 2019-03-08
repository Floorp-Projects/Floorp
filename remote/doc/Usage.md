Usage
=====

When using the CDP-based remote debugger in Firefox, there are
three different programs/components running simultaneously:

  * the __client__, being the out-of-process script or library
    (such as Puppeteer) or web inspector frontend you use to control
    and retrieve information out of Firefox;

  * the __agent__ that the client connects to which is an HTTPD living
    inside Firefox, facilitating communication between clients
    and targets;

  * and the __target__, which is the web document being debugging.

As the remote agent is not compiled into Firefox by default, you
will first have to get a build that has it.  You can read more
about self-servicing such a build in the developer documentation,
under [_Building_].

To check if your Firefox build has support for the new CDP-based
remote debugger, you can check if its help message includes this:

	% ./firefox -h
	…
	  --remote-debugger [<host>][:<port>] Start the Firefox remote agent, which is 
	                     a low-level debugging interface based on the CDP protocol.
	                     Defaults to listen on localhost:9222.
	…

As you will tell from the flag description, `--remote-debugger`
takes an optional address spec as input:

	[<host>][:<port>]

You can use this to instruct the remote agent to bind to a particular
interface and port on your system.  Either host and port are optional,
which means `./firefox --remote-debugger` will bind the HTTPD to
the default `localhost:9222`.

Other examples of address specs include:

	localhost:9222
	127.0.0.1:9999
	[::1]:4567
	:0

The use of `localhost` in the first example above will, depending
on whether the system supports IPv6, bind to both IP layers and
accept incoming connections from either IPv4 or IPv6.  The second
(`127.0.0.1`) and third (`[::1]`) examples will, respecitvely,
force the HTTP to listen on IPv4 or IPv6.

The fourth example will use the default hostname, which is `localhost`
to listen on all available IP layers, but override the default port
with the special-purpose port 0.  When you ask the remote agent to
listen on port 0, the system will atomatically allocate an arbitrary
free port.

Allocating an atomic port can be useful if you want to be cautious
race conditions.  The atomically allocated port will be somewhere
in the ephemeral port range, which varies depending on your system
and system configuration.

As the atomic port is bound arbitrarily, there is intrisincly no
way to detect upfront what it is.  However, Firefox will write the
port it bound to, both to stdout and to the `remote.httpd.port`
preference in the current profile’s _user.js_ file.

[_Building_]: ./Building.html
