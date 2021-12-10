set -x

source ../versions/build.ver
module purge

module load envvar/${envvar_ver}
module load intel/${intel_ver}
module load PrgEnv-intel/${PrgEnvintel_ver}
module load craype/${craype_ver}
module load zlib/${zlib_ver}
module load libxml2/${libxml2_ver}
module load bufr/${bufr_ver}
module load gempak/${gempak_ver}

module list

sleep 1

BASE=`pwd`

if [ -d $BASE/../exec ]; then
  rm -f $BASE/../exec/*
else
  mkdir $BASE/../exec
fi


##############################

cd ${BASE}/cpg.fd
make clean
make
cp cpg_obx $BASE/../exec
cp cpg_tex $BASE/../exec

cd ${BASE}/cpgmsset.fd
make clean
cp cpg_msset $BASE/../exec

