# libprio - A Prio library in C using NSS 

**Warning:**
We do our best to write bug-free code, but I have no doubt
that there are scary bugs, side-channel attacks, and memory leaks 
lurking herein. 

**Important:**
We have not yet implemented the items
described in the "Security-Critical TODOs" section below.
Without these features, do not use the code in a production environment.


## Overview

This is an implementation of the core cryptographic routines
for the [Prio system](https://crypto.stanford.edu/prio/) 
for the private computation of aggregate statistics:
> "Prio: Private, Robust, and Scalable Computation of Aggregate Statistics"<br>
> by Henry Corrigan-Gibbs and Dan Boneh<br>
> USENIX Symposium on Networked Systems Design and Implementation<br>
> March 2017
>
> Available online at:
>    https://crypto.stanford.edu/prio/

**Usage scenario.**
The library implements the cryptographic routines necessary
for the following application scenario:
Each client holds a vector of boolean values.
Each client uses the library to encode her private vector into two 
encoded packets&mdash;one for server A and one for server B.

After receiving shares from a client, the servers can use the routines
implemented here to check whether the client-provided packets are 
well formed. 
(Without this check, a single malicious client can corrupt the
output of the computation.)

After collecting data packets from many clients, the servers
can combine their state to learn how many clients had the
*i*th bit of their data vector set to `true` and how many
clients had the *i*th bit of their data vector set to `false`.
As long as at least one of the two servers is honest 
(i.e., runs the correct code), 
the servers learn *nothing else* about the clients' data, 
under standard cryptographic assumptions.

For example, the *i*th bit of the data vector could indicate
whether the client ever visited the *i*th-ranked website
in the Alexa Top 500.
The servers would learn how many clients visited each of the 
Top 500 websites *without learning* which clients visited
which websites.

**Efficiency considerations.**
The code makes no use of public-key crypto, so it should 
be relatively fast.
When each a data packet is of length *N*, 
all arithmetic is modulo a prime *p* (we use an 87-bit prime by default), 
and "elements" are integers modulo *p*, 
the dominant costs of the system are:
* **Client compute:** O(*N* log *N*) multiplications 
* **Client-to-server communication:** 2*N* + O(1) elements<br>
* **Server compute:** O(*N* log *N*) multiplications to check each packet<br> 
    (NOTE: Using an optimization we haven't yet implemented, we can 
    drop this cost to O(*N*) multiplications per packet.)
* **Server-to-server communication:** O(1) elements
* **Server storage:** O(*N*) elements

## Running the code

You must first install 
[NSS/NSPR](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/NSS), 
[scons](http://scons.org/), and 
[msgpack-c](https://github.com/msgpack/msgpack-c) version 2.1.5 (or newer?).
On Ubuntu, you can instal NSS and scons with:

    $ sudo apt install scons libnspr4-dev libnss3-dev 

and you will have to download [msgpack-c 2.1.5 or newer here](https://github.com/msgpack/msgpack-c/releases),
since the Ubuntu packages for msgpack are far out of date.

For macOS using Homebrew:
    $ brew install nss nspr scons msgpack

    $ export LDFLAGS="-L/usr/local/opt/nss/lib"
    $ export CPPFLAGS="-I/usr/local/opt/nss/include -I/usr/local/opt/nspr/include/nspr"

To compile the code, run:

    $ scons

To run the test suite, execute:

    $ build/ptest/ptest -v

To print debug messages while compiling:

    $ scons VERBOSE=1

To compile with debug symbols, run:

    $ scons BUILDTYPE=DEBUG

To clean up the object and binary files, run:

    $ scons -c

The files in this directory are:
````
/build      - Binaries, object files, etc.
/include    - Exported header files
              (Note: The public header is <mprio.h> since
              NSPR already has a file called <prio.h>.)
/mpi        - NSS MPI bignum library 
/pclient    - Example code that uses the Prio library
/prio       - Prio library core code
/ptest      - Tests and test runner
````

## Optimizations and features not yet implemented
* **Server compute.**
  By using a fast polynomial interpolation-and-evaluation
  routine, we can reduce the cost of checking a single client
  request from O(*N* log *N*) multiplications down to O(*N*)
  multiplications, for a data packet of *N* items.
* **Differential privacy.**
  It would be very straightforward to add some small amount of 
  noise to the final statistics to provide differential privacy.
  If this would be useful, I can add it.
* **Misc.**
  There are TODO notes scattered throughout code indicating
  places for potential performance optimizations.
  

