#if (  defined(_WIN32) || defined(__CYGWIN__) )
/* include/sphinx_config.h, defaults for Win32 */
/* sphinx_config.h: Externally visible configuration parameters for
 * SphinxBase.
 */

/* Default radix point for fixed-point */
/* #undef DEFAULT_RADIX */

/* Use Q15 fixed-point computation */
/* #undef FIXED16 */

/* Use fixed-point computation */
/* #undef FIXED_POINT */

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 4
#else
/* include/sphinx_config.h.  Generated from sphinx_config.h.in by configure.  */
/* sphinx_config.h: Externally visible configuration parameters */

/* Default radix point for fixed-point */
/* #undef DEFAULT_RADIX */

/* Use Q15 fixed-point computation */
/* #undef FIXED16 */

/* Use fixed-point computation */
/* #undef FIXED_POINT */

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 8

/* Define to 1 if the system has the type `long long'. */
/*#define HAVE_LONG_LONG 1*/

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* Enable debugging output */
/* #undef SPHINX_DEBUG */
#endif
