/*
	mac_stdlib.cpp

	replacement functions for the Metrowerks plugin.
	
	by Patrick C. Beard.
 */

#include <stdio.h>
#include <stdlib.h>

// simply throw us out of here!

void std::exit(int status)
{
	throw status;
}


