/************************************************************************
 * fdate								*
 *									*
 * This function searches for a	given hourly date-time within the	*
 * station information structure for a particular station.		*
 *									*
 * fdate ( dh, st, *pidx )						*
 *									*
 * Input parameters:							*
 *	dh		dhinfo		Hourly date-time		*
 *	st		stinfo		Station information structure	*
 *                                                                      *
 * Output parameters:							*
 *	*pidx		unsigned short	Index to matching hourly	*
 *					date-time within st		*
 *	fdate		int		 0 = normal return		*
 *					-1 = dh was not found within st	*
 **                                                                     *
 *  Log:                                                                *
 *  J. Ator/NCEP         11/06                                          *
 ***********************************************************************/

#include "cpg.h"

int fdate( dhinfo dh, stinfo st, unsigned short *pidx ) {

	for ( *pidx = 0; *pidx < st.nrpts; (*pidx)++ ) {
	    if ( ( st.hydh[*pidx].yr == dh.yr ) && ( st.hydh[*pidx].mo == dh.mo ) &&
		 ( st.hydh[*pidx].dy == dh.dy ) && ( st.hydh[*pidx].hr == dh.hr ) ) break;
	}
	if ( *pidx == st.nrpts ) {
	    printf( "Warning - no available data at date-time %u %02u %02u %02u for station %s.\n",
		dh.yr, dh.mo, dh.dy, dh.hr, st.stid );
	    return FAILURE;
	}

	return SUCCESS;
}
