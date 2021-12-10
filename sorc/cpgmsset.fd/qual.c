/************************************************************************
 * qual 								*
 *									*
 * Using SHEF data qualifier flag to discard bad data	 		*
 *									*
 * qual( shql )								*
 *									*
 * Input parameters:							*
 *	shql		int		SHEF data qualifier flag	*
 *                                                                      *
 * Output parameters:							*
 *	qual		int		 0 = normal return		*
 *					-1 = bad data			*
 **                                                                     *
 *  Log:                                                                *
 *  S. Guan/NCEP         04/11                                          *
 ***********************************************************************/
#include "cpg.h"

int qual( int shql ) {
    switch (shql)
   {
        case 2:
        case 6:
        case 17:
        case 18: return FAILURE;
        default: return SUCCESS;
   }
}

/*
0-33-231 - SHQL
SHEF data qualifier flag


    CODE FIGURE   DESCRIPTION

         0-1      Reserved
          2       Bad, manual QC
         3-4      Reserved
          5       Estimated
          6       Questionable, flagged by sensor or telemetry
          7       Good, manual QC
        8-12      Reserved
         13       Good, manual Edit
       14-15      Reserved
         16       Good, passed by level 1, level 2 and level 3
         17       Questionable, flagged by level 2 and level level 3
         18       Bad, rejected by level 1
         19       Good, screened by level 1
         20       Trigger for database function (N/A within NCEP)
         21       Reserved
         22       Good, verified by level 1 and level 2
       23-25      Reserved
         26       No QC performed
       27-30      Reserved
         31       Missing value


    where level 1 = validity checks (e.g. range checking)
          level 2 = internal and temporal consistency checks (e.g. rate of change checks)
          level 3 = spatial consistency checks (e.g. temperature and precipitation field outliers)

*/
