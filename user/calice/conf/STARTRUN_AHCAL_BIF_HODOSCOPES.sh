#!/bin/bash

if [ -f KILLRUN.local ]
then
    sh KILLRUN.local
else
    sh KILLRUN.sh
fi

export RCPORT=44000
export HOSTIP=127.0.0.1
#################  Run control ###################
#xterm -sb -sl 1000000 -T "Runcontrol" -e 'bin/euRun -n RunControl -a tcp://$RCPORT ; read '&
xterm -r -sb -sl 100000 -T "Runcontrol" -e 'bin/euRun -n AhcalRunControl |tee -a data/logs/runcontrol.log; read '&
sleep 3

#################  Log collector #################
xterm -r -sb -sl 1000 -geometry 160x24 -T "Logger" -e 'bin/euLog ; read' &
#sleep 1

#################  Data collector #################
#collector 1: event monitor. Only ahcal
xterm -r -sb -sl 100000 -geometry 80x4 -T "Collector 1" -e 'bin/euCliCollector -n DirectSaveDataCollector -t dc1 ; read' &
#collector 3: slcio file writer.
#xterm -r -sb -sl 100000 -geometry 80x4 -T "Collector 3" -e 'bin/euCliCollector -n DirectSaveDataCollector -t dc3 ; read' &
#calice data collector
xterm -r -sb -sl 100000 -geometry 160x24 -T "BXID collector ahcalbif" -e 'bin/euCliCollector -n CaliceAhcalBifBxidDataCollector -t bxidColl1 ; read' &
xterm -sb -sl 100000 -geometry 160x30 -T "Hodoscope Data collector" -e 'bin/euCliCollector -n AhcalHodoscopeDataCollector -t dc2 | tee -a data/logs/dc2.log; read' &
#xterm -sb -sl 100000 -geometry 160x30 -T "slcio Hodoscope Data collector" -e 'bin/euCliCollector -n AhcalHodoscopeDataCollector -t dc3 | tee -a data/logs/dc3.log; read' &
#xterm -sb -sl 100000  -T "Data collector2" -e 'bin/euCliCollector -n DirectSaveDataCollector -t dc2 ; read' &
#sleep 1

#################  Producer #################
xterm -r -sb -sl 100000 -geometry 160x5 -T "Hodoscope 1" -e 'bin/euCliProducer -n CaliceEasirocProducer -t Hodoscope1 |tee -a data/logs/hodoscope1.log ; read'&
xterm -r -sb -sl 100000 -geometry 160x5 -T "Hodoscope 2" -e 'bin/euCliProducer -n CaliceEasirocProducer -t Hodoscope2 |tee -a data/logs/hodoscope2.log ; read'&
# sleep 1
xterm -r -sb -sl 100000 -geometry 160x24 -T "AHCAL" -e 'bin/euCliProducer -n AHCALProducer -t AHCAL1 |tee -a data/logs/ahcal.log ; read'&
xterm -r -sb -sl 100000 -geometry 160x24 -T "BIF" -e 'bin/euCliProducer -n caliceahcalbifProducer -t BIF1 |tee -a data/logs/bif.log ; read'&
#xterm -r -sb -sl 10000 -geometry 160x24 -T "Table DESY" -e 'bin/euCliProducer -n DesyTableProducer -t desytable1 |tee -a data/logs/desytable.log ;read'& 
#################  Online Monitor #################
echo "starting online monitor"
xterm -r -sb -sl 100000  -T "Online monitor" -e 'bin/StdEventMonitor -c conf/onlinemonitor.conf --monitor_name StdEventMonitor --root --reset -r tcp://$HOSTIP:$RCPORT ; read' &
echo "online monitor started"
exit
