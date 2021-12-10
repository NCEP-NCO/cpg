/*
**  Headers imported from GEMPAK.
*/
#include "geminc.h"
#include "gemprm.h"

/*
**  Headers imported from the MDL SCS_Obs_definitions library.
*/
#include "SCS_Obs_definitions.h"

/*
**  Headers imported from the XML2 library.
*/
#include <libxml/parser.h>
#include <libxml/tree.h>

/*
**  Local macros.
*/
#define XML_CHAR	( xmlChar * )
				/* xmlChar is a typedef from the XML2 library */
#define NUMELEM(array)	( sizeof(array) / sizeof(array[0]))
				/* returns the number of elements in an array */
#ifdef OBX
#define MXRPTS		( 100 )
				/* maximum number of METAR reports to store per station */
#else
#define MXRPTS		( 100 )
				/* use larger maximum for TeX, since many TeX stations
				   report multiple times per hour */
#endif

#define MXTIMES		( 12 )
				/* maximum number of times of each type to store per timezone */
#define MXTZ		( 10 )
				/* maximum number of timezones to process */
#define MXLK		( MXTZ * 5 )
				/* maximum number of layout-key strings */
#define MXLLK		( 15 )
				/* maximum length of a layout-key string */
#define MXLVT		( 20 )
				/* maximum length of a valid-time string */
#define BMISS		( 10E10 )
				/* BUFR "missing" value */
#define SYNOINTV        ( 6 )
                              /* hours between synoptic update */
#define K2F(x)		( ( ( x - TMCK ) * 1.8 ) + 32. )
				/* converts input value from Kelvin to Fahrenheit */
#define F2K(x)		( ( ( x - 32. ) / 1.8 ) + TMCK )
				/* converts input value from Fahrenheit to Kelvin */
#define MXVTMPF		( 130 )
				/* maximum valid temperature (in Fahrenheit) */
#define MIVTMPF		( -50 )
				/* minimum valid temperature (in Fahrenheit) */

#ifdef OBX
    #define MM2IN(x)	( x / 25.4 )
				/* converts input value from millimeters to inches */
#else
    #define MXTXSTN	( 5 )
				/* maximum number of stations that can simultaneously be reported
				   as having the CONUS maximum or minimum temperature within TeX */
#endif

#define SUCCESS		( 0 )
				/* function return code indicating success */
#define FAILURE		( -1 )
				/* function return code indicating failure */
/*
**  Function-like macro to verify return codes from certain GEMPAK subroutines and
**  then respond accordingly.
*/
#define CHK_RTN( iret, subp, rtnval ) \
    if ( iret != 0 ) { \
	printf( "ERROR - return code of %ld from GEMPAK subroutine " #subp , ( long ) iret ); \
	return rtnval; \
    }
/*
**  Function-like macro to dynamically allocate memory.
*/
#define ALLOC( flag, p, type, np, str ) \
    if ( flag == 0 ) G_MALLOC( p, type, np, str ) \
    else G_CALLOC( p, type, np, str ) \
    if ( p == NULL ) return FAILURE;

/*
**  In order to ensure that the C <-> FORTRAN interface works properly (and
**  portably!), the default size of an "INTEGER" declared in FORTRAN must be
**  identical to that of an "int" declared in C.  If this is not the case (e.g.
**  some FORTRAN compilers, most notably AIX via the -qintsize option, allow the
**  sizes of INTEGERs to be definitively prescribed outside of the source code
**  itself!), then the following conditional directive (or a variant of it) can
**  be used to ensure that the size of an "int" in C remains identical to that
**  of an "INTEGER" in FORTRAN.
*/
#ifdef F77_INTSIZE_8
    typedef long f77int;
#else
    typedef int f77int;
#endif

/*
**  In order to ensure that the C <-> FORTRAN interface works properly (and
**  portably!), the following typedef must be a floating type of length 8 bytes,
**  because it will be equivalenced to a FORTRAN REAL*8 value.
*/
typedef double f77r8;

/*
** Define a structure to hold date (year, month, day) and hour information.
*/
typedef struct {
    unsigned short yr;
    unsigned short mo;
    unsigned short dy;
    unsigned short hr;
} dhinfo;

/*
** Define a structure to hold the time-layout information.
*/
typedef struct {
    char layout_key[MXLLK];
    char svtime[MXLVT];
    char evtime[MXLVT];
} tlinfo;

/*
** Define a structure to hold the METAR station information.
*/
typedef struct {

    char stid[5];		  /* METAR station ID */
    char stnnam[34];		  /* name of station */
    char state[3];		  /* state in which station is located */
    char tz[11];		  /* timezone in which station is located */

    unsigned short nrpts;	  /* number of reports stored */
    dhinfo rpdh[MXRPTS];	  /* report date and hour */
    unsigned short rpmi[MXRPTS];  /* report minute */
    dhinfo hydh[MXRPTS];	  /* date and hour for which this is the hourly report */
    f77r8 tmdb[MXRPTS];		  /* hourly temperature */
#ifdef OBX
    f77r8 tp06[MXRPTS];		  /* 6-hr total precipitation */
#endif
    f77r8 mxtm06[MXRPTS];	  /* 6-hr maximum temperature */
    f77r8 mitm06[MXRPTS];	  /* 6-hr minimum temperature */

} stinfo;

/*
** Define a structure to hold the timezone information.
*/
typedef struct {

    char tz[11];

    unsigned short nmxth;		/* number of maximum temperature hourly times */
    dhinfo	   mxth[MXTIMES];	/* maximum temperature hourly times */
    unsigned short nmxts;		/* number of maximum temperature synoptic times */
    dhinfo	   mxts[MXTIMES];	/* maximum temperature synoptic times */
    char           mxt_lk[MXLLK];	/* maximum temperature layout key */
 
    unsigned short nmxth_sf;		/* number of maximum temperature so far hourly times */
    dhinfo	   mxth_sf[MXTIMES];	/* maximum temperature so far hourly times */
    unsigned short nmxts_sf;		/* number of maximum temperature so far synoptic times */
    dhinfo	   mxts_sf[MXTIMES];	/* maximum temperature so far synoptic times */
    char           mxt_sf_lk[MXLLK];	/* maximum temperature so far layout key */

    unsigned short nmith;		/* number of minimum temperature hourly times */
    dhinfo	   mith[MXTIMES];	/* minimum temperature hourly times */
    unsigned short nmits;		/* number of minimum temperature synoptic times */
    dhinfo	   mits[MXTIMES];	/* minimum temperature synoptic times */
    char           mit_lk[MXLLK];	/* minimum temperature layout key */

    unsigned short nmith_sf;		/* number of minimum temperature so far hourly times */
    dhinfo	   mith_sf[MXTIMES];	/* minimum temperature so far hourly times */
    unsigned short nmits_sf;		/* number of minimum temperature so far synoptic times */
    dhinfo	   mits_sf[MXTIMES];	/* minimum temperature so far synoptic times */
    char           mit_sf_lk[MXLLK];	/* minimum temperature so far layout key */

    unsigned short npcps;		/* number of precipitation synoptic times */
    dhinfo	   pcps[MXTIMES];	/* precipitation synoptic times */
    char           pcp_lk[MXLLK];	/* precipitation layout key */

} tzinfo;

/*
**  On certain operating systems, the FORTRAN compiler appends an underscore
**  to subprogram names in its object namespace.  Therefore, on such systems,
**  a matching underscore must be appended to any C language references to the
**  same subprogram names so that the linker can correctly resolve such
**  references across the C <-> FORTRAN interface at link time.
*/
#ifdef UNDERSCORE
   #define copenbf copenbf_
   #define ireadns ireadns_
   #define ufbint ufbint_
   #define closbf closbf_
   #define strnum strnum_
   #define ibfms ibfms_
#endif

/*
** Declare prototypes for ANSI C compatibility.
*/
int cmpstra( const char *, const char * );
int cshrly( stinfo *, unsigned short );
char *cstli( tlinfo *, unsigned short *, dhinfo *, unsigned short, dhinfo *, unsigned short );
int fdate( dhinfo, stinfo, unsigned short * );
void copenbf( char *, f77int *, f77int );
f77int ireadns( f77int *, char *, f77int *, f77int );
void ufbint( f77int *, f77r8 *, f77int *, f77int *, f77int *, char *, unsigned long );
void closbf( f77int * );
void strnum( char *, f77int *, f77int );
f77int ibfms( f77r8 * );
void cnv2vtm(f77int *idt, char * vtm);
int  cnv2int( f77int *);

/*
** Declare global variable to avoid warning
*/
char worklk[MXLLK];

void cnvtimes(tzinfo pt, unsigned short *nmith, int num_max_temp_hourly_times, time_t * max_temp_hourly_times, dhinfo *mith) ;
void csynohr(tzinfo pt, unsigned short *nmith,  unsigned short *nmits, unsigned short *nts,  dhinfo *mith, dhinfo *mits);
void prtimes(unsigned short nmith, dhinfo *mith);
