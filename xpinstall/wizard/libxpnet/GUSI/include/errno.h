/*  Metrowerks Standard Library  Version 2.4  1998 March 10  */

/*
 *	errno.h
*/

/* Adapted for GUSI by Matthias Neeracher <neeri@iis.ee.ethz.ch> */

#ifndef _ERRNO_H
#define _ERRNO_H

#ifdef __MWERKS__
#include <cerrno>
/*
 * Undef error codes defined by MSL. We are overriding the MSL implementations, so
 * these versions of the codes should never be generated anyway.
 */
#undef EEXIST
#undef ENOTEMPTY
#undef EISDIR
#undef EPERM
#undef EACCES
#undef EBADF
#undef EDEADLOCK
#undef EMFILE
#undef ENOENT
#undef ENFILE
#undef ENOSPC
#undef EINVAL
#undef EIO
#undef ENOMEM
#undef ENOSYS
#undef ENAMETOOLONG
#undef EDEADLK
#undef EAGAIN
#else
#include <mpw/errno.h>
#endif

#include <sys/errno.h>

#if defined(__cplusplus) && defined(_MSL_USING_NAMESPACE) && (__MSL__ < 0x5000)
	using namespace std;
#endif

#endif
