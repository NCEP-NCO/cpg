/*****************************************************************************
 *
 *    SCS_Obs_definitions.h
 *
 *    DESCRIPTION
 *
 *       This is the include file for the SCS_Obs_definitions function
 *       found in SCS_Obs_definitions.c.
 *
 *       This file should be included by any application that intends
 *       to invoke the SCS_Obs_definitions() function.
 *
 *       This file includes all of the necessary standard header files
 *       that are needed by the SCS_Obs_definitions() function.
 *
 *       This file also contains macro definitions, type definitions,and
 *       function prototypes used within the SCS_Obs_definitions.c
 *       source file.
 *
 *    Source Code Control Information
 *    %M% %I%:  %G% %U%
 *
 *****************************************************************************/

#ifndef _SCS_Obs_definitions_h

#define _SCS_Obs_definitions_h

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600 /* for strptime() in time.h */
#endif

#ifndef	_BSD_SOURCE
#define	_BSD_SOURCE 1 /* for setenv() in stdlib.h */
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef TRUE
#undef TRUE
#endif

#define TRUE 1

#ifdef FALSE
#undef FALSE
#endif

#define FALSE 0

#ifdef YES
#undef YES
#endif

#define YES TRUE

#ifdef NO
#undef NO
#endif

#define NO FALSE

#ifdef CHAR_NULL
#undef CHAR_NULL
#endif

#define CHAR_NULL ((char *) NULL)

#ifdef NULL_CHAR
#undef NULL_CHAR
#endif

#define NULL_CHAR '\0'

#define SAFE_COPY(D,S) sprintf( D, "%.*s", (int) sizeof( D ) - 1, S );

#define SCS_WIDTH 32
#define MAX_HOURLY_TIMES 48  /* in hours, 2 days */
#define SYNOPTIC_TIME_PERIOD 6  /** hours **/
#define PRECIP_SYNOPTIC_TIME_PERIOD SYNOPTIC_TIME_PERIOD
#define MAX_PRECIP_SYNOPTIC_TIMES 4

#define SECONDS_PER_HOUR ((time_t) 3600)
#define HOURS_PER_DAY 24

#define TIME0 ((time_t) 0)


/* a utility structure used within the SCS_Obs_definitions.c source file. */
typedef struct
{
char date_time_str[BUFSIZ];
char timezone_str[BUFSIZ];
struct tm ts;
time_t input_time;
time_t t;
int hours_west;
int is_dst;
int consider_dst;
int is_so_far;
int so_far_local_hour;
int the_local_hour;
int do_all_timezones;
time_t hourly_times[MAX_HOURLY_TIMES];
int num_hourly_times;
time_t most_historical_time;
int only_max_min_temps;
} SCS_utility_t, * SCS_utility_tp;

/**
This type definition defines the structure of the derived data produced by
 one invocation of the SCS_Obs_definitions() function.
**/
typedef struct
{

char datetime[SCS_WIDTH];
char timezone[SCS_WIDTH];
time_t max_temp_hourly_times[MAX_HOURLY_TIMES];
int num_max_temp_hourly_times;
time_t max_temp_synoptic_times[MAX_HOURLY_TIMES];
int num_max_temp_synoptic_times;
time_t max_temp_so_far_hourly_times[MAX_HOURLY_TIMES];
int num_max_temp_so_far_hourly_times;
time_t max_temp_so_far_synoptic_times[MAX_HOURLY_TIMES];
int num_max_temp_so_far_synoptic_times;

time_t min_temp_hourly_times[MAX_HOURLY_TIMES];
int num_min_temp_hourly_times;
time_t min_temp_synoptic_times[MAX_HOURLY_TIMES];
int num_min_temp_synoptic_times;
time_t min_temp_so_far_hourly_times[MAX_HOURLY_TIMES];
int num_min_temp_so_far_hourly_times;
time_t min_temp_so_far_synoptic_times[MAX_HOURLY_TIMES];
int num_min_temp_so_far_synoptic_times;

time_t precip_synoptic_times[MAX_HOURLY_TIMES];
int num_precip_synoptic_times;

} SCS_Obs_definitions_t, * SCS_Obs_definitions_tp, ** SCS_Obs_definitions_tpp;

/* function prototypes. */
SCS_Obs_definitions_tp SCS_Obs_definitions( char *datetime_str,
   char *timezone_str,
   int only_max_min_temps,
   int *num_defs );

#endif
