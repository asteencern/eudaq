#include "TriggerController.h"
#include "boost/thread/thread.hpp"

TriggerController::TriggerController(std::vector< ipbus::IpbusHwController* > rdout) : m_state(WAIT),
										       m_acqmode(BEAMTEST),
										       m_run(0),
										       m_evt(0),
										       m_rdout_orms(rdout),
										       m_gotostop(false),
										       m_rdoutcompleted(false)
{;}

void TriggerController::readoutCompleted() 
{ 
    m_rdoutcompleted=true; 
    //    std::cout << "receive rdoutcompleted command : rdoutcompleted = " << m_rdoutcompleted << std::endl; 
}

void TriggerController::startrunning( uint32_t runNumber, const ACQ_MODE mode )
{
  m_run=runNumber;
  m_evt=0;
  m_state=WAIT;
  m_acqmode=mode;
  m_gotostop=false;
  
  switch( m_acqmode ){
  case BEAMTEST : run(); break;
  case DEBUG : runDebug(); break;
  }
}

void TriggerController::run()
{
  while(1){
    if( m_state==END_RUN ) break;
    bool rdout_ready=true;
    for( std::vector<ipbus::IpbusHwController*>::iterator it=m_rdout_orms.begin(); it!=m_rdout_orms.end(); ++it )
      if( (*it)->ReadRegister("TOP.BLOCK_READY")!=1 ) 
	rdout_ready=false;
    if( !rdout_ready ){
      boost::this_thread::sleep( boost::posix_time::microseconds(1) );
      continue;
    }
    m_rdoutcompleted=false;
    m_evt=m_rdout_orms[0]->ReadRegister("TOP.TRIG_COUNT");
    m_state=RDOUT_RDY;
    while(1){
      if( m_rdoutcompleted==true ){
	m_state=RDOUT_FIN;
	break;
      }
      boost::this_thread::sleep( boost::posix_time::microseconds(1) );
    }
    for( std::vector<ipbus::IpbusHwController*>::iterator it=m_rdout_orms.begin(); it!=m_rdout_orms.end(); ++it )
      (*it)->SetRegister("TOP.RDOUT_DONE",1); 
    if( m_gotostop ) m_state=END_RUN;
    else m_state=WAIT;
  }
  return;
}

void TriggerController::runDebug()
{
  while(1){
    if( m_state==END_RUN ) break;
    bool rdout_ready=true;
    for( std::vector<ipbus::IpbusHwController*>::iterator it=m_rdout_orms.begin(); it!=m_rdout_orms.end(); ++it )
      if( (*it)->ReadRegister("TOP.BLOCK_READY")!=1 ) 
	rdout_ready=false;
    if( !rdout_ready ){
      std::cout << "trigger for BLOCK_READY not received yet -> wait for 1 seconds" << std::endl;
      boost::this_thread::sleep( boost::posix_time::milliseconds(1000) );
      continue;
    }
    else
      std::cout << "BLOCK_READY ok for each boards -> try to read fifos" << std::endl;
    m_rdoutcompleted=false;
    m_evt=m_rdout_orms[0]->ReadRegister("TOP.TRIG_COUNT");
    m_state=RDOUT_RDY;
    while(1){
      if( m_rdoutcompleted==true ){
	m_state=RDOUT_FIN;
	break;
      }
      boost::this_thread::sleep( boost::posix_time::microseconds(1) );
    }
    std::cout << "finish to read fifos" << std::endl;
    boost::this_thread::sleep( boost::posix_time::milliseconds(1000) );
    for( std::vector<ipbus::IpbusHwController*>::iterator it=m_rdout_orms.begin(); it!=m_rdout_orms.end(); ++it )
      (*it)->SetRegister("TOP.RDOUT_DONE",1);
    std::cout << "finish to send RDOUT_DONE to each board -> wait for 1 seconds" << std::endl;
    boost::this_thread::sleep( boost::posix_time::milliseconds(1000) );
    if( m_gotostop ) m_state=END_RUN;
    else m_state=WAIT;
  }
  return;
}
