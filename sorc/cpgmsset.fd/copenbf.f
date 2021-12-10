	SUBROUTINE COPENBF ( BFILE, LUNIT )

C************************************************************************
C* COPENBF								*
C*									*
C* This subroutine opens a BUFR file for reading.			*
C*									*
C* COPENBF  ( BFILE, LUNIT )						*
C*									*
C* Input parameters:							*
C*	BFILE		CHAR*		BUFR file. 			*
C*	LUNIT		INTEGER		FORTRAN logical unit number to	*
C*					assign to BFILE.		*
C**									*
C* Log:									*
C* J. Ator/NCEP		11/06						*
C************************************************************************

	CHARACTER*(*)	BFILE

	OPEN ( UNIT = LUNIT, FILE = BFILE, FORM = 'UNFORMATTED' )

	CALL OPENBF ( LUNIT, 'IN', LUNIT )

	RETURN
	END
