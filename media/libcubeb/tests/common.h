/*
 * Copyright © 2013 Sebastien Alaiwan
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#if defined( _WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

void delay(unsigned int ms)
{
#if defined(_WIN32)
	Sleep(ms);
#else
	sleep(ms / 1000);
	usleep(ms % 1000 * 1000);
#endif
}

#if !defined(M_PI)
#define M_PI 3.14159265358979323846
#endif

