"""A thin, practical wrapper around terminal coloring, styling, and
positioning"""

from contextlib import contextmanager
import curses
from curses import setupterm, tigetnum, tigetstr, tparm
from fcntl import ioctl
from six import text_type, string_types

try:
    from io import UnsupportedOperation as IOUnsupportedOperation
except ImportError:
    class IOUnsupportedOperation(Exception):
        """A dummy exception to take the place of Python 3's
        ``io.UnsupportedOperation`` in Python 2"""

from os import isatty, environ
import struct
import sys
from termios import TIOCGWINSZ


__all__ = ['Terminal']


class Terminal(object):
    """An abstraction around terminal capabilities

    Unlike curses, this doesn't require clearing the screen before doing
    anything, and it's friendlier to use. It keeps the endless calls to
    ``tigetstr()`` and ``tparm()`` out of your code, and it acts intelligently
    when somebody pipes your output to a non-terminal.

    Instance attributes:

      ``stream``
        The stream the terminal outputs to. It's convenient to pass the stream
        around with the terminal; it's almost always needed when the terminal
        is and saves sticking lots of extra args on client functions in
        practice.

    """
    def __init__(self, kind=None, stream=None, force_styling=False):
        """Initialize the terminal.

        If ``stream`` is not a tty, I will default to returning an empty
        Unicode string for all capability values, so things like piping your
        output to a file won't strew escape sequences all over the place. The
        ``ls`` command sets a precedent for this: it defaults to columnar
        output when being sent to a tty and one-item-per-line when not.

        :arg kind: A terminal string as taken by ``setupterm()``. Defaults to
            the value of the ``TERM`` environment variable.
        :arg stream: A file-like object representing the terminal. Defaults to
            the original value of stdout, like ``curses.initscr()`` does.
        :arg force_styling: Whether to force the emission of capabilities, even
            if we don't seem to be in a terminal. This comes in handy if users
            are trying to pipe your output through something like ``less -r``,
            which supports terminal codes just fine but doesn't appear itself
            to be a terminal. Just expose a command-line option, and set
            ``force_styling`` based on it. Terminal initialization sequences
            will be sent to ``stream`` if it has a file descriptor and to
            ``sys.__stdout__`` otherwise. (``setupterm()`` demands to send them
            somewhere, and stdout is probably where the output is ultimately
            headed. If not, stderr is probably bound to the same terminal.)

            If you want to force styling to not happen, pass
            ``force_styling=None``.

        """
        if stream is None:
            stream = sys.__stdout__
        try:
            stream_descriptor = (stream.fileno() if hasattr(stream, 'fileno')
                                 and callable(stream.fileno)
                                 else None)
        except IOUnsupportedOperation:
            stream_descriptor = None

        self._is_a_tty = (stream_descriptor is not None and
                         isatty(stream_descriptor))
        self._does_styling = ((self.is_a_tty or force_styling) and
                              force_styling is not None)

        # The descriptor to direct terminal initialization sequences to.
        # sys.__stdout__ seems to always have a descriptor of 1, even if output
        # is redirected.
        self._init_descriptor = (sys.__stdout__.fileno()
                                 if stream_descriptor is None
                                 else stream_descriptor)
        if self.does_styling:
            # Make things like tigetstr() work. Explicit args make setupterm()
            # work even when -s is passed to nosetests. Lean toward sending
            # init sequences to the stream if it has a file descriptor, and
            # send them to stdout as a fallback, since they have to go
            # somewhere.
            setupterm(kind or environ.get('TERM', 'unknown'),
                      self._init_descriptor)

        self.stream = stream

    # Sugary names for commonly-used capabilities, intended to help avoid trips
    # to the terminfo man page and comments in your code:
    _sugar = dict(
        # Don't use "on" or "bright" as an underscore-separated chunk in any of
        # these (e.g. on_cology or rock_on) so we don't interfere with
        # __getattr__.
        save='sc',
        restore='rc',

        clear_eol='el',
        clear_bol='el1',
        clear_eos='ed',
        # 'clear' clears the whole screen.
        position='cup',  # deprecated
        enter_fullscreen='smcup',
        exit_fullscreen='rmcup',
        move='cup',
        move_x='hpa',
        move_y='vpa',
        move_left='cub1',
        move_right='cuf1',
        move_up='cuu1',
        move_down='cud1',

        hide_cursor='civis',
        normal_cursor='cnorm',

        reset_colors='op',  # oc doesn't work on my OS X terminal.

        normal='sgr0',
        reverse='rev',
        # 'bold' is just 'bold'. Similarly...
        # blink
        # dim
        # flash
        italic='sitm',
        no_italic='ritm',
        shadow='sshm',
        no_shadow='rshm',
        standout='smso',
        no_standout='rmso',
        subscript='ssubm',
        no_subscript='rsubm',
        superscript='ssupm',
        no_superscript='rsupm',
        underline='smul',
        no_underline='rmul')

    def __getattr__(self, attr):
        """Return a terminal capability, like bold.

        For example, you can say ``term.bold`` to get the string that turns on
        bold formatting and ``term.normal`` to get the string that turns it off
        again. Or you can take a shortcut: ``term.bold('hi')`` bolds its
        argument and sets everything to normal afterward. You can even combine
        things: ``term.bold_underline_red_on_bright_green('yowzers!')``.

        For a parametrized capability like ``cup``, pass the parameters too:
        ``some_term.cup(line, column)``.

        ``man terminfo`` for a complete list of capabilities.

        Return values are always Unicode.

        """
        resolution = (self._resolve_formatter(attr) if self.does_styling
                      else NullCallableString())
        setattr(self, attr, resolution)  # Cache capability codes.
        return resolution

    @property
    def does_styling(self):
        """Whether attempt to emit capabilities

        This is influenced by the ``is_a_tty`` property and by the
        ``force_styling`` argument to the constructor. You can examine
        this value to decide whether to draw progress bars or other frippery.

        """
        return self._does_styling

    @property
    def is_a_tty(self):
        """Whether my ``stream`` appears to be associated with a terminal"""
        return self._is_a_tty

    @property
    def height(self):
        """The height of the terminal in characters

        If no stream or a stream not representing a terminal was passed in at
        construction, return the dimension of the controlling terminal so
        piping to things that eventually display on the terminal (like ``less
        -R``) work. If a stream representing a terminal was passed in, return
        the dimensions of that terminal. If there somehow is no controlling
        terminal, return ``None``. (Thus, you should check that the property
        ``is_a_tty`` is true before doing any math on the result.)

        """
        return self._height_and_width()[0]

    @property
    def width(self):
        """The width of the terminal in characters

        See ``height()`` for some corner cases.

        """
        return self._height_and_width()[1]

    def _height_and_width(self):
        """Return a tuple of (terminal height, terminal width).

        Start by trying TIOCGWINSZ (Terminal I/O-Control: Get Window Size),
        falling back to environment variables (LINES, COLUMNS), and returning
        (None, None) if those are unavailable or invalid.

        """
        # tigetnum('lines') and tigetnum('cols') update only if we call
        # setupterm() again.
        for descriptor in self._init_descriptor, sys.__stdout__:
            try:
                return struct.unpack(
                        'hhhh', ioctl(descriptor, TIOCGWINSZ, '\000' * 8))[0:2]
            except IOError:
                # when the output stream or init descriptor is not a tty, such
                # as when when stdout is piped to another program, fe. tee(1),
                # these ioctls will raise IOError
                pass
        try:
            return int(environ.get('LINES')), int(environ.get('COLUMNS'))
        except TypeError:
            return None, None

    @contextmanager
    def location(self, x=None, y=None):
        """Return a context manager for temporarily moving the cursor.

        Move the cursor to a certain position on entry, let you print stuff
        there, then return the cursor to its original position::

            term = Terminal()
            with term.location(2, 5):
                print 'Hello, world!'
                for x in xrange(10):
                    print 'I can do it %i times!' % x

        Specify ``x`` to move to a certain column, ``y`` to move to a certain
        row, both, or neither. If you specify neither, only the saving and
        restoration of cursor position will happen. This can be useful if you
        simply want to restore your place after doing some manual cursor
        movement.

        """
        # Save position and move to the requested column, row, or both:
        self.stream.write(self.save)
        if x is not None and y is not None:
            self.stream.write(self.move(y, x))
        elif x is not None:
            self.stream.write(self.move_x(x))
        elif y is not None:
            self.stream.write(self.move_y(y))
        try:
            yield
        finally:
            # Restore original cursor position:
            self.stream.write(self.restore)

    @contextmanager
    def fullscreen(self):
        """Return a context manager that enters fullscreen mode while inside it
        and restores normal mode on leaving."""
        self.stream.write(self.enter_fullscreen)
        try:
            yield
        finally:
            self.stream.write(self.exit_fullscreen)

    @contextmanager
    def hidden_cursor(self):
        """Return a context manager that hides the cursor while inside it and
        makes it visible on leaving."""
        self.stream.write(self.hide_cursor)
        try:
            yield
        finally:
            self.stream.write(self.normal_cursor)

    @property
    def color(self):
        """Return a capability that sets the foreground color.

        The capability is unparametrized until called and passed a number
        (0-15), at which point it returns another string which represents a
        specific color change. This second string can further be called to
        color a piece of text and set everything back to normal afterward.

        :arg num: The number, 0-15, of the color

        """
        return ParametrizingString(self._foreground_color, self.normal)

    @property
    def on_color(self):
        """Return a capability that sets the background color.

        See ``color()``.

        """
        return ParametrizingString(self._background_color, self.normal)

    @property
    def number_of_colors(self):
        """Return the number of colors the terminal supports.

        Common values are 0, 8, 16, 88, and 256.

        Though the underlying capability returns -1 when there is no color
        support, we return 0. This lets you test more Pythonically::

            if term.number_of_colors:
                ...

        We also return 0 if the terminal won't tell us how many colors it
        supports, which I think is rare.

        """
        # This is actually the only remotely useful numeric capability. We
        # don't name it after the underlying capability, because we deviate
        # slightly from its behavior, and we might someday wish to give direct
        # access to it.
        if not self._does_styling:
            return 0

        colors = tigetnum('colors')  # Returns -1 if no color support, -2 if no
                                     # such cap.
        #  self.__dict__['colors'] = ret  # Cache it. It's not changing.
                                          # (Doesn't work.)
        return colors if colors >= 0 else 0

    def _resolve_formatter(self, attr):
        """Resolve a sugary or plain capability name, color, or compound
        formatting function name into a callable capability.

        Return a ``ParametrizingString`` or a ``FormattingString``.

        """
        if attr in COLORS:
            return self._resolve_color(attr)
        elif attr in COMPOUNDABLES:
            # Bold, underline, or something that takes no parameters
            return self._formatting_string(self._resolve_capability(attr))
        else:
            formatters = split_into_formatters(attr)
            if all(f in COMPOUNDABLES for f in formatters):
                # It's a compound formatter, like "bold_green_on_red". Future
                # optimization: combine all formatting into a single escape
                # sequence.
                return self._formatting_string(
                    u''.join(self._resolve_formatter(s) for s in formatters))
            else:
                return ParametrizingString(self._resolve_capability(attr))

    def _resolve_capability(self, atom):
        """Return a terminal code for a capname or a sugary name, or an empty
        Unicode.

        The return value is always Unicode, because otherwise it is clumsy
        (especially in Python 3) to concatenate with real (Unicode) strings.

        """
        code = tigetstr(self._sugar.get(atom, atom))
        if code:
            # See the comment in ParametrizingString for why this is latin1.
            return code.decode('latin1')
        return u''

    def _resolve_color(self, color):
        """Resolve a color like red or on_bright_green into a callable
        capability."""
        # TODO: Does curses automatically exchange red and blue and cyan and
        # yellow when a terminal supports setf/setb rather than setaf/setab?
        # I'll be blasted if I can find any documentation. The following
        # assumes it does.
        color_cap = (self._background_color if 'on_' in color else
                     self._foreground_color)
        # curses constants go up to only 7, so add an offset to get at the
        # bright colors at 8-15:
        offset = 8 if 'bright_' in color else 0
        base_color = color.rsplit('_', 1)[-1]
        return self._formatting_string(
            color_cap(getattr(curses, 'COLOR_' + base_color.upper()) + offset))

    @property
    def _foreground_color(self):
        return self.setaf or self.setf

    @property
    def _background_color(self):
        return self.setab or self.setb

    def _formatting_string(self, formatting):
        """Return a new ``FormattingString`` which implicitly receives my
        notion of "normal"."""
        return FormattingString(formatting, self.normal)


def derivative_colors(colors):
    """Return the names of valid color variants, given the base colors."""
    return set([('on_' + c) for c in colors] +
               [('bright_' + c) for c in colors] +
               [('on_bright_' + c) for c in colors])


COLORS = set(['black', 'red', 'green', 'yellow', 'blue', 'magenta', 'cyan',
              'white'])
COLORS.update(derivative_colors(COLORS))
COMPOUNDABLES = (COLORS |
                 set(['bold', 'underline', 'reverse', 'blink', 'dim', 'italic',
                      'shadow', 'standout', 'subscript', 'superscript']))


class ParametrizingString(text_type):
    """A Unicode string which can be called to parametrize it as a terminal
    capability"""

    def __new__(cls, formatting, normal=None):
        """Instantiate.

        :arg normal: If non-None, indicates that, once parametrized, this can
            be used as a ``FormattingString``. The value is used as the
            "normal" capability.

        """
        new = text_type.__new__(cls, formatting)
        new._normal = normal
        return new

    def __call__(self, *args):
        try:
            # Re-encode the cap, because tparm() takes a bytestring in Python
            # 3. However, appear to be a plain Unicode string otherwise so
            # concats work.
            #
            # We use *latin1* encoding so that bytes emitted by tparm are
            # encoded to their native value: some terminal kinds, such as
            # 'avatar' or 'kermit', emit 8-bit bytes in range 0x7f to 0xff.
            # latin1 leaves these values unmodified in their conversion to
            # unicode byte values. The terminal emulator will "catch" and
            # handle these values, even if emitting utf8-encoded text, where
            # these bytes would otherwise be illegal utf8 start bytes.
            parametrized = tparm(self.encode('latin1'), *args).decode('latin1')
            return (parametrized if self._normal is None else
                    FormattingString(parametrized, self._normal))
        except curses.error:
            # Catch "must call (at least) setupterm() first" errors, as when
            # running simply `nosetests` (without progressive) on nose-
            # progressive. Perhaps the terminal has gone away between calling
            # tigetstr and calling tparm.
            return u''
        except TypeError:
            # If the first non-int (i.e. incorrect) arg was a string, suggest
            # something intelligent:
            if len(args) == 1 and isinstance(args[0], string_types):
                raise TypeError(
                    'A native or nonexistent capability template received '
                    '%r when it was expecting ints. You probably misspelled a '
                    'formatting call like bright_red_on_white(...).' % args)
            else:
                # Somebody passed a non-string; I don't feel confident
                # guessing what they were trying to do.
                raise


class FormattingString(text_type):
    """A Unicode string which can be called upon a piece of text to wrap it in
    formatting"""

    def __new__(cls, formatting, normal):
        new = text_type.__new__(cls, formatting)
        new._normal = normal
        return new

    def __call__(self, text):
        """Return a new string that is ``text`` formatted with my contents.

        At the beginning of the string, I prepend the formatting that is my
        contents. At the end, I append the "normal" sequence to set everything
        back to defaults. The return value is always a Unicode.

        """
        return self + text + self._normal


class NullCallableString(text_type):
    """A dummy callable Unicode to stand in for ``FormattingString`` and
    ``ParametrizingString``

    We use this when there is no tty and thus all capabilities should be blank.

    """
    def __new__(cls):
        new = text_type.__new__(cls, u'')
        return new

    def __call__(self, *args):
        """Return a Unicode or whatever you passed in as the first arg
        (hopefully a string of some kind).

        When called with an int as the first arg, return an empty Unicode. An
        int is a good hint that I am a ``ParametrizingString``, as there are
        only about half a dozen string-returning capabilities on OS X's
        terminfo man page which take any param that's not an int, and those are
        seldom if ever used on modern terminal emulators. (Most have to do with
        programming function keys. Blessings' story for supporting
        non-string-returning caps is undeveloped.) And any parametrized
        capability in a situation where all capabilities themselves are taken
        to be blank are, of course, themselves blank.

        When called with a non-int as the first arg (no no args at all), return
        the first arg. I am acting as a ``FormattingString``.

        """
        if len(args) != 1 or isinstance(args[0], int):
            # I am acting as a ParametrizingString.

            # tparm can take not only ints but also (at least) strings as its
            # second...nth args. But we don't support callably parametrizing
            # caps that take non-ints yet, so we can cheap out here. TODO: Go
            # through enough of the motions in the capability resolvers to
            # determine which of 2 special-purpose classes,
            # NullParametrizableString or NullFormattingString, to return, and
            # retire this one.
            return u''
        return args[0]  # Should we force even strs in Python 2.x to be
                        # unicodes? No. How would I know what encoding to use
                        # to convert it?


def split_into_formatters(compound):
    """Split a possibly compound format string into segments.

    >>> split_into_formatters('bold_underline_bright_blue_on_red')
    ['bold', 'underline', 'bright_blue', 'on_red']

    """
    merged_segs = []
    # These occur only as prefixes, so they can always be merged:
    mergeable_prefixes = ['on', 'bright', 'on_bright']
    for s in compound.split('_'):
        if merged_segs and merged_segs[-1] in mergeable_prefixes:
            merged_segs[-1] += '_' + s
        else:
            merged_segs.append(s)
    return merged_segs
