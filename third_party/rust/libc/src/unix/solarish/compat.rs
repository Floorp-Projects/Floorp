// Common functions that are unfortunately missing on illumos and
// Solaris, but often needed by other crates.

use unix::solarish::*;

const PTEM: &[u8] = b"ptem\0";
const LDTERM: &[u8] = b"ldterm\0";

pub unsafe fn cfmakeraw(termios: *mut ::termios) {
    (*termios).c_iflag &=
        !(IMAXBEL | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    (*termios).c_oflag &= !OPOST;
    (*termios).c_lflag &= !(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    (*termios).c_cflag &= !(CSIZE | PARENB);
    (*termios).c_cflag |= CS8;

    // By default, most software expects a pending read to block until at
    // least one byte becomes available.  As per termio(7I), this requires
    // setting the MIN and TIME parameters appropriately.
    //
    // As a somewhat unfortunate artefact of history, the MIN and TIME slots
    // in the control character array overlap with the EOF and EOL slots used
    // for canonical mode processing.  Because the EOF character needs to be
    // the ASCII EOT value (aka Control-D), it has the byte value 4.  When
    // switching to raw mode, this is interpreted as a MIN value of 4; i.e.,
    // reads will block until at least four bytes have been input.
    //
    // Other platforms with a distinct MIN slot like Linux and FreeBSD appear
    // to default to a MIN value of 1, so we'll force that value here:
    (*termios).c_cc[VMIN] = 1;
    (*termios).c_cc[VTIME] = 0;
}

pub unsafe fn cfsetspeed(termios: *mut ::termios, speed: ::speed_t) -> ::c_int {
    // Neither of these functions on illumos or Solaris actually ever
    // return an error
    ::cfsetispeed(termios, speed);
    ::cfsetospeed(termios, speed);
    0
}

unsafe fn bail(fdm: ::c_int, fds: ::c_int) -> ::c_int {
    let e = *___errno();
    if fds >= 0 {
        ::close(fds);
    }
    if fdm >= 0 {
        ::close(fdm);
    }
    *___errno() = e;
    return -1;
}

pub unsafe fn openpty(
    amain: *mut ::c_int,
    asubord: *mut ::c_int,
    name: *mut ::c_char,
    termp: *const termios,
    winp: *const ::winsize,
) -> ::c_int {
    // Open the main pseudo-terminal device, making sure not to set it as the
    // controlling terminal for this process:
    let fdm = ::posix_openpt(O_RDWR | O_NOCTTY);
    if fdm < 0 {
        return -1;
    }

    // Set permissions and ownership on the subordinate device and unlock it:
    if ::grantpt(fdm) < 0 || ::unlockpt(fdm) < 0 {
        return bail(fdm, -1);
    }

    // Get the path name of the subordinate device:
    let subordpath = ::ptsname(fdm);
    if subordpath.is_null() {
        return bail(fdm, -1);
    }

    // Open the subordinate device without setting it as the controlling
    // terminal for this process:
    let fds = ::open(subordpath, O_RDWR | O_NOCTTY);
    if fds < 0 {
        return bail(fdm, -1);
    }

    // Check if the STREAMS modules are already pushed:
    let setup = ::ioctl(fds, I_FIND, LDTERM.as_ptr());
    if setup < 0 {
        return bail(fdm, fds);
    } else if setup == 0 {
        // The line discipline is not present, so push the appropriate STREAMS
        // modules for the subordinate device:
        if ::ioctl(fds, I_PUSH, PTEM.as_ptr()) < 0 || ::ioctl(fds, I_PUSH, LDTERM.as_ptr()) < 0 {
            return bail(fdm, fds);
        }
    }

    // If provided, set the terminal parameters:
    if !termp.is_null() && ::tcsetattr(fds, TCSAFLUSH, termp) != 0 {
        return bail(fdm, fds);
    }

    // If provided, set the window size:
    if !winp.is_null() && ::ioctl(fds, TIOCSWINSZ, winp) < 0 {
        return bail(fdm, fds);
    }

    // If the caller wants the name of the subordinate device, copy it out.
    //
    // Note that this is a terrible interface: there appears to be no standard
    // upper bound on the copy length for this pointer.  Nobody should pass
    // anything but NULL here, preferring instead to use ptsname(3C) directly.
    if !name.is_null() {
        ::strcpy(name, subordpath);
    }

    *amain = fdm;
    *asubord = fds;
    0
}

pub unsafe fn forkpty(
    amain: *mut ::c_int,
    name: *mut ::c_char,
    termp: *const termios,
    winp: *const ::winsize,
) -> ::pid_t {
    let mut fds = -1;

    if openpty(amain, &mut fds, name, termp, winp) != 0 {
        return -1;
    }

    let pid = ::fork();
    if pid < 0 {
        return bail(*amain, fds);
    } else if pid > 0 {
        // In the parent process, we close the subordinate device and return the
        // process ID of the new child:
        ::close(fds);
        return pid;
    }

    // The rest of this function executes in the child process.

    // Close the main side of the pseudo-terminal pair:
    ::close(*amain);

    // Use TIOCSCTTY to set the subordinate device as our controlling
    // terminal.  This will fail (with ENOTTY) if we are not the leader in
    // our own session, so we call setsid() first.  Finally, arrange for
    // the pseudo-terminal to occupy the standard I/O descriptors.
    if ::setsid() < 0
        || ::ioctl(fds, TIOCSCTTY, 0) < 0
        || ::dup2(fds, 0) < 0
        || ::dup2(fds, 1) < 0
        || ::dup2(fds, 2) < 0
    {
        // At this stage there are no particularly good ways to handle failure.
        // Exit as abruptly as possible, using _exit() to avoid messing with any
        // state still shared with the parent process.
        ::_exit(EXIT_FAILURE);
    }
    // Close the inherited descriptor, taking care to avoid closing the standard
    // descriptors by mistake:
    if fds > 2 {
        ::close(fds);
    }

    0
}
