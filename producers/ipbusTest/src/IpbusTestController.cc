#include "IpbusTestController.h"

namespace ipbus{

  IpbusTestController::IpbusTestController( const std::string &connectionFile, const std::string &deviceName ){
    uhal::ConnectionManager manager( connectionFile );
    m_hw = new uhal::HwInterface( manager.getDevice( deviceName ) );
  }

  IpbusTestController::~IpbusTestController()
  {
    m_data.clear();
    delete m_hw;
  }
  
  uint32_t IpbusTestController::ReadRegister( const std::string &name )
  {
    try {
      uhal::ValWord< uint32_t > test = m_hw->getNode(name).read();
      m_hw->dispatch();
      if(test.valid()) {
	return test.value();
      } else {
	std::cout << "Error reading " << name << std::endl;
	return 0;
      }
    } catch (...) {
      return 0;
    }
  }

  void IpbusTestController::SetRegister( const std::string &name, uint32_t val )
  {
    try {
      m_hw->getNode(name).write( val );
      m_hw->dispatch();
    } catch (...) {
      return;
    }
  }

  void IpbusTestController::ReadDataBlock( const std::string &name, uint32_t blkSize )
  {
    try {
      uhal::ValVector<uint32_t> data = m_hw->getNode(name.c_str()).readBlock(blkSize);
      m_hw->dispatch();
      if(data.valid()) {
	CastTheData( data );
      } else {
	std::cout << "Error reading " << name << std::endl;
	return ;
      }
    } catch (...) {
      return;
    }
  }
  
  void IpbusTestController::SetUhalLogLevel(unsigned char lvl){
    switch(lvl){
    case 0:
      uhal::disableLogging();
      break;
    case 1:
      uhal::setLogLevelTo(uhal::Fatal());
      break;
    case 2:
      uhal::setLogLevelTo(uhal::Error());
      break;
    case 3:
      uhal::setLogLevelTo(uhal::Warning());
      break;
    case 4:
      uhal::setLogLevelTo(uhal::Notice());
      break;
    case 5:
      uhal::setLogLevelTo(uhal::Info());
      break;
    case 6:
      uhal::setLogLevelTo(uhal::Debug());
      break;
    default:
      uhal::setLogLevelTo(uhal::Debug());      
    }
  }
  
void IpbusTestController::CastTheData(const uhal::ValVector<uint32_t> &data)
{
  for( uhal::ValVector<uint32_t>::const_iterator cit=data.begin(); cit!=data.end(); ++cit )
    m_data.push_back(*cit);
    // re-implement the method as soon as the data format is better known
    // a data format class must be implemented
    // m_data should be a vector of this data format class
  }

  void IpbusTestController::ResetTheData()
  {
    m_data.clear();
    // it might be needed to re-implement this method as soon as the data format is better known
  }

}
