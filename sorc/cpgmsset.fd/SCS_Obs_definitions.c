/*****************************************************************************
 *
 *    SCS_Obs_definitions.c
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    DESCRIPTION
 *
 *       This file contains the essential function, SCS_Obs_definitions(),
 *       that is utilized to determine/define the
 *       max temp hourly date/times,
 *       max temp synoptic date/times,
 *       max temp so far hourly date/times,
 *       max temp so far synoptic date/times,
 *       min temp hourly date/times,
 *       min temp synoptic date/times,
 *       min temp so far hourly date/times,
 *       min temp so far synoptic date/times,
 *       and
 *       precip synoptic date/times,
 *       for the weather observations required by the Selected Cities
 *       Summary (SCS) given the input date/time and the input timezone.
 *
 *       In addition to the essential function described above, there are
 *       also about 30 static utility functions that are directly or
 *       indirectly utilized to contribute and perform the logic necessary
 *       to complete the tasks of the essential function.
 *       The utility functions are declared to be static because their
 *       functionality and utilized data types are very specific to the
 *       essential function and their use in other applications is not
 *       defined.
 *
 *    Source Code Control Information
 *    %M% %I%:  %G% %U%
 *
 *****************************************************************************/

#ifndef __LINT__
static const char _WhatStringID[] = "%PWHAT%%PM%;%PR% %PRT%";
#endif

#include "SCS_Obs_definitions.h"


/* static function prototypes. */
static int st_in_major_so_far_window( SCS_utility_tp utsp,
   int window_first_hour,
   int window_second_hour );

static void st_essential_initializations( int ef_call_count,
   char *datetime_str, char *timezone_str,
   int only_max_min_temps,
   int *num_defs, SCS_utility_tp utsp, SCS_Obs_definitions_tp a_def,
   SCS_Obs_definitions_tpp return_defs );

static void st_save_definition_datetime_and_timezone( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static void st_copy_times( time_t *dest_times, int num_times,
   time_t *source_times );

static int is_all_timezone( char *tz );

static void st_max_temp_logic( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static void st_min_temp_logic( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static void st_precip_logic( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static int st_is_in_so_far_window( SCS_utility_tp utsp,
   int window_first_hour, int window_second_hour,
   int at_least_hour );

static void st_get_temp_hourly_times( SCS_utility_tp utsp,
   int first_local_hour, int second_local_hour );

static void st_finalize_hourly_times( SCS_utility_tp utsp,
   time_t *the_times, int *num_times );

static int st_is_synoptic_period( time_t *time_array, int array_index,
   int array_size, time_t *synoptic_time );

static void st_eliminate_most_historical_time( SCS_utility_tp utsp,
   time_t *time_array, int *array_size );

static void st_error( int stop_process, char *fmt, ... );

static int st_is_the_local_hour( SCS_utility_tp utsp,
   int the_local_hour, time_t t );

static void st_get_hours_west( SCS_utility_tp utsp );

static void st_determine_dst( SCS_utility_tp utsp );

static void st_zero_hourly_times( SCS_utility_tp utsp );

static void st_validate_date_time( SCS_utility_tp utsp );

static void st_validate_timezone( SCS_utility_tp utsp );

static void st_get_temp_synoptic_times( time_t *mta, int num_mta,
   time_t *temp_synoptic_times, int *num_temp_synoptic_times );

static void st_get_max_temp_hourly_times( SCS_utility_tp utsp );

static void st_get_max_temp_synoptic_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static void st_finalize_max_temp_hourly_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static void st_get_max_temp_so_far_hourly_times( SCS_utility_tp utsp );

static void st_get_max_temp_so_far_synoptic_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static void st_finalize_max_temp_so_far_hourly_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static void st_get_min_temp_hourly_times( SCS_utility_tp utsp );

static void st_get_min_temp_synoptic_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static void st_finalize_min_temp_hourly_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static void st_get_min_temp_so_far_hourly_times( SCS_utility_tp utsp );

static void st_get_min_temp_so_far_synoptic_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static void st_finalize_min_temp_so_far_hourly_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static void st_get_precip_synoptic_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr );

static int st_comp_hourly_times( const void *cvp1, const void *cvp2 );

static void st_allocate_definition_memory( SCS_Obs_definitions_tp in_defs,
   SCS_Obs_definitions_tpp return_defs,
   int *num_return_defs );

static void st_make_ncep_tz_adjustment( int ef_call_count,
   int set_tz_to_gmt );

/* global-static data. */

/**
The all_str variable insures that the "ALL" string is utilized consistently
 throughout this code to represent the constant string that indicates that
 all of the timezones are to be processed by the essential function.
**/
static char all_str[] = "ALL";

/**
The recognized_timezones array defines the set of timezones that this
 software is prepared to process.
**/
static char *recognized_timezones[] =
{
"AST4",
"AST4ADT",
"EST5EDT",
"CST6CDT",
"MST7MDT",
"MST7",
"PST8PDT",
"AKST9AKDT",
"HST10",
"LST-10",
all_str,
CHAR_NULL
};


/*****************************************************************************
 *
 *    SCS_Obs_definitions()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function (the essential function) is designed to coordinate
 *       the logic for deriving the:
 *       max temp hourly date/times,
 *       max temp synoptic date/times,
 *       max temp so far hourly date/times,
 *       max temp so far synoptic date/times,
 *       min temp hourly date/times,
 *       min temp synoptic date/times,
 *       min temp so far hourly date/times,
 *       min temp so far synoptic date/times,
 *       and
 *       precip synoptic date/times,
 *       for the weather observations required by the Selected Cities
 *       Summary (SCS) given the input date/time and the input timezone.
 *
 *       The derived information is stored in a dynamically allocated
 *       memory buffer and the address of that buffer is returned
 *       to the caller.
 *       The caller is then responsible for the dynamically allocated
 *       memory which should be released via the free() function.
 *
 *    ARGUMENTS
 *
 *       datetime_str = A string that is of the form "YYYYMMDDHH" that
 *                    specifies the year, month, day, and hour to
 *                      be consider by this function when deriving the
 *                      date/times for the necessary SCS ovbservations.
 *                      (char * input)
 *
 *       timezone_str = Specifies the timezone that should be considered
 *                      by this function when computing the date/times
 *                      for the SCS observations.
 *
 *                      The array of recognized timezones (found above),
 *                      are the only timezones that will be processed
 *                      by this function, otherwise an error message
 *                      will be displayed and the process will be terminated
 *                      via the exit() function.
 *
 *                      The case of the alphabetic characters
 *                      is insignificant.
 *
 *                      (char * input)
 *
 *
 * only_max_min_temps = This argument indicates to this function that
 *                    the caller only wants the max temp and min temp
 *                    observation definitions to be derived.
 *This variable evaluates to either TRUE (non-zero) or FALSE (zero.
 *                      (int input)
 *
 *           num_defs = A pointer to an integer's memory location that
 *                    will indicate to the caller the number of SCS Obs
 *                    definition structures that are in the buffer pointed
 *                    to by the address that is returned by this function.
 *                      (int * output)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       The address of the dynamically allocated buffer of memory composed
 *       of SCS Obs definition structures that contain the data derived
 *       by this function.
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *         10/02/2006          Joseph Lang added only_max_min_temps argument
 *
 *    NOTES
 *       Please remember that the address returned by this function points
 *       to a dynamically allocated buffer of memory, and the caller
 *       is responsible for releasing (via free()) that memory once the
 *       buffered data is no longer needed.
 *
 *****************************************************************************/

SCS_Obs_definitions_tp SCS_Obs_definitions( char *datetime_str,
   char *timezone_str,
   int only_max_min_temps,
   int *num_defs )
{
static SCS_Obs_definitions_tp return_defs;
static int call_count = 0;
SCS_utility_t uts; /* utility structure */
SCS_Obs_definitions_t a_def;
int i;

/**
adjust the TZ environment variable for systems not configured with UTC
as the system time.
**/
   st_make_ncep_tz_adjustment( call_count, YES );

/* increment the call count variable for recursion control. */
   ++call_count;

/* perform essential initializations required by the essential function. */
   st_essential_initializations( call_count, datetime_str, timezone_str,
      only_max_min_temps,
      num_defs, &uts, &a_def,
      &return_defs );

/* make sure the input date-time string is valid. */
   st_validate_date_time( &uts );

/* make sure we recognize the input timezone. */
   st_validate_timezone( &uts );

/* get the hours west of gmt value for this timezone. */
   st_get_hours_west( &uts );

/* determine if this timezone is observing daylight savings time (DST). */
   st_determine_dst( &uts );

/* save these values in our definition structure. */
   st_save_definition_datetime_and_timezone( &uts, &a_def );

/* special processing for a "all timezones" request. */
   if ( uts.do_all_timezones )
   {

/* process "all" of the recognized timezones. */
      for (i = 0; recognized_timezones[i] != CHAR_NULL; i++)
      {

/* when we hit the "ALL" string, we are done recursing. */
         if ( is_all_timezone( recognized_timezones[i] ) ) break;

/* a recursive call to this function will do the work for us. */
         return_defs = SCS_Obs_definitions( datetime_str,
            recognized_timezones[i], only_max_min_temps, num_defs );

      } /* for (i) */

   } /* if ( uts.do_all_timezones ) */
   else
/* here we are getting the observation definitions for a single timezone. */
   {

/* derive the information for max temp. */
      st_max_temp_logic( &uts, &a_def );

/* derive the information for min temp. */
      st_min_temp_logic( &uts, &a_def );

/* derive the information for precip. */
      st_precip_logic( &uts, &a_def );

/* allocate memory to store the derived information. */
      st_allocate_definition_memory( &a_def, &return_defs, num_defs );

   }

/* we're about to leave, so decrement our recursion call count variable. */
   --call_count;

/**
now reset the TZ environment variable back to its original value for
 systems not configured with UTC as the system time.
**/
   st_make_ncep_tz_adjustment( call_count, NO );

   return return_defs;
} /* SCS_Obs_definitions() */


/*****************************************************************************
 *
 *    st_error()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function writes the supplied error message to the standard
 *       error file and the process is then terminated via the exit()
 *       function. This function utilizes variable argument logic similar
 *       to that which is utilized by the family of printf() functions.
 *
 *    ARGUMENTS
 *
 *       stop_process = A logical (True or False) variable that indicates
 *                    to this function if the caller wants the process
 *                    to be stopped because of the detected error.
 *                      (int input)
 *
 *                fmt = The format of the data to be displayed by this
 *                    function. Works exactly like the fmt argument supplied
 *                    to printf().
 *                      (char * input)
 *
 *                ... = The variable arguments that are supplied to this
 *                    function.
 *                      (unknown input)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_error( int stop_process, char *fmt, ... )
{
va_list ap;

/* leave if the fmt value is invalid. */
   if ( fmt == CHAR_NULL ) return;

   if ( *fmt == NULL_CHAR ) return;

/* set pointer to the variable argument list. */
   va_start( ap, fmt );

/* output the error message. */
   vfprintf( stderr, fmt, ap );

/* free up the argument list pointer. */
   va_end( ap );

/* print a system error message if there is one. */
   if ( errno ) fprintf( stderr, "\n%s\n", strerror( errno ) );

/* does the caller want us to terminate the process? */
   if ( stop_process )
   {
   fprintf( stderr, "\nProgram Aborted!\n\n" );
      exit( EXIT_FAILURE );
   }

   return;
} /* st_error() */


/*****************************************************************************
 *
 *    st_validate_date_time()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function evaluates the date/time value that was received
 *       by the essential function. If the date/time is determined to
 *       be invalid, the st_error() function is called and the process
 *       is terminated.
 *
 *       If the input date/time value is valid, the input_time (time_t)
 *       member of the utsp structure is initialized with the time_t
 *       value that corresponds to the input date/time.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                    the date/time value to be evaluated by this function.
 *                      (SCS_utility_tp input)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_validate_date_time( SCS_utility_tp utsp )
{
static char st_fn[] = "st_validate_date_time()";
char *ptr;
char editted_date_time_str[BUFSIZ];

/* initialization. */
   memset( &utsp->ts, 0, sizeof( utsp->ts ) );

/**
"edit" the input date/time value to accomodate the AIX implementation
of strptime().

From Jeff Ator's email message:

Jeff Ator <Jeff.Ator@noaa.gov>
Sent
Tuesday, September 19, 2006 12:25 pm
To
James Calkins <James.Calkins@noaa.gov>
Cc
"Dr. John M. Huddleston" <John.M.Huddleston@noaa.gov>
 ,
Joseph Lang <Joseph.Lang@noaa.gov>

Unfortunately there aren't any manpages on the CCS.  However,
using Google I was
able to find what I needed at
http://publib.boulder.ibm.com/infocenter/pseries/v5r3/index.jsp
?topic=/com.ibm.aix.basetechref/doc/basetrf2/strptime.htm
Simply put, on IBM systems strptime doesn't
allow adjacent format specifiers without
intervening whitespace or other separators.  So the string "%Y%m%d%H"
doesn't work,
but if you instead make it "%Y %m %d %h" and add
corresponding spaces to a copy of
the input date-time string, then it works.

so we will edit the date/time data so it will appear as "YYYY MM DD HH".
**/
   sprintf( editted_date_time_str, "%.4s %.2s %.2s %.2s",
      utsp->date_time_str + 0, /* points to the "YYYY" */
      utsp->date_time_str + 4, /* points to the "MM" */
      utsp->date_time_str + 6, /* points to the "DD" */
      utsp->date_time_str + 8 ); /* points to the "HH" */

/* let strptime() do the evaluation. */
   ptr = strptime( editted_date_time_str, "%Y %m %d %H", &utsp->ts );

/* the date/time was not valid. */
   if ( ptr == CHAR_NULL )
   {

      st_error( TRUE,
         "Invalid date/time value [%s]\n"
         "detected by strptime()\n"
         "in %s.\n",
         utsp->date_time_str, st_fn );

   }

/* convert the struct tm representation of time into a time_t value. */
   utsp->input_time = mktime( &utsp->ts );

   return;
} /* st_validate_date_time() */


/*****************************************************************************
 *
 *    st_validate_timezone()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function evaluates the timezone value that was received
 *       by the essential function. If the timezone is determined to
 *       be invalid, the st_error() function is called and the process
 *       is terminated.
 *
 *       The timezone value is validated by comparing the supplied value
 *       with the values contained in the recognized_timezones array
 *       found above.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                    the timezone value to be evaluated by this function.
 *                      (SCS_utility_tp input)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       The case of the input timezone is insignificant when being compared
 *       with the values in the recognized timezones array.
 *
 *       The set of recognized timezones is displayed in the error message
 *       when an invalid timezone is detected.
 *
 *****************************************************************************/

static void st_validate_timezone( SCS_utility_tp utsp )
{
static char st_fn[] = "st_validate_timezone()";
int i;
char *rt;
int valid_timezone = FALSE;

/* search the recognized timezones for the input timezone. */
   for (i = 0; recognized_timezones[i] != CHAR_NULL; i++)
   {

      valid_timezone = (strcasecmp( recognized_timezones[i],
         utsp->timezone_str ) == 0);

      if ( valid_timezone ) break;

   } /* for (i) */

/* not valid, not good! */
   if ( ! valid_timezone )
   {

      st_error( FALSE, "Invalid timezone [%s] found in: %s\n",
         utsp->timezone_str, st_fn );

      st_error( FALSE, "\nRecognized Timezones:\n\n" );

      for (i = 0; recognized_timezones[i] != CHAR_NULL; i++)
      {

         rt = recognized_timezones[i];

         st_error( FALSE, "%-11s", rt );

         if ( is_all_timezone( rt ) )
         {

            st_error( FALSE, "  > utilized when all of the above timezone "
         "definitions are desired." );

         }

         st_error( FALSE, "\n" );

      } /* for (i) */

      st_error( TRUE, "\n" );

   }

   return;
} /* st_validate_timezone() */


/*****************************************************************************
 *
 *    st_get_max_temp_hourly_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function defines the local time "window" for max temperature.
 *       The function then invokes the st_get_temp_hourly_times() function
 *       to derive the max temp date/times given the local time window
 *       for max temp.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the necessary members for buffering the values
 *                      computed for hourly times.
 *                      (SCS_utility_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *         02/25/2009          Shucai Guan Adopts new Alaskan definitions for maximum 
 *                                    and minimum temperatures for stations in Alaska.
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_get_max_temp_hourly_times( SCS_utility_tp utsp )
{
int local_hour_7pm = 19;
int local_hour_7am = 7;

/* New Alaskan definitions for MaxT and MinT grids */ 
if (strcmp(utsp->timezone_str, "AKST9AKDT") == 0)
{
    local_hour_7pm = 20;
    local_hour_7am = 5;
}

/* get st_get_temp_hourly_times() to derive the max temp hourly times. */
   st_get_temp_hourly_times( utsp, local_hour_7pm, local_hour_7am );

   return;
} /* st_get_max_temp_hourly_times() */


/*****************************************************************************
 *
 *    st_get_max_temp_synoptic_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function invokes the st_get_temp_synoptic_times() function
 *       with the correct arguments for max temp synoptic times
 *       to derive the max temp synoptic date/times.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the necessary members for buffering the values
 *                      computed for hourly times.
 *                      (SCS_utility_tp input)
 *
 *            def_ptr = The address of a SCS_Obs_definitions structure
 *                    that will return to the caller the computed synoptic
 *                    date/time values for max temp.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_get_max_temp_synoptic_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{

/* get st_get_temp_synoptic_times() to derive the max temp synoptic times. */
   st_get_temp_synoptic_times( utsp->hourly_times, utsp->num_hourly_times,
      def_ptr->max_temp_synoptic_times,
      &def_ptr->num_max_temp_synoptic_times );

   return;
} /* st_get_max_temp_synoptic_times() */


/*****************************************************************************
 *
 *    st_finalize_max_temp_hourly_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function invokes the st_finalize_temp_hourly_times() function
 *       with the correct arguments for max temp hourly times
 *       to finalize the set of valid date/time values for hourly
 *       max temp.
 *       To "finalize" involves removing the max temp synoptic time periods
 *       along with some other date/time values that are not to be considered
 *       for max temp hourly times. See the documentation for
 *       st_finalize_hourly_times()
 *       for further details about the "finalize" logic.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the necessary members for buffering the values
 *                      computed for hourly times.
 *                      (SCS_utility_tp input)
 *
 *            def_ptr = The address of an SCS_Obs_definitions structure
 *                      that will return to the caller the finalized
 *                      hourly date/time values for max temp.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_finalize_max_temp_hourly_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{

/**
get st_finalize_hourly_times() to finalize the hourly times for max temp.
**/
   st_finalize_hourly_times( utsp, def_ptr->max_temp_hourly_times,
      &def_ptr->num_max_temp_hourly_times );

   return;
} /* st_finalize_max_temp_hourly_times()  */


/*****************************************************************************
 *
 *    st_get_min_temp_hourly_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function defines the local time "window" for min temperature.
 *       The function then invokes the st_get_temp_hourly_times() function
 *       to derive the min temp date/times given the local time window
 *       for min temp.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the necessary members for buffering the values
 *                      computed for hourly times.
 *                      (SCS_utility_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *         02/25/2009          Shucai Guan Adopts new Alaskan definitions for maximum
 *                                    and minimum temperatures for stations in Alaska.
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_get_min_temp_hourly_times( SCS_utility_tp utsp )
{
int local_hour_8am = 8;
int local_hour_7pm = 19;

/* New Alaskan definitions for MaxT and MinT grids */
if (strcmp(utsp->timezone_str, "AKST9AKDT") == 0)
{
    local_hour_8am = 11;
    local_hour_7pm = 17;	
}

/* get st_get_temp_hourly_times() to derive the min temp hourly times. */
   st_get_temp_hourly_times( utsp, local_hour_8am, local_hour_7pm );

   return;
} /* st_get_min_temp_hourly_times() */


/*****************************************************************************
 *
 *    st_get_min_temp_synoptic_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function invokes the st_get_temp_synoptic_times() function
 *       with the correct arguments for min temp synoptic times
 *       to derive the min temp synoptic date/times.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the necessary members for buffering the values
 *                      computed for hourly times.
 *                      (SCS_utility_tp input)
 *
 *            def_ptr = The address of an SCS_Obs_definitions structure
 *                    that will return to the caller the computed synoptic
 *                    date/time values for min temp.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_get_min_temp_synoptic_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{

/* get st_get_temp_synoptic_times() to derive the min temp synoptic times. */
   st_get_temp_synoptic_times( utsp->hourly_times, utsp->num_hourly_times,
      def_ptr->min_temp_synoptic_times,
      &def_ptr->num_min_temp_synoptic_times );

   return;
} /* st_get_min_temp_synoptic_times() */


/*****************************************************************************
 *
 *    st_finalize_min_temp_hourly_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function invokes the st_finalize_temp_hourly_times() function
 *       with the correct arguments for min temp hourly times
 *       to finalize the the set of valid date/time values for hourly
 *       min temp.
 *       To "finalize" involves removing the min temp synoptic time periods
 *       along with some other date/time values that are not to be considered
 *       for min temp hourly times. See the documentation for
 *       st_finalize_hourly_times()
 *       for further details about the "finalize" logic.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the necessary members for buffering the values
 *                      computed for hourly times.
 *                      (SCS_utility_tp input)
 *
 *            def_ptr = The address of an SCS_Obs_definitions structure
 *                      that will return to the caller the finalized
 *                      hourly date/time values for min temp.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_finalize_min_temp_hourly_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{

/**
get st_finalize_hourly_times() to finalize the hourly times for min temp.
**/
   st_finalize_hourly_times( utsp, def_ptr->min_temp_hourly_times,
      &def_ptr->num_min_temp_hourly_times );

   return;
} /* st_finalize_min_temp_hourly_times()  */


/*****************************************************************************
 *
 *    st_get_precip_synoptic_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function derives the synoptic times for the 4 complete
 *       synoptic periods that exist prior to, and possibly including,
 *       the input date/time value that was provided to the essential
 *       function.
 *
 *       Synoptic hours are recognized at 0, 6, 12, and 18 hours. A synoptic
 *       period consists of a 6 hour period terminated by a synoptic
 *       hour.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the input_time member. The input_time member
 *                      is the time_t representation for the input date/time
 *                      that was provided to the essential function.
 *                      (SCS_utility_tp input)
 *
 *            def_ptr = The address of an SCS_Obs_definitions structure
 *                      that will return to the caller the
 *                      synoptic date/time values for precip.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_get_precip_synoptic_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{
int pstp = PRECIP_SYNOPTIC_TIME_PERIOD;
time_t seconds_per_precip_period = SECONDS_PER_HOUR * pstp;
struct tm synoptic_ts;
int synoptic_hour;
time_t synoptic_time[MAX_PRECIP_SYNOPTIC_TIMES];
int i;

/* some initializations. */
   synoptic_ts = *gmtime( &utsp->input_time );

   synoptic_hour = (synoptic_ts.tm_hour / pstp) * pstp;

/* set the struct tm hour for the most recent precip synoptic time. */
   synoptic_ts.tm_hour = synoptic_hour;

/* now generate the time_t representation for the precip synoptic times. */
   for (i = 0; i < MAX_PRECIP_SYNOPTIC_TIMES; i++)
   {

/* get the time_t representation of the precip synoptic time. */
      if ( i == 0 )
      {

/* compute the first time_t value for the synoptic ts structure. */
         synoptic_time[i] = mktime( &synoptic_ts );

      }
      else
      {

/* get the time_t value for the subsequent precip synoptic time periods. */
         synoptic_time[i] = synoptic_time[i - 1] - seconds_per_precip_period;

      }

   } /* for (i) */

/* record how many precip synoptic times we stored. */
   def_ptr->num_precip_synoptic_times = i;

/**
now sort the synoptic time array so that the order represents
the oldest synoptic time period to the youngest synoptic time period.
**/
   qsort( synoptic_time, MAX_PRECIP_SYNOPTIC_TIMES,
      sizeof( synoptic_time[0] ),
      st_comp_hourly_times );

/* now store our computed times in our precip time buffer.*/
   st_copy_times( def_ptr->precip_synoptic_times,
      def_ptr->num_precip_synoptic_times,
      synoptic_time );

   return;
} /* st_get_precip_synoptic_times() */


/*****************************************************************************
 *
 *    st_comp_hourly_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function is utilized as the "comparison" function for the
 *       qsort() function. It is designed to organize the hourly time data
 *       such  that it is ordered from the most recent time back to the most
 *       historical of all hours contained in the hourly times buffer.
 *
 *    ARGUMENTS
 *
 *               cvp1 = A pointer to the first hourly time to be compared.
 *                      (const void * input)
 *
 *               cvp2 = A pointer to the second hourly time to be compared.
 *                      (const void * input)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       A value less than zero if the value pointed to by cvp2 is less
 *       than the value pointed to by cvp2, a value of 0 if the value
 *       pointed to by cvp1 equals the value pointed to by cvp2, and
 *       greather than one if the value pointed to by cvp1 is greater
 *       than the value pointed to by cvp2.
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static int st_comp_hourly_times( const void *cvp1, const void *cvp2 )
{
time_t *p1 = (time_t *) cvp1;
time_t *p2 = (time_t *) cvp2;
time_t diff = (*p1) - (*p2);
int rv = (int) diff;

   return rv;
} /* st_comp_hourly_times() */


/*****************************************************************************
 *
 *    st_allocate_definition_memory()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function dynamically allocates the memory needed by the
 *       essential function to buffer the SCS Obs definitions that have
 *       been produced.
 *
 *    ARGUMENTS
 *
 *            in_defs = Points to the SCS Obs definition structure containing
 *                    the data derived by the current invocation of the
 *                    essential function. Once this function allocates
 *                    additional memory, the data stored at this memory
 *                    location will be copied into the buffer of SCS
 *                    Obs definitions structures being maintained by
 *                    the essential function.
 *                      (SCS_Obs_definitions_tp input)
 *
 *        return_defs = A pointer to a pointer to SCS Obs definition
 *                    structures being maintained by the essential function
 *                    to buffer the definitions being derived. This function
 *                    will use realloc() to incrementally increase the
 *                    size of the buffer so that it can store the data
 *                    pointed to by the in_defs argument.
 *                      (SCS_Obs_definitions_tpp updated)
 *
 *    num_return_defs = The number of SCS Obs definition structures in
 *                    the buffer being maintained by the essential function.
 *                    The value at this memory location will be incrementally
 *                    updated to inform the caller of the number of SCS
 *                    Obs definition structures in the buffer being maintained
 *                    by the essential function.
 *                      (int * updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_allocate_definition_memory( SCS_Obs_definitions_tp in_defs,
   SCS_Obs_definitions_tpp return_defs,
   int *num_return_defs )
{
static char st_fn[] = "st_allocate_definition_memory()";
SCS_Obs_definitions_tp new_defs = *return_defs;
int num_new_defs = *num_return_defs;
int bytes;
char buf[BUFSIZ];

/* prepare to allocate the new SCS Obs definition buffer memory. */

/* we are adding a definition. */
   ++num_new_defs;

/* compute the buffer size. */
   bytes = num_new_defs * sizeof( new_defs[0] );

/* initialize an error message buffer. */
   sprintf( buf,
      "Error trying to allocate %d bytes of memory.\n"
      "In: %s\n"
      "Source File: %s\n",
      bytes, st_fn, __FILE__ );

/* attempt to allocate the memory. */
   new_defs = (SCS_Obs_definitions_tp) realloc( new_defs, bytes );

/* did the allocation request fail? */
   if ( new_defs == (SCS_Obs_definitions_tp) NULL )
   {

      st_error( TRUE, buf );

   }

/**
copy the input definition structure data, pointed to by in_defs,
into the newly allocated slot in the buffer of definition structures.
**/
   new_defs[num_new_defs - 1] = in_defs[0];

/**
update the caller's pointers to indicate the allocation and data storage
 performed by this function.
**/
   *return_defs = new_defs;

   *num_return_defs = num_new_defs;

   return;
} /* st_allocate_definition_memory() */


/*****************************************************************************
 *
 *    is_all_timezone()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function indicates to the caller if the input timezone
 *       (tz) is the "ALL" timezone that is afforded "special" processing
 *       by the essential function.
 *
 *    ARGUMENTS
 *
 *                 tz = The string that is evaluated to determine if
 *                    it indicates "all timezones" by containing the
 *                    value "all". The case of the alphabetic characters
 *                    is insignificant.
 *                      (char * input)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       A non-zero value if tz contains a value of "all", and zero otherwise.
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static int is_all_timezone( char *tz )
{

/**
return the result of the strcasecmp() function and the comparison of that
 return value with 0.
**/
   return (strcasecmp( tz, all_str ) == 0);
} /* is_all_timezone() */


/*****************************************************************************
 *
 *    is_the_local_hour()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function determines if the input time_t value (t) represents
 *       the local hour indicated by the_local_hour.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      many important configuration members for processing
 *                      the current timezone for the essential function.
 *                      This function is concerned with the members relative
 *                      to Daylight Savings Time (DST) and how many hours
 *                      west of UTC is the current timezone being processed.
 *                      (SCS_utility_tp input)
 *
 *     the_local_hour = The local hour that is the test value to see
 *                    if the argument value in (t) represents that hour
 *                    in local time.
 *                      (int input)
 *
 *                  t = The time_t epoch value (in UTC time) that is
 *                    evaluated to determine if it represents the value
 *                    of local_hour when the UTC time is converted to
 *                    local time with DST considerations aplied when
 *                    necessary.
 *                      (time_t input)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       A non-zero value if the the_local_hour is represented by the time_t
 *       (t) value with DST considerations applied when necessary, and
 *       zero if not.
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static int st_is_the_local_hour( SCS_utility_tp utsp,
   int the_local_hour, time_t t )
{
struct tm ts;
int hours_west = utsp->hours_west;
int found_the_local_hour;

/* do we need a DST adjustment? */
   if ( ( utsp->consider_dst ) && ( utsp->is_dst ) )
   {

      --hours_west;

   }

/* adjust to local time. */
   t -= (hours_west * SECONDS_PER_HOUR);

/* get our time structure. */
   ts = *gmtime( &t );

   utsp->the_local_hour = ts.tm_hour;

   found_the_local_hour = (utsp->the_local_hour == the_local_hour);

   return found_the_local_hour;
} /* st_is_the_local_hour() */


/*****************************************************************************
 *
 *    st_is_synoptic_period()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function evaluates the supplied time array, beginning at
 *       the array_index offset to determine if the value at the array_index
 *       offset is a synoptic time preceded by enough hours to include
 *       the previous synoptic time in the time array.
 *
 *    ARGUMENTS
 *
 *         time_array = An array of time_t time values that are evaluated
 *                    by this function.
 *                      If this function finds that the array_index points
 *                      to the start of an entire synoptic period, the
 *                      values in this array that represent that synoptic
 *                      period are negated to indicate that they have
 *                      been processed.
 *                      (time_t * updated)
 *
 *        array_index = The offset into the time_array where this function
 *                    should begin its evaluation.
 *                      (int input)
 *
 *         array_size = The number of elements in the time_array array.
 *                      (int input)
 *
 *      synoptic_time = Points to a time_t variable that will return
 *                    to the caller the value of the synoptic time found
 *                    at the start of the synoptic period. This value
 *                    is only valid if this function returns a TRUE value.
(time_t * output)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       TRUE if the array_index points to the start of an entire synoptic
 *       time period, and FALSE if not.
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static int st_is_synoptic_period( time_t *time_array, int array_index,
   int array_size, time_t *synoptic_time )
{
struct tm ts;
int i;
int is_synoptic_hour;
int remaining_values;
time_t t;

/* see if this time value has already ben processed by this function. */
   if ( time_array[array_index] < TIME0 ) return FALSE;

/**
now check to see if the time's hour is a "synoptic hour",
 * 0, 6, 12, 18.
**/
   ts = *gmtime( time_array + array_index );

   is_synoptic_hour = ((ts.tm_hour % 6) == 0);

   if ( ! is_synoptic_hour ) return FALSE;

/**
Now*
now see if we have an entire synoptic period of time remaining
in our array of times.
**/
   remaining_values = array_size - array_index;

/**
The next if statement utilizes "<=" because,
the "synoptic period" for this function,
must start with a synoptic time,
and have enough remaining values in the array
to include the previous synoptic time.
**/
   if ( remaining_values <=SYNOPTIC_TIME_PERIOD ) return FALSE;

/* save this synoptic time for the caller. */
   *synoptic_time = time_array[array_index];

/* now mark these time values for deletion. */
   for (i = 0; i < SYNOPTIC_TIME_PERIOD; i++)
   {

      t = time_array[array_index];

      t = -t;

      time_array[array_index] = t;

      ++array_index;

   } /* for (i) */

/* it is a synoptic time period! */
   return TRUE;
} /* st_is_synoptic_period() */


/*****************************************************************************
 *
 *    st_get_hours_west()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function scans the input timezone string to the essential
 *       function and converts the hours west string into an integer.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the input timezone value from the essential function.
 *                      This function will convert the hours west string
 *                      value into an integer and stores that integer
 *                      in the hours_west member of this structure.
 *                      (SCS_utility_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_get_hours_west( SCS_utility_tp utsp )
{
char buf[BUFSIZ];
int rv;

   rv = sscanf( utsp->timezone_str, "%[^0-9-]%d", buf, &utsp->hours_west );

   return;
} /* st_get_hours_west() */


/*****************************************************************************
 *
 *    st_zero_hourly_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function will set all of the members of the utsp SCS utility
 *       structure that are concerned with the hourly times computations
 *       to zero.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      members that are concerned with the hourly
 *                      times computations of the essential function.
 *                      This function sets the hourly time members to
 *                      zero.
 *                      (SCS_utility_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_zero_hourly_times( SCS_utility_tp utsp )
{
int i;
int ht_size = sizeof( utsp->hourly_times ) / sizeof( utsp->hourly_times[0] );

   for (i = 0; i < ht_size; i++)
   {

      utsp->hourly_times[i] = TIME0;

   } /* for (i) */

   utsp->num_hourly_times = 0;

   return;
} /* st_zero_hourly_times() */


/*****************************************************************************
 *
 *    st_eliminate_most_historical_time()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function will eliminate the most historical time value
 *       if, the historical time value is still in the time array, and
 *       if the most historical time value is the only value in the time
 *       array, or if the most historical time is not followed by the
 *       next hour in time.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the most historical time determined earlier in
 *                      the essential's logic, and the is_so_far member
 *                      that is concerned with the processing of max
 *                      temp so far and min temp so far.
 *                      (SCS_utility_tp input)
 *
 *         time_array = The array of time_t time values that are processed
 *                    by this function. If the designed criterion is
 *                    met, the most historical time value is "eliminated"
 *                    from this array.
 *                      (time_t * updated)
 *
 *         array_size = A pointer to an integer that indicates to this
 *                    function the number of elements in the time_array
 *                    array. If the most historical time is eliminated,
 *                    the value to which this pointer points is decremented.
 *                      (int * updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_eliminate_most_historical_time( SCS_utility_tp utsp,
   time_t *time_array, int *array_size )
{
int ind = *array_size - 1;
time_t older;
time_t younger;
time_t diff;
int i;
int found_most_historical_time = FALSE;

/* don't process an empty array. */
   if ( ind < 1 )
   {
      *array_size = 0;
      return;
   }

/* see if the most historical time is still in the array. */
   for (i = 0; i < *array_size; i++)
   {

      found_most_historical_time = (time_array[i] ==
         utsp->most_historical_time);

/* stop looking if we found it. */
      if ( found_most_historical_time ) break;

   } /* for (i) */

/* if the most historical time wasn't there, we can leave now. */
   if ( ! found_most_historical_time ) return;

/* set index to the most historical time. */
   ind = i;

/*
so we found the most historical time,
if it's the only value in the array,
eliminate it then leave.
**/
   if ( *array_size == 1 )
   {
      *array_size = 0;
      time_array[ind] = -time_array[ind];
      return;
   }

/*
now let's see how the most historical values in the time array
relate to each other in time.
**/
   older = time_array[ind];

   younger = time_array[ind - 1];

   diff = younger - older;

   if ( diff != SECONDS_PER_HOUR )
   {
      *array_size = ind;
      time_array[ind] = -time_array[ind];
   }

   return;
} /* st_eliminate_most_historical_time() */


/*****************************************************************************
 *
 *    st_get_temp_synoptic_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function processes the input hourly times for max or min
 *       temp, found in the mta argument, and "extracts" the synoptic
 *       times found within the mta array.
 *
 *    ARGUMENTS
 *
 *                mta = The array of hourly times derived for either
 *                    min temp or max temp relative to the input date/time
 *                    provided to the essential function. If synoptic
 *                    periods are found, they are indirectly "eliminated"
 *                    from the mta array.
 *                      (time_t * updated)
 *
 *            num_mta =
Specifies the number of elements in the mta array.
 *                      (int input)
 *
 *temp_synoptic_times = The array where this function will store the
 *                    found synoptic times.
 *                      (time_t * updated)
 *
 *num_temp_synoptic_times = A pointer to an integer that will inform
 *                    the caller of the number of synoptic times that
 *                    this function stored in the temp_synoptic_times
 *                    array.
 *                      (int * output)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_get_temp_synoptic_times( time_t *mta, int num_mta,
   time_t *temp_synoptic_times, int *num_temp_synoptic_times )
{
int i;
int j = 0;
time_t st; /* synoptic time (st) */
time_t synoptic_time[MAX_HOURLY_TIMES];

/* look through the temp hourly times for complete synoptic periods. */
   for (i = 0; i < num_mta; i++)
   {

/**
save the synoptic time if a temp hourly time
marks the end of a synoptic period.
**/
      if ( st_is_synoptic_period( mta, i, num_mta, &st ) )
      {
         synoptic_time[j] = st;
         ++j;
      }

   } /* for (i) */

/* record the number of temp synoptic time values stored. */
   *num_temp_synoptic_times = j;

/**
now sort the synoptic time array so that the order represents
the oldest synoptic time period to the youngest synoptic time period.
**/
   qsort( synoptic_time, *num_temp_synoptic_times,
      sizeof( synoptic_time[0] ),
      st_comp_hourly_times );

/* now store the out-bound temp synoptic time values. */
   st_copy_times( temp_synoptic_times,
      *num_temp_synoptic_times,
      synoptic_time );

   return;
} /* st_get_synoptic_times() */


/*****************************************************************************
 *
 *    st_get_temp_hourly_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function derives the hourly times for either max temp or
 *       min temp given the input date/time specification given to the
 *       essential function and the first local hour and second local
 *       hour values specified to this function.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      input_time, the time_t representation of the
 *                      input date/time supplied to the essential function.
 *                      This function also references the hourly time
 *                      members of this structure, plus other important
 *                      members.
 *                      (SCS_utility_tp updated)
 *
 *   first_local_hour = The first hour relative to the input time to
 *                    the essential function that this routine seeks
 *                    in order to define one end of the "hourly duration
 *                    window" for either min or max temp.
 *                      (int input)
 *
 *  second_local_hour = The second hour relative to the input time to
 *                    the essential function that this routine seeks
 *                    in order to define the other end of the "hourly
 *                    duration window" for either min or max temp.
 *                      (int input)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       The hourly times derived by this function will be stored in
 *       the hourly_times member array pointed to by the utsp structure
 *       pointer.
 *
 *****************************************************************************/

static void st_get_temp_hourly_times( SCS_utility_tp utsp,
   int first_local_hour, int second_local_hour )
{
int i = 0;
int found_first_local_hour = FALSE;
int found_second_local_hour = FALSE;

/* zero out our hourly times buffer and related variables. */
   st_zero_hourly_times( utsp );

/* start by looking at the input time. */
   utsp->t = utsp->input_time;

/* we need to find the first local time hour in GMT. */
   for ( ; ; )
   {

/* is this the first local time/ */
      found_first_local_hour = st_is_the_local_hour( utsp, first_local_hour,
         utsp->t );

/* stop searching when we find it. */
      if ( found_first_local_hour ) break;

/* move back an hour. */
      utsp->t -= SECONDS_PER_HOUR;

   } /* for (;;) */

/**
now we need to find the second local time hour in GMT.
and we will collect the hourly times along the way.
**/
   for ( ; ; )
   {

/* is this the second local time/ */
      found_second_local_hour = st_is_the_local_hour( utsp, second_local_hour,
         utsp->t );

/* collect the hour. */
      utsp->hourly_times[i] = utsp->t;

/* increment our index/counter. */
      ++i;

/* we're done if we found the second local time hour. */
      if ( found_second_local_hour ) break;

/* continue to move back an hour. */
      utsp->t -= SECONDS_PER_HOUR;

   } /* for (;;) */

/* record the number of times saved. */
   utsp->num_hourly_times = i;

/* record the most historical time value. */
   utsp->most_historical_time = utsp->t;

   return;
} /* st_get_temp_hourly_times() */


/*****************************************************************************
 *
 *    st_finalize_hourly_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function processes the derived hourly times and physically
 *       removes the "eliminated" values and stores the remaining valid
 *       hourly times.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the hourly times that will be processed and finalized
 *                      by this function.
 *                      (SCS_utility_tp input)
 *
 *          the_times = The array that will return the finalized hourly
 *                    times to the caller.
 *                      (time_t * output)
 *
 *          num_times = A pointer to an integer that will return to the
 *                    caller the number of finalized items stored in the
 *                    the_times array.
 *                      (int * output)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_finalize_hourly_times( SCS_utility_tp utsp,
   time_t *the_times, int *num_times )
{
int i;
time_t ht; /* hourly time. */
time_t hourly_times[MAX_HOURLY_TIMES];
int num_hourly_times = 0;

/* evaluate the hourly time values. */
   for (i = 0; i < utsp->num_hourly_times; i++)
   {

      ht = utsp->hourly_times[i];

     if ( ht > TIME0 )
      {
         hourly_times[num_hourly_times] = ht;
         ++num_hourly_times;
      }

   } /* for (i) */

/* eliminate the oldest time if the next hour isn't available. */
   st_eliminate_most_historical_time( utsp, hourly_times, &num_hourly_times );

/* now order the hourly time values. */
   qsort( hourly_times, num_hourly_times,
      sizeof( hourly_times[0] ),
      st_comp_hourly_times );

/**
now save the local hourly times in the buffers pointed to by the arguments.
**/
   *num_times = num_hourly_times;

   st_copy_times( the_times,
      *num_times,
      hourly_times );

/**
Now that the hourly times have been "finalized",
zero out our hourly times buffer and related variables,
so that no other logic will be executed upon the finalized hourly times.
**/
   st_zero_hourly_times( utsp );

   return;
} /* st_finalize_hourly_times() */


/*****************************************************************************
 *
 *    st_determine_dst()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function determines if Daylight Savings Time (DST) is currently
 *       being observed by the timezone that was provided to the essential
 *       function given the specified date/time value.
 *       The determination is made by putting the timezone into the program's
 *       environment and by using the input time (the time_t value) to
 *       the essential function in conjunction with the localtime() function
 *       to determine if DST is being observed by the timezone of concern.
 *       DST is of concern when processing min temp so far and max temp
 *       so far.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      members that provide important functional values
 *                      utilized in determining DST. hese members include
 *                      input_time, the time_t representation of the
 *                      input date/time provided to the essential function,
 *                      is_dst, a "logical" member to indicate to the
 *                      rest of the code that DST is or is not being
 *                      observed.
 *                      (SCS_utility_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_determine_dst( SCS_utility_tp utsp )
{
static char env_var[] = "TZ";
static char org_tz_value[BUFSIZ];
char *env_ptr;
int rv;

/* not DST yet. */
   utsp->is_dst = FALSE;

/* nothing to do if this is the "ALL" timezone. */
   if ( utsp->do_all_timezones ) return;

/* get the current tz value. */
   env_ptr = getenv( env_var );

/* save the original value. */
   sprintf( org_tz_value, "%s", (env_ptr != CHAR_NULL) ? env_ptr : "" );

/**
now set the TZ environment variable so the time functions will do the
 work of determining whether or not DST is being observed.
**/
   rv = setenv( env_var, utsp->timezone_str, TRUE );

/* now let the time functions help us. */
   utsp->ts = *localtime( &utsp->input_time );

/* evaluate the result structure member. */
   utsp->is_dst = (utsp->ts.tm_isdst > 0);

/**
before we go, return the original TZ value, if we had one,
to the environment.
**/
   if ( *org_tz_value != NULL_CHAR )
   {

      rv = setenv( env_var, org_tz_value, TRUE );

   }
   else
   {

      rv = unsetenv( env_var );

   }

   return;
} /* st_determine_dst() */


/*****************************************************************************
 *
 *    st_max_temp_logic()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function organizes all of the logic necessary to provide
 *       the SCS Obs definitions for max temp for the essential function.
 *       The definitions include max temp hourly times, max temp synoptic
 *       times, max temp so far hourly times, and max temp so far synoptic
 *       times.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the members that were initialized with the input
 *                      date/time and timezone values in the essential
 *                      function. The values contained by this structure
 *                      will be utilized to derive the max temp definition
 *                      values.
 *                      (SCS_utility_tp input)
 *
 *            def_ptr = A pointer to an SCS Obs definition structure
 *                    that will return to the caller the max temp definitions
 *                    derived by the execution of this function.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *         10/03/2006          Joseph Lang added logic to only do
 *                                         max temps when desired
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_max_temp_logic( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{

/* get the hourly  times for max-temp. */
   st_get_max_temp_hourly_times( utsp );

/**
utilize the max temp hourly times to get the synoptic times for max-temp.
**/
   st_get_max_temp_synoptic_times( utsp, def_ptr );

/**
now finalize the max temp hourly times.
That is, we'll get rid of the synoptic periods, where they exist.
**/
   st_finalize_max_temp_hourly_times( utsp, def_ptr );

/* are max temp so far definitions desired? */
   if ( ! utsp->only_max_min_temps )
   {

/* get the max temp so far hourly times. */
      st_get_max_temp_so_far_hourly_times( utsp );

/* from now on, the so far functions should consider  DST. */
      utsp->consider_dst = TRUE;

/* get the max temp so far synoptic times. */
      st_get_max_temp_so_far_synoptic_times( utsp, def_ptr );

/**
now finalize the max temp so far hourly times.
That is, we'll get rid of the max temp so far synoptic periods.
**/
      st_finalize_max_temp_so_far_hourly_times( utsp, def_ptr );

/* done with so far functions, so no longer consider DST. */
      utsp->consider_dst = FALSE;

/* no longer concerned with so far logic either. */
      utsp->is_so_far = FALSE;
   
   } /* if ( ! utsp->only_max_min_temps ) */

   return;
} /* st_max_temp_logic() */


/*****************************************************************************
 *
 *    st_min_temp_logic()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function organizes all of the logic necessary to provide
 *       the SCS Obs definitions for min temp for the essential function.
 *       The definitions include min temp hourly times, min temp synoptic
 *       times, min temp so far hourly times, and min temp so far synoptic
 *       times.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the members that were initialized with the input
 *                      date/time and timezone values in the essential
 *                      function. The values contained by this structure
 *                      will be utilized to derive the min temp definition
 *                      values.
 *                      (SCS_utility_tp input)
 *
 *            def_ptr = A pointer to an SCS Obs definition structure
 *                    that will return to the caller the min temp definitions
 *                    derived by the execution of this function.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *         10/03/2006          Joseph Lang added logic to only do
 *                                         min temps when desired
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_min_temp_logic( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{

/* get the hourly times for min-temp. */
      st_get_min_temp_hourly_times( utsp );

/* get the synoptic times for min-temp. */
      st_get_min_temp_synoptic_times( utsp, def_ptr );

/* now finalize the min temp hourly times. */
      st_finalize_min_temp_hourly_times( utsp, def_ptr );

/* are min temp so far definitions desired? */
   if ( ! utsp->only_max_min_temps )
   {

/* get the min temp so far hourly times. */
      st_get_min_temp_so_far_hourly_times( utsp );

/* from now on, the so far functions should consider  DST. */
      utsp->consider_dst = TRUE;

/* get the min temp so far synoptic times. */
      st_get_min_temp_so_far_synoptic_times( utsp, def_ptr );

/**
now finalize the min temp so far hourly times.
That is, we'll get rid of the min temp so far synoptic periods.
**/
      st_finalize_min_temp_so_far_hourly_times( utsp, def_ptr );

/* done with so far functions, so no longer consider DST. */
      utsp->consider_dst = FALSE;

/* no longer concerned with so far logic either. */
      utsp->is_so_far = FALSE;

   } /* if ( ! utsp->only_max_min_temps ) */

   return;
} /* st_min_temp_logic() */


/*****************************************************************************
 *
 *    st_precip_logic()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function organizes all of the logic necessary to provide
 *       the SCS Obs definitions for precip for the essential function.
 *       The only SCS Obs definitions for precip are the precip synoptic
 *       times.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the members that were initialized with the input
 *                      date/time and timezone values in the essential
 *                      function. The values contained by this structure
 *                      will be utilized to derive the precip definition
 *                      values to derive the precip synoptic times.
 *                      (SCS_utility_tp input)
 *
 *            def_ptr = A pointer to an SCS Obs definition structure
 *                    that will return to the caller the precip definitions
 *                      for the precip synoptic times.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *         10/03/2006          Joseph Lang added logic to only do
 *                                         max and min temps when desired
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_precip_logic( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{

/* is this information desired? */
   if ( ! utsp->only_max_min_temps )
   {

/* get the synoptic times for precip. */
      st_get_precip_synoptic_times( utsp, def_ptr );

   }

   return;
} /* st_precip_logic() */


/*****************************************************************************
 *
 *    st_get_max_temp_so_far_hourly_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function defines the hourly times window that specifies
 *       the period of time in which max temp so far hourly times should
 *       be considered. If the input date/time is found to be within
 *       this period of time, the max temp so far hourly times are derived
 *       by this function.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      many members that are directly or indirectly
 *                      utilized by this function. These important members
 *                      include the hourly times variables, DST variables,
 *                      and so far local hour that are utilized in
 *                      evaluating the max temp so far times period,
 *                      and for deriving the max temp so far hourly times
 *                      themselves.
 *                      (SCS_utility_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *         11/23/2009          Shucai Guan Adopts new Alaskan definitions for maximum
 *                                    and minimum temperatures for stations in Alaska.
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_get_max_temp_so_far_hourly_times( SCS_utility_tp utsp )
{
int in_window;
int local_hour_7am = 7;
int local_hour_7pm = 19;
int local_hour_noon = 12;

/* New Alaskan definitions for MaxT and MinT grids */
if (strcmp(utsp->timezone_str, "AKST9AKDT") == 0)
{
    local_hour_7pm = 20;
    local_hour_7am = 5;
}

/* make sure we are in the "so far" time range window for max temp. */
   in_window = st_is_in_so_far_window( utsp, local_hour_7am, local_hour_7pm,
      local_hour_noon );

   if ( in_window )
   {

      st_get_temp_hourly_times( utsp, utsp->so_far_local_hour,
         local_hour_7am );

   }

   return;
} /* st_get_max_temp_so_far_hourly_times() */


/*****************************************************************************
 *
 *    st_get_max_temp_so_far_synoptic_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function evaluates the utsp->is_so_far structure member
 *       to determine if the input time to the essential function fell
 *       within the max temp so far hourly times period. That determination
 *       was made earlier within the the st_get_max_so_far_hourly_times()
 *       function. If the input time was in the max temp so far hourly
 *       time period, this function then evaluates the hourly times and
 *       derives the max temp so far synoptic times.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      many members that are directly or indirectly
 *                      utilized by this function. These important members
 *                      include the hourly times variables,
 *                      and especially the is_so_far member.
 *                      (SCS_utility_tp updated)
 *
 *            def_ptr = A pointer to an SCS Obs definition structure
 *                      that will return to the caller the max temp so
 *                      far synoptic times if the input date/time to the
 *                      essential function falls within the time period
 *                      where max temp so far hourly times should be
 *                      considered.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_get_max_temp_so_far_synoptic_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{

/**
make sure that the input time was in the max temp so far time range
window as determined in the early part of:
st_get_max_temp_so_far_hourly_times()
**/
   if ( utsp->is_so_far )
   {

      st_get_temp_synoptic_times( utsp->hourly_times,
         utsp->num_hourly_times,
         def_ptr->max_temp_so_far_synoptic_times,
         &def_ptr->num_max_temp_so_far_synoptic_times );

   }

   return;
} /* st_get_max_temp_so_far_synoptic_times() */


/*****************************************************************************
 *
 *    st_finalize_max_temp_so_far_hourly_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function evaluates the utsp->is_so_far structure member
 *       to determine if max temp so far logic should be executed. And,
 *       if that structure member indicates TRUE, then this function
 *       invokes the st_finalize_temp_hourly_times() function * with
 *       the correct arguments for max temp so far hourly times to finalize
 *       the set of valid date/time values for max temp so far hourly
 *       times.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the necessary members for buffering the values
 *                      computed for hourly times.
 *                      This function evaluates the is_so_far member
 *                      to determine if "so far" logic should be executed.
 *                      (SCS_utility_tp input)
 *
 *            def_ptr = The address of an SCS_Obs_definitions structure
 *                      that will return to the caller the finalized
 *                      hourly date/time values for max temp so far hourly
 *                      times.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_finalize_max_temp_so_far_hourly_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{

/**
make sure that the input time was in the max temp so far time range
window as determined in the early part of:
st_get_max_temp_so_far_hourly_times()
**/
   if ( utsp->is_so_far )
   {

      st_finalize_hourly_times( utsp, def_ptr->max_temp_so_far_hourly_times,
         &def_ptr->num_max_temp_so_far_hourly_times );

   }

   return;
} /* st_finalize_max_temp_so_far_hourly_times()  */


/*****************************************************************************
 *
 *    st_is_in_so_far_window()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function determines if the input date/time provided to
 *       the essential function, when converted to local time falls within
 *       the time window specified by the first and second local hour
 *       time arguments.
 *       This function will return TRUE if the input time is >= the first
 *       local hour and < the second local hour.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      is_so_far which will indicate to the caller if
 *                      the input time was in the so far window, the_local_hour
 *                      will be derived and utilized in comparison with
 *                      the first_local_hour and second_local_hour arguments,
 *                      and the so_far_local_hour member will be utilized
 *                      in future "so far" processing.
 *                      (SCS_utility_tp updated)
 *
 *   first_local_hour = Specifies the initial hour in the "so far window".
 *                      0 = midnight, 11 = 11 am, 12 = noon, 21 = 9 pm,
 *                      and so on.
 *                      (int input)
 *
 *  second_local_hour = Specifies the last hour in the "so far window".
 *                      (int input)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       TRUE if the input time to the essential function, when converted
 *       to local time is within the "so far window", and FALSE if not.
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static int st_is_in_so_far_window( SCS_utility_tp utsp,
   int window_first_hour, int window_second_hour,
   int at_least_hour )
{
int saved_consider_dst = utsp->consider_dst;
int junk = 0;
int good;
int saved_local_hour;
int b1;
int b2;
int b3;

/* initialize. */
   utsp->is_so_far = FALSE;

/* get the local hour for the input time */
   st_is_the_local_hour( utsp, junk, utsp->input_time );

/* is the local hour in the so far major window? */
   good = st_in_major_so_far_window( utsp, window_first_hour,
      window_second_hour );

   if ( ! good ) return FALSE;

/* save the local hour value. */
   saved_local_hour = utsp->the_local_hour;

/**
Now get the local time with DST consideration activated.
The local time considering DST,
must be >= noon for Max Temp So Far,
and,
The local time considering DST,
must be >= midnight for Min Temp So Far.
**/
   utsp->consider_dst = TRUE;

/* derive the local hour. */
   st_is_the_local_hour( utsp, junk, utsp->input_time );

/**
how does the local hour (with DST consideration)
compare with the "at least" hour?
**/

/**
initialize some logical variables to simplify the expression's appearance.
**/
   b1 = (utsp->the_local_hour >= at_least_hour);

   b2 = (utsp->the_local_hour <= window_second_hour);

   b3 = b1 && b2;

   if ( ! b3 )
   {
      utsp->consider_dst = saved_consider_dst;
      return FALSE;
   }

/* the local hour is in the desired time range window! */
   utsp->is_so_far = TRUE;

   utsp->consider_dst = saved_consider_dst;

/* save the local hour for later use. */
   utsp->so_far_local_hour = saved_local_hour;

/* it's in the window, so let the caller know that fact. */
   return TRUE;
} /* st_is_in_so_far_window() */


/*****************************************************************************
 *
 *    st_get_min_temp_so_far_hourly_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function defines the hourly times window that specifies
 *       the period of time in which min temp so far hourly times should
 *       be considered. If the input date/time is found to be within
 *       this period of time, the min temp so far hourly times are derived
 *       by this function.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      many members that are directly or indirectly
 *                      utilized by this function. These important members
 *                      include the hourly times variables, DST variables,
 *                      and so far local hour that are utilized in
 *                      evaluating the min temp so far times period,
 *                      and for deriving the min temp so far hourly times
 *                      themselves.
 *                      (SCS_utility_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *         11/23/2009          Shucai Guan Adopts new Alaskan definitions for maximum
 *                                    and minimum temperatures for stations in Alaska.
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_get_min_temp_so_far_hourly_times( SCS_utility_tp utsp )
{
int in_window;
int local_hour_7pm = 19;
int local_hour_8am = 8;
int local_hour_midnight = 0;

/* New Alaskan definitions for MaxT and MinT grids */
if (strcmp(utsp->timezone_str, "AKST9AKDT") == 0)
{
    local_hour_8am = 11;
    local_hour_7pm = 17;
}

/* make sure we are in the "so far" time range window for min temp. */
   in_window = st_is_in_so_far_window( utsp, local_hour_7pm, local_hour_8am,
      local_hour_midnight);

   if ( in_window )
   {

      st_get_temp_hourly_times( utsp, utsp->so_far_local_hour,
         local_hour_7pm );

   }

   return;
} /* st_get_min_temp_so_far_hourly_times() */


/*****************************************************************************
 *
 *    st_get_min_temp_so_far_synoptic_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function evaluates the utsp->is_so_far structure member
 *       to determine if the input time to the essential function fell
 *       within the min temp so far hourly times period. That determination
 *       was made earlier within the the st_get_min_so_far_hourly_times()
 *       function. If the input time was in the min temp so far hourly
 *       time period, this function then evaluates the hourly times and
 *       derives the min temp so far synoptic times.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      many members that are directly or indirectly
 *                      utilized by this function. These important members
 *                      include the hourly times variables,
 *                      and especially the is_so_far member.
 *                      (SCS_utility_tp updated)
 *
 *            def_ptr = A pointer to an SCS Obs definition structure
 *                      that will return to the caller the min temp so
 *                      far synoptic times if the input date/time to the
 *                      essential function falls within the time period
 *                      where min temp so far hourly times should be
 *                      considered.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_get_min_temp_so_far_synoptic_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{

/**
make sure that the input time was in the min temp so far time range
window as determined in the early part of:
st_get_min_temp_so_far_hourly_times()
**/
   if ( utsp->is_so_far )
   {

      st_get_temp_synoptic_times( utsp->hourly_times, utsp->num_hourly_times,
         def_ptr->min_temp_so_far_synoptic_times,
         &def_ptr->num_min_temp_so_far_synoptic_times );

   }

   return;
} /* st_get_max_temp_so_far_synoptic_times()  */


/*****************************************************************************
 *
 *    st_finalize_min_temp_so_far_hourly_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function evaluates the utsp->is_so_far structure member
 *       to determine if min temp so far logic should be executed. And,
 *       if that structure member indicates TRUE, then this function
 *       invokes the st_finalize_temp_hourly_times() function * with
 *       the correct arguments for min temp so far hourly times to finalize
 *       the set of valid date/time values for min temp so far hourly
 *       times.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the necessary members for buffering the values
 *                      computed for hourly times.
 *                      This function evaluates the is_so_far member
 *                      to determine if "so far" logic should be executed.
 *                      (SCS_utility_tp input)
 *
 *            def_ptr = The address of an SCS_Obs_definitions structure
 *                      that will return to the caller the finalized
 *                      hourly date/time values for min temp so far hourly
 *                      times.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_finalize_min_temp_so_far_hourly_times( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{

/**
make sure that the input time was in the min temp so far time range
window as determined in the early part of:
st_get_min_temp_so_far_hourly_times()
**/
   if ( utsp->is_so_far )
   {

      st_finalize_hourly_times( utsp, def_ptr->min_temp_so_far_hourly_times,
         &def_ptr->num_min_temp_so_far_hourly_times );

   }

   return;
} /* st_finalize_min_temp_so_far_hourly_times()  */


/*****************************************************************************
 *
 *    st_copy_times()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function copies the time data in the buffer referenced
 *       by the argument source_times into the destination buffer referenced
 *       by the argument dest_times. The num_times argument indicates
 *       the number of time values to be copied.
 *
 *    ARGUMENTS
 *
 *         dest_times = The destination buffer where this function will
 *                    store the copied data.
 *                      (time_t * updated)
 *
 *          num_times = Specifies the number of time values to be copied
 *                    from the source times buffer into the destination
 *                    times buffer.
 *                      (int input)
 *
 *       source_times = Specifies the buffer from where the time values
 *                    will be copied.
 *                      (time_t * input)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_copy_times( time_t *dest_times, int num_times,
   time_t *source_times )
{
int i;

/* let the copying begin. */
   for (i = 0; i < num_times; i++)
   {

      dest_times[i] = source_times[i];

   } /* for (i) */

   return;
} /* st_copy_times() */


/*****************************************************************************
 *
 *    st_essential_initializations()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function encapsulates the required initializations performed
 *       by the essential function, SCS_Obs_definitions(). This function
 *       will make the review of the essential function easier by
 *       segregating the exclusive logic in compartmentalized functional
 *       units of code.
 *
 *    ARGUMENTS
 *
 *      ef_call_count = Specifies which invocation of the essential function
 *                    is being processed. This control is necessary because
 *                    some variables are initialized on every invocation
 *                    of the essential function, while other variables
 *                    are initialized only on the first invocation relative
 *                    to recursion.
 *                      (int input)
 *
 *       datetime_str = Specifies the input date/time string that was
 *                    input to the essential function. This value will
 *                    be stored in the utility structure pointed to by
 *                    utsp.
 *                      (char * input)
 *
 *       timezone_str = Specifies the input timezone string that was
 *                    input to the essential function. This value will
 *                    be stored in the utility structure pointed to by
 *                    utsp.
 *                      (char * input)
 *
 * only_max_min_temps = This argument indicates that the essential function
 *                    will only be producing max temp and min temp observation
 *                    definition values. It will be either TRUE or FALSE.
 *                      This value will be stored in the utility structure
 *                      pointed to by utsp.
 *                      (int input)
 *
 *           num_defs = This argument is a pointer to an integer that
 *                    the essential function utilizes to record how many
 *                    definition structures have been allocated.
 *                      the integer at this memory location
 *                    is only initialized to zero on the first invocation
 *                    of the essential function relative to recursion.
 *                      (int * updated)
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      a date/time member and a timezone member that
 *                      are specifically referenced by this function.
 *                      This structure
 *                      is filled with zeroes by the memset() function,
 *                      and then the date/time and timezone members are
 *                      initialized with the inputs to this function.
 *                      (SCS_utility_tp updated)
 *
 *              a_def = A pointer to a SCS_Obs_definitions_t structure
 *                    that the essential function utilizes to store definition
 *                    values for one date/time/timezone.
 *                      This function will fill the memory pointed to
 *                      by this pointer with zeroes by calling the memset()
 *                      function.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *        return_defs = A pointer to a pointer to an SCS Obs definitions
 *                    structure. This pointer's referenced data is set
 *                    to NULL on the initial invocation of the essential
 *                    function relative to recursion.
 *                      (SCS_Obs_definitions_tpp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/11/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_essential_initializations( int ef_call_count,
   char *datetime_str, char *timezone_str,
   int only_max_min_temps,
   int *num_defs, SCS_utility_tp utsp, SCS_Obs_definitions_tp a_def,
   SCS_Obs_definitions_tpp return_defs )
{
int do_all_timezones = is_all_timezone( timezone_str );

/**
only do these initializations on the primary invocation
of the essential function relative to recursion.
**/
   if ( ef_call_count == 1 )
   {
      *num_defs = 0;
      *return_defs = (SCS_Obs_definitions_tp) NULL;
   }

/* do these initializations regardless of recursion. */
   errno = 0;

   memset( a_def, 0, sizeof( *a_def ) );

   memset( utsp, 0, sizeof( *utsp ) );

   SAFE_COPY( utsp->date_time_str, datetime_str )

   SAFE_COPY( utsp->timezone_str, timezone_str )

   utsp->do_all_timezones = do_all_timezones;

   utsp->only_max_min_temps = only_max_min_temps;

   return;
} /* st_essential_initializations() */


/*****************************************************************************
 *
 *    st_save_definition_datetime_and_timezone()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function encapsulates the logic that saves the validated
 *       date/time and timezone that were supplied to the essential
 *       function.
 *       The data is transfered from our utility structure (utsp) into
 *       the definition structure (def_ptr).
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the date/time and timezone values that were validated;
 *                      their values will be transfered into the definitions
 *                      structure pointed to by def_ptr.
 *                      (SCS_utility_tp input)
 *
 *            def_ptr = A pointer to an SCS Obs definition structure
 *                      that will return to the caller the date/time
 *                      and the timezone values that were transfered
 *                      from the utility data structure.
 *                      (SCS_Obs_definitions_tp updated)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/14/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static void st_save_definition_datetime_and_timezone( SCS_utility_tp utsp,
   SCS_Obs_definitions_tp def_ptr )
{

/* save the validated date/time value in our definitions structure. */
   SAFE_COPY( def_ptr->datetime, utsp->date_time_str );

/* save the validated timezone value in our definitions structure. */
   SAFE_COPY( def_ptr->timezone, utsp->timezone_str )

   return;
} /* st_save_definition_datetime_and_timezone() */


/*****************************************************************************
 *
 *    st_in_major_so_far_window()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function determines if the local hour,
 *       specified in utsp->the_local_hour,
 *       is within the "window of time" bracketed by the values specified
 *       in the arguments window_first_hour and window_second_hour. For
 *       a true return value, the local hour is >= the window's first
 *       hour and < the window's second hour.
 *       This function is necessary because the time window may cross
 *       over the midnight boundary, and most, if not all, of the values
 *       within the window must be evaluated before making
 *       a determination.
 *
 *    ARGUMENTS
 *
 *               utsp = A pointer to an SCS utility structure that contains
 *                      the local hour that is evaluated by this function.
 *                      utsp->the_locall_hour was previously initialized
 *                      by a call to the st_is_the_local_hour() function.
 *                      (SCS_utility_tp input)
 *
 *  window_first_hour = Specifies the first hour of the time window to
 *                    be evaluated by this function. For example, for
 *                    max temp so far, the window's first hour is 7 am.
 *                      (int input)
 *
 * window_second_hour = Specifies the second hour of the time window
 *                    to be evaluated by this function. For example,
 *                    for min temp so far, the window's second hour is
 *                    8 am.
 *                      (int input)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       Returns TRUE if the local hour is whithin the specified time
 *       window, and FALSE if not.
 *
 *    HISTORY
 *         09/20/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       None
 *
 *****************************************************************************/

static int st_in_major_so_far_window( SCS_utility_tp utsp,
   int window_first_hour,
   int window_second_hour )
{
int hour_was_found_in_window = FALSE;
int the_hour = window_first_hour;

/* look through the window for the current local hour. */
   for ( ; ; )
   {

/* watch out for crossing over midnight into the next day. */
      the_hour %= HOURS_PER_DAY;

/* set our logical variable. */
      hour_was_found_in_window = (the_hour == utsp->the_local_hour);

/* are we done looking? */
      if ( hour_was_found_in_window ) break;

/* increment the hour. */
      ++the_hour;

/* have we broken the window? */
      if ( the_hour == window_second_hour ) break;

   } /* for (;;) */

/* return the result to the caller. */
   return hour_was_found_in_window;
} /* st_in_major_so_far_window() */


/*****************************************************************************
 *
 *    st_make_ncep_tz_adjustment()
 *
 *    Joseph Lang/MDL             Linux
 *    joseph.lang@noaa.gov
 *
 *    PURPOSE
 *
 *       This function handles the setting and resetting of the TZ environment
 *       variable relative to which invocation of the essential function
 *       is being processed. This function makes sure that the TZ environment
 *       variable is set to "GMT" during the execution time of the essential
 *       function.
 *
 *    ARGUMENTS
 *
 *      ef_call_count = Specifies which invocation of the essential function
 *                    is being processed. This control is necessary because
 *                      this function should only be executed before
 *                      and after all of the essential processing.
 *                      The "call count" is relative to which invocation
 *                      of the essential function is being processed
 *                      considering recursion.
 *                      (int input)
 *
 *      set_tz_to_gmt = This argument indicates to this function whether
 *                    or not the TZ variable in the environment should
 *                    be set to the value of "GMT" so that all time values
 *                    will be considered relative to UTC. If this argument
 *                    is "true", the TZ environment variable is set to
 *                    "GMT", and if it is set to "false", the original
 *                    value for the TZ variable is restored.
 *                      (int input)
 *
 *    FILES
 *       None
 *
 *    RETURNS
 *       None
 *
 *    HISTORY
 *         09/28/2006          Joseph Lang initial development complete
 *
 *    NOTES
 *       This function must be invoked at the start of the first execution of
 *       the essential function to obtain the original TZ value and
 *       to set the TZ value to "GMT", and
 *       it must be invoked again before leaving the last invocation
 *       of the essential function so that the original TZ value can
 *       be restored.
 *
 *****************************************************************************/

static void st_make_ncep_tz_adjustment( int ef_call_count,
   int set_tz_to_gmt )
{
static const char tz_env_variable[] = "TZ";
static const char tz_gmt[] = "GMT";
static char original_tz_value[64];
char *env_ptr;

/**
only execute this function before and after
all essential function processing.
**/
   if ( ef_call_count != 0 ) return;

/* should we set TZ to GMT? */
   if ( set_tz_to_gmt )
   {

      env_ptr = getenv( tz_env_variable );

      if ( env_ptr != CHAR_NULL ) SAFE_COPY( original_tz_value, env_ptr )

      setenv( tz_env_variable, tz_gmt, TRUE );

   }
   else /* restore TZ's original value if there was one. */
   {

/* if there was a TZ value, restore it here. */
      if ( *original_tz_value )
      {

         setenv( tz_env_variable, original_tz_value, TRUE );

      }
      else /* remove the TZ variable from the environment. */
      {

         unsetenv( tz_env_variable );

      }

   }

   return;
} /* st_make_ncep_tz_adjustment() */

