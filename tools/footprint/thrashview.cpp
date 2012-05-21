/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * ``thrashview'' is a program that reads a binary stream of addresses
 * from stdin and displays the pattern graphically in a window.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <fstream>
#include <getopt.h>

#define GET_DISPLAY_FD(display_) ConnectionNumber(display_)

static bool opt_working_set = false;
static bool opt_fixed = false;

static Display *display;
static Window window;
static GC gc;
static XColor colors[256];
const unsigned int cellsize = 4;
static unsigned int width = 64 * cellsize;
static unsigned int height = 64 * cellsize;

#define PAGESIZE 4096
#define MAXPAGES 4096  // should hold 16MB worth of code
static unsigned int minpage = static_cast<unsigned int>(-1);
static unsigned int maxpage = 0;
static unsigned char pages[MAXPAGES];


/**
 * Create a simple window and the X objects we'll need to talk with it.
 */
static int
init()
{
    display = XOpenDisplay(0);
    if (! display)
        return 0;

    window =
        XCreateSimpleWindow(display,
                            RootWindow(display, 0),
                            1, 1, width, height,
                            0,
                            BlackPixel(display, 0),
                            BlackPixel(display, 0));

    if (! window)
        return 0;

    gc = XCreateGC(display, window, 0, 0);
    if (! gc)
        return 0;

    // Set up a grayscale
    const unsigned int ncolors = sizeof colors / sizeof colors[0];
    const unsigned short step = 65536 / ncolors;
    unsigned short brightness = 0;

    XColor *color = colors;
    XColor *limit = colors + ncolors;
    for (; color < limit; ++color, brightness += step) {
        color->red   = brightness;
        color->green = brightness;
        color->blue  = brightness;
        XAllocColor(display, DefaultColormap(display, 0), color);
    }

    // We want exposes and resizes.
    XSelectInput(display, window, ExposureMask | StructureNotifyMask);

    XMapWindow(display, window);
    XFlush(display);

    return 1;
}

/**
 * Age pages that haven't been recently touched.
 */
static void
decay()
{
    int ws_immediate = 0, ws_longterm = 0;

    unsigned char *page = pages;
    unsigned char *limit = pages + (maxpage - minpage) + 1;
    for (; page < limit; ++page) {
        if (opt_working_set) {
            if (*page == 255)
                ++ws_immediate;
            if (*page)
                ++ws_longterm;
        }

        if (*page) {
            *page /= 8;
            *page *= 7;
        }
    }

    if (opt_working_set) {
        dec(cout);
        cout << "immediate: " << ws_immediate << " pages, ";
        cout << "longterm: " << ws_longterm << " pages, ";
        cout << "mapped: " << ((maxpage - minpage) + 1) << " pages";
        cout << endl;
    }
}

/**
 * Blast the state of our pages to the screen.
 */
static int
handle_expose(const XExposeEvent& event)
{
    //printf("handle_expose(%d, %d, %d, %d)\n", event.x, event.y, event.width, event.height);

    int i = event.x / cellsize;
    int imost = i + event.width / cellsize + 1;

    int j = event.y / cellsize;
    int jmost = j + event.height / cellsize + 1;

    unsigned char *last_cell = pages + maxpage - minpage;
    unsigned char *row = pages + j;
    for (int y = j * cellsize, ymost = jmost * cellsize;
         y < ymost;
         y += cellsize, row += width / cellsize) {
        unsigned char *cell = row + i;
        for (int x = i * cellsize, xmost = imost * cellsize;
             x < xmost;
             x += cellsize, ++cell) {
            unsigned int pixel = (cell <= last_cell) ? colors[*cell].pixel : colors[0].pixel;
            XSetForeground(display, gc, pixel);
            XFillRectangle(display, window, gc, x, y, cellsize - 1, cellsize - 1);
        }
    }

    XFlush(display);

    return 1;
}

/**
 * Invalidate the entire window.
 */
static void
invalidate_window()
{
    XExposeEvent event;
    event.x = event.y = 0;
    event.width = width;
    event.height = height;

    handle_expose(event);
}

/**
 * Handle a configure event.
 */
static int
handle_configure(const XConfigureEvent& event)
{
    //printf("handle_resize(%d, %d)\n", event.width, event.height);
    width = event.width - event.width % cellsize;
    height = event.height;
    return 1;
}

/**
 * Filter to select any message.
 */
static Bool
any_event(Display *display, XEvent *event, XPointer arg)
{
    return 1;
}

/**
 * An X event occurred. Process it and flush the queue.
 */
static int
handle_xevents()
{
    int ok;
    
    XEvent event;
    XNextEvent(display, &event);
    do {
        switch (event.type) {
        case Expose:
            ok = handle_expose(reinterpret_cast<const XExposeEvent&>(event));
            break;

        case ConfigureNotify:
            ok = handle_configure(reinterpret_cast<const XConfigureEvent&>(event));
            break;

        default:
            ok = 1;
        }
    } while (ok && XCheckIfEvent(display, &event, any_event, 0));

    return ok;
}

/**
 * Read address data from stdin.
 */
static int
read_addrs()
{
    unsigned int buf[1024];
    ssize_t count;
    while ((count = read(0, buf, sizeof buf)) > 0) {
        if (count % sizeof(unsigned int))
            cerr << "truncating unaligned read" << endl;

        count /= sizeof buf[0];

        unsigned int *addr = reinterpret_cast<unsigned int *>(buf);
        unsigned int *limit = addr + count;

        for (; addr < limit; ++addr) {
            // map the address to a page
            unsigned int page = *addr / PAGESIZE;

            // XXX Don't let stray addresses bring us down. Should
            // really fix this by knowing what the ranges of addresses
            // we ought to expect are (e.g., by reading the symtab)
            if (maxpage && page > maxpage && page - maxpage > MAXPAGES)
                continue;

            if (! opt_fixed) {
                // Potentially adjust minpage and maxpage to
                // accomodate an out-of-bounds address.
                if (page < minpage) {
                    if (maxpage) {
                        // everything needs to shift.
                        unsigned int shift = minpage - page;
                        memmove(pages + shift, pages, maxpage - minpage);
                        memset(pages, 0, shift);
                    }
                    minpage = page;
                }

                if (page > maxpage)
                    maxpage = page;
            }

            page -= minpage;
            pages[page] = 255;
        }
    }

    if (count < 0 && errno != EWOULDBLOCK) {
        perror("read");
        return 0;
    }

    return 1;
}

/**
 * Run the program
 */
static void
run()
{
    // We want non-blocking I/O on stdin so we can select on it.
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    // The last time we refreshed the window.
    struct timeval last;
    gettimeofday(&last, 0);

    int ok;

    do {
        // Select on stdin and the connection to the X server.
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(GET_DISPLAY_FD(display), &fds);

        struct timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        ok = select(GET_DISPLAY_FD(display) + 1, &fds, 0, 0, &tv);
        if (ok < 0)
            break;

        if (maxpage) {
            // See if we've waited long enough to refresh the window.
            struct timeval now;
            gettimeofday(&now, 0);

            if (now.tv_sec != last.tv_sec) {
                // At least a second has gone by. Decay and refresh.
                last = now;
                decay();
                invalidate_window();
            }
            else if (now.tv_usec - last.tv_usec > 100000) {
                // At least 100msec have gone by. Refresh.
                last.tv_usec = now.tv_usec;
                invalidate_window();
            }
        }

        // Now check for X events and input.
        ok = 1;

        if (FD_ISSET(GET_DISPLAY_FD(display), &fds))
            ok = handle_xevents();

        if (FD_ISSET(STDIN_FILENO, &fds))
            ok = read_addrs();
    } while (ok);
}

/**
 * Tear down our window and stuff.
 */
static void
finish()
{
    if (window) {
        XUnmapWindow(display, window);
        XDestroyWindow(display, window);
    }

    if (display)
        XCloseDisplay(display);
}

static struct option opts[] = {
    { "working-set", no_argument,       0, 'w' },
    { "min",         required_argument, 0, 'm' },
    { "size",        required_argument, 0, 's' },
    { "max",         required_argument, 0, 'x' },
    { 0,             0,                 0, 0   }
};

static void
usage()
{
    cerr << "thrashview [--working-set] [--min=<min>] [--max=<max>] [--size=<size>]" << endl;
}

/**
 * Program starts here.
 */
int
main(int argc, char *argv[])
{
    int size = 0;

    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "wm:x:s:", opts, &option_index);

        if (c < 0)
            break;

        switch (c) {
        case 'w':
            opt_working_set = true;
            break;

        case 'm':
            minpage = strtol(optarg, 0, 0) / PAGESIZE;
            opt_fixed = true;
            break;

        case 's':
            size = strtol(optarg, 0, 0) / PAGESIZE;
            break;

        case 'x':
            maxpage = strtol(optarg, 0, 0) / PAGESIZE;
            opt_fixed = true;
            break;

        default:
            usage();
            return 1;
        }
    }

    if (minpage && !maxpage) {
        if (!size) {
            cerr << argv[0] << ": minpage specified without maxpage or size" << endl;
            return 1;
        }

        maxpage = minpage + size;
    }

    if (opt_fixed && minpage > maxpage) {
        cerr << argv[0] << ": invalid page range" << endl;
        return 1;
    }

    if (init())
        run();

    finish();

    return 0;
}
