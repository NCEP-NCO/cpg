/************************************************************************
 * cstli								*
 *									*
 * This function computes and stores time-layout information for a	*
 * given pair of hourly and synoptic date-time arrays.			*
 *									*
 * cstli ( *pl, *n, pdhh[], ndhh, pdhs[], ndhs )			*
 *									*
 * Input parameters:							*
 *	pdhh[]		dhinfo		Hourly date-time array		*
 *	ndhh		unsigned short	Number of hourly date-times	*
 *	pdhs[]		dhinfo		Synoptic date-time array	*
 *	ndhs		unsigned short	Number of synoptic date-times	*
 *									*
 * Input and output parameters:						*
 *	*pl		tlinfo		Time-layout information		*
 *					structure			*
 *	*n		unsigned short	Number of time-layouts stored	*
 *					within tlinfo			*
 *                                                                      *
 * Output parameters:							*
 *	*cstli		char		Layout key corresponding to	*
 *					the date-times within pdhh[]	*
 *					and pdhs[]			*
 **                                                                     *
 *  Log:                                                                *
 *  J. Ator/NCEP         11/06                                          *
 ***********************************************************************/

#include "cpg.h"

void cpy2idt( dhinfo dh, f77int *idt)
    {
        idt[0] = ( f77int ) dh.yr;
        idt[1] = ( f77int ) dh.mo;
        idt[2] = ( f77int ) dh.dy;
        idt[3] = ( f77int ) dh.hr;
        idt[4] = ( f77int ) 0;
    }

void cnv2vtm(f77int *idt, char * vtm)
    { 
        sprintf( vtm, "%.4ld-%.2ld-%.2ldT%.2ld:00:00", (long) idt[0], (long) idt[1], (long) idt[2], (long) idt[3]);
    }

int  cnv2int( f77int *idt)
    {
        return 1000000 * ( idt[0] ) + ( 10000 * ( idt[1] ) ) + ( 100 * ( idt[2] ) ) + idt[3];
    }


char *cstli( tlinfo *pl, unsigned short *n,
	     dhinfo *pdhh, unsigned short ndhh, dhinfo *pdhs, unsigned short ndhs ) {

	/*
	**  Function-like macro to create a GEMPAK date array from a dhinfo date-time.
	#define cpy2idt( dh, idt )  \
	    idt ## [0] = ( f77int ) dh ## .yr; \
	    idt ## [1] = ( f77int ) dh ## .mo; \
	    idt ## [2] = ( f77int ) dh ## .dy; \
	    idt ## [3] = ( f77int ) dh ## .hr; \
	    idt ## [4] = ( f77int ) 0;

	**  Function-like macro to create a DWML valid time from a GEMPAK date array.
	#define cnv2vtm( idt, vtm )  \
	    sprintf( vtm, "%.4ld-%.2ld-%.2ldT%.2ld:00:00", \
			( long ) idt ## [0], ( long ) idt ## [1], ( long ) idt ## [2], ( long ) idt ## [3] );

	**  Function-like macro to create an integer YYYYMMDDHH date-time from a GEMPAK date array.
	#define cnv2int( idt ) \
	    ( ( 1000000 * ( idt ## [0] ) ) + ( 10000 * ( idt ## [1] ) ) + ( 100 * ( idt ## [2] ) ) + ( idt ## [3] ) )
        */

	f77int idtar1[5], idtar2[5], idtar3[5];
	f77int nmi, iret;
	f77int i360 = 360;

	f77int *p_earliest = idtar1, *p_latest = idtar2, *p_work = idtar3;

	unsigned short i, j, nhr;

	char workvs[MXLVT], workve[MXLVT];

/*---------------------------------------------------------------------*/

	/*
	**  Initialize the work string to NULL.
	*/
	worklk[0] = '\0';
 
	/*
	**  Determine the earliest and latest out of all of the hourly and synoptic date-times.
	*/
	if ( ndhs == 0 ) {
	    if ( ndhh == 0 ) return worklk;
	    cpy2idt( pdhh[0], p_earliest );
	    cpy2idt( pdhh[ndhh-1], p_latest );
	}
	else {
	    if ( ndhh == 0 ) {
		cpy2idt( pdhs[0], p_earliest );
		ti_subm( p_earliest, &i360, p_earliest, &iret );
		CHK_RTN( iret, ti_subm, worklk )
		cpy2idt( pdhs[ndhs-1], p_latest );
	    }
	    else {
		cpy2idt( pdhs[0], p_earliest );
		ti_subm( p_earliest, &i360, p_earliest, &iret );
		CHK_RTN( iret, ti_subm, worklk )
	        cpy2idt( pdhh[0], p_latest );
		if ( cnv2int( p_earliest ) > cnv2int( p_latest ) ) {
		    p_earliest = p_latest;
		    p_latest = idtar1;
		}
		cpy2idt( pdhh[ndhh-1], p_latest );
		cpy2idt( pdhs[ndhs-1], p_work );
		if ( cnv2int( p_work ) > cnv2int( p_latest ) ) {
		    p_latest = p_work;
		}
	    }
	}

	/*
	**  Create the start and end valid time strings.
	*/
	cnv2vtm( p_earliest, workvs );
	cnv2vtm( p_latest, workve );

	/*
	**  Check whether this time-layout is already stored within the time-layout information
	**  structure.  If so, we can re-use it, and then we're done!
	*/
	for ( i = 0; i < *n; i++ ) {
	    if ( ( strcmp( workvs, pl[i].svtime ) == 0 ) &&
		 ( strcmp( workve, pl[i].evtime ) == 0 ) )
		return pl[i].layout_key;
	}

	/*
	**  We need to create and store a new entry within the time-layout information structure.
	*/
	if ( *n >= MXLK ) {
	    printf( "ERROR - number of layout-keys exceeded MXLK.\n" );
	    return worklk;
	}

	/*
	**  Compute the difference (in hours) between the earliest and latest date-times.
	*/
	ti_mdif( p_latest, p_earliest, &nmi, &iret );
	CHK_RTN( iret, ti_mdif, worklk )
	nhr = nmi / 60;

	/*
	**  Create a unique layout key.
	*/
	i = 0;
	sprintf( worklk, "k-p%uh-n1-%u", nhr, ++i );
	while ( G_TRUE ) {
	    for ( j = 0; j < *n; j++ ) {
		if ( strcmp( worklk, pl[j].layout_key ) == 0 ) break;
	    }
	    if ( j == *n ) break;
	    sprintf( worklk, "k-p%uh-n1-%u", nhr, ++i );
	}

	/*
	**  Store the results in the time-layout information structure and return.
	*/
	strcpy( pl[*n].layout_key, worklk );
	strcpy( pl[*n].svtime, workvs );
	strcpy( pl[*n].evtime, workve );
	(*n)++;
	
	return worklk;
}
