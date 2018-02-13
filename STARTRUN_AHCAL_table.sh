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
#xterm -sb -sl 1000000 -T "Runcontrol" -e 'bin/euRun -n RunControl ; read '&
xterm -r -sb -sl 100000 -T "Runcontrol" -e 'bin/euRun -n AhcalRunControl |tee -a logs/runcontrol.log; read '&
sleep 2
#################  Log collector #################
xterm -r -sb -sl 1000 -geometry 160x30 -T "Logger" -e 'bin/euLog ; read' &
#sleep 1
#################  Data collector #################
xterm -r -sb -sl 100000 -T "directsave Data collector" -e 'bin/euCliCollector -n DirectSaveDataCollector -t dc1 ; read' &
xterm -r -sb -sl 100000 -T "directsave Data collector 3" -e 'bin/euCliCollector -n DirectSaveDataCollector -t dc3 ; read' &
#xterm -sb -sl 100000 -geometry 160x30 -T "Hodoscope Data collector" -e 'bin/euCliCollector -n AhcalHodoscopeDataCollector -t dc2 | tee -a logs/dc2.log; read' &
#xterm -sb -sl 100000 -geometry 160x30 -T "slcio Hodoscope Data collector" -e 'bin/euCliCollector -n AhcalHodoscopeDataCollector -t dc3 | tee -a logs/dc3.log; read' &
#xterm -sb -sl 100000  -T "Data collector2" -e 'bin/euCliCollector -n DirectSaveDataCollector -t dc2 ; read' &
#sleep 1

#################  Producer #################
#xterm -sb -sl 1000000 -geometry 160x30 -T "Hodoscope" -e 'bin/euCliProducer -n CaliceEasirocProducer -t hodoscope1 -r tcp://$HOSTIP:$RCPOR|tee -a logs/hodoscope1.log && read || read'&
#xterm -sb -sl 1000000 -geometry 160x30 -T "Hodoscope 2" -e 'bin/euCliProducer -n CaliceEasirocProducer -t hodoscope2 -r tcp://$HOSTIP:$RCPOR|tee -a logs/hodoscope2.log && read || read'&
# sleep 1
xterm -r -sb -sl 100000 -geometry 160x30 -T "AHCAL" -e 'bin/euCliProducer -n AHCALProducer -t Calice1 |tee -a logs/ahcal.log ; read'&
xterm -r -sb -sl 10000 -geometry 160x30 -T "DESY table" -e 'bin/euCliProducer -n DesyTableProducer -t desytable1 |tee -a logs/desytable.log ;read'&

#################  Online Monitor #################
echo "starting online monitor"
#xterm -sb -sl 100000  -T "Online monitor" -e 'bin/StdEventMonitor -c conf/onlinemonitor.conf --monitor_name StdEventMonitor --reset ; read' &
xterm -sb -sl 100000  -T "Online monitor" -e 'bin/StdEventMonitor -c conf/onlinemonitor.conf --monitor_name StdEventMonitor --reset -r tcp://$HOSTIP:$RCPORT ; read' &
 echo "online monitor started"
exit

