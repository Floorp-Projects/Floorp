/*
	mac_strings.h
 */

#pragma once

#include <MacTypes.h>

StringPtr c2p_strcpy(StringPtr pstr, const char* cstr);
char* p2c_strcpy(char* cstr, const StringPtr pstr);
char* p2c_strdup(StringPtr pstr);
