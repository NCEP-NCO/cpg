/************************************************************************
 * cshrly								*
 *									*
 * Given a report date-time stored within a station information		*
 * structure, this function computes the corresponding hourly date-time	*
 * and stores it within the same structure.				*
 *									*
 * cshrly ( *ps, k )							*
 *									*
 * Input parameters:							*
 *	*ps		stinfo		Station information structure	*
 *	k		unsigned short	Station report index		*
 *                                                                      *
 * Output parameters:							*
 *	cshrly		int		 0 = normal return		*
 *					-1 = the given report isn't	*
 *					     an hourly report		*
 **                                                                     *
 *  Log:                                                                *
 *  J. Ator/NCEP         11/06                                          *
 *  S. Guan/NCEP         06/11            Expand [45, 15] to [30, 30)   *
 *                                        for SHEF data                 *
 ***********************************************************************/

#include "cpg.h"

int cshrly( stinfo *ps, unsigned short k ) {

	f77int idtarr[5], iret;
	f77int i60 = 60;

	if ( ( ps->rpmi[k] >= 30 ) && ( ps->rpmi[k] <= 59 ) ) {
	    /*
	    **  This is the next hour's hourly report.
	    */
	    idtarr[0] = ( f77int ) ps->rpdh[k].yr;
	    idtarr[1] = ( f77int ) ps->rpdh[k].mo;
	    idtarr[2] = ( f77int ) ps->rpdh[k].dy;
	    idtarr[3] = ( f77int ) ps->rpdh[k].hr;
	    idtarr[4] = 0;
	    ti_addm( idtarr, &i60, idtarr, &iret );
	    CHK_RTN( iret, ti_addm, FAILURE )
	    ps->hydh[k].yr = ( unsigned short ) idtarr[0];
	    ps->hydh[k].mo = ( unsigned short ) idtarr[1];
	    ps->hydh[k].dy = ( unsigned short ) idtarr[2];
	    ps->hydh[k].hr = ( unsigned short ) idtarr[3];
	}
	else if ( ( (int) ps->rpmi[k] >= 0 ) && ( ps->rpmi[k] < 30 ) ) {
	    /*
	    **  This is the current hour's hourly report.
	    */
	    ps->hydh[k].yr = ps->rpdh[k].yr;
	    ps->hydh[k].mo = ps->rpdh[k].mo;
	    ps->hydh[k].dy = ps->rpdh[k].dy;
	    ps->hydh[k].hr = ps->rpdh[k].hr;
	}
	else {
	    /*
	    **  This isn't an hourly report.
	    */
	    return FAILURE;
	}

	return SUCCESS;
}
