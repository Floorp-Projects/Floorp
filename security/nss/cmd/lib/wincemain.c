/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WINCE
#include <windows.h>

int
wmain(int argc, WCHAR **wargv)
{
    char **argv;
    int i, ret;

    argv = malloc(argc * sizeof(char*));

    for (i = 0; i < argc; i++) {
        int len = WideCharToMultiByte(CP_ACP, 0, wargv[i], -1, NULL, 0, 0, 0);
        argv[i] = malloc(len * sizeof(char));
        WideCharToMultiByte(CP_ACP, 0, wargv[i], -1, argv[i], len, 0, 0);
    }

    ret = main(argc, argv);

    for (i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);

    return ret;
}

#endif

