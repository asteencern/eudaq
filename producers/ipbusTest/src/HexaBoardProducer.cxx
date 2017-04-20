#include "eudaq/Configuration.hh"
#include "eudaq/Producer.hh"
#include "eudaq/Logger.hh"
#include "eudaq/RawDataEvent.hh"
#include "eudaq/Timer.hh"
#include "eudaq/Utils.hh"
#include "eudaq/OptionParser.hh"

#include <iostream>
#include <ostream>
#include <vector>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include <boost/crc.hpp>
#include <boost/cstdint.hpp>
#include <boost/timer/timer.hpp>
#include <boost/thread/thread.hpp>

#include <TFile.h>
#include <TH1D.h>

#include "IpbusTestController.h"

// A name to identify the raw data format of the events generated
// Modify this to something appropriate for your producer.
static const std::string EVENT_TYPE = "HEXABOARD";


void readFIFOThread( ipbus::IpbusTestController* orm, uint32_t *blockSize)
{
  orm->ReadDataBlock("HEXA.FIFO",*blockSize);
}

// Declare a new class that inherits from eudaq::Producer
class HexaBoardProducer : public eudaq::Producer {
public:
  // The constructor must call the eudaq::Producer constructor with the name
  // and the runcontrol connection string, and initialize any member variables.
  HexaBoardProducer(const std::string & name, const std::string & runcontrol)
    : eudaq::Producer(name, runcontrol), m_run(0), m_ev(0), m_uhalLogLevel(5), m_blockSize(963), m_state(STATE_UNCONF), m_controlHw(nullptr){}

private:
  unsigned m_run, m_ev, m_uhalLogLevel, m_blockSize;
  std::vector< ipbus::IpbusTestController* > m_orms;
  std::unique_ptr<ipbus::IpbusTestController>  m_controlHw;
  TFile *m_outrootfile;
  TH1D *m_htime;
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

public:
  bool checkCRC( const std::string & crcNodeName, ipbus::IpbusTestController *ptr )
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
    os.str(""); os << "in HexaBoardProducer.cxx : checkCRC( const std::string & crcNodeName ) -> checksum fail : sent " << crc << "\t compute" << checksum.checksum() << "-> try again";
    EUDAQ_WARN( os.str().c_str() );
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
	if( m_controlHw->ReadRegister( "CONTROL.READ" )!=1 ) continue;
	boost::timer::cpu_timer timer;
	boost::timer::cpu_times times;
	eudaq::RawDataEvent ev(EVENT_TYPE,m_run,m_ev);
	boost::thread threadVec[m_orms.size()];
	for( int i=0; i<(int)m_orms.size(); i++)
	  threadVec[i]=boost::thread(readFIFOThread,m_orms[i],&m_blockSize);
	for( int i=0; i<(int)m_orms.size(); i++){
	  threadVec[i].join();
	  ev.AddBlock( i,m_orms[i]->getData() );
	}
	times=timer.elapsed();
	m_htime->Fill(times.wall/1e9);
	m_controlHw->SetRegister( "CONTROL.READ",0 );
	SendEvent(ev);
	m_ev++;
	for( std::vector<ipbus::IpbusTestController*>::iterator it=m_orms.begin(); it!=m_orms.end(); ++it )
	  (*it)->ResetTheData();
	std::cout << "receive and save a new event" << std::endl;
      }
      if (m_state == STATE_GOTOSTOP) {
	std::cout << "on envoie un evt tout pourri pour la fin" << std::endl;
	SendEvent( eudaq::RawDataEvent::EORE(EVENT_TYPE,m_run,++m_ev) );
	std::cout << "ca a du marcher" << std::endl;
	m_state = STATE_CONFED;
	eudaq::mSleep(100);
	continue;
      }
    };
  }

private:
  // This gets called whenever the DAQ is configured
  virtual void OnConfigure(const eudaq::Configuration & config) 
  {
    std::cout << "Configuring: " << config.Name() << std::endl;
    
    // Do any configuration of the hardware here
    // Configuration file values are accessible as config.Get(name, default)
    m_uhalLogLevel = config.Get("UhalLogLevel", 5);
    m_blockSize = config.Get("DataBlockSize",962);

    int n_orms = config.Get("NumberOfORMs",1);
    std::ostringstream deviceName( std::ostringstream::ate );
    for( int iorm=0; iorm<n_orms; iorm++ ){
      deviceName.str(""); deviceName << config.Get("DeviceNamePrefix","HEXABOARD") << iorm;
      ipbus::IpbusTestController *orm = new ipbus::IpbusTestController(config.Get("ConnectionFile","file://./etc/connection.xml"),deviceName.str());
      m_orms.push_back( orm );
    }
    
    m_controlHw = std::unique_ptr<ipbus::IpbusTestController>( new ipbus::IpbusTestController(config.Get("ConnectionFile","file://./etc/connection.xml"),
											      config.Get("ControlDeviceName","DummyControl")) );
    
    m_state=STATE_CONFED;
    // At the end, set the status that will be displayed in the Run Control.
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
    m_htime = new TH1D("time","",10000,0,1);

    m_state = STATE_RUNNING;
    // At the end, set the status that will be displayed in the Run Control.
    SetStatus(eudaq::Status::LVL_OK, "Started");
  }

  // // This gets called whenever a run is stopped
  virtual void OnStopRun() {
    try {
      SetStatus(eudaq::Status::LVL_OK, "Stopping");
      //m_tlu->SetTriggerVeto(1);
      m_state = STATE_GOTOSTOP;
      m_outrootfile->Write();
      m_outrootfile->Close();
      while (m_state == STATE_GOTOSTOP) {
	eudaq::mSleep(1000); //waiting for EORE being send
      }
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
    m_state = STATE_GOTOTERM;
    eudaq::mSleep(1000);
  }
};

// The main function that will create a Producer instance and run it
int main(int /*argc*/, const char ** argv) {
  eudaq::OptionParser op("HexaBoard Producer", "1.0", "The Producer task for the HExaBoard");
  eudaq::Option<std::string> rctrl(op, "r", "runcontrol", "tcp://localhost:44000", "address", "The address of the RunControl application");
  eudaq::Option<std::string> level(op, "l", "log-level", "NONE", "level", "The minimum level for displaying log messages locally");
  eudaq::Option<std::string> name (op, "n", "name", "HEXABOARD", "string", "The name of this Producer");
  try {
    op.Parse(argv);
    EUDAQ_LOG_LEVEL(level.Value());
    HexaBoardProducer producer(name.Value(),rctrl.Value());
    std::cout << "on demarre la main loop" << std::endl;
    producer.MainLoop();
    std::cout << "Quitting" << std::endl;
    eudaq::mSleep(300);
  } catch (...) {
    return op.HandleMainException();
  }
  return 0;
}
