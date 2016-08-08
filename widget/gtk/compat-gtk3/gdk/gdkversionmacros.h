#ifndef GDKVERSIONMACROS_WRAPPER_H
#define GDKVERSIONMACROS_WRAPPER_H

/**
 * Suppress all GTK3 deprecated warnings as deprecated functions are often
 * used for GTK2 compatibility.
 *
 * GDK_VERSION_MIN_REQUIRED cannot be used to suppress warnings for functions
 * deprecated in 3.0, but still needs to be set because gdkversionmacros.h
 * asserts that GDK_VERSION_MAX_ALLOWED >= GDK_VERSION_MIN_REQUIRED and
 * GDK_VERSION_MIN_REQUIRED >= GDK_VERSION_3_0.
 *
 * Setting GDK_DISABLE_DEPRECATION_WARNINGS would also disable
 * GDK_UNAVAILABLE() warnings, which are useful.
 */

#define GDK_VERSION_MIN_REQUIRED GDK_VERSION_3_0

#include_next <gdk/gdkversionmacros.h>

/* GDK_AVAILABLE_IN_ALL was introduced in 3.10 */
#ifndef GDK_AVAILABLE_IN_ALL
#define GDK_AVAILABLE_IN_ALL
#endif

#undef GDK_DEPRECATED
#define GDK_DEPRECATED GDK_AVAILABLE_IN_ALL
#undef GDK_DEPRECATED_FOR
#define GDK_DEPRECATED_FOR(f) GDK_AVAILABLE_IN_ALL

#endif /* GDKVERSIONMACROS_WRAPPER_H */
