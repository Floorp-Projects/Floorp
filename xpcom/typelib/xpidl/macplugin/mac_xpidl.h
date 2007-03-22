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

size_t mac_get_file_length(const char* filename);
void mac_warning(const char* warning_message);
void mac_error(const char* error_message);

#ifdef __cplusplus
}
#endif
