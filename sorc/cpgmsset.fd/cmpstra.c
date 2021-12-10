/************************************************************************
 * cmpstra								*
 *									*
 * Defines the algorithm to be used by the binary search function	*
 * bsearch when comparing two strings.					*
 *									*
 * cmpstra ( *pf1, *pf2 )						*
 *									*
 * Input parameters:							*
 *	*pf1		char		First string for comparison	*
 *	*pf2		char		Second string for comparison	*
 *                                                                      *
 * Output parameters:							*
 *	cmpstra		int		-1 = first string is lexically	*
 *					     less than second string	*
 *					 0 = strings are lexically equal*
 *					 1 = first string is lexically	*
 *					     greater than second string	*
 **                                                                     *
 *  Log:                                                                *
 *  J. Ator/NCEP         11/06                                          *
 ***********************************************************************/

#include "cpg.h"

int cmpstra( const char *pf1, const char *pf2 )
{
	return strcmp( pf1, pf2 );
}
