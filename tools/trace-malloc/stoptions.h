/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
**  stoptions.h
**
**  Abstract the spacetrace options into a reusable format, such that
**      many different pieces of the code can utilize the common list.
*/

/*
**  There are three types of options.
**  The destinction is quite important.
**
**  CMD options are accessible from only the comamnd line.
**      Such options should be considered global/static for the entire
**          run of the application.
**      Once set, no one can change these options during the run.
**
**  WEB options are accessible from the web server options page.
**      Such options can and will be changed on a per user basis during
**          the run of the application.
**      You should NEVER make an option a WEB only option, as this will
**          break batch mode processing, and will likely not correctly
**          define the options structure itself.
**      These options will control the data caching used in the application
**          to match a client to a cache of data.
**
**  ALL options are both CMD and WEB options, with the properties of WEB
**      options (the user will change these on a per client basis).
**      Most likely this is the type of option you will desire to create.
*/

/*
**  All types of options have some combination of the following elements:
**
**  option_name             The name of the option.
**  option_genre            Area the option effects; STOptionGenre.
**  default_value           The default value for the option.
**  array_size              Used to size a string array.
**  multiplier              Some numbers prefer conversion.
**  option_help             Help text to explain the option.
**
**  NOTE! that the multiplier should be applied to the default value if you
**      are going to assign the default_value into anything.
**
**  Be very aware that adding things to a particular genre, or adding a genre,
**      may completely screw up the caching algorithms of SpaceTrace.
**  See contextLookup() or ask someone that knows if you are in doubt.
**
**  The actual definition of the WEB and CMD macros however is left to the
**      end user.
**  We cover those that you do not define herein.
*/
#if !defined(ST_CMD_OPTION_BOOL)
#define ST_CMD_OPTION_BOOL(option_name, option_genre, option_help)
#endif
#if !defined(ST_WEB_OPTION_BOOL)
#define ST_WEB_OPTION_BOOL(option_name, option_genre, option_help)
#endif
#if !defined(ST_CMD_OPTION_STRING)
#define ST_CMD_OPTION_STRING(option_name, option_genre, default_value, option_help)
#endif
#if !defined(ST_WEB_OPTION_STRING)
#define ST_WEB_OPTION_STRING(option_name, option_genre, default_value, option_help)
#endif
#if !defined(ST_CMD_OPTION_STRING_ARRAY)
#define ST_CMD_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help)
#endif
#if !defined(ST_WEB_OPTION_STRING_ARRAY)
#define ST_WEB_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help)
#endif
#if !defined(ST_CMD_OPTION_STRING_PTR_ARRAY)
#define ST_CMD_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help)
#endif
#if !defined(ST_WEB_OPTION_STRING_PTR_ARRAY)
#define ST_WEB_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help)
#endif
#if !defined(ST_CMD_OPTION_UINT32)
#define ST_CMD_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help)
#endif
#if !defined(ST_WEB_OPTION_UINT32)
#define ST_WEB_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help)
#endif
#if !defined(ST_CMD_OPTION_UINT64)
#define ST_CMD_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help)
#endif
#if !defined(ST_WEB_OPTION_UINT64)
#define ST_WEB_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help)
#endif

/*
**  ALL macros expand to both CMD and WEB macros.
**  This basically means such options are accessible from both the command
**      line and from the web options.
*/
#define ST_ALL_OPTION_BOOL(option_name, option_genre, option_help) \
    ST_CMD_OPTION_BOOL(option_name, option_genre, option_help) \
    ST_WEB_OPTION_BOOL(option_name, option_genre, option_help)
#define ST_ALL_OPTION_STRING(option_name, option_genre, default_value, option_help) \
    ST_CMD_OPTION_STRING(option_name, option_genre, default_value, option_help) \
    ST_WEB_OPTION_STRING(option_name, option_genre, default_value, option_help)
#define ST_ALL_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help) \
    ST_CMD_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help) \
    ST_WEB_OPTION_STRING_ARRAY(option_name, option_genre, array_size, option_help)
#define ST_ALL_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help) \
    ST_CMD_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help) \
    ST_WEB_OPTION_STRING_PTR_ARRAY(option_name, option_genre, option_help)
#define ST_ALL_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help) \
    ST_CMD_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help) \
    ST_WEB_OPTION_UINT32(option_name, option_genre, default_value, multiplier, option_help)
#define ST_ALL_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help) \
    ST_CMD_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help) \
    ST_WEB_OPTION_UINT64(option_name, option_genre, default_value, multiplier, option_help)



/****************************************************************************
**  BEGIN, THE OPTIONS
**
**  Order is somewhat relevant in that it will control 3 different things:
**      1)  The order the members will be in the options structure.
**      2)  The order the options are presented on the command line.
**      3)  The order the options are presented on the web options page.
*/

ST_ALL_OPTION_STRING(CategoryName,
                     CategoryGenre,
                     ST_ROOT_CATEGORY_NAME,
                     "Specify a category for reports to focus upon.\n"
                     "See http://lxr.mozilla.org/mozilla/source/tools/trace-malloc/rules.txt\n")

ST_ALL_OPTION_UINT32(OrderBy,
                     DataSortGenre,
                     ST_SIZE, /* for dp :-D */
                     1,
                     "Determine the sort order.\n"
                     "0 by weight (size * lifespan).\n"
                     "1 by size.\n"
                     "2 by lifespan.\n"
                     "3 by allocation count.\n"
                     "4 by performance cost.\n")

ST_ALL_OPTION_STRING_ARRAY(RestrictText,
                           DataSetGenre,
                           ST_SUBSTRING_MATCH_MAX,
                           "Exclude allocations which do not have this text in their backtrace.\n"
                           "Multiple restrictions are treated as a logical AND operation.\n")

ST_ALL_OPTION_UINT32(SizeMin,
                     DataSetGenre,
                     0,
                     1,
                     "Exclude allocations that are below this byte size.\n")

ST_ALL_OPTION_UINT32(SizeMax,
                     DataSetGenre,
                     0xFFFFFFFF,
                     1,
                     "Exclude allocations that are above this byte size.\n")

ST_ALL_OPTION_UINT32(LifetimeMin,
                     DataSetGenre,
                     ST_DEFAULT_LIFETIME_MIN,
                     ST_TIMEVAL_RESOLUTION,
                     "Allocations must live this number of seconds or be ignored.\n")

ST_ALL_OPTION_UINT32(LifetimeMax,
                     DataSetGenre,
                     ST_TIMEVAL_MAX / ST_TIMEVAL_RESOLUTION,
                     ST_TIMEVAL_RESOLUTION,
                     "Allocations living longer than this number of seconds will be ignored.\n")

ST_ALL_OPTION_UINT32(TimevalMin,
                     DataSetGenre,
                     0,
                     ST_TIMEVAL_RESOLUTION,
                     "Allocations existing solely before this second will be ignored.\n"
                     "Live allocations at this second and after can be considered.\n")

ST_ALL_OPTION_UINT32(TimevalMax,
                     DataSetGenre,
                     ST_TIMEVAL_MAX / ST_TIMEVAL_RESOLUTION,
                     ST_TIMEVAL_RESOLUTION,
                     "Allocations existing solely after this second will be ignored.\n"
                     "Live allocations at this second and before can be considered.\n")

ST_ALL_OPTION_UINT32(AllocationTimevalMin,
                     DataSetGenre,
                     0,
                     ST_TIMEVAL_RESOLUTION,
                     "Live and dead allocations created before this second will be ignored.\n")

ST_ALL_OPTION_UINT32(AllocationTimevalMax,
                     DataSetGenre,
                     ST_TIMEVAL_MAX / ST_TIMEVAL_RESOLUTION,
                     ST_TIMEVAL_RESOLUTION,
                     "Live and dead allocations created after this second will be ignored.\n")

ST_ALL_OPTION_UINT32(AlignBy,
                     DataSizeGenre,
                     ST_DEFAULT_ALIGNMENT_SIZE,
                     1,
                     "All allocation sizes are made to be a multiple of this number.\n"
                     "Closer to actual heap conditions; set to 1 for true sizes.\n")

ST_ALL_OPTION_UINT32(Overhead,
                     DataSizeGenre,
                     ST_DEFAULT_OVERHEAD_SIZE,
                     1,
                     "After alignment, all allocations are made to increase by this number.\n"
                     "Closer to actual heap conditions; set to 0 for true sizes.\n")

ST_ALL_OPTION_UINT32(ListItemMax,
                     UIGenre,
                     500,
                     1,
                     "Specifies the maximum number of list items to present in each list.\n")

ST_ALL_OPTION_UINT64(WeightMin,
                     DataSetGenre,
                     LL_INIT(0, 0),
                     LL_INIT(0, 1),
                     "Exclude allocations that are below this weight (lifespan * size).\n")

ST_ALL_OPTION_UINT64(WeightMax,
                     DataSetGenre,
                     LL_INIT(0xFFFFFFFF, 0xFFFFFFFF),
                     LL_INIT(0, 1),
                     "Exclude allocations that are above this weight (lifespan * size).\n")

ST_CMD_OPTION_STRING(FileName,
                     DataSetGenre,
                     "-",
                     "Specifies trace-malloc input file.\n"
                     "\"-\" indicates stdin will be used as input.\n")

ST_CMD_OPTION_STRING(CategoryFile,
                     CategoryGenre,
                     "rules.txt",
                     "Specifies the category rules file.\n"
                     "This file contains rules about how to categorize allocations.\n")

                     
ST_CMD_OPTION_UINT32(HttpdPort,
                     ServerGenre,
                     1969,
                     1,
                     "Specifies the default port the web server will listen on.\n")

ST_CMD_OPTION_STRING(OutputDir,
                     BatchModeGenre,
                     ".",
                     "Specifies a directory to output batch mode requests.\n"
                     "The directory must exist and must not use a trailing slash.\n")

ST_CMD_OPTION_STRING_PTR_ARRAY(BatchRequest,
                               BatchModeGenre,
                               "This implicitly turns on batch mode.\n"
                               "Save each requested file into the output dir, then exit.\n")

ST_CMD_OPTION_UINT32(Contexts,
                     ServerGenre,
                     1,
                     1,
                     "How many configurations to cache at the cost of a lot of memory.\n"
                     "Dedicated servers can cache more client configurations for performance.\n")

ST_CMD_OPTION_BOOL(Help,
                   UIGenre,
                   "Show command line help.\n"
                   "See http://www.mozilla.org/projects/footprint/spaceTrace.html\n")

/*
**  END, THE OPTIONS
****************************************************************************/

/*
**  Everything is undefined after the header is included.
**  This sets it up for multiple inclusion if so desired.
*/
#undef ST_ALL_OPTION_BOOL
#undef ST_CMD_OPTION_BOOL
#undef ST_WEB_OPTION_BOOL
#undef ST_ALL_OPTION_STRING
#undef ST_CMD_OPTION_STRING
#undef ST_WEB_OPTION_STRING
#undef ST_ALL_OPTION_STRING_ARRAY
#undef ST_CMD_OPTION_STRING_ARRAY
#undef ST_WEB_OPTION_STRING_ARRAY
#undef ST_ALL_OPTION_STRING_PTR_ARRAY
#undef ST_CMD_OPTION_STRING_PTR_ARRAY
#undef ST_WEB_OPTION_STRING_PTR_ARRAY
#undef ST_ALL_OPTION_UINT32
#undef ST_CMD_OPTION_UINT32
#undef ST_WEB_OPTION_UINT32
#undef ST_ALL_OPTION_UINT64
#undef ST_CMD_OPTION_UINT64
#undef ST_WEB_OPTION_UINT64
