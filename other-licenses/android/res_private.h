/*	$NetBSD: res_private.h,v 1.1.1.1 2004/05/20 17:18:54 christos Exp $	*/

/*
 * This version of this file is derived from Android 2.3 "Gingerbread",
 * which contains uncredited changes by Android/Google developers.  It has
 * been modified in 2011 for use in the Android build of Mozilla Firefox by
 * Mozilla contributors (including Michael Edwards <m.k.edwards@gmail.com>,
 * and Steve Workman <sjhworkman@gmail.com>).
 * These changes are offered under the same license as the original NetBSD
 * file, whose copyright and license are unchanged above.
 */

#ifndef res_private_h
#define res_private_h

struct __res_state_ext {
	union res_sockaddr_union nsaddrs[MAXNS];
	struct sort_list {
		int     af;
		union {
			struct in_addr  ina;
			struct in6_addr in6a;
		} addr, mask;
	} sort_list[MAXRESOLVSORT];
	char nsuffix[64];
	char nsuffix2[64];
};

extern int
res_ourserver_p(const res_state statp, const struct sockaddr *sa);

#endif
