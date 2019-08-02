/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "secutil.h"

/*
 * NOTE:  The contents of this file are NOT used by the client.
 * (They are part of the security library as a whole, but they are
 * NOT USED BY THE CLIENT.)  Do not change things on behalf of the
 * client (like localizing strings), or add things that are only
 * for the client (put them elsewhere).
 */

#ifdef XP_UNIX
#include <termios.h>
#endif

#if defined(XP_UNIX) || defined(XP_BEOS)
#include <unistd.h> /* for isatty() */
#endif

#if defined(_WINDOWS)
#include <conio.h>
#include <io.h>
#define QUIET_FGETS quiet_fgets
static char *quiet_fgets(char *buf, int length, FILE *input);
#else
#define QUIET_FGETS fgets
#endif

static void
echoOff(int fd)
{
#if defined(XP_UNIX)
    if (isatty(fd)) {
        struct termios tio;
        tcgetattr(fd, &tio);
        tio.c_lflag &= ~ECHO;
        tcsetattr(fd, TCSAFLUSH, &tio);
    }
#endif
}

static void
echoOn(int fd)
{
#if defined(XP_UNIX)
    if (isatty(fd)) {
        struct termios tio;
        tcgetattr(fd, &tio);
        tio.c_lflag |= ECHO;
        tcsetattr(fd, TCSAFLUSH, &tio);
    }
#endif
}

char *
SEC_GetPassword(FILE *input, FILE *output, char *prompt,
                PRBool (*ok)(char *))
{
#if defined(_WINDOWS)
    int isTTY = (input == stdin);
#define echoOn(x)
#define echoOff(x)
#else
    int infd = fileno(input);
    int isTTY = isatty(infd);
#endif
    char phrase[200] = { '\0' }; /* ensure EOF doesn't return junk */

    for (;;) {
        /* Prompt for password */
        if (isTTY) {
            fprintf(output, "%s", prompt);
            fflush(output);
            echoOff(infd);
        }

        if (QUIET_FGETS(phrase, sizeof(phrase), input) == NULL) {
            return NULL;
        }

        if (isTTY) {
            fprintf(output, "\n");
            echoOn(infd);
        }

        /* stomp on newline */
        phrase[PORT_Strlen(phrase) - 1] = 0;

        /* Validate password */
        if (!(*ok)(phrase)) {
            /* Not weird enough */
            if (!isTTY)
                return NULL;
            fprintf(output, "Password must be at least 8 characters long with one or more\n");
            fprintf(output, "non-alphabetic characters\n");
            continue;
        }
        return (char *)PORT_Strdup(phrase);
    }
}

PRBool
SEC_CheckPassword(char *cp)
{
    int len;
    char *end;

    len = PORT_Strlen(cp);
    if (len < 8) {
        return PR_FALSE;
    }
    end = cp + len;
    while (cp < end) {
        unsigned char ch = *cp++;
        if (!((ch >= 'A') && (ch <= 'Z')) &&
            !((ch >= 'a') && (ch <= 'z'))) {
            /* pass phrase has at least one non alphabetic in it */
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}

PRBool
SEC_BlindCheckPassword(char *cp)
{
    if (cp != NULL) {
        return PR_TRUE;
    }
    return PR_FALSE;
}

/* Get a password from the input terminal, without echoing */

#if defined(_WINDOWS)
static char *
quiet_fgets(char *buf, int length, FILE *input)
{
    int c;
    char *end = buf;

    /* fflush (input); */
    memset(buf, 0, length);

    if (!isatty(fileno(input))) {
        return fgets(buf, length, input);
    }

    while (1) {
        c = getch(); /* getch gets a character from the console */

        if (c == '\b') {
            if (end > buf)
                end--;
        }

        else if (--length > 0)
            *end++ = c;

        if (!c || c == '\n' || c == '\r')
            break;
    }

    return buf;
}
#endif
