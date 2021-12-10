SHELL =  /bin/sh

OBJS =  SCS_Obs_definitions.o cmpstra.o copenbf.o cshrly.o cstli.o fdate.o cpg.o qual.o ibfms.o
 
#FC =  ifort 
FC =  ftn 
FFLAGS =  -g  

#CC =  icc 
CC =  cc 
#OS_LIB = /usr/lib64
#OS_INC = /usr/include
OS_INC = $(XML2_INC)
#GEMINC = /gpfs/dell1/ptmp/Alexander.Richert/gempak/nawips/gempak/include/
CFLAGS =  -g -DUNDERSCORE -DVERBOSE -D$(TARGET_UC) -I$(GEMINC) -I$(OS_INC)/libxml2

LDFLAGS =  -nofor-main  

#LIBS =  -L/nwprod/lib $(GEMLIB) $(CGEMLIB) -L$(OS_LIB) -lxml2 -lz -lpthread
#export OS_LIB=/nwprod/gempak/nawips_intel/os/linux2.6.32_x86_64/lib
#LIBS =  -L/nwprod/lib -L$(OS_LIB) -lgemlib -lxml2 -liconv -lz -lpthread -lbufr_4_64 -lgemlib
LIBS =  -L$(OS_LIB) -lxml2 $(Z_LIBa) -lpthread $(BUFR_LIB4) $(OS_LIB)/libgemlib.a /usr/lib64/libgfortran.so.4

OS_INC = $(LIBXML2_INC)
LIBS =  -L$(OS_LIB) $(LIBXML2_LIB)  -lpthread $(BUFR_LIB4) $(OS_LIB)/libgemlib.a $(Z_LIB) /usr/lib64/liblzma.so.5
LIBS =  -L$(OS_LIB) $(LIBXML2_LIB)  -liconv $(BUFR_LIB4) $(OS_LIB)/libgemlib.a $(Z_LIB) /usr/lib64/liblzma.so.5

#  The default rule is to build both $(TARGET) executables.
all:	MsseT

MsseT:
	sh -c "export TARGET=msset; make build"
	make clean

#  Note that the below rule will force make to rebuild all $(OBJS) files.  This is necessary
#  because the $(OBJS) files are specific to each $(TARGET) executable, based upon the
#  corresponding value of $(TARGET_UC) which is used as a conditional compilation macro.
build:	clean
	sh -c " source ../../versions/build.ver; module purge; module load envvar/1.0 \
            module load PrgEnv-intel/$(PrgEnv_intel_ver) intel/$(intel_ver) craype/$(craype_ver) zlib/$(zlib_ver) libxml2/$(libxml2_ver) bufr/$(bufr_ver); \
            module load gempak/$(gempak_ver) ; \
	    module list; \
	    export TARGET_UC=`echo $(TARGET) | tr '[a-z]' '[A-Z]'`; \
	    make $(TARGET)"

$(TARGET):  $(OBJS)
	$(FC) $(LDFLAGS) -o cpg_$(@) $(OBJS) $(LIBS)

clean:
	rm -f *.o

clobber:    clean
	rm -f cpg_msset
