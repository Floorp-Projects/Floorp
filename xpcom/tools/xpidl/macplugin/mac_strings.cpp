/*
	mac_strings.cpp
 */

#include "mac_strings.h"

#include <string.h>
#include <Memory.h>
#include <new>

StringPtr c2p_strcpy(StringPtr pstr, const char* cstr)
{
	size_t len = ::strlen(cstr);
	if (len > 255) len = 255;
	BlockMoveData(cstr, pstr + 1, len);
	pstr[0] = len;
	return pstr;
}

char* p2c_strcpy(char* cstr, const StringPtr pstr)
{
	size_t len = pstr[0];
	BlockMoveData(pstr + 1, cstr, len);
	cstr[len] = '\0';
	return cstr;
}

char* p2c_strdup(StringPtr pstr)
{
	size_t len = pstr[0];
	char* cstr = new char[1 + len];
	if (cstr != NULL) {
		BlockMoveData(pstr + 1, cstr, len);
		cstr[len] = '\0';
	}
	return cstr;
}

