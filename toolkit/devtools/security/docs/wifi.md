Overview
--------

### Remote Debugging Today

Connecting to the Dev Tools debugging server on a remote device (like
B2G) via USB (which requires ADB) is too complex to setup and use.
Dealing with ADB is confusing, especially on Windows and Linux where
there are driver issues / udev rules to set up first. We have made
various attempts to simplify this and probably will continue to try our
best, but it doesn't seem like the UX will ever be great with ADB
involved.

### Wi-Fi

We're interested in making the debugging server available over Wi-Fi,
mainly in an attempt to simplify the UX. This of course presents new
security challenges to address, but we must also keep in mind that **if
our plan to address security results in a complex UX, then it may not be
a net gain over the USB & ADB route**.

To be clear, we are not trying to expose ADB over Wi-Fi at all, only the
Dev Tools debugging server.

### Security

TLS is used to provide encryption of the data in transit. Both parties
use self-signed certs to identify themselves. There is a one-time setup
process to authenticate a new device. This is explained in many more
details later on in this document.

Definitions
-----------

-   **Device / Server**: Firefox OS phone (or Fennec, remote Firefox,
    etc.)
-   **Computer / Client**: Machine running desktop Firefox w/ WebIDE

Proposal
--------

This proposal uses TLS with self-signed certs to allow Clients to
connect to Servers through an encrypted, authenticated channel. After the
first connection from a new Client, the Client is saved on the Server
(if the user wants to always allow) and can connect freely in the future
(assuming Wi-Fi debugging is still enabled).

### Default State

The device does not listen over Wi-Fi at all by default.

### Part A: Enabling Wi-Fi Debugging

1.  User goes to Developer menu on Device
2.  User checks "DevTools over Wi-Fi" to enable the feature
    -   Persistent notification displayed in notification bar reminding
        user that this is enabled

3.  Device begins listening on random TCP socket via Wi-Fi only
4.  Device announces itself via service discovery
    -   Announcements only go to the local LAN / same subnet
    -   The announcement contains hash(DeviceCert) as additional data

The Device remains listening as long as the feature is enabled.

### Part B: Using Wi-Fi Debugging (new computer)

Here are the details of connecting from a new computer to the device:

1.  Computer detects Device as available for connection via service
    discovery
2.  User chooses device to start connection on Computer
3.  TLS connection established, authentication begins
4.  Device sees that ComputerCert is from an unknown client (since it is
    new)
5.  User is shown an Allow / Deny / Always Allow prompt on the Device
    with Computer name and hash(ComputerCert)
    -   If Deny is chosen, the connection is terminated and exponential
        backoff begins (larger with each successive Deny)
    -   If Allow is chosen, the connection proceeds, but nothing is
        stored for the future
    -   If Always Allow is chosen, the connection proceeds, and
        hash(ComputerCert) is saved for future attempts

6.  Device waits for out-of-band data
7.  Computer verifies that Device's cert matches hash(DeviceCert) from
    the advertisement
8.  Computer creates hash(ComputerCert) and K(random 128-bit number)
9.  Out-of-band channel is used to move result of step 8 from Computer
    to Device
    -   For Firefox Desktop -\> Firefox OS, Desktop will make a QR code,
        and FxOS will scan it
    -   For non-mobile servers, some other approach is likely needed,
        perhaps a short code form for the user to transfer

10. Device verifies that Computer's cert matches hash(ComputerCert) from
    out-of-band channel
11. Device sends K to Computer over the TLS connection
12. Computer verifies received value matches K
13. Debugging begins

### Part C: Using Wi-Fi Debugging (known computer)

Here are the details of connecting from a known computer (saved via
Always Allow) to the device:

1.  Computer detects Device as available for connection via service
    discovery
2.  User choose device to start connection on Computer
3.  TLS connection established, authentication begins
4.  Device sees that ComputerCert is from a known client via
    hash(ComputerCert)
5.  Debugging begins

### Other Details

-   When there is a socket listening for connections, they will only be
    accepted via Wi-Fi
    -   The socket will only listen on the external, Wi-Fi interface
    -   This is to ensure local apps can't connect to the socket
-   Socket remains listening as long as the feature is enabled

### UX

This design seems convenient and of relatively low burden on the user.
If they choose to save the Computer for future connections, it becomes a
one click connection from Computer to Device, as it is over USB today.

### Possible Attacks

Someone could try to DoS the phone via many connection attempts. The
exponential backoff should mitigate this concern. ([bug
1022692](https://bugzilla.mozilla.org/show_bug.cgi?id=1022692))

### Comparison to ADB

While it would be nice if we could instead leverage ADB here, that
doesn’t seem viable because:

-   ADB comes with a lot of setup / troubleshooting pain
    -   Users don’t understand it or why it is needed for us
    -   Each OS has several UX issues with ADB that make it annoying to
        use
-   ADB has a much larger attack surface area, simply because it has
    many more lower level functions than the Developer Tools protocol we
    are exposing here

Acknowledgments
---------------

-   J. Ryan Stinnett started this project from the DevTools team
-   Brian Warner created many of the specific details of the authentication
    protocol
-   Trevor Perrin helped vet the authentication protocol
