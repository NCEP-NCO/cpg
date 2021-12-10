/************************************************************************
 * CPG									*
 *									*
 * This program generates the <data> content for either the ObX product	*
 * or the TeX product, depending on how it was originally compiled.	*
 * When it runs, it returns a value of 0 if successful and -1 otherwise.*
 * All diagnostic messages are written to STDOUT.			*
 *									*
 *	Command line:							*
 *	    (ObX|TeX) rundt bfrfil stnfil nstn xmlfil			*
 *									*
 *	    rundt	= run date-time (YYYYMMDDHH)			*
 *	    bfrfil	= filename containing METAR BUFR data		*
 *	    stnfil	= filename containing station information	*
 *	    nstn	= number of stations in stnfil			*
 *	    xmlfil	= filename to contain output <data> content 	*
 **									*
 * Log:									*
 * J. Ator/NCEP         11/06						*
 * J. Ator/NCEP         05/07	Allow for xsi:nil values		*
 * S. Guan/NCEP    	04/08   Allow to look for data of hourly        *
 *                      date-time array if data of given synoptic       *
 *			date-time are missing when computing Max/Min    *
 *			temperatures.                                   *
 * S. Guan/NCEP         07/08   Implement QC for Min temperature,       *
 *                      proposed by Scott.                              *
 * S. Guan/NCEP         12/08   Fix a bug in calculating maximum        *
 *                      tempurature at 00Z                              *
 * S. Guan/NCEP         06/11   If a station reports multiple times     * 
 *                      per hour, only store the data once              *
 * S. Guan/NCEP         10/11 QC for 6-hour maximum/minimum             * 
 *                      temperatures                                    *
 * S. Guan/NCEP         01/13 Ported to WCOSS                           *
 ***********************************************************************/

#include "cpg.h"
#define T25        25 
                   /* For QC */ 

int main( int argc, char *argv[ ] ) {

	/*  Declare variables for use with the XML2 library functions. */
	xmlDocPtr doc = NULL;
	xmlNodePtr node1 = NULL, node2 = NULL, node3 = NULL, node4 = NULL;

	/*  Declare variables for use with the MDL SCS_Obs_definitions function. */
	SCS_Obs_definitions_tp scsodtp = NULL;
	int nsdt;
       
        int nldbhst, nldbhst_total;


	/*  Function-like macro to write location information to the output XML file. */
	#define WRITE_XMLLOC( location_key, city, state, summarization ) \
	    node2 = xmlNewChild( node1, NULL, XML_CHAR "location", NULL ); \
	    xmlNewChild( node2, NULL, XML_CHAR "location-key", XML_CHAR location_key ); \
	    node3 = xmlNewChild( node2, NULL, XML_CHAR "city", XML_CHAR city ); \
	    xmlNewProp( node3, XML_CHAR "state", XML_CHAR state ); \
	    xmlNewProp( node3, XML_CHAR "summarization", XML_CHAR summarization );

	/*  Function-like macro to write time-layout information to the output XML file. */
	#define WRITE_XMLTL( ) \
	    for ( i = 0; i < ntl; i++ ) { \
		node2 = xmlNewChild( node1, NULL, XML_CHAR "time-layout", NULL ); \
		xmlNewProp( node2, XML_CHAR "time-coordinate", XML_CHAR "UTC" ); \
		xmlNewChild( node2, NULL, XML_CHAR "layout-key", XML_CHAR pl[i].layout_key ); \
		xmlNewChild( node2, NULL, XML_CHAR "start-valid-time", XML_CHAR pl[i].svtime ); \
		xmlNewChild( node2, NULL, XML_CHAR "end-valid-time", XML_CHAR pl[i].evtime ); \
	    }

	/*  Function-like macro to start a parameters entry within the output XML file. */
	#define WRITE_XMLSPE( location_key ) \
	    node2 = xmlNewChild( node1, NULL, XML_CHAR "parameters", NULL ); \
	    xmlNewProp( node2, XML_CHAR "applicable-location", XML_CHAR location_key );

	/*  Function-like macro to write temperature data to the output XML file. */
	#define WRITE_XMLTEMP( type, layout_key, name, value ) \
	    node3 = xmlNewChild( node2, NULL, XML_CHAR "temperature", NULL ); \
	    xmlNewProp( node3, XML_CHAR "type", XML_CHAR type ); \
	    xmlNewProp( node3, XML_CHAR "units", XML_CHAR "Fahrenheit" ); \
	    if ( ( value < mxvtmpk ) && ( value > mivtmpk ) ) { \
		xmlNewProp( node3, XML_CHAR "time-layout", XML_CHAR layout_key ); \
		xmlNewChild( node3, NULL, XML_CHAR "name", XML_CHAR name ); \
		sprintf( str, "%d", G_NINT( K2F( value ) ) ); \
		xmlNewChild( node3, NULL, XML_CHAR "value", XML_CHAR str ); \
	    } \
	    else { \
		xmlNewChild( node3, NULL, XML_CHAR "name", XML_CHAR name ); \
		node4 = xmlNewChild( node3, NULL, XML_CHAR "value", NULL ); \
		xmlNewProp( node4, XML_CHAR "xsi:nil", XML_CHAR "true" ); \
	    }

	FILE *fst;

	char subset[9], rpid[5], usg, *pbs, *pc;
	char str[81], cblk[2] = " ", str1[81];
        char cyc[3];
        char c00[3] = "00";
        char c12[3] = "12";

	char mxtstr[39] = ".DTHMXTM<7 .DTHMXTM MXTM .DTHMITM MITM";

	char mnem[][9] = {
	    "THRPT",
	    "RPID",
	    "YEAR",
	    "MNTH",
	    "DAYS",
	    "HOUR",
	    "MINU",
	    "TMDB",
#ifdef OBX
	    "TP06",
#endif
	    "{MTTPSQ}"
	};

	f77r8 arr[NUMELEM(mnem)];
	f77r8 r8val;
	f77r8 r8vak = 9999999.;
	f77r8 r8vam = 9999999.;
        
	f77r8 mxvtmpk = F2K( MXVTMPF ), mivtmpk = F2K( MIVTMPF );
#ifdef TEX
	f77r8 mxt_conus, mit_conus;
#endif

        f77r8  temp6max, temp6min;
        f77int idxmax, indxmin; 
	f77int lunit = 12;
	f77int idate, ict, iret, nstns;
	f77int i1 = NUMELEM(mnem), i2 = 1;

	unsigned short i, j, k, l;
	unsigned short idx, ntl = 0;

#ifdef TEX
	unsigned short mxt_stnidx[MXTXSTN], mit_stnidx[MXTXSTN];
	unsigned short mxt_tzidx[MXTXSTN], mit_tzidx[MXTXSTN];
	unsigned short nmxt_conus, nmit_conus;
#else
	unsigned short ngpcp;
#endif

	unsigned int szrp = sizeof( rpid );

	/*  Declare a union to use in converting RPID from f77r8 to char. */
	union {
	    f77r8 frp;
	    char crp[8];
	} cnvrp;

        time_t secs;
	struct tm *ptm, ptms;

	tzinfo *pt, *ptsyh;

	tlinfo *pl;

	stinfo *ps;

/*---------------------------------------------------------------------*/
        ptm = &ptms;
	/*
	**  Check for correct number of arguments.
	*/
	if ( argc != 6 ) {
	    printf( "Usage:  %s rundt bfrfil stnfil nstn xmlfil\n", argv[0] );
	    return FAILURE;
	}
        strcpy (cyc,&argv[1][8]);
	strnum( argv[4], &nstns, strlen( argv[4] ) );
	#ifdef VERBOSE
	/*
	**  Print out the input arguments.
	*/
	for ( i = 1; i < argc; i++ ) {
	    printf( "input argument #%u: %s\n", i, argv[i] );
	}
	#endif

	/*
	**  Check for correct length of f77r8.
	*/
	if ( sizeof( f77r8 ) != 8 ) {
	    printf( "FAILURE - the typedef for f77r8 must be a floating type of length 8 bytes, "
		    "but it only has %lu bytes.\n", ( unsigned long ) sizeof( f77r8 ) );
	    return FAILURE;
	}

	/*
	**  Allocate space to store the station information.
	*/
	ALLOC( 1, ps, stinfo, nstns, "station information" ) 
	ALLOC( 0, pc, char, ( nstns * szrp ), "station IDs" ) 

	/*
	**  Open and read the station table.
	*/
	if ( ( fst = fopen( argv[3], "r" ) ) == NULL ) {
	    printf( "FAILURE - couldn't open station table %s.\n", argv[3] );
	    return FAILURE;
	}
	i = 0;
	while ( ( i < nstns ) && ( fgets( str, 80, fst ) != NULL ) ) {
	    if ( str[0] != '#' ) {
	        sscanf( str, "%4c%*4c%33c%*c%2c%*4c%10c%*4c%c",
			ps[i].stid, ps[i].stnnam, ps[i].state, ps[i].tz, &usg );
		/*
		**  Check the usage key for this station.  
		*/
#ifdef OBX
		#define USAGE_KEY ('1')
#else
		#define USAGE_KEY ('2')
#endif
		if ( ( usg == USAGE_KEY ) || ( usg == '3' ) ) {
		    strcpy( &pc[i*szrp], ps[i].stid );
		    ps[i].nrpts = 0;
		    /*
		    **  Remove trailing blanks from the station name.
		    */
		    cst_lstr( ps[i].stnnam, &ict, &iret );
		    ps[i].stnnam[ict] = '\0';
		    /*
		    **  Remove trailing blanks from the timezone.
		    */
		    cst_lstr( ps[i].tz, &ict, &iret );
		    ps[i].tz[ict] = '\0';
		    /*
		    **  Increment the counter.
		    */
		    i++;
		}
	    }
	}
	if ( i != nstns ) {
	    printf( "FAILURE - error reading station table %s.\n", argv[3] );
	    return FAILURE;
	}
	fclose( fst );

	/*
	**  Initialize the output XML file.
	*/
	doc = xmlNewDoc( XML_CHAR "1.0" );
	node1 = xmlNewNode( NULL, XML_CHAR "data" );
	xmlDocSetRootElement( doc, node1 );

#ifdef OBX
	/*
	**  Write the location information to the output XML file.
	*/
	for ( i = 0; i < nstns; i++ ) {
	    WRITE_XMLLOC( ps[i].stid, ps[i].stnnam, ps[i].state, "none" )
	}
	/*
	**  Allocate space to store the time-layout information.
	*/
	ALLOC( 0, pl, tlinfo, MXLK, "time-layout information" ) 
#endif

	/*
	**  Allocate space to store the timezone information.
	*/
	ALLOC( 0, pt, tzinfo, MXTZ, "timezone information" ) 

        /*
        **  Allocate space to store the timezone information.
            Actually only need to store SYNOINTV (6) hourly  
  	    date-time array for each given synoptic date-time.
        */
        ALLOC( 0, ptsyh, tzinfo, MXTZ, "timezone information" )

	/*
	**  Get the applicable information for each timezone.
	*/
	scsodtp = SCS_Obs_definitions( argv[1], "ALL", G_FALSE, &nsdt );
	for ( i = 0; i < nsdt; i++ ) {
	    strcpy( pt[i].tz, scsodtp[i].timezone ); 
            strcpy( ptsyh[i].tz, scsodtp[i].timezone );

	    /*
	    **  Convert each set of date-times from time_t format to dhinfo format
	    **  and store the results into the timezone structure.
	    */

            cnvtimes(pt[i], &pt[i].nmxth, scsodtp[i].num_max_temp_hourly_times, scsodtp[i].max_temp_hourly_times, pt[i].mxth);
            cnvtimes(pt[i], &pt[i].nmxts, scsodtp[i].num_max_temp_synoptic_times, scsodtp[i].max_temp_synoptic_times, pt[i].mxts);
            cnvtimes(pt[i], &pt[i].nmxth_sf, scsodtp[i].num_max_temp_so_far_hourly_times, scsodtp[i].max_temp_so_far_hourly_times, pt[i].mxth_sf);
            cnvtimes(pt[i], &pt[i].nmxts_sf, scsodtp[i].num_max_temp_so_far_synoptic_times, scsodtp[i].max_temp_so_far_synoptic_times, pt[i].mxts_sf);

            cnvtimes(pt[i], &pt[i].nmith, scsodtp[i].num_min_temp_hourly_times, scsodtp[i].min_temp_hourly_times, pt[i].mith);
            cnvtimes(pt[i], &pt[i].nmits, scsodtp[i].num_min_temp_synoptic_times, scsodtp[i].min_temp_synoptic_times, pt[i].mits);
            cnvtimes(pt[i], &pt[i].nmith_sf, scsodtp[i].num_min_temp_so_far_hourly_times, scsodtp[i].min_temp_so_far_hourly_times, pt[i].mith_sf);

            cnvtimes(pt[i], &pt[i].nmits_sf, scsodtp[i].num_min_temp_so_far_synoptic_times, scsodtp[i].min_temp_so_far_synoptic_times, pt[i].mits_sf);

           /*
            **  computes SYNOINTV (6) hourly date-time array for each given synoptic 
                date-time and store the results into the timezone structure.
            */
            csynohr(pt[i], &ptsyh[i].nmxth, &ptsyh[i].nmxts, &pt[i].nmxts, ptsyh[i].mxth, pt[i].mxts);
            csynohr(pt[i], &ptsyh[i].nmxth_sf, &ptsyh[i].nmxts_sf, &pt[i].nmxts_sf, ptsyh[i].mxth_sf, pt[i].mxts_sf);
            csynohr(pt[i], &ptsyh[i].nmith, &ptsyh[i].nmits, &pt[i].nmits, ptsyh[i].mith, pt[i].mits);
            csynohr(pt[i], &ptsyh[i].nmith_sf, &ptsyh[i].nmits_sf, &pt[i].nmits_sf, ptsyh[i].mith_sf, pt[i].mits_sf);

#ifdef OBX
            cnvtimes(pt[i], &pt[i].npcps, scsodtp[i].num_precip_synoptic_times, scsodtp[i].precip_synoptic_times, pt[i].pcps);

	    /*
	    **  Compute and store the layout keys.
	    */
	    strcpy( pt[i].mxt_lk, cstli( pl, &ntl, pt[i].mxth, pt[i].nmxth, pt[i].mxts, pt[i].nmxts ) );
	    strcpy( pt[i].mxt_sf_lk,
			cstli( pl, &ntl, pt[i].mxth_sf, pt[i].nmxth_sf, pt[i].mxts_sf, pt[i].nmxts_sf ) );
	    strcpy( pt[i].mit_lk, cstli( pl, &ntl, pt[i].mith, pt[i].nmith, pt[i].mits, pt[i].nmits ) );
	    strcpy( pt[i].mit_sf_lk,
			cstli( pl, &ntl, pt[i].mith_sf, pt[i].nmith_sf, pt[i].mits_sf, pt[i].nmits_sf ) );
	    strcpy( pt[i].pcp_lk, cstli( pl, &ntl, pt[i].pcps, 0, pt[i].pcps, pt[i].npcps ) );
#endif

	    #ifdef VERBOSE
	    /*
	    **  Print out the contents of the timezone structure.
	    */
	    printf( "=======================\n" );
	    printf( "timezone %s\n", pt[i].tz );

            printf("mxth array:\n" );
            prtimes(pt[i].nmxth, pt[i].mxth);
            printf("mxts array:\n" );
            prtimes(pt[i].nmxts, pt[i].mxts);
            printf("mxth array:\n" );
            prtimes(ptsyh[i].nmxth, ptsyh[i].mxth);
            printf( "layout-key: %s\n", pt[i].mxt_lk );

            printf("mxth_sf array:\n" );
            prtimes(pt[i].nmxth_sf, pt[i].mxth_sf);
            printf("mxts_sf array:\n" );
            prtimes(pt[i].nmxts_sf, pt[i].mxts_sf);
            printf("mxth_sf array:\n" );
            prtimes(ptsyh[i].nmxth_sf, ptsyh[i].mxth_sf);
            printf( "layout-key: %s\n", pt[i].mxt_sf_lk );

            printf("mith array:\n" );
            prtimes(pt[i].nmith, pt[i].mith);
            printf("mits array:\n" );
            prtimes(pt[i].nmits, pt[i].mits);
            printf("mith array:\n" );
            prtimes(ptsyh[i].nmith, ptsyh[i].mith);
	    printf( "layout-key: %s\n", pt[i].mit_lk );

            printf("mith_sf array:\n" );
            prtimes(pt[i].nmith_sf, pt[i].mith_sf);
            printf("mits_sf array:\n" );
            prtimes(pt[i].nmits_sf, pt[i].mits_sf);
            printf("mith_sf array:\n" );
            prtimes(ptsyh[i].nmith_sf, ptsyh[i].mith_sf);
	    printf( "layout-key: %s\n", pt[i].mit_sf_lk );
#ifdef OBX
            printf("pcps array:\n" );
            prtimes(pt[i].npcps, pt[i].pcps);
	    printf( "layout-key: %s\n", pt[i].pcp_lk );
#endif
	    #endif
        }

#ifdef OBX
	/*
	**  Write the time-layout information to the output XML file.
	*/
	WRITE_XMLTL( )
#endif

	/*
	**  Free up allocated space that is no longer needed.
	free( scsodtp );
	free( ptm );

	**  Assemble the BUFR mnemonic string.
	*/
        str[0] = '\0';
        for ( i = 1; i < 81; i++ ) {
            str[i] = ' ';
        }
	for ( i = 0; i < i1; i++ ) {
	    strcat( str, mnem[i] );
	    if (i < (i1-1) ) strcat( str, cblk );
	}
	
        str1[0] = '\0';
        for ( i = 1; i < 81; i++ ) {
            str1[i] = ' ';
        }
        strcat( str1, mxtstr );
	/*
	**  Open the BUFR file for reading.
	*/
	copenbf( argv[2], &lunit, strlen( argv[2] ) );

	/*
	**  Get the next report from the BUFR file.
	*/
	while ( ireadns( &lunit, subset, &idate, sizeof( subset ) ) == 0 ) {

	    ufbint( &lunit, arr, &i1, &i2, &iret, str, strlen( str ) );

	    /*
	    **  Confirm that this is a METAR report (and not a SPECI, SAO, etc.).
	    */
	    if ( G_NINT( arr[0] ) > 0 ) {
		/*  This isn't a METAR report, so we can ignore it. */
		continue;
	    }
	    /*
	    **  Convert RPID from f77r8 to char.
	    */
	    cnvrp.frp = arr[1];
	    strncpy( rpid, cnvrp.crp, 4 );
	    rpid[4] = '\0';

	    /*
	    **  Check whether RPID exists within the station table.
	    */
	    if ( ( pbs = ( char * ) bsearch( rpid, pc, ( size_t ) nstns, szrp,
			    ( int (*) ( const void *, const void * ) ) cmpstra ) ) == NULL ) {  
		/*
		**  No, so this isn't a station for which we'll need to generate any output,
		**  and we can therefore ignore the entire report.
		*/

		continue;  
	    }
	    j = ( pbs - pc ) / szrp;

	    /*
	    **  Confirm that there's enough space to store this report.
	    */
	    if ( ( k = ps[j].nrpts ) >= MXRPTS ) {
		printf( "ERROR - number of reports exceeded MXRPTS for station %s.\n", rpid );
		continue;
	    }
	    /*
	    **  Store the report date-time.
	    */
	    ps[j].rpdh[k].yr = ( unsigned short ) arr[2];
	    ps[j].rpdh[k].mo = ( unsigned short ) arr[3];
	    ps[j].rpdh[k].dy = ( unsigned short ) arr[4];
	    ps[j].rpdh[k].hr = ( unsigned short ) arr[5];
	    ps[j].rpmi[k]    = ( unsigned short ) arr[6];

            /*
            **  If the report date-time is within the hour that stored previously,
            **  do not store the report.
            */

            if (k > 1) {
               if ( ps[j].rpdh[k].hr == ps[j].rpdh[k-1].hr ) {
                   if ( ps[j].rpmi[k] < 45 )  continue;
                   if ( ps[j].rpmi[k-1] >= 45 )  continue;
               }

               if ( ps[j].rpdh[k].hr == ps[j].rpdh[k-1].hr + 1 ) {
                   if (( ps[j].rpmi[k] < 45 ) && ( ps[j].rpmi[k-1] >= 45 ))  continue;
               }
            }

	    /*
	    **  Compute and store the date-time for which this is the hourly report.
	    */
	    if ( cshrly( &ps[j], k ) != SUCCESS ) continue;

	    /*
	    **  Store the temperature values.
	    */

           /*
            printf( "arr= %Lf\n", ( long double ) arr[7] );
           */
            if ( arr[7] <  99999.) {
               r8vam = arr[7] - r8vak;
               if (r8vam < 0 ) {
                  r8vam = -r8vam;
               } 
               if ( r8vak < 99999.) {
                  if ( r8vam > T25 ) {
                     arr[7] = BMISS;
/*
                     printf( " setting temp to missing  \n");
*/
                  }  
               }
             }
             else {
/*
               printf( " missing value of temp here \n");
*/
             }

	    ps[j].tmdb[k] = arr[7];
            r8vak = arr[7];
            
	    ps[j].mxtm06[k] = ps[j].mitm06[k] = BMISS;
#ifdef OBX
	    ps[j].tp06[k] = arr[8];
	    #define MAXMIN_INDEX ( 9 )
#else
	    #define MAXMIN_INDEX ( 8 )
#endif

	    /*
	    **  Check whether 6-hr maximum and minimum temperatures were reported.
	    */
	    if ( arr[MAXMIN_INDEX] > 0 ) { 
		ufbint( &lunit, arr, &i1, &i2, &iret, str1, strlen( str1 ) );
		if ( iret > 0 ) {
		    if ( G_NINT( arr[0] ) == 6 )  ps[j].mxtm06[k] = arr[1];
                if ( G_NINT( arr[2] ) == 6 )  ps[j].mitm06[k] = arr[3];    
		}
	    }

	    /*
	    **  Increment the report counter.
	    */
	    ps[j].nrpts++;

	}
	closbf( &lunit );

/*
        Richard Bann in HPC found that certain stations do not reset their 6-hour maximum/minimum 
        temperatures for many weeks, so that they routinely send out of date and incorrect values.
        QC for 6-hour maximum/minimum temperatures done here 
*/
        printf( "6-hour maximum/minimum temperatures of the following stations are suspect, thus they are set to missing.\n" );
        for ( i = 0; i < nstns; i++ ) {
                   f77r8  temp6max, temp6min;
            idxmax = 0;
            indxmin = 0;
            for ( k = 0; k < ps[i].nrpts; k++ ) {
               if ( ! ibfms( &ps[i].mxtm06[k] ) ) {
                   if ( idxmax == 0 ) temp6max = ps[i].mxtm06[k];
                   idxmax++;
                   if ( fabs(ps[i].mxtm06[k] - temp6max) > 0.1) break;
               }
               if ( ! ibfms( &ps[i].mitm06[k] ) ) { 
                   if ( indxmin == 0 ) temp6min = ps[i].mitm06[k]; 
                   indxmin++; 
                   if ( fabs( ps[i].mitm06[k] - temp6min) > 0.1 )  break;
               }
            }
            if ( (k == ps[i].nrpts) && ((idxmax != 0) || (indxmin != 0)) ) {
                printf("%7s  %s \n", ps[i].stid, ps[i].stnnam);
                for ( k = 0; k < ps[i].nrpts; k++ ) {
                   ps[i].mxtm06[k] =  BMISS;
                   ps[i].mitm06[k] = BMISS;
                }  
            }
        }
        printf( "End of the station list. \n");


#ifdef OBX
	/*
	**  For each station in the station information structure, compute the parameters
	**  information and write it to the output XML file.
	*/
#else
        /*
        **  Determine the maximum and minimum temperatures for the CONUS.  Note that either value
        **  could have possibly occurred at more than one station.
        */
        mxt_conus = mivtmpk;  /* Initialize to minimum possible value. */
        mit_conus = mxvtmpk;  /* Initialize to maximum possible value. */
        nmxt_conus = nmit_conus = 0;
#endif
	for ( i = 0; i < nstns; i++ ) {

	    #ifdef VERBOSE
	    /*
	    **  Print out the contents of the station information structure.
	    */
/*
	    printf( "===========================\n" );
	    printf( "stid stnnam state nrpts = %s  %s  %s  %s  %u\n" ,
		    ps[i].stid, ps[i].stnnam, ps[i].state, ps[i].tz, ps[i].nrpts);
	    for ( k = 0; k < ps[i].nrpts; k++ ) {
		printf( "k = %u\n", k );
	        printf( "report date-time = %u %02u %02u %02u %02u\n",
		    ps[i].rpdh[k].yr, ps[i].rpdh[k].mo, ps[i].rpdh[k].dy, ps[i].rpdh[k].hr, ps[i].rpmi[k] );
	        printf( "hourly date-time = %u %02u %02u %02u\n",
		    ps[i].hydh[k].yr, ps[i].hydh[k].mo, ps[i].hydh[k].dy, ps[i].hydh[k].hr );
		if ( ! ibfms( &ps[i].tmdb[k] ) )  printf( "tmdb = %Lf\n", ( long double ) ps[i].tmdb[k] );
#ifdef OBX
		if ( ! ibfms( &ps[i].tp06[k] ) )  printf( "tp06 = %Lf\n", ( long double ) ps[i].tp06[k] );
#endif
		if ( ! ibfms( &ps[i].mxtm06[k] ) )  printf( "mxtm06 = %Lf\n", ( long double ) ps[i].mxtm06[k] );
		if ( ! ibfms( &ps[i].mitm06[k] ) )  printf( "mitm06 = %Lf\n", ( long double ) ps[i].mitm06[k] );
	    }
*/
	    #endif

	    /*
	    **  Locate the timezone for this station within the timezone structure.
	    */
	    for ( j = 0; j < nsdt; j++ ) {
		if ( strcmp( ps[i].tz, pt[j].tz ) == 0 ) break;
	    }
	    if ( j == nsdt ) {
		printf( "ERROR - information for %s is not available in timezone structure.\n", ps[i].tz );
		continue;
	    }

	    /*
	    **  Compute the maximum temperature for this station.
	    */
	    r8val = mivtmpk;
	    for ( k = 0; k < pt[j].nmxth; k++ ) {
		if ( ( fdate( pt[j].mxth[k], ps[i], &idx ) == SUCCESS ) &&
		    ( ! ibfms( &ps[i].tmdb[idx] ) ) && ( ps[i].tmdb[idx] < mxvtmpk ) )
		     r8val = G_MAX( r8val, ps[i].tmdb[idx] );
	    }
            /*
            ** If data of given synoptic date-time are missing, then look for data of hourly 
               date-time array when computing maximum temperature for this station.              
           */
	    for ( k = 0; k < pt[j].nmxts; k++ ) {
		if ( ( fdate( pt[j].mxts[k], ps[i], &idx ) == SUCCESS ) && 
                    (! ibfms( &ps[i].mxtm06[idx] ) ) && ( ps[i].mxtm06[idx] < mxvtmpk ) ) {
		       r8val = G_MAX( r8val, ps[i].mxtm06[idx] );
                    }
                    else
                    {
                       for ( l = 0; l < ptsyh[j].nmxth; l++ ) {
                          if ( ( fdate( ptsyh[j].mxth[l], ps[i], &idx ) == SUCCESS ) &&
                             ( ! ibfms( &ps[i].tmdb[idx] ) ) && ( ps[i].tmdb[idx] < mxvtmpk ) )
                             r8val = G_MAX( r8val, ps[i].tmdb[idx] );
                       }
                       break;
                    }
	    }

#ifdef OBX
	    /*
	    **  Start a parameters entry for this station within the output XML file.
	    */
	    WRITE_XMLSPE( ps[i].stid )

	    if ( ( pt[j].nmxth > 0 ) || ( pt[j].nmxts > 0 ) ) {
		/*
		**  Write out the maximum temperature for this station.
		*/
		WRITE_XMLTEMP( "maximum", pt[j].mxt_lk, "Maximum Temperature", r8val )
	    }

	    /*
	    **  Compute and write out the maximum temperature so far for this station.
	    */
	    r8val = mivtmpk;
	    for ( k = 0; k < pt[j].nmxth_sf; k++ ) {
		if ( ( fdate( pt[j].mxth_sf[k], ps[i], &idx ) == SUCCESS ) &&
		    ( ! ibfms( &ps[i].tmdb[idx] ) ) && ( ps[i].tmdb[idx] < mxvtmpk ) )
		     r8val = G_MAX( r8val, ps[i].tmdb[idx] );
	    }
            /*
            ** If data of given synoptic date-time are missing, then look for data of hourly
               date-time array when computing maximum temperature so far for this station.
           */
	    for ( k = 0; k < pt[j].nmxts_sf; k++ ) {
		if (( fdate( pt[j].mxts_sf[k], ps[i], &idx ) == SUCCESS ) &&
                    ( ! ibfms( &ps[i].mxtm06[idx] )) && ( ps[i].mxtm06[idx] < mxvtmpk ) ) {
		       r8val = G_MAX( r8val, ps[i].mxtm06[idx] );
                    }
                    else
                    {
                       for ( l = 0; l < ptsyh[j].nmxth_sf; l++ ) {
                          if ( ( fdate( ptsyh[j].mxth_sf[l], ps[i], &idx ) == SUCCESS ) &&
                             ( ! ibfms( &ps[i].tmdb[idx] ) ) && ( ps[i].tmdb[idx] < mxvtmpk ) )
                             r8val = G_MAX( r8val, ps[i].tmdb[idx] );
                       }
                       break;
                    }
	    }
	    if ( ( pt[j].nmxth_sf > 0 ) || ( pt[j].nmxts_sf > 0 ) ) {
		WRITE_XMLTEMP( "maximum", pt[j].mxt_sf_lk, "Maximum Temperature So Far", r8val )
	    }
#else
            /*
            **  At 00Z use  maximum temperature so far for max temp      
            */
           /*
             At 00Z, there is no maximum temperature so far for sometime zones such as AST4, EST5EDT,
             corret this bug by Guan at 12/23/08
           */
        if (( strcmp ( cyc, c00  )== 0) && (pt[j].nmxts_sf > 0)) {
            r8val = mivtmpk;
            for ( k = 0; k < pt[j].nmxth_sf; k++ ) {
                if ( ( fdate( pt[j].mxth_sf[k], ps[i], &idx ) == SUCCESS ) &&
                    ( ! ibfms( &ps[i].tmdb[idx] ) ) && ( ps[i].tmdb[idx] < mxvtmpk ) )
                     r8val = G_MAX( r8val, ps[i].tmdb[idx] );
            }
            /*
            ** If data of given synoptic date-time are missing, then look for data of hourly
               date-time array when computing maximum temperature so far for this station.
           */
            for ( k = 0; k < pt[j].nmxts_sf; k++ ) {
                if (( fdate( pt[j].mxts_sf[k], ps[i], &idx ) == SUCCESS ) &&
                    ( ! ibfms( &ps[i].mxtm06[idx] )) && ( ps[i].mxtm06[idx] < mxvtmpk ) ) {
                       r8val = G_MAX( r8val, ps[i].mxtm06[idx] );
                    }
                    else
                    {
                       for ( l = 0; l < ptsyh[j].nmxth_sf; l++ ) {
                          if ( ( fdate( ptsyh[j].mxth_sf[l], ps[i], &idx ) == SUCCESS ) &&
                             ( ! ibfms( &ps[i].tmdb[idx] ) ) && ( ps[i].tmdb[idx] < mxvtmpk ) )
                             r8val = G_MAX( r8val, ps[i].tmdb[idx] );
                       }
                       break;
                    }
            }
         }
	    #define STORE_XTMP( type ) \
	    if ( n ## type ## _conus > MXTXSTN ) { \
		printf( "ERROR - number of " #type " stations exceeded MXTXSTN at %LfK (=%dF).\n", \
			( long double ) type ## _conus, G_NINT( K2F( type ## _conus ) ) ); \
	    } \
	    else { \
		type ## _stnidx[n ## type ## _conus] = i; \
		type ## _tzidx[n ## type ## _conus] = j; \
		n ## type ## _conus++; \
	    }
	    /* end of STORE_XTMP macro definition */

	    /*
	    **  Is this a new maximum temperature for the CONUS?
	    */
	    if ( r8val > mivtmpk ) {
		if ( r8val > mxt_conus ) {
		    mxt_conus = r8val;
		    nmxt_conus = 0; /* set the count back to 0 since we now have a new mxt */
		    STORE_XTMP( mxt )
		}
	        else if ( G_ABS( r8val - mxt_conus ) < GDIFFD ) {  /* a safe test for equality */
		    STORE_XTMP( mxt )
		}
		#ifdef VERBOSE
		printf( "maximum tempurature at this station = %Lf\n" \
			"maximum temperature so far within CONUS = %Lf\n", \
			( long double ) r8val, ( long double ) mxt_conus );
		#endif
	    }
#endif

	    /*
	    **  Compute the minimum temperature for this station.
	    */
	    r8val = mxvtmpk;
	    for ( k = 0; k < pt[j].nmith; k++ ) {
		if ( ( fdate( pt[j].mith[k], ps[i], &idx ) == SUCCESS ) && ( ps[i].tmdb[idx] > mivtmpk ) )
		    r8val = G_MIN( r8val, ps[i].tmdb[idx] );
	    }
            r8vak = r8val;
            /*
            ** If data of given synoptic date-time are missing, then look for data of hourly
               date-time array when computing minimum temperature for this station.
           */
	    for ( k = 0; k < pt[j].nmits; k++ ) {
		if (( fdate( pt[j].mits[k], ps[i], &idx ) == SUCCESS ) &&
                    (! ibfms( &ps[i].mitm06[idx] )) &&  ps[i].mitm06[idx] > mivtmpk ) {
		        r8val = G_MIN( r8val, ps[i].mitm06[idx] );
                    }
                    else 
                    {
                        for ( l = 0; l < ptsyh[j].nmith; l++ ) {
                            if ( ( fdate( ptsyh[j].mith[l], ps[i], &idx ) == SUCCESS ) && ( ps[i].tmdb[idx] > mivtmpk ) )
                                r8val = G_MIN( r8val, ps[i].tmdb[idx] );
                        }
                        break;
                    }
	    }
            r8vam = r8vak - r8val;
            if (abs (r8vam) > T25) {
               if ( r8vak < 313. ) {
                  printf (" xxxxxx  possible bad min here:");
                  printf (" min from temps is %f",r8vak);
                  printf (" minimun found %f\n",r8val);

                  /* The number of (hourly_temperature - min_synoptic_temperature) > T25 */
                  nldbhst = 0;
                  nldbhst_total = 0;
                  r8vak = mxvtmpk;
                  for ( k = 0; k < pt[j].nmith; k++ ) {
                      if ( ( fdate( pt[j].mith[k], ps[i], &idx ) == SUCCESS ) && ( ps[i].tmdb[idx] > mivtmpk ) )
                      {
                          nldbhst_total++;
                          if (abs(r8val - ps[i].tmdb[idx]) > T25 ) 
                          {
                             nldbhst++;
                             r8vak = G_MIN( r8vak, ps[i].tmdb[idx] );
                          }
                      }
                  }
                  for ( l = 0; l < ptsyh[j].nmith; l++ ) {
                      if ( ( fdate( ptsyh[j].mith[l], ps[i], &idx ) == SUCCESS ) && ( ps[i].tmdb[idx] > mivtmpk ) )
                      {
                         nldbhst_total++;
                         if (abs(r8val - ps[i].tmdb[idx]) > T25 ) 
                         {
                             nldbhst++;
                             r8vak = G_MIN( r8vak, ps[i].tmdb[idx] );
                         }
                      }
                  }
                  /* If half of hour temperature is 25 degree C higher than synoptic min, do not use  synoptic min. 
                      QC proposed by Scott */
                  if (nldbhst > nldbhst_total/2 ) r8val = r8vak;
               }
            }
#ifdef OBX
	    if ( ( pt[j].nmith > 0 ) || ( pt[j].nmits > 0 ) ) {
		/*
		**  Write out the minimum temperature for this station.
		*/
		WRITE_XMLTEMP( "minimum", pt[j].mit_lk, "Minimum Temperature", r8val )
	    }

	    /*
	    **  Compute and write out the minimum temperature so far for this station.
	    */
	    r8val = mxvtmpk;
	    for ( k = 0; k < pt[j].nmith_sf; k++ ) {
		if ( ( fdate( pt[j].mith_sf[k], ps[i], &idx ) == SUCCESS ) && ( ps[i].tmdb[idx] > mivtmpk ) )
		    r8val = G_MIN( r8val, ps[i].tmdb[idx] );
	    }
            /*
            ** If data of given synoptic date-time are missing, then look for data of hourly
               date-time array when computing minimum temperature so far for this station.
           */

	    for ( k = 0; k < pt[j].nmits_sf; k++ ) {
		if (( fdate( pt[j].mits_sf[k], ps[i], &idx ) == SUCCESS ) &&
                     (! ibfms( &ps[i].mitm06[idx] )) && ps[i].mitm06[idx] > mivtmpk ) { 
		        r8val = G_MIN( r8val, ps[i].mitm06[idx] );
                    }
                    else
                    {
                        for ( l = 0; l < ptsyh[j].nmith_sf; l++ ) {
                            if ( ( fdate( ptsyh[j].mith_sf[l], ps[i], &idx ) == SUCCESS ) && 
                                ( ps[i].tmdb[idx] > mivtmpk ) )
                                r8val = G_MIN( r8val, ps[i].tmdb[idx] );
                        }
                        break;
                    }
            }
	    if ( ( pt[j].nmith_sf > 0 ) || ( pt[j].nmits_sf > 0 ) ) {
		WRITE_XMLTEMP( "minimum", pt[j].mit_sf_lk, "Minimum Temperature So Far", r8val )
	    }
	    
	    /*
	    **  Compute the 24-hr precipitation for this station.
	    */
	    ngpcp = 0;
	    r8val = 0;
	    for ( k = 0; k < pt[j].npcps; k++ ) {
		/*
		**  Check each 6-hour value to make sure it's neither "missing" nor "trace".
		*/
		if ( ( fdate( pt[j].pcps[k], ps[i], &idx ) == SUCCESS ) &&
			( ! ibfms( &ps[i].tp06[idx] ) ) && ( ps[i].tp06[idx] >= 0 ) ) {
		     r8val += ps[i].tp06[idx];
		     ngpcp++;
		}
	    }
	    /*
	    **  Write the 24-hour precipitation to the output XML file, so long as there was at least
	    **  one good (i.e. neither "missing" nor "trace") 6-hour value available.  Otherwise,
	    **  write a nil value.
	    */
	    node3 = xmlNewChild( node2, NULL, XML_CHAR "precipitation", NULL );
	    xmlNewProp( node3, XML_CHAR "type", XML_CHAR "liquid" );
	    xmlNewProp( node3, XML_CHAR "units", XML_CHAR "inches" );
	    xmlNewProp( node3, XML_CHAR "time-layout", XML_CHAR pt[j].pcp_lk );
	    xmlNewChild( node3, NULL, XML_CHAR "name", XML_CHAR "Yesterday's Liquid Precipitation Amount" );
	    if ( ngpcp > 0 ) {
		sprintf( str, "%.2Lf", ( long double ) MM2IN( r8val ) );
		xmlNewChild( node3, NULL, XML_CHAR "value", XML_CHAR str );
	    }
	    else {
		node4 = xmlNewChild( node3, NULL, XML_CHAR "value", NULL );
		xmlNewProp( node4, XML_CHAR "xsi:nil", XML_CHAR "true" );
	    }
#else
            /*
            **  At 12Z, use the minimum temperature so far for min temp
            */
        if ( strcmp ( cyc, c12  )== 0) {
            r8val = mxvtmpk;
            for ( k = 0; k < pt[j].nmith_sf; k++ ) {
                if ( ( fdate( pt[j].mith_sf[k], ps[i], &idx ) == SUCCESS ) &&
 ( ps[i].tmdb[idx] > mivtmpk ) )
                    r8val = G_MIN( r8val, ps[i].tmdb[idx] );
            }
            r8vak = r8val;
            /*
            ** If data of given synoptic date-time are missing, then look for data of hourly
               date-time array when computing minimum temperature so far for this station.
           */
            for ( k = 0; k < pt[j].nmits_sf; k++ ) {
                if (( fdate( pt[j].mits_sf[k], ps[i], &idx ) == SUCCESS ) &&
                   ( ! ibfms( &ps[i].mitm06[idx]) ) && ( ps[i].mitm06[idx] > mivtmpk) ) {
                       r8val = G_MIN( r8val, ps[i].mitm06[idx] );
                    }
                    else
                    {
                        for ( l = 0; l < ptsyh[j].nmith_sf; l++ ) {
                            if ( ( fdate( ptsyh[j].mith_sf[l], ps[i], &idx ) == SUCCESS ) &&
                                ( ps[i].tmdb[idx] > mivtmpk ) )
                                r8val = G_MIN( r8val, ps[i].tmdb[idx] );
                        }
                        break;
                    }
                }
            r8vam = r8vak - r8val;
            if (abs (r8vam) > T25) {
               if ( r8vak < 313. ) {
                  printf (" xxxxxx  possible bad min here:");
                  printf (" min from temps is %f",r8vak);
                  printf (" minimun found %f\n",r8val);

                  /* The number of (hourly_temperature - min_synoptic_temperature) > T25 */
                  nldbhst = 0;
                  nldbhst_total = 0;
                  r8vak = mxvtmpk;
                  for ( k = 0; k < pt[j].nmith_sf; k++ ) {
                      if ( ( fdate( pt[j].mith_sf[k], ps[i], &idx ) == SUCCESS ) && ( ps[i].tmdb[idx] > mivtmpk ) )
                      {
                          nldbhst_total++;
                          if (abs(r8val - ps[i].tmdb[idx]) > T25 ) 
                          {
                              nldbhst++;
                              r8vak = G_MIN( r8vak, ps[i].tmdb[idx] );
                          }
                      }
                  }
                  for ( l = 0; l < ptsyh[j].nmith_sf; l++ ) {
                      if ( ( fdate( ptsyh[j].mith_sf[l], ps[i], &idx ) == SUCCESS ) && ( ps[i].tmdb[idx] > mivtmpk ) )
                      {
                         nldbhst_total++;
                         if (abs(r8val - ps[i].tmdb[idx]) > T25 ) 
                         {
                            nldbhst++;
                            r8vak = G_MIN( r8vak, ps[i].tmdb[idx] );
                         }
                      }
                  }
                  /* If half of hour temperature is 25 degree C higher than synoptic min, do not use  synoptic min. 
                     QC proposed by Scott */
                  if (nldbhst > nldbhst_total/2 ) r8val = r8vak;
               }
            }

         }
              
	    /*
	    **  Is this a new minimum temperature for the CONUS?
	    */
	    if ( r8val < mxvtmpk ) {
		if ( r8val < mit_conus ) {
		    mit_conus = r8val;
		    nmit_conus = 0; /* set the count back to 0 since we now have a new mit */
		    STORE_XTMP( mit )
		}
	        else if ( G_ABS( r8val - mit_conus ) < GDIFFD ) {  /* a safe test for equality */
		    STORE_XTMP( mit )
		}
		#ifdef VERBOSE
		printf( "minimum tempurature at this station = %Lf\n" \
			"minimum temperature so far within CONUS = %Lf\n", \
			( long double ) r8val, ( long double ) mit_conus );
		#endif
	    }
#endif
	}

#ifdef TEX
	/*
	**  Allocate space to store the time-layout information.
	*/
	G_FREE( pc, char )
	ALLOC( 0, pc, char, ( ( nmxt_conus + nmit_conus ) * MXLLK ), "layout keys" )
	ALLOC( 0, pl, tlinfo, ( nmxt_conus + nmit_conus ), "time-layout information" )

	/*
	**  Write the location information to the output XML file.
	*/
	for ( i = 0; i < nmxt_conus; i++ ) {
	    WRITE_XMLLOC( ps[mxt_stnidx[i]].stid, ps[mxt_stnidx[i]].stnnam, ps[mxt_stnidx[i]].state, "conus" )
	    /*
	    **  Compute and store the layout keys.
	    */
            if ( strcmp ( cyc, c00  )== 0) {
	       strcpy( &pc[i*MXLLK],
		    cstli( pl, &ntl, pt[mxt_tzidx[i]].mxth_sf, pt[mxt_tzidx[i]].nmxth_sf,
				     pt[mxt_tzidx[i]].mxts_sf, pt[mxt_tzidx[i]].nmxts_sf ) );
            }
            else {
               strcpy( &pc[i*MXLLK],
                    cstli( pl, &ntl, pt[mxt_tzidx[i]].mxth, pt[mxt_tzidx[i
]].nmxth,
                                     pt[mxt_tzidx[i]].mxts, pt[mxt_tzidx[i
]].nmxts ) );
            }
	}
	for ( i = 0; i < nmit_conus; i++ ) {
	    /*
	    **  Make sure that this station didn't also have the maximum temperature for the CONUS.
	    **  If it did, then its location information would have already been written to the
	    **  output XML file.
	    */
	    for ( j = 0; j < nmxt_conus; j++ ) {
		if ( mit_stnidx[i] == mxt_stnidx[j] ) break;
	    }
	    if ( j == nmxt_conus ) {
		WRITE_XMLLOC( ps[mit_stnidx[i]].stid, ps[mit_stnidx[i]].stnnam, ps[mit_stnidx[i]].state, "conus" )
	    }
	    /*
	    **  Compute and store the layout keys.
	    */
            if ( strcmp ( cyc, c12  )== 0) {
	      strcpy( &pc[(i+nmxt_conus)*MXLLK],
		    cstli( pl, &ntl, pt[mit_tzidx[i]].mith_sf, pt[mit_tzidx[i]].nmith_sf,
				     pt[mit_tzidx[i]].mits_sf, pt[mit_tzidx[i]].nmits_sf ) );
            }
            else {
	      strcpy( &pc[(i+nmxt_conus)*MXLLK],
		    cstli( pl, &ntl, pt[mit_tzidx[i]].mith, pt[mit_tzidx[i]].nmith,
				     pt[mit_tzidx[i]].mits, pt[mit_tzidx[i]].nmits ) );
            }
	}

	if ( ( nmxt_conus > 0 ) || ( nmit_conus > 0 ) ) {
	    /*
	    **  Write the time-layout information to the output XML file.
	    */
	    WRITE_XMLTL( )
	}

	if ( nmxt_conus > 0 ) {
	    /*
	    **  Write the maximum CONUS temperature to the output XML file.
	    */
	    for ( i = 0; i < nmxt_conus; i++ ) {
		WRITE_XMLSPE( ps[mxt_stnidx[i]].stid )
		WRITE_XMLTEMP( "maximum", &pc[i*MXLLK], "National High Temperature", mxt_conus )
	    }
	}
	else {
	    /*
	    **  Write a nil entry.
	    */
	    node2 = xmlNewChild( node1, NULL, XML_CHAR "parameters", NULL );
	    WRITE_XMLTEMP( "maximum", &pc[i*MXLLK], "National High Temperature", mxt_conus )
	}

	if ( nmxt_conus > 0 ) {
	    /*
	    **  Write the minimum CONUS temperature to the output XML file.
	    */
	    for ( i = 0; i < nmit_conus; i++ ) {
		WRITE_XMLSPE( ps[mit_stnidx[i]].stid )
		WRITE_XMLTEMP( "minimum", &pc[(i+nmxt_conus)*MXLLK], "National Low Temperature", mit_conus )
	    }
	}
	else {
	    /*
	    **  Write a nil entry.
	    */
	    node2 = xmlNewChild( node1, NULL, XML_CHAR "parameters", NULL );
	    WRITE_XMLTEMP( "minimum", &pc[i*MXLLK], "National Low Temperature", mit_conus )
	}
#endif

	/*
	**  Finish writing the output XML file.
	*/
	xmlSaveFormatFile( argv[5], doc, 1 );

	/*
	**  Free all allocated memory.
	*/
	G_FREE( ps, stinfo )
	G_FREE( pt, tzinfo )
        G_FREE( ptsyh, tzinfo )
	G_FREE( pl, tlinfo )
	G_FREE( pc, char )
	xmlFreeDoc( doc );
	xmlCleanupParser( );

	return SUCCESS;
}

void cnvtimes(tzinfo pt, unsigned short * nmith, int num_max_temp_hourly_times, time_t * max_temp_hourly_times, dhinfo *mith) {
   if ( ( *nmith = ( unsigned short ) num_max_temp_hourly_times ) > MXTIMES ) {
     printf( "ERROR - number of times exceeded MXTIMES in timezone %s.\n", pt.tz);
     *nmith =  MXTIMES ;
   }
   int j = 0;
   struct tm *ptm, ptms;
   ptm = &ptms;
   for ( j = 0; j < *nmith; j++ ) {
      ptm = gmtime( &max_temp_hourly_times[j]);
      mith[j].yr =  ( unsigned short ) ptm->tm_year + 1900;
      mith[j].mo = ( unsigned short ) ptm->tm_mon + 1;
      mith[j].dy = ( unsigned short ) ptm->tm_mday;
      mith[j].hr = ( unsigned short ) ptm->tm_hour;
   }
}

void csynohr(tzinfo pt, unsigned short *nmith,  unsigned short *nmits, unsigned short *nts,  dhinfo *mith, dhinfo *mits) {
   int j, k;
   time_t secs;
   struct tm *ptm, ptms;
   ptm = &ptms;
   *nmits = ( unsigned short ) *nts;
   if ( ( *nmith = SYNOINTV * *nts ) > MXTIMES ) {
       printf( "ERROR - number of times exceeded MXTIMES  in timezone %s.\n", pt.tz );
       *nts = MXTIMES ;
   } 
   for ( j = 0; j < *nts; j++ ) {
       ptm->tm_year = mits[j].yr -1900;
       ptm->tm_mon  = mits[j].mo -1;
       ptm->tm_mday = mits[j].dy;
       ptm->tm_hour = mits[j].hr;
       ptm->tm_min = 0;
       ptm->tm_sec = 0;
       ptm->tm_isdst = -1;
       secs = mktime(ptm);
       secs-=3600* (SYNOINTV -1 );
       ptm = localtime(&secs);
       for ( k = SYNOINTV * j ; k < SYNOINTV * (j + 1);  k++ ) {
           mith[k].yr =  ( unsigned short ) ptm->tm_year + 1900;
           mith[k].mo = ( unsigned short ) ptm->tm_mon + 1;
           mith[k].dy = ( unsigned short ) ptm->tm_mday;
           mith[k].hr = ( unsigned short ) ptm->tm_hour;
           secs = mktime(ptm);
           secs+=3600; 
           ptm = localtime(&secs);
       }  
   }
}


void prtimes(unsigned short nmith, dhinfo *mith) {
   int j;
   for ( j = 0; j < nmith; j++ ) {
       printf( "  %u %02u %02u %02u\n", mith[j].yr, mith[j].mo, mith[j].dy, mith[j].hr); 
   }
}
