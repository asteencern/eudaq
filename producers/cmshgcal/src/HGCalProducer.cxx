#include "eudaq/Configuration.hh"
#include "eudaq/Producer.hh"
#include "eudaq/Logger.hh"
#include "eudaq/RawDataEvent.hh"
#include "eudaq/Timer.hh"
#include "eudaq/Utils.hh"
#include "eudaq/OptionParser.hh"

#include <iostream>
#include <ostream>
#include <sstream>
#include <vector>
#include <memory>
#include <iomanip>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include <boost/crc.hpp>
#include <boost/cstdint.hpp>
#include <boost/timer/timer.hpp>
#include <boost/thread/thread.hpp>
#include <boost/format.hpp>

#include <TFile.h>
#include <TH1D.h>

#include "IpbusHwController.h"
#include "TriggerController.h"

#define FORMAT_VERSION 1
#define MAX_NUMBER_OF_ORM 16
// A name to identify the raw data format of the events generated
// Modify this to something appropriate for your producer.
static const std::string EVENT_TYPE = "HexaBoard";


void readFIFOThread( ipbus::IpbusHwController* orm, uint32_t *blockSize)
{
  orm->ReadDataBlock("FIFO",*blockSize);
}
// void readFIFOThread( ipbus::IpbusHwController* orm)
// {
//   orm->ReadDataBlock("FIFO");
// }

void startTriggerThread( TriggerController* trg_ctrl, uint32_t *run, ACQ_MODE* mode)
{
  trg_ctrl->startrunning( *run, *mode );
}

// Declare a new class that inherits from eudaq::Producer
class HGCalProducer : public eudaq::Producer {
public:
  // The constructor must call the eudaq::Producer constructor with the name
  // and the runcontrol connection string, and initialize any member variables.
  HGCalProducer(const std::string & name, const std::string & runcontrol) : eudaq::Producer(name, runcontrol)
  {
    m_run=m_ev=0;
    m_uhalLogLevel=5;
    m_blockSize=30787;
    m_state=STATE_UNCONF;
    m_rdoutMask=0x0;
  }

 private:
  unsigned m_run, m_ev, m_uhalLogLevel, m_blockSize;
  uint16_t m_rdoutMask;
  ACQ_MODE m_acqmode;
  
  TriggerController *m_triggerController;
  std::vector< ipbus::IpbusHwController* > m_rdout_orms;
  boost::thread m_triggerThread;
  std::string m_syncRPIAlias;
  std::vector<std::string> m_rdoutRPIAliases;
  enum DAQState {
    STATE_ERROR,
    STATE_UNCONF,
    STATE_GOTOCONF,
    STATE_CONFED,
    STATE_GOTORUN,
    STATE_RUNNING,
    STATE_GOTOSTOP,
    STATE_GOTOTERM
  } m_state;
    
  std::ofstream m_rawFile;
  
  TFile *m_outrootfile;
  TH1D *m_hreadouttime;
  TH1D *m_hwritertime;

public:
  bool checkCRC( const std::string & crcNodeName, ipbus::IpbusHwController *ptr )
  {
    //   std::cout << "Start checkCRC" << std::endl;
    std::vector<uint32_t> tmp=ptr->getData();
    uint32_t *data=&tmp[0];
    boost::crc_32_type checksum;
    checksum.process_bytes(data,(std::size_t)tmp.size());
    uint32_t crc=ptr->ReadRegister(crcNodeName.c_str());
    std::ostringstream os( std::ostringstream::ate );
    if( crc==checksum.checksum() ){
      os.str(""); os << "checksum success (" << crc << ")";
      EUDAQ_DEBUG( os.str().c_str() );
      return true;
    }
    os.str(""); os << "in HGCalProducer.cxx : checkCRC( const std::string & crcNodeName, ipbus::IpbusHwController *ptr ) -> checksum fail : sent " << crc << "\t compute" << checksum.checksum() << " -> sleep 10 sec; PLEASE REACT";
    EUDAQ_ERROR( os.str().c_str() );
    eudaq::mSleep(10000);
    return false;
  }

  void MainLoop() 
  {
    std::ostringstream os( std::ostringstream::ate );
    while (m_state != STATE_GOTOTERM){
      if( m_state != STATE_RUNNING ) {
	os.str("");
	os << "je suis dans la main loop et le state c'est STATE_";
	switch(m_state){
	case STATE_ERROR: {os << "ERROR"; break;}
	case STATE_UNCONF: {os << "UNCONF"; break;}
	case STATE_GOTOCONF: {os << "GOTOCONF"; break;}
	case STATE_CONFED: {os << "CONFED"; break;}
	case STATE_GOTORUN: {os << "GOTORUN"; break;}
	case STATE_GOTOSTOP: {os << "GOTOSTOP"; break;}
	}
	std::cout << os.str() << std::endl;
	eudaq::mSleep(1000);
      }
      if (m_state == STATE_UNCONF) {
	eudaq::mSleep(100);
	continue;
      }    

      if (m_state == STATE_RUNNING) {
	if( !m_triggerController->checkState( (STATES)RDOUT_RDY ) ) continue;
	if( m_ev==m_triggerController->eventNumber() ) continue;

	boost::timer::cpu_timer timerReadout;
	boost::timer::cpu_times times;
	eudaq::RawDataEvent ev(EVENT_TYPE,m_run,m_ev);
	boost::thread threadVec[m_rdout_orms.size()];
	
	for( int i=0; i<(int)m_rdout_orms.size(); i++)
	  //threadVec[i]=boost::thread(readFIFOThread,m_rdout_orms[i]);
	  threadVec[i]=boost::thread(readFIFOThread,m_rdout_orms[i],&m_blockSize);
	
	for( int i=0; i<(int)m_rdout_orms.size(); i++){
	  threadVec[i].join();}

	times=timerReadout.elapsed();
	m_hreadouttime->Fill(times.wall/1e9);

	int head[1];
	boost::timer::cpu_timer timerWriter;
	for( int i=0; i<(int)m_rdout_orms.size(); i++){
	  std::vector<uint32_t> the_data = m_rdout_orms[i]->getData() ;
	  // Adding trailer
	  //  checkCRC( "RDOUT.CRC",m_rdout_orms[i]);
	  uint32_t trailer=i;//8 bits for orm id
	  //std::cout << "board id = " << trailer;
	  trailer|=m_triggerController->eventNumber()<<8;//24 bits for trigger number
	  //std::cout << "\t event number id = " << m_triggerController->eventNumber();
	  //std::cout << "\t trailer = " << trailer << std::endl;	  //m_rdout_orms[i]->addTrailerToData(trailer);
	  
	  the_data.push_back(trailer);

	  // Send it to euDAQ converter plugins:
          head[0] = i+1;
          ev.AddBlock(   2*i, head, sizeof(head));
	  ev.AddBlock( 2*i+1, the_data);

	  std::cout<<"rdout board "<<i<<"  skiroc mask="<<std::setw(8)<<std::setfill('0')<<std::hex<<the_data[0]<<"  Size of the data (bytes): "<<std::dec<<the_data.size()*4<<std::endl;

	  //Add trigger timestamp to raw data :
	  the_data.push_back( m_rdout_orms[i]->ReadRegister("CLK_COUNT0") );
	  the_data.push_back( m_rdout_orms[i]->ReadRegister("CLK_COUNT1") );
	  //std::cout << std::setw(8) << std::setfill('0') << std::hex << m_rdout_orms[i]->ReadRegister("CLK_COUNT0") << "\t" << m_rdout_orms[i]->ReadRegister("CLK_COUNT1") << std::endl;
	  
	  // Write it into raw file:
          m_rawFile.write(reinterpret_cast<const char*>(&the_data[0]), the_data.size()*sizeof(uint32_t));
	}
	times=timerWriter.elapsed();
	m_hwritertime->Fill(times.wall/1e9);

	m_ev=m_triggerController->eventNumber();
	SendEvent(ev);
	for( std::vector<ipbus::IpbusHwController*>::iterator it=m_rdout_orms.begin(); it!=m_rdout_orms.end(); ++it ){
	  (*it)->ResetTheData();
	  while(1){
	    if( (*it)->ReadRegister("DATE_STAMP") )
	      break;
            else
              boost::this_thread::sleep( boost::posix_time::microseconds(1) );     
	  }
	}
	std::cout << "receive and save a new event" << std::endl;
	m_triggerController->readoutCompleted();
      }
      if (m_state == STATE_GOTOSTOP) {
	std::cout << "on envoie un evt tout pourri pour la fin" << std::endl;
	SendEvent( eudaq::RawDataEvent::EORE(EVENT_TYPE,m_run,++m_ev) );
	std::cout << "ca a du marcher" << std::endl;
	eudaq::mSleep(1000);
	m_state = STATE_CONFED;
	continue;
      }
    };
  }

private:
  // This gets called whenever the DAQ is configured
  virtual void OnConfigure(const eudaq::Configuration & config) 
  {
    std::cout << "Configuring: " << config.Name() << std::endl;

    m_syncRPIAlias=config.Get("SyncBoardAlias", "piS");
    EUDAQ_INFO("Starting sync_debug.exe on syncboard:"+m_syncRPIAlias);
    int executionStatus = 0; 
    executionStatus = system(("ssh -T "+m_syncRPIAlias+" \" sudo killall sync_debug.exe \"").data());
    if (executionStatus != 0)
      EUDAQ_WARN("Warning: unable to kill on syncboard. It's may be already dead...");
    else
      EUDAQ_INFO("Successfully killed on syncboard!");
    executionStatus = system(("ssh -T "+m_syncRPIAlias+" \" nohup sudo /home/pi/SYNCH_BOARD/bin/sync_debug.exe 0 > log.log 2>&1& \" ").data());
    if (executionStatus != 0)
      EUDAQ_ERROR("Error: unable to run sync on "+m_syncRPIAlias);
    else
      EUDAQ_INFO("Successfully run sync on "+m_syncRPIAlias+"!");

    m_rdoutMask = config.Get("RDoutMask",1);
    m_uhalLogLevel = config.Get("UhalLogLevel", 5);
    m_blockSize = config.Get("DataBlockSize",962);
    const int mode=config.Get("AcquisitionMode",0);
    switch( mode ){
    case 0 : m_acqmode = DEBUG; break;
    case 1 : m_acqmode = BEAMTEST; break;
    default : m_acqmode = DEBUG; break;
    }

    std::ostringstream os( std::ostringstream::ate );
    unsigned int bit=1;
    std::cout << "m_rdoutMask = " << m_rdoutMask << std::endl;
    for( int iorm=0; iorm<MAX_NUMBER_OF_ORM; iorm++ ){
      std::cout << "iorm = " << iorm << "..." << std::endl;
      bool activeSlot=(m_rdoutMask&bit);
      bit = bit << 1;
      if(!activeSlot) continue;
      std::cout << "... found in the rdout mask " << std::endl;
      os.str("");os<<"RDoutBoardRPIAlias"<<iorm;
      m_rdoutRPIAliases.push_back(config.Get(os.str(),"piRBDev"));
      EUDAQ_INFO("Starting new_rdout.exe on "+m_rdoutRPIAliases.back());
      executionStatus = system(("ssh -T "+m_rdoutRPIAliases.back()+" \" sudo killall new_rdout.exe \"").data());
      if (executionStatus != 0)
      	EUDAQ_WARN("Error: unable to kill on "+m_rdoutRPIAliases.back()+". It's probably already dead...");
      else 
      	EUDAQ_INFO("Successfully killed on "+m_rdoutRPIAliases.back());
      executionStatus = system(("ssh -T "+m_rdoutRPIAliases.back()+" \" nohup sudo /home/pi/RDOUT_BOARD_IPBus/rdout_software/bin/new_rdout.exe 200 200000 0 > log.log 2>&1& \" ").data());
      if (executionStatus != 0)
    	EUDAQ_ERROR("Error: unable to run exe on "+m_rdoutRPIAliases.back());
      else
    	EUDAQ_INFO("Successfully run exe on "+m_rdoutRPIAliases.back());

      os.str(""); os << "RDOUT_ORM" << iorm;
      ipbus::IpbusHwController *orm = new ipbus::IpbusHwController(config.Get("ConnectionFile","file://./etc/connection.xml"),os.str());
      m_rdout_orms.push_back( orm );
    }    
    m_triggerController = new TriggerController(m_rdout_orms);

    for( std::vector<ipbus::IpbusHwController *>::iterator it=m_rdout_orms.begin(); it!=m_rdout_orms.end(); ++it ){
      std::string ormId=(*it)->getInterface()->id();
      std::cout << "ORM " << ormId << "\n"
    		<< "Check0 = " << std::hex << (*it)->ReadRegister("check0") << "\t"
    		<< "Check1 = " << std::hex << (*it)->ReadRegister("check1") << "\t"
    		<< "Check2 = " << std::hex << (*it)->ReadRegister("check2") << "\t"
    		<< "Check3 = " << std::hex << (*it)->ReadRegister("check3") << "\t"
    		<< "Check4 = " << std::hex << (*it)->ReadRegister("check4") << "\t"
    		<< "Check5 = " << std::hex << (*it)->ReadRegister("check5") << "\t"
    		<< "Check6 = " << std::hex << (*it)->ReadRegister("check6") << "\n"
    		<< "Check31 = " << std::dec << (*it)->ReadRegister("check31") << "\t"
    		<< "Check32 = " << std::dec << (*it)->ReadRegister("check32") << "\t"
    		<< "Check33 = " << std::dec << (*it)->ReadRegister("check33") << "\t"
    		<< "Check34 = " << std::dec << (*it)->ReadRegister("check34") << "\t"
    		<< "Check35 = " << std::dec << (*it)->ReadRegister("check35") << "\t"
    		<< "Check36 = " << std::dec << (*it)->ReadRegister("check36") << std::endl;
    
      const uint32_t mask=(*it)->ReadRegister("SKIROC_MASK");
      std::cout << "ORM " << ormId << "\t SKIROC_MASK = " <<std::setw(8)<< std::hex<<mask << std::endl;
      const uint32_t cst0=(*it)->ReadRegister("CONSTANT0");
      std::cout << "ORM " << ormId << "\t CONST0 = " << std::hex << cst0 << std::endl;
      const uint32_t cst1=(*it)->ReadRegister("CONSTANT1");
      std::cout << "ORM " << ormId << "\t CONST1 = " << std::hex << cst1 << std::endl;
    
      boost::this_thread::sleep( boost::posix_time::milliseconds(1000) );
    }    

    m_state=STATE_CONFED;
    SetStatus(eudaq::Status::LVL_OK, "Configured (" + config.Name() + ")");
}

  // This gets called whenever a new run is started
  // It receives the new run number as a parameter
  virtual void OnStartRun(unsigned param) 
  {
    m_state = STATE_GOTORUN;
    SetStatus(eudaq::Status::LVL_OK, "Starting");
    
    m_run = param;
    m_ev = 0;
    
    // It must send a BORE to the Data Collector
    // change this as soon as we have a data format
    std::cout << "on envoi un evt tout pourri pour commencer" << std::endl;
    SendEvent( eudaq::RawDataEvent::BORE(EVENT_TYPE, m_run) );
    std::cout << "ca a du marcher" << std::endl;

    //create root objects
    m_outrootfile = new TFile("../data/time.root","RECREATE");
    m_hreadouttime = new TH1D("rdoutTime","",10000,0,1);    
    m_hwritertime = new TH1D("writingTime","",10000,0,1);
    
    // Let's open a file for raw data:
    char rawFilename[256];
    sprintf(rawFilename, "../raw_data/HexaData_Run%04d.raw", m_run); // The path is relative to eudaq/bin; raw_data is a symbolic link
    m_rawFile.open(rawFilename, std::ios::binary);

    uint32_t header[3];
    header[0]=time(0);
    header[1]=m_rdout_orms.size();
    header[1]|=m_run<<8;
    header[2]=FORMAT_VERSION;
    m_rawFile.write(reinterpret_cast<const char*>(&header[0]), sizeof(header));
    
    
    for( std::vector<ipbus::IpbusHwController*>::iterator it=m_rdout_orms.begin(); it!=m_rdout_orms.end(); ++it ){
      (*it)->ResetTheData();
      while(1){
	if( (*it)->ReadRegister("DATE_STAMP") )
	  break;
	else
	  boost::this_thread::sleep( boost::posix_time::microseconds(1) );     
      }
    }
    m_triggerThread=boost::thread(startTriggerThread,m_triggerController,&m_run,&m_acqmode);

    m_state = STATE_RUNNING;
    // At the end, set the status that will be displayed in the Run Control.
    SetStatus(eudaq::Status::LVL_OK, "Running");
  }

  // // This gets called whenever a run is stopped
  virtual void OnStopRun() {
    try {
      SetStatus(eudaq::Status::LVL_OK, "Stopping");
      m_triggerController->stopRun();
      eudaq::mSleep(1000);
      m_state = STATE_GOTOSTOP;
      m_outrootfile->Write();
      m_outrootfile->Close();

      while (m_state == STATE_GOTOSTOP) {
	eudaq::mSleep(1000); //waiting for EORE being send
      }

      uint32_t trailer=time(0); 
      m_rawFile.write(reinterpret_cast<const char*>(&trailer), sizeof(trailer));
      m_rawFile.close();

      SetStatus(eudaq::Status::LVL_OK, "Stopped");
      
    } catch (const std::exception & e) {
      SetStatus(eudaq::Status::LVL_ERROR, "Stop Error: " + eudaq::to_string(e.what()));
    } catch (...) {
      SetStatus(eudaq::Status::LVL_ERROR, "Stop Error");
    }
  }

  // // This gets called when the Run Control is terminating,
  // // we should also exit.
  virtual void OnTerminate() {
    SetStatus(eudaq::Status::LVL_OK, "Terminating...");
    
    int executionStatus = 0;
    executionStatus = system(("ssh -T "+m_syncRPIAlias+" \" sudo killall sync_debug.exe \"").data());
    int bit=1;
    for( int iorm=0; iorm<MAX_NUMBER_OF_ORM; iorm++ ){
      if( !m_rdoutMask&bit ) continue;
      bit = bit << 1;
      executionStatus = system(("ssh -T "+m_rdoutRPIAliases[iorm]+" \" sudo killall new_rdout.exe \"").data());
      if (executionStatus != 0)
	std::cout << "Error: unable to kill new_rdout.exe on " << m_rdoutRPIAliases[iorm] << std::endl;
      else
	std::cout << "Successfully kill new_rdout.exe on " << m_rdoutRPIAliases[iorm] << std::endl;
    }
    m_triggerController->stopRun();

    m_state = STATE_GOTOTERM;
    
    eudaq::mSleep(1000);
  }
};

// The main function that will create a Producer instance and run it
int main(int /*argc*/, const char ** argv) {
  eudaq::OptionParser op("HGCal Producer", "1.0", "The Producer task for the CMS-HGCal prototype");
  eudaq::Option<std::string> rctrl(op, "r", "runcontrol", "tcp://localhost:44000", "address", "The address of the RunControl application");
  eudaq::Option<std::string> level(op, "l", "log-level", "NONE", "level", "The minimum level for displaying log messages locally");
  eudaq::Option<std::string> name (op, "n", "name", "CMS-HGCAL", "string", "The name of this Producer");
  try {
    op.Parse(argv);
    EUDAQ_LOG_LEVEL(level.Value());
    HGCalProducer producer(name.Value(),rctrl.Value());
    std::cout << "on demarre la main loop" << std::endl;
    producer.MainLoop();
    std::cout << "Quitting" << std::endl;
    eudaq::mSleep(300);
  } catch (...) {
    return op.HandleMainException();
  }
  return 0;
}
