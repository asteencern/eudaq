#!/bin/bash

echo "Script to generate the EUDAQ configuration files for long LED runs"
echo "usage: source create_from1file.sh"
echo "  -l(--labviewConffolder) LabviewPCConfigurationFolder"
echo "  -v(--v0) V0"
echo "  -n(--nvoltages) NVoltages"
echo "  -s(--step) dmv_step"
echo "  -r(--readoutcycles) EVENTS"
echo "  -e(--eudaqfolder) EUDAQdatafolder"
echo "  -c(--createDatafolder) y/n"
echo "requires the existence of:"
echo "  a) the xxLED.conf file, see the end of the script to see how it should look"
echo "  b) the longLED_wind/LED_00.ini file, see the end of the script to see how it should look"

LABVFolder="SHORT"
NV="3"
V0="5400"
dmV="300"
EVENTS="32000"
EUDAQ_FOLDER="LED"
MAKE_DIR="n"

echo "  "
echo " default values "
echo "  "
echo "--labviewConffolder=${LABVFolder}, it assumes: F:\\LEDEUDAQ\\+labviewConffolder+\\LED_yy.ini"
echo "--v0=${NV}"
echo "--nvoltages=${V0}      --> (file 0= pedestal, file 1= voltage 1...)"
echo "--step=${dmV}      --> (increase of mV)"
echo "--readoutcycles=${EVENTS}      --> (increase of mV)"
echo "--eudaqfolder=${EUDAQ_FOLDER=}, it assumes LED/+eudaqfolder+/Run_"
echo "--createDatafolder=${MAKE_DIR}, only for eudaq pc"

for i in "$@"
do
    case $i in
	-l=*|--labviewConffolder=*)
	    LABVFolder="${i#*=}"
	    ;;
	-v=*|--v0=*)
	    V0="${i#*=}"
	    ;;
	-n=*|--nvoltages=*)
	    NV="${i#*=}"
	    ;;
	-r=*|--readoutcycles=*)
	    dmV="${i#*=}"
	    ;;
	-s=*|--step=*)
	    ="${i#*=}"
	    ;;
	-e=*|--eudaqfolder=*)
	    EUDAQ_FOLDER="${i#*=}"
	    ;;
	-c=*|--createfolder=*)
	    MAKE_DIR="${i#*=}"
	    ;;
	*)
	    ;;
    esac
done

echo " "
echo LABVFolder = ${LABVFolder} 
echo V0 = ${V0}
echo NV = ${NV}
echo dmV = ${dmV}
echo EUDAQ_FOLDER = ${EUDAQ_FOLDER}
echo MAKE_DIR = ${MAKE_DIR}


# if [ "${MAKE_DIR}" == "y" ]
# then
#     mkdir /home/calice/Desktop/EUDAQ1.6/data/LED/${EUDAQ_FOLDER}
#     echo "Creating folder: /home/calice/Desktop/EUDAQ1.6/data/LED/${EUDAQ_FOLDER}"
# else
#     if [ "${MAKE_DIR}" != "n" ] 
#     then
# 	echo "wrong option, choose --createDatafolder=y or --createDatafolder=n"
# 	break
#     fi
# fi

#create file LED_00.conf, pedestal run
#echo "Creating Pedestal file, LED_00.conf"
# /bin/cp -f $PWD/xxLED.conf $PWD/LED_000.conf
# sed -i "s/GENERATE_EVENTS/${EVENTS}/g" $PWD/LED_000.conf
# sed -i "s/GENERATE_LABVIEW_FOLDER/${LABVFolder}/g" $PWD/LED_000.conf
# # sed -i "s/eudaqfold/${EUDAQ_FOLDER}/g" $PWD/LED_000.conf
# sed -i "s/GENERATE_FILE/${PWD}/LED_001.conf/g" $PWD/LED_000.conf

#create the others, from minimum VLEDbegin to VLEDend 
#files LED_yy.conf

COUNTERI=0
COUNTERJ=1

for i in `seq 0 $((NV + 0))`
do
    echo "Generating configuration file ${i}"
    CNT2DI=`printf "%03d" ${i}`
    CNT2DJ=`printf "%03d" $((i + 1))`
    /bin/cp -f $PWD/xxLED.conf $PWD/LED_$CNT2DI.conf
    sed -i "s/GENERATE_EVENTS/${EVENTS}/g" $PWD/LED_${CNT2DI}.conf
    sed -i "s/GENERATE_LABVIEW_FOLDER/${LABVFolder}/g" $PWD/LED_${CNT2DI}.conf
    # sed -i "s/eudaqfold/${EUDAQ_FOLDER}/g" $PWD/LED_000.conf
    sed -i "s#GENERATE_FILE#$PWD/LED_${CNT2DJ}.conf#g" $PWD/LED_${CNT2DI}.conf
    # let COUNTERI=COUNTERI + 1
done

# while [ $COUNTERI -lt ${NV} ]; 
# do  
#     CNT2DI=`printf "%03d" ${COUNTERI}`
#     # echo "2digit counter I: ${CNT2DI}"
#     CNT2DJ=`printf "%03d" ${COUNTERJ}`
#     echo "Creating File-J: ${CNT2DJ}"
#     /bin/cp -f $PWD/LED_$CNT2DI.conf $PWD/test.conf
#     sed -i "s/LED_$CNT2DI/LED_$CNT2DJ/g" $PWD/test.conf
#     /bin/cp -f $PWD/test.conf $PWD/LED_$CNT2DJ.conf
#     let COUNTERI=COUNTERI+1
#     let COUNTERJ=COUNTERJ+1
# done

# sed -i "s/NextConfigFileOnFileLimit/\#NextConfigFileOnFileLimit/g" $PWD/LED_$CNT2DJ.conf

rm $PWD/test.conf

## Create Windows, configuration files
echo "Create Windows, configuration files"

cd ini

COUNTERI=1
COUNTERJ=2
LEDV=${V0}
LEDV2=${V0}

#prepare the first voltage file LED_01.ini
/bin/cp -f $PWD/LED_000.ini $PWD/test.ini
sed -i "s/voltage\=0/voltage\=$LEDV2/g" $PWD/test.ini
/bin/cp -f $PWD/test.ini $PWD/LED_001.ini

while [ $COUNTERI -lt ${NV} ]; 
do  
    let LEDV2=LEDV+${dmV}
    CNT2DI=`seq -f "%03.0f" ${COUNTERI} ${COUNTERI}`
    echo "2digit counter I: ${CNT2DI}"
    CNT2DJ=`seq -f "%03.0f" ${COUNTERJ} ${COUNTERJ}`
    echo "2digit counter J: ${CNT2DJ}"
    /bin/cp -f $PWD/LED_$CNT2DI.ini $PWD/test.ini
    sed -i "s/\=$LEDV/\=$LEDV2/g" $PWD/test.ini
    /bin/cp -f $PWD/test.ini $PWD/LED_$CNT2DJ.ini
    let COUNTERI=COUNTERI+1
    let COUNTERJ=COUNTERJ+1
    let LEDV=LEDV+${dmV}
done

rm $PWD/test.ini

scp *.ini calice@192.168.1.11:F:\\CERN_2018_May\\LED_CONFIG\\${LABVFolder}\\.


# xxLED.conf
# This is an example config file, you can adapt it to your needs.
# All text following a # character is treated as comments
# This is an example config file, you can adapt it to your needs.
# All text following a # character is treated as comments

##[RunControl]
#RunEventLimit = readoutcycles
#NextConfigFileOnFileLimit = 1
#
#[LogCollector]
#SaveLevel = EXTRA
#PrintLevel = INFO
#
#
#[Producer.CaliceSc]
#FileLEDsettings ="F:\\LEDEUDAQ\\labfold\\LED_00.ini"
#FileMode = 0
#WaitMillisecForFile = 1000
## Sleeping time (in seconds) after clicking stop: needed to read
## all the events stored in the Labview data queue
#waitsecondsForQueuedEvents = 6
#
#Port = 5622
#IPAddress = "192.168.1.11"
#Reader = "Scintillator"
#
#WriteRawOutput = 1
#RawFileName = "LED/eudaqfold/DetectorRawData_Run_%05d"
#WriteRawFileNameTimestamp = 0
#
#
#[DataCollector.CaliceDataCollector]
#FileType = "lcio"                                                       
#FilePattern = "LED/eudaqfold/Run_$5R$X"


# longLED_wind/LED_00.ini
#;==================================================
#; This is test configuration file for auotmatic LED scan.
#; The format of this file is based on Windows configuration
#; settings file and is divided into sections by brackets.
#; Brackets enclose section name which in our case is the
#; Module ID. Each section has a key named "voltage" which
#; takes a 4 digits integer as LED voltage of that layer in mV. 
#;==================================================
#
#[1]
#voltage=0 ; One can write comments for each line
#
#[2]
#voltage=0
#
#[3]
#voltage=0
#
#[4]
#voltage=0
#
#[5]
#voltage=0
#
#etc



#====================================================
#New conf file
#====================================================
# This is an example config file, you can adapt it to your needs.
# All text following a # character is treated as comments

# [RunControl]
# STOP_RUN_AFTER_N_SECONDS = 1800
# NEXT_RUN_CONF_FILE = GENERATE_FILE
# STOP_RUN_AFTER_N_EVENTS = GENERATE_EVENTS
# NextConfigFileOnFileLimit = 1

# [DataCollector.dc1]
# EUDAQ_FW=native
# EUDAQ_FW_PATTERN="data/eudaqRaw/Run$6R_LED_$13D$X"
# DISABLE_PRINT=1

# [LogCollector]
# SaveLevel = EXTRA
# PrintLevel = INFO 

# [Producer.CaliceSc]
# EUDAQ_DC="dc1,dc2,dc3,bxidColl1"
# FileLEDsettings ="F:\\CERN_2018_May\\LED_CONFIG\\GENERATE_LABVIEW_FOLDER\\GENERATE_LEDFILE"
# FileMode = 0
# WaitMillisecForFile = 1000
# # Sleeping time (in seconds) after clicking stop: needed to read
# # all the events stored in the Labview data queue
# waitsecondsForQueuedEvents = 0
# Port = 5622
# IPAddress = "192.168.1.11"
# Reader = "Scintillator"
# WriteRawOutput = 1
# RawFileName = "data/AhcalRaw/ahcalRaw_Run%06d"
# WriteRawFileNameTimestamp = 1
# ColoredTerminalMessages = 1
# EventBuildingMode = "ROC"
# #EventBuildingMode = "BUILD_BXID_ALL"
# EventNumberingPreference = "TIMESTAMP"
# KeepBuffered = 10
# MaximumROCJump = 100
# AHCALBXIDWidth = 160
# MaximumTrigidSkip = 30000 # maximum numbers of triggers, that are allowed to skip. If more, treated as bad data
# MaximumROCJump = 100
# ChipidAddBeforeMasking = -1 # -1 to count chipid from 0
# ChipidKeepBits = 4
# ChipidAddAdterMasking = 0
# AppendDifidToChipidBitPosition = 8 # where to add the difid
# MinimumBxid = 0 #minimal accepted bxid. bxid0 has a TDC bug


