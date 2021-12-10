SHELL =  /bin/sh
OBJS =  SCS_Obs_definitions.o cmpstra.o copenbf.o cshrly.o cstli.o fdate.o ibfms.o cpg.o
FC =  ftn 
FFLAGS = -assume no2underscores

CC =  cc 
OS_INC = $(XML2_INC)
CFLAGS =  -DUNDERSCORE -DVERBOSE -D$(TARGET_UC) -I$(GEMINC) -I$(OS_INC)/libxml2
LDFLAGS =  -nofor-main   

LIBS =  -L$(OS_LIB) -lxml2 $(Z_LIBa) -lpthread $(BUFR_LIB4) $(OS_LIB)/libgemlib.a /usr/lib64/libgfortran.so.4

OS_INC = $(LIBXML2_INC)
LIBS =  -L$(OS_LIB) $(LIBXML2_LIB)  -lpthread $(BUFR_LIB4) $(OS_LIB)/libgemlib.a $(Z_LIB) /usr/lib64/liblzma.so.5
LIBS =  -L$(OS_LIB) $(LIBXML2_LIB)  -liconv $(BUFR_LIB4) $(OS_LIB)/libgemlib.a $(Z_LIB) /usr/lib64/liblzma.so.5

#  The default rule is to build both $(TARGET) executables.
all:	ObX TeX

ObX:
	sh -c "export TARGET=obx; make build"
	make clean

TeX:
	sh -c "export TARGET=tex; make build"
	make clean

#  Note that the below rule will force make to rebuild all $(OBJS) files.  This is necessary
#  because the $(OBJS) files are specific to each $(TARGET) executable, based upon the
#  corresponding value of $(TARGET_UC) which is used as a conditional compilation macro.
#
build:	clean
#	sh -c " module load ips/19.0.5.281 impi/19.0.5 bufr/11.3.0 zlib/1.2.11 gempak/7.3.3 ; \
#
	sh -c " source ../../versions/build.ver; module purge; module load envvar/1.0 \
            module load PrgEnv-intel/$(PrgEnv_intel_ver) intel/$(intel_ver) craype/$(craype_ver) zlib/$(zlib_ver) libxml2/$(libxml2_ver) bufr/$(bufr_ver); \
            module load gempak/$(gempak_ver) ; \
	    module list; \
	    export TARGET_UC=`echo $(TARGET) | tr '[a-z]' '[A-Z]'`; \
	    make $(TARGET)"

$(TARGET):  $(OBJS)
	$(FC) $(LDFLAGS) -o cpg_$(@) $(OBJS) $(LIBS)

copenbf.o: copenbf.f
	@echo " "
	@echo "  Compiling the waitfor fortran module...."
	$(FC) $(FFLAGS) -c copenbf.f -o copenbf.o

ibfms.o: ibfms.f
	@echo " "
	@echo "  Compiling the waitfor fortran module...."
	$(FC) $(FFLAGS) -c ibfms.f -o ibfms.o

clean:
	rm -f *.o

clobber:    clean
	rm -f cpg_obx cpg_tex
