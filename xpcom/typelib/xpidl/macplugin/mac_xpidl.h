/*
	mac_xpidl.h
	
	prototypes for the Mac CodeWarrior plugin version of xpidl.
	
	by Patrick C. Beard.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _STDIO_H
#include <stdio.h>
#endif

FILE* mac_fopen(const char* filename, const char *mode);

void mac_warning(const char* warning_message);
void mac_error(const char* error_message);

#ifdef __cplusplus
}
#endif
