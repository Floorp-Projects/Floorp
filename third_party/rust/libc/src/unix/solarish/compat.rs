// Common functions that are unfortunately missing on illumos and
// Solaris, but often needed by other crates.

use unix::solarish::*;

pub unsafe fn cfmakeraw(termios: *mut ::termios) {
    (*termios).c_iflag &= !(IMAXBEL
        | IGNBRK
        | BRKINT
        | PARMRK
        | ISTRIP
        | INLCR
        | IGNCR
        | ICRNL
        | IXON);
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

pub unsafe fn cfsetspeed(
    termios: *mut ::termios,
    speed: ::speed_t,
) -> ::c_int {
    // Neither of these functions on illumos or Solaris actually ever
    // return an error
    ::cfsetispeed(termios, speed);
    ::cfsetospeed(termios, speed);
    0
}
