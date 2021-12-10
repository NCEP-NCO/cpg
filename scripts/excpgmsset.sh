#!/bin/sh -vx

###########################################################################
#  Confirm that the required environment variable was set to a valid value.
###########################################################################

export PRODUCT=tex

if [ -z "$PRODUCT" ]
then
    echo 'ERROR - Environment variable $PRODUCT must be defined!'
    exit -1
elif [ $PRODUCT = "obx" ]
then
    title="Selected US Observations"
    subctr="NCEP Central Operations"
    rffq=PT1H
else
    title="US High/Low Temperature Extremes"
    subctr="Hydrometeorological Prediction Center"
    rffq=PT6H
fi

###########################################################################
#  Set default values for any optional environment variables which were undefined.
###########################################################################

[ -z "$RUNDT" ] && RUNDT=`$NDATE`
[ -z "$MODE" ] && MODE="test"

###########################################################################
#  For T=$RUNDT, we need to dump the previous 38 hours worth of METAR data in
#  order to cover all possible time zones and UTC hours.  Doing an actual dump
#  (versus just reading directly from the applicable METAR tanks) will ensure
#  that duplicates are filtered out.  However, since any single dump can read
#  from at most two METAR tanks, then we'll need to run two separate dumps:
#  	1 - to cover from T-38 hours up to T-18.5 hours
#  	2 - to cover from T-18.5 hours up to T+1 hour
#  The output from these two dumps will then be concatenated together into a
#  single BUFR file to be passed into the program.
###########################################################################
center1=`$NDATE -29 $RUNDT`.75
center2=`$NDATE -9 $RUNDT`.25

for center in $center1 $center2
do
    $DUMPJB $center 9.75 metar
    for type in ibm out
    do
        cat $DATA/metar.$type >> $DATA/metar.$type.all
    done
done

###########################################################################
#  Determine how many stations to read from the stations file.
###########################################################################

if [ $PRODUCT = "obx" ]
then
    usg=1
else
    usg=2
fi
nstns=`cat $FIXcpg/cpg_stns | grep -v "^#" | cut -c63 | egrep "($usg|3)" | wc -l`

###########################################################################
#  Initialize the output XML file.
###########################################################################

xmlfile=$DATA/xmldoc_temp

cat << XMLHEAD > $xmlfile
<?xml version="1.0"?>
<dwml version="1.0" xmlns:xsd="http://www.w3.org/2001/XMLSchema"
  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
  xsi:noNamespaceSchemaLocation="http://www.nws.noaa.gov/forecasts/xml/DWMLgen/schema/DWML.xsd">
  <head>
    <product concise-name="tabular-digital" operational-mode="$MODE">
      <title>$title</title>
      <field>meteorological</field>
      <category>observations</category>
      <creation-date refresh-frequency="$rffq">`date -u +%Y-%m-%dT%TZ`</creation-date>
    </product>
    <source>
      <more-information>http://www.nws.noaa.gov/forecasts/xml/</more-information>
      <production-center>National Centers For Environmental Prediction
        <sub-center>$subctr</sub-center>
      </production-center>
      <disclaimer>http://www.nws.noaa.gov/disclaimer.html</disclaimer>
      <credit>http://www.weather.gov/</credit>
      <credit-logo>http://www.weather.gov/images/xml_logo.gif</credit-logo>
      <feedback>http://www.weather.gov/survey/nws-survey.php?code=tpex</feedback>
    </source>
  </head>
XMLHEAD

###########################################################################
#  Call the program.
###########################################################################

export pgm=cpg_$PRODUCT
prep_step
startmsg

$EXECcpg/cpg_$PRODUCT $RUNDT $DATA/metar.ibm.all $FIXcpg/cpg_stns $nstns $DATA/xmldata \
	>> $pgmout 2> errfile

export err=$?; err_chk

###########################################################################
#   Append the data to the output XML file.
###########################################################################

cat $DATA/xmldata | grep -v "xml version" >> $xmlfile
rm -f $DATA/xmldata

###########################################################################
#   Add the closing tag.
###########################################################################

echo "</dwml>" >> $xmlfile

###########################################################################
#   Run cpg_msset for state summary.
###########################################################################

export PRODUCT=msset
for center in $center1 $center2
do
    $DUMPJB $center 9.75 shefcm coopal
    for type in ibm out
    do
        cat $DATA/shefcm.$type >> $DATA/shef.$type.all
        cat $DATA/coopal.$type >> $DATA/shef.$type.all
    done
done

###########################################################################
#  Determine how many stations to read from the stations file.
###########################################################################

if [ $PRODUCT = "obx" ]
then
    usg=1
else
    usg=2
fi
nstns=`cat $FIXcpg/cpg_ss_stns | grep -v "^#" | cut -c63 | egrep "($usg|3)" | wc -l`

###########################################################################
#  Initialize the output XML file.
###########################################################################

xmlfile=$DATA/xmldoc

cat << XMLHEAD > $xmlfile
<?xml version="1.0"?>
<dwml version="1.0" xmlns:xsd="http://www.w3.org/2001/XMLSchema"
  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
  xsi:noNamespaceSchemaLocation="http://www.nws.noaa.gov/forecasts/xml/DWMLgen/schema/DWML.xsd">
  <head>
    <product concise-name="tabular-digital" operational-mode="$MODE">
      <title>$title</title>
      <field>meteorological</field>
      <category>observations</category>
      <creation-date refresh-frequency="$rffq">`date -u +%Y-%m-%dT%TZ`</creation-date>
    </product>
    <source>
      <more-information>http://www.nws.noaa.gov/forecasts/xml/</more-information>
      <production-center>National Centers For Environmental Prediction
        <sub-center>$subctr</sub-center>
      </production-center>
      <disclaimer>http://www.nws.noaa.gov/disclaimer.html</disclaimer>
      <credit>http://www.weather.gov/</credit>
      <credit-logo>http://www.weather.gov/images/xml_logo.gif</credit-logo>
      <feedback>http://www.weather.gov/survey/nws-survey.php?code=tpex</feedback>
    </source>
  </head>
XMLHEAD

###########################################################################
#  Call the program.
###########################################################################

export pgm=cpg_$PRODUCT
prep_step
startmsg

$EXECcpg/cpg_$PRODUCT $RUNDT $DATA/shef.ibm.all $FIXcpg/cpg_ss_stns $nstns $DATA/xmldata $DATA/xmldoc_temp \
        >> $pgmout 2> errfile

export err=$?; err_chk

###########################################################################
#   Append the data to the output XML file.
###########################################################################

cat $DATA/xmldata | grep -v "xml version" >> $xmlfile

###########################################################################
#   Add the closing tag.
###########################################################################

echo "</dwml>" >> $xmlfile

###########################################################################
#   Send the output XML file to the required destination.
###########################################################################

if [ $PRODUCT = "obx" ]
then
    # Prepend a bulletin header and send to TOC (via NTC).
    $USHcpg/make_NTC_file.pl RXUS30 KWNO $RUNDT "XOBUS " $xmlfile $COMOUTwmo/$PRODUCT.$RUNDT.xml
    dbntag1=NTC_LOW
else
    # Send to HPC, who will review/edit and then forward to TOC (via AWIPS).
    mv $xmlfile $COMOUT/comb.$RUNDT.xml
    dbntag1=BULS_TEXT
    dbntag2=CPGTEX
fi

if [ $SENDDBN = "YES" ]
then
    $DBNROOT/bin/dbn_alert $dbntag1 $dbntag2 $job $COMOUT/comb.$RUNDT.xml
fi
