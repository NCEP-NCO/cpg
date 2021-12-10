/************************************************************************
 * TSS									*
 *									*
 * This program generates the <data> content for the TsS product. 	*
 * When it runs, it returns a value of 0 if successful and -1 otherwise.*
 * All diagnostic messages are written to STDOUT.			*
 * 90% of Codes are from CPG.						*
 *									*
 *	Command line:							*
 *	    cpg_tss rundt bfrfil stnfil nstn xmlfil xmlfil_temp         *
 *									*
 *	    rundt	= run date-time (YYYYMMDDHH)			*
 *	    bfrfil	= filename containing METAR BUFR data		*
 *	    stnfil	= filename containing station information	*
 *	    nstn	= number of stations in stnfil			*
 *	    xmlfil	= filename to contain output <data> content 	*
 *          xmlfil_temp = xml file from cpg_tex                         *
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
 * S. Guan/NCEP         08/11   decode shelf data (state summary)       *
 * S. Guan/NCEP         02/13 Ported to WCOSS                           *
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
	    if ( ( value <  MXVTMPF ) && ( value > MIVTMPF ) ) { \
		xmlNewProp( node3, XML_CHAR "time-layout", XML_CHAR layout_key ); \
		xmlNewChild( node3, NULL, XML_CHAR "name", XML_CHAR name ); \
		sprintf( str, "%d", G_NINT( value ) ); \
		xmlNewChild( node3, NULL, XML_CHAR "value", XML_CHAR str ); \
	    } \
	    else { \
		xmlNewChild( node3, NULL, XML_CHAR "name", XML_CHAR name ); \
		node4 = xmlNewChild( node3, NULL, XML_CHAR "value", NULL ); \
		xmlNewProp( node4, XML_CHAR "xsi:nil", XML_CHAR "true" ); \
	    }

	FILE *fst;

	char subset[11], rpid[9], usg, *pbs, *pc, *pct, *pc2;
	char str[181], cblk[2] = " ";
        char cyc[3];
        char c00[3] = "00";
        char c12[3] = "12";
        char prodc[6];
        char filename[120];
        char *xname, *xvalue;
        xmlTextReaderPtr reader;
        int ret;


        char indstr[61] = "<SHTMDBSQ> <SHMXTMSQ> <SHMITMSQ> BUHD";
        char tmpstr[61] =  "TMDB    SHRV  SHQL";
        char maxtstr[61] = "MXTM    SHRV  SHQL";
        char mintstr[61] = "MITM    SHRV  SHQL";
        char str1[61], str2[61], str3[61], str4[61];

	char mnem[][11] = {
            "<SHTPMOSQ>",
            "<SHTPHRSQ>",
	    "<SHTPMISQ>",
	    "RPID",
	    "YEAR",
	    "MNTH",
	    "DAYS",
	    "HOUR",
	    "MINU"
	};

	f77r8 arr[NUMELEM(mnem)];
        f77r8 arr0[NUMELEM(mnem)];
        f77r8 arr1[NUMELEM(mnem)];
	f77r8 r8val;
	f77r8 r8vak = 9999999.;
	f77r8 r8vam = 9999999.;
	f77r8 mxvtmpk = F2K( MXVTMPF ), mivtmpk = F2K( MIVTMPF );
	f77r8 mxt_conus, mit_conus;
	f77int lunit = 12;
	f77int idate, ict, iret, nstns, mxt_conust, mit_conust, mxt_conusf, mit_conusf;
	f77int i1 = NUMELEM(mnem), i2 = 1;

	unsigned short i, j, k, l, j1, j2;
	unsigned short idx, ntl = 0;

	unsigned short mxt_stnidx[MXTXSTN], mit_stnidx[MXTXSTN];
	unsigned short mxt_tzidx[MXTXSTN], mit_tzidx[MXTXSTN];
	unsigned short nmxt_conus, nmit_conus;
        unsigned short ma, mi, mak, mik, ma_stat, mi_stat;
	unsigned int szrp = sizeof( rpid );

	/*  Declare a union to use in converting RPID from f77r8 to char. */
	union {
	    f77r8 frp;
	    char crp[8];
	} cnvrp;

        time_t secs;
        struct tm *ptm, ptms;

	tzinfo *pt, *ptsyh;

	tlinfo *pl, *plt;

	stinfo *ps, *pst, *psf;

/*---------------------------------------------------------------------*/
        ptm = &ptms;

	/*
	**  Check for correct number of arguments.
	*/
	if ( argc != 7 ) {
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
        ps = NULL;
	ALLOC( 0, ps, stinfo, nstns, "station information" ) 
        pc = NULL;
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
                sscanf( str, "%8c%33c%*c%2c%*4c%10c%*4c%c",
                        ps[i].stid, ps[i].stnnam, ps[i].state, ps[i].tz, &usg );
                /*
 *                 **  Check the usage key for this station.
 *                                 */

                #define USAGE_KEY ('2')
                if ( ( usg == USAGE_KEY ) || ( usg == '3' ) ) {
                    strcpy( &pc[i*szrp], ps[i].stid );
                    ps[i].nrpts = 0;
                    /*
 *                     **  Remove trailing blanks from the station name.
 *                                         */
                    cst_lstr( ps[i].stnnam, &ict, &iret );
                    ps[i].stnnam[ict] = '\0';
                    /*
 *                     **  Remove trailing blanks from the timezone.
 *                                         */
                    cst_lstr( ps[i].tz, &ict, &iret );
                    ps[i].tz[ict] = '\0';
                    /*
 *                     **  Increment the counter.
 *                                         */
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
 *         **  Initialize the output XML file.
 *                 */
        doc = xmlNewDoc( XML_CHAR "1.0" );
        node1 = xmlNewNode( NULL, XML_CHAR "data" );
        xmlDocSetRootElement( doc, node1 );

        /*
 *         **  Allocate space to store the timezone information.
 *                 */
        pt = NULL;
        ALLOC( 0, pt, tzinfo, MXTZ, "timezone information" )

        /*
 *         **  Allocate space to store the timezone information.
 *                     Actually only need to store SYNOINTV (6) hourly
 *                                 date-time array for each given synoptic date-time.
 *                                         */
        ptsyh = NULL;
        ALLOC( 0, ptsyh, tzinfo, MXTZ, "timezone information" )

        reader = xmlReaderForFile(argv[6], NULL, 0);
        /*
 *         **  Get the applicable information for each timezone.
 *                 */
        scsodtp = SCS_Obs_definitions( argv[1], "ALL", G_FALSE, &nsdt );
        for ( i = 0; i < nsdt; i++ ) {
            printf(" %s  %d\n", scsodtp[i].timezone, i);
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
            #endif
        }
        /*
         **  Initialize to maximum/minimum possible value
        **  for all stations.
        */
        for ( i = 0; i < nstns; i++ ) {
            ps[i].maxtemp = mivtmpk;
            ps[i].maxt_sf = mivtmpk;
            ps[i].mintemp = mxvtmpk;
            ps[i].mint_sf = mxvtmpk;
        }

        /*
         **  Assemble the BUFR mnemonic string.
        */
        str[0] = '\0';
        for ( i = 1; i < 181; i++ ) {
            str[i] = ' ';
        }

        for ( i = 0; i < i1; i++ ) {
            strcat( str, mnem[i] );
            if (i < (i1-1) ) strcat( str, cblk );
        }

        str1[0] = '\0';
        for ( i = 0; i < 61; i++ ) {
            str1[i] = indstr[i];
            indstr[i] = ' ';
            str2[i] = tmpstr[i];
            tmpstr[i] = ' ';
            str3[i] = maxtstr[i];
            maxtstr[i] = ' ';
            str4[i] = mintstr[i];
            mintstr[i] = ' ';
        }
        indstr[0] = '\0';
        tmpstr[0] = '\0';
        maxtstr[0] = '\0';
        mintstr[0] = '\0';
        strcat( indstr, str1);
        strcat( tmpstr, str2);
        strcat( maxtstr, str3);
        strcat( mintstr, str4);

        /*
         **  Open the BUFR file for reading.
        */
        copenbf( argv[2], &lunit, strlen( argv[2] ) );

        /*
         **  Get the next report from the BUFR file.
        */
        while ( ireadns( &lunit, subset, &idate, sizeof( subset ) ) == 0 ) {

            ufbint( &lunit, arr0, &i1, &i2, &iret, indstr, strlen( indstr ) );

            /*
             **  Confirm there are temperature, or max/Min temperature
            */
            if ( G_NINT( arr0[0] +  arr0[1] +  arr0[2]) <= 0 ) {
                /*  There is no temperature, or max/Min temperature in recorder, so we can ignore it. */
                continue;
            }

            /*
             ** Check whether it is a state summary product.
            */
            cnvrp.frp = arr0[3];
            strncpy( prodc, cnvrp.crp, 5 );
            prodc[5]  = '\0';
            if ( strcmp(prodc, "ASUS6") != 0 ) continue;

            ufbint( &lunit, arr, &i1, &i2, &iret, str, strlen( str ) );

            /*
             **  Convert RPID from f77r8 to char.
            */
            cnvrp.frp = arr[3];
            strncpy( rpid, cnvrp.crp, 8 );
            rpid[8] = '\0';

            /*
             **  Check whether RPID exists within the station table.
            */
            if ( ( pbs = ( char * ) bsearch( rpid, pc, ( size_t ) nstns, szrp,
                            ( int (*) ( const void *, const void * ) ) cmpstra ) ) == NULL ) {
                /*
 *                 **  No, so this isn't a station for which we'll need to generate any output,
 *                                 **  and we can therefore ignore the entire report.
 *                                                 */
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
            ps[j].rpdh[k].yr = ( unsigned short ) arr[4];
            ps[j].rpdh[k].mo = ( unsigned short ) arr[5];
            ps[j].rpdh[k].dy = ( unsigned short ) arr[6];
            ps[j].rpdh[k].hr = ( unsigned short ) arr[7];
            ps[j].rpmi[k]    = ( unsigned short ) arr[8];

            /*
            **  Compute and store the date-time for which this is the hourly report.
            */
            if ( cshrly( &ps[j], k ) != SUCCESS ) continue;

            /*
             **  Check whether temperatures was reported.
            */
            if ( arr0[0] > 0 ) {
                ufbint( &lunit, arr1, &i1, &i2, &iret, tmpstr, strlen( tmpstr ) );
                if ( iret > 0 ) {
                    if ( qual( G_NINT( arr1[2] ) ) != SUCCESS ) continue;
                }
           /*
             **  Store the temperature values.
            */

            printf( "arr= %Lf \n", ( long double ) arr1[0] );
            if ( arr1[0] <  99999.) {
               r8vam = arr1[0] - r8vak;
               if (r8vam < 0 ) {
                  r8vam = -r8vam;
               }
               printf( "r8vam= %Lf  ", ( long double ) r8vam );
               if ( r8vak < 99999.) {
                  if ( r8vam > T25 ) {
                     arr1[0] = BMISS;
                     printf( " setting temp to missing  ");
                  }
               }
             }
             else {
               printf( " missing value of temp here ");
             }

            ps[j].tmdb[k] = arr1[0];
            r8vak = arr1[0];
           }

            ps[j].mxtm06[k] = ps[j].mitm06[k] = BMISS;

            /*
            **  Check whether 6-hr maximum and minimum temperatures were reported.
            */
            if ( arr0[1] > 0 ) {
                ufbint( &lunit, arr1, &i1, &i2, &iret, maxtstr, strlen( maxtstr ) );
                if ( iret > 0 ) {
                    if ( qual( G_NINT( arr1[2] ) ) != SUCCESS ) continue;
                    if ( arr[1] <= 0 ) continue;
                    if ( G_NINT( arr[1] ) == 6 )  ps[j].mxtm06[k] = arr1[0];
                }
            }

            if ( arr0[2] > 0 ) {
                ufbint( &lunit, arr1, &i1, &i2, &iret, mintstr, strlen( mintstr) );
                if ( iret > 0 ) {
                    if ( qual( G_NINT( arr1[2] ) ) != SUCCESS ) continue;
                    if ( arr[1] <= 0 )  continue;
                    if ( G_NINT( arr[1] ) == 6 )  ps[j].mitm06[k] = arr1[0];
                }
            }

           /*
             **  Locate the timezone for this station within the timezone structure.
            */
            for ( j1 = 0; j1 < nsdt; j1++ ) {
                if ( strcmp( ps[j].tz, pt[j1].tz ) == 0 ) break;
            }
            if ( j1 == nsdt ) {
                printf( "ERROR - information for %s is not available in timezone structure.\n", ps[j].tz );
                continue;
            }
            /*
            **  Compute the maximum temperature for this stations, since some statioons report
            *   temperature every 5 minutes, and we still use codes of cpg_tex
            *               */
            if ( arr0[0] > 0 ) {
               for ( j2 = 0; j2 < pt[j1].nmxth; j2++ ) {
                    if ( ( pt[j1].mxth[j2].yr == ps[j].hydh[k].yr ) && ( pt[j1].mxth[j2].mo == ps[j].hydh[k].mo ) &&
                        ( pt[j1].mxth[j2].dy == ps[j].hydh[k].dy ) && ( pt[j1].mxth[j2].hr == ps[j].hydh[k].hr ) ) {
                        if ( ( ! ibfms( &ps[j].tmdb[k] ) ) && ( ps[j].tmdb[k] < mxvtmpk ) )
                            ps[j].maxtemp =  G_MAX( ps[j].maxtemp, ps[j].tmdb[k]);
                        break;
                    }
               }

               for ( j2 = 0; j2 < ptsyh[j1].nmxth; j2++ ) {
                    if ( ( ptsyh[j1].mxth[j2].yr == ps[j].hydh[k].yr ) && ( ptsyh[j1].mxth[j2].mo == ps[j].hydh[k].mo ) &&
                        ( ptsyh[j1].mxth[j2].dy == ps[j].hydh[k].dy ) && ( ptsyh[j1].mxth[j2].hr == ps[j].hydh[k].hr ) ) {
                        if ( ( ! ibfms( &ps[j].tmdb[k] ) ) && ( ps[j].tmdb[k] < mxvtmpk ) )
                            ps[j].maxtemp =  G_MAX( ps[j].maxtemp, ps[j].tmdb[k]);
                        break;
                    }
               }
            /*
            **  At 00Z use  maximum temperature so far for max temp
            */
              if (( strcmp ( cyc, c00  )== 0) && (pt[j1].nmxts_sf > 0)) {
               for ( j2 = 0; j2 < pt[j1].nmxth_sf; j2++ ) {
                    if ( ( pt[j1].mxth_sf[j2].yr == ps[j].hydh[k].yr ) && ( pt[j1].mxth_sf[j2].mo == ps[j].hydh[k].mo ) &&
                        ( pt[j1].mxth_sf[j2].dy == ps[j].hydh[k].dy ) && ( pt[j1].mxth_sf[j2].hr == ps[j].hydh[k].hr ) ) {
                        if ( ( ! ibfms( &ps[j].tmdb[k] ) ) && ( ps[j].tmdb[k] < mxvtmpk ) )
                            ps[j].maxt_sf =  G_MAX( ps[j].maxt_sf, ps[j].tmdb[k]);
                            break;
                    }
               }
                for ( j2 = 0; j2 < ptsyh[j1].nmxth_sf; j2++ ) {
                    if ( ( ptsyh[j1].mxth_sf[j2].yr == ps[j].hydh[k].yr ) && ( ptsyh[j1].mxth_sf[j2].mo == ps[j].hydh[k].mo ) &&
                        ( ptsyh[j1].mxth_sf[j2].dy == ps[j].hydh[k].dy ) && ( ptsyh[j1].mxth_sf[j2].hr == ps[j].hydh[k].hr ) ) {
                        if ( ( ! ibfms( &ps[j].tmdb[k] ) ) && ( ps[j].tmdb[k] < mxvtmpk ) )
                            ps[j].maxt_sf =  G_MAX( ps[j].maxt_sf, ps[j].tmdb[k]);
                        break;
                    }
                }
              }

            /*
            **  Compute the minimum temperature for this station.
            */
               for ( j2 = 0; j2 < pt[j1].nmith; j2++ ) {
                    if ( ( pt[j1].mith[j2].yr == ps[j].hydh[k].yr ) && ( pt[j1].mith[j2].mo == ps[j].hydh[k].mo ) &&
                        ( pt[j1].mith[j2].dy == ps[j].hydh[k].dy ) && ( pt[j1].mith[j2].hr == ps[j].hydh[k].hr ) ) {
                        if ( ( ! ibfms( &ps[j].tmdb[k] ) ) && ( ps[j].tmdb[k]  > mivtmpk ) )
                            ps[j].mintemp =  G_MIN( ps[j].mintemp, ps[j].tmdb[k]);
                        break;
                    }
               }

               for ( j2 = 0; j2 < ptsyh[j1].nmith; j2++ ) {
                    if ( ( ptsyh[j1].mith[j2].yr == ps[j].hydh[k].yr ) && ( ptsyh[j1].mith[j2].mo == ps[j].hydh[k].mo ) &&
                        ( ptsyh[j1].mith[j2].dy == ps[j].hydh[k].dy ) && ( ptsyh[j1].mith[j2].hr == ps[j].hydh[k].hr ) ) {
                        if ( ( ! ibfms( &ps[j].tmdb[k] ) ) && ( ps[j].tmdb[k]  > mivtmpk ) )
                            ps[j].mintemp =  G_MIN( ps[j].mintemp, ps[j].tmdb[k]);
                        break;
                    }
               }

            /*
            **  At 12Z, use the minimum temperature so far for min temp
            */
             if ( strcmp ( cyc, c12  )== 0) {
               for ( j2 = 0; j2 < pt[j1].nmith_sf; j2++ ) {
                    if ( ( pt[j1].mith_sf[j2].yr == ps[j].hydh[k].yr ) && ( pt[j1].mith_sf[j2].mo == ps[j].hydh[k].mo ) &&
                        ( pt[j1].mith_sf[j2].dy == ps[j].hydh[k].dy ) && ( pt[j1].mith_sf[j2].hr == ps[j].hydh[k].hr ) ) {
                        if ( ( ! ibfms( &ps[j].tmdb[k] ) ) && ( ps[j].tmdb[k]  > mivtmpk ) )
                            ps[j].mint_sf =  G_MIN( ps[j].mint_sf, ps[j].tmdb[k]);
                            break;
                    }
               }
                for ( j2 = 0; j2 < ptsyh[j1].nmith_sf; j2++ ) {
                    if ( ( ptsyh[j1].mith_sf[j2].yr == ps[j].hydh[k].yr ) && ( ptsyh[j1].mith_sf[j2].mo == ps[j].hydh[k].mo ) &&
                        ( ptsyh[j1].mith_sf[j2].dy == ps[j].hydh[k].dy ) && ( ptsyh[j1].mith_sf[j2].hr == ps[j].hydh[k].hr ) ) {
                        if ( ( ! ibfms( &ps[j].tmdb[k] ) ) && ( ps[j].tmdb[k]  > mivtmpk ) )
                            ps[j].mint_sf =  G_MIN( ps[j].mint_sf, ps[j].tmdb[k]);
                        break;
                    }
                }
              }

            }

            /*
            **  If the report date-time is within the hour that stored previously,
            **  do not store the report.
            */

            if (k > 1) {
               if ( ps[j].rpdh[k].hr == ps[j].rpdh[k-1].hr ) {
                   if ( ps[j].rpmi[k] < 30 )  continue;
                   if ( ps[j].rpmi[k-1] >= 30 )  continue;
               }

               if ( ps[j].rpdh[k].hr == ps[j].rpdh[k-1].hr + 1 ) {
                   if (( ps[j].rpmi[k] < 30 ) && ( ps[j].rpmi[k-1] >= 30 ))  continue;
               }
            }
            /*
            **  Increment the report counter.
            */
            ps[j].nrpts++;

        }
        closbf( &lunit );

        /*
        **  Determine the maximum and minimum temperatures for the CONUS.  Note that either value
        **  could have possibly occurred at more than one station.
        */
        mxt_conus = mivtmpk;  /* Initialize to minimum possible value. */
        mit_conus = mxvtmpk;  /* Initialize to maximum possible value. */
        nmxt_conus = nmit_conus = 0;
        for ( i = 0; i < nstns; i++ ) {

            #ifdef VERBOSE
            /*
             **  Print out the contents of the station information structure.
            */
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
                if ( ! ibfms( &ps[i].mxtm06[k] ) )  printf( "mxtm06 = %Lf\n", ( long double ) ps[i].mxtm06[k] );
                if ( ! ibfms( &ps[i].mitm06[k] ) )  printf( "mitm06 = %Lf\n", ( long double ) ps[i].mitm06[k] );
            }
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
            r8val =  ps[i].maxtemp;
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

            /*
            **  At 00Z use  maximum temperature so far for max temp
            */
           /*
              At 00Z, there is no maximum temperature so far for sometime zones such as AST4, EST5EDT,
             corret this bug by Guan at 12/23/08
           */
        if (( strcmp ( cyc, c00  )== 0) && (pt[j].nmxts_sf > 0)) {
            r8val = ps[i].maxt_sf;
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

            /*
            **  Compute the minimum temperature for this station.
            */
            r8val = ps[i].mintemp;
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
                  printf("r8val = %f,  r8vak = %f\n", r8val , r8vak);
                  if (nldbhst > nldbhst_total/2 ) r8val = r8vak;
               }
            }

            /*
            **  At 12Z, use the minimum temperature so far for min temp
            */
        if ( strcmp ( cyc, c12  )== 0) {
            r8val = ps[i].mint_sf;
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
 *                      QC proposed by Scott */
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
        }

        /*
        **  Allocate space to store the time-layout information.
        */
        pc2 = NULL;
        pl = NULL;
        pct = NULL;
        plt = NULL;
        pst = NULL;
        psf = NULL;
        ALLOC( 0, pc2, char, ( ( 2 * MXTXSTN ) * MXLLK ), "layout keys" )
        ALLOC( 0, pl, tlinfo, ( 2 * MXTXSTN ), "time-layout information" )
        ALLOC( 0, pct, char, ( 2 * MXTXSTN * MXLLK ), "layout keys" )
        ALLOC( 0, plt, tlinfo, ( 2 * MXTXSTN ), "time-layout information" )
        ALLOC( 0, pst, stinfo, ( 2 * MXTXSTN ), "station information" )
        ALLOC( 0, psf, stinfo, ( 2 * MXTXSTN ), "station information" )

        ma = -1;
        mi = -1;
        i = -1;
        j = -1;
        ma_stat = 0;
        mi_stat = 0;

        /*
        ** Derive existed xmlfile from input xmlfile_temp
        */
        if (reader != NULL) {
           ret = xmlTextReaderRead(reader);
           while (ret == 1) {
            xname = (char *) BAD_CAST "--";
            xname = (char *) xmlTextReaderConstName(reader);
            if (xmlTextReaderNodeType(reader) == XML_ELEMENT_NODE && strcmp(xname, "location-key") == 0 ) {
               i++;
               while (xmlTextReaderRead(reader)) {
                  xvalue = (char *) xmlTextReaderConstValue(reader);
                  if (xmlTextReaderNodeType(reader) == XML_TEXT_NODE && xvalue != NULL) break;
               }
               strcpy (pst[i].stid, xvalue);
               printf(" stid = %s, i= %d\n", xvalue, i);
            }
            if (xmlTextReaderNodeType(reader) == XML_ELEMENT_NODE && strcmp(xname, "city") == 0 ) {
               xvalue = (char *) xmlTextReaderGetAttribute (reader, "state");
               strcpy (pst[i].state, xvalue);
               printf(" state = %s, i= %d\n", xvalue, i);
               while (xmlTextReaderRead(reader)) {
                  xvalue = (char *) xmlTextReaderConstValue(reader);
                  if (xmlTextReaderNodeType(reader) == XML_TEXT_NODE && xvalue != NULL) break;
               }
               strcpy (pst[i].stnnam, xvalue);
               printf(" city = %s, i= %d\n", xvalue, i);
            }
            if (xmlTextReaderNodeType(reader) == XML_ELEMENT_NODE && strcmp(xname, "layout-key") == 0 ) {
               j++;
               while (xmlTextReaderRead(reader)) {
                  xvalue = (char *) xmlTextReaderConstValue(reader);
                  if (xmlTextReaderNodeType(reader) == XML_TEXT_NODE && xvalue != NULL) break;
               }
               printf(" layout = %s, j= %d\n", xvalue, j);
               strcpy (plt[j].layout_key, xvalue);
            }
            if (xmlTextReaderNodeType(reader) == XML_ELEMENT_NODE && strcmp(xname, "start-valid-time") == 0 ) {
               while (xmlTextReaderRead(reader)) {
                  xvalue = (char *) xmlTextReaderConstValue(reader);
                  if (xmlTextReaderNodeType(reader) == XML_TEXT_NODE && xvalue != NULL) break;
               }
               printf(" star = %s, j= %d\n", xvalue, j);
               strcpy (plt[j].svtime, xvalue);
            }
            if (xmlTextReaderNodeType(reader) == XML_ELEMENT_NODE && strcmp(xname, "end-valid-time") == 0 ) {
               while (xmlTextReaderRead(reader)) {
                  xvalue = (char *) xmlTextReaderConstValue(reader);
                  if (xmlTextReaderNodeType(reader) == XML_TEXT_NODE && xvalue != NULL) break;
               }
               printf(" end = %s, j= %d\n", xvalue, j);
               strcpy (plt[j].evtime, xvalue);
            }
            if (xmlTextReaderNodeType(reader) == XML_ELEMENT_NODE && strcmp(xname, "temperature") == 0 ) {
               xvalue = (char *) xmlTextReaderGetAttribute (reader, "type");
               if ( strcmp(xvalue, "maximum") == 0 ) {
                  ma++;
                  xvalue = (char *) xmlTextReaderGetAttribute (reader, "time-layout");
                  strcpy ( &pct[ma*MXLLK], xvalue); 
                  for ( k = 0; k < 7; k++)
                  {
                      xmlTextReaderRead(reader);
                      xvalue = (char *) xmlTextReaderConstValue(reader);
                  }
                  mxt_conust = atoi( xvalue);
                  printf(" max= %d\n", mxt_conust);
                  printf(" i2 max = %hd, %d, %hd \n", nstns, mxt_conust, ma);
               }
               if ( strcmp(xvalue, "minimum") == 0 ) {
                  mi++;
                  xvalue = (char *) xmlTextReaderGetAttribute (reader, "time-layout");
                  strcpy ( &pct[(mi+ma+1)*MXLLK], xvalue);
                  for ( k = 0; k < 7; k++)
                  {
                      xmlTextReaderRead(reader);
                      xvalue = (char *) xmlTextReaderConstValue(reader);
                  }
                  mit_conust = atoi(xvalue);
                  printf(" min = %d \n", mit_conust);
               }
            }

            ret = xmlTextReaderRead(reader);
        }
        xmlFreeTextReader(reader);
        if (ret != 0) {
            fprintf(stderr, "%s : failed to parse\n", filename);
        }
      } else {
        fprintf(stderr, "Unable to open %s\n", filename);
      }
/*
        Compare the maximum (minimum) temperatures from Meta and State Summary
*/
        for ( k = 0; k < ma +1; k++) {
           for (j1 = 0; j1 < j+1; j1++) {
              if ( strcmp(&pct[k*MXLLK], plt[j1].layout_key ) == 0 ) break;
           }
           if ( j1 == j + 1) {
                printf(" ERROR, Can not find layout key: %s\n", &pct[i*MXLLK]);
           } 
           else {
              mak = j1;
           }
        }
        for ( k = 0; k < mi +1; k++) {
           for (j1 = 0; j1 < j+1; j1++) {
              if ( strcmp(&pct[(k+ma +1)*MXLLK], plt[j1].layout_key ) == 0 ) break;
           }
           if ( j1 == j + 1) {
                printf(" ERROR, Can not find layout key: %s\n", &pct[(i+ma +1)*MXLLK]);
           }
           else {
              mik = j1;
           }
        }
        if ( mak > j)  mak = 0;
        if ( mik > j)  mik = j;

        mxt_conusf = G_NINT( K2F(mxt_conus ));
        mit_conusf = G_NINT( K2F(mit_conus ));
        if ( mxt_conust >= mxt_conusf) {
           ma_stat = 1;
           mxt_conusf = mxt_conust; 
           nmxt_conus = ma + 1;
        }
 
        if ( mit_conust <= mit_conusf ) {
           mi_stat = 1;
           mit_conusf = mit_conust;
           nmit_conus = mi + 1;
        }

	/*
	**  Write the location information to the output XML file.
	*/
	for ( i = 0; i < nmxt_conus; i++ ) {
            if ( ma_stat == 1) {
                WRITE_XMLLOC( pst[i].stid, pst[i].stnnam, pst[i].state, "conus" );
                strcpy( &pc2[i*MXLLK], &pct[i*MXLLK]);
            }
            else { 
	        WRITE_XMLLOC( ps[mxt_stnidx[i]].stid, ps[mxt_stnidx[i]].stnnam, ps[mxt_stnidx[i]].state, "conus" )
	    /*
	    **  Compute and store the layout keys.
	    */
           /*
             At 00Z, there is no maximum temperature so far for sometime zones such as AST4, EST5EDT,
             corret this bug by Guan at 09/11
           */
            if (( strcmp ( cyc, c00  )== 0) && (pt[mxt_tzidx[i]].nmxts_sf > 0)) {
	       strcpy( &pc2[i*MXLLK],
		    cstli( pl, &ntl, pt[mxt_tzidx[i]].mxth_sf, pt[mxt_tzidx[i]].nmxth_sf,
				     pt[mxt_tzidx[i]].mxts_sf, pt[mxt_tzidx[i]].nmxts_sf ) );
            }
            else {
               strcpy( &pc2[i*MXLLK],
                    cstli( pl, &ntl, pt[mxt_tzidx[i]].mxth, pt[mxt_tzidx[i]].nmxth,
                                     pt[mxt_tzidx[i]].mxts, pt[mxt_tzidx[i]].nmxts ) );
            }
            }
	}
        if (ma_stat == 1)  {
           ntl = mak + 1; 
           for ( i = 0; i < ntl; i++ ) {
              strcpy (pl[i].layout_key, plt[i].layout_key);
              strcpy (pl[i].svtime, plt[i].svtime);
              strcpy (pl[i].evtime, plt[i].evtime); 
           }
        }
        j2 = ntl;

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
                if ( mi_stat == 1) {
                    WRITE_XMLLOC( pst[i+ma+1].stid, pst[i+ma+1].stnnam, pst[i+ma+1].state, "conus" );
                    strcpy( &pc2[(i+nmxt_conus)*MXLLK], &pct[(i+ma+1)*MXLLK]);
                 }
                 else {
		    WRITE_XMLLOC( ps[mit_stnidx[i]].stid, ps[mit_stnidx[i]].stnnam, ps[mit_stnidx[i]].state, "conus" )
	    /*
	    **  Compute and store the layout keys.
	    */
            if ( strcmp ( cyc, c12  )== 0) {
	      strcpy( &pc2[(i+nmxt_conus)*MXLLK],
		    cstli( pl, &ntl, pt[mit_tzidx[i]].mith_sf, pt[mit_tzidx[i]].nmith_sf,
				     pt[mit_tzidx[i]].mits_sf, pt[mit_tzidx[i]].nmits_sf ) );
            }
            else {
	      strcpy( &pc2[(i+nmxt_conus)*MXLLK],
		    cstli( pl, &ntl, pt[mit_tzidx[i]].mith, pt[mit_tzidx[i]].nmith,
				     pt[mit_tzidx[i]].mits, pt[mit_tzidx[i]].nmits ) );
            }
            }
            }
	}
        if (mi_stat == 1) {
           for ( i = j2; i < j2+mik-mak; i++ ) {
              strcpy (pl[i].layout_key, plt[mak+i-j2+1].layout_key);
              strcpy (pl[i].svtime, plt[mak+i-j2+1].svtime);
              strcpy (pl[i].evtime, plt[mak+i-j2+1].evtime);
           }
           ntl = j2+mik-mak;
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
                if ( ma_stat == 1) {
                    WRITE_XMLSPE( pst[i].stid );
                }
                else {
		    WRITE_XMLSPE( ps[mxt_stnidx[i]].stid )
                }
		WRITE_XMLTEMP( "maximum", &pc2[i*MXLLK], "National High Temperature", mxt_conusf )
	    }
	}
	else {
	    /*
	    **  Write a nil entry.
	    */
	    node2 = xmlNewChild( node1, NULL, XML_CHAR "parameters", NULL );
	    WRITE_XMLTEMP( "maximum", &pc2[i*MXLLK], "National High Temperature", mxt_conusf )
	}

	if ( nmit_conus > 0 ) {
	    /*
	    **  Write the minimum CONUS temperature to the output XML file.
	    */
	    for ( i = 0; i < nmit_conus; i++ ) {
                if ( mi_stat == 1) {
                    WRITE_XMLSPE( pst[i+ma+1].stid )
                }
                else {
		    WRITE_XMLSPE( ps[mit_stnidx[i]].stid )
                }
		WRITE_XMLTEMP( "minimum", &pc2[(i+nmxt_conus)*MXLLK], "National Low Temperature", mit_conusf )
	    }
	}
	else {
	    /*
	    **  Write a nil entry.
	    */
	    node2 = xmlNewChild( node1, NULL, XML_CHAR "parameters", NULL );
	    WRITE_XMLTEMP( "minimum", &pc2[i*MXLLK], "National Low Temperature", mit_conusf )
	}

	/*
	**  Finish writing the output XML file.
	*/
	xmlSaveFormatFile( argv[5], doc, 1 );

	/*
	**  Free all allocated memory.
	*/
/*
	G_FREE( ps, stinfo )
	G_FREE( pt, tzinfo )
        G_FREE( ptsyh, tzinfo )
	G_FREE( pl, tlinfo )
	G_FREE( pc, char )
        G_FREE( pst, stinfo )
        G_FREE( psf, stinfo )
        G_FREE( plt, tlinfo )
        G_FREE( pct, char )
	xmlFreeDoc( doc );
	xmlCleanupParser( );
*/
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

