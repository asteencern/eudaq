#include "eudaq/Configuration.hh"
#include "eudaq/Producer.hh"
#include "eudaq/Logger.hh"
#include "eudaq/RawDataEvent.hh"
#include "eudaq/Timer.hh"
#include "eudaq/Utils.hh"
#include "eudaq/OptionParser.hh"
#include "eudaq/ExampleHardware.hh"
#include <iostream>
#include <ostream>
#include <vector>
#include <cstdio>

// A name to identify the raw data format of the events generated
// Modify this to something appropriate for your producer.
static const std::string EVENT_TYPE = "HGCal";
static const std::string HGCAL_SPECIFIC_DIR = "/home/daq/daq/hgcalSpecific";

// Declare a new class that inherits from eudaq::Producer
class HGCalProducer : public eudaq::Producer {
public:

  // The constructor must call the eudaq::Producer constructor with the name
  // and the runcontrol connection string, and initialize any member variables.
  HGCalProducer(const std::string & name, const std::string & runcontrol)
    : eudaq::Producer(name, runcontrol),
      m_run(0), m_ev(0), stopping(false), done(false),started(0) {}

  int tryCopyFile(std::string localFilePath, std::string hostUsername, std::string hostAddress, std::string targetDir) {
    int returnValue;
    returnValue = system((HGCAL_SPECIFIC_DIR + "/copyHelper.sh " + localFilePath + " " + hostUsername + " " + hostAddress + " " + targetDir).data());
    return returnValue;
  }

  int tryExecuteScript(std::string localScriptDir, std::string scriptName, std::string hostUsername, std::string hostAddress, std::string targetDir) {
    int returnValue;
    returnValue = system((HGCAL_SPECIFIC_DIR + "/executeScriptHelper.sh " + localScriptDir + " " + scriptName + " " + hostUsername + " " + hostAddress + " " + targetDir).data());
    return returnValue;
  }

  // This gets called whenever the DAQ is configured
  virtual void OnConfigure(const eudaq::Configuration & config) {
    std::cout << "Configuring: " << config.Name() << std::endl;

    bool configurationSuccessful = true;

    // Do any configuration of the hardware here
    //   Configuration file values are accessible as config.Get(name, default)
    m_pathToExampleFile = config.Get("pathToExampleFile", "");
    m_exampleparam = config.Get("Parameter", 0);
    std::cout << "Example Parameter = " << m_exampleparam << std::endl;
    hardware.Setup(m_exampleparam);
    m_commonHostnamePrefix = config.Get("commonHostnamePrefix", "");
    m_piCounter = config.Get("piCounter", 0);
    m_localScriptDir = config.Get("localScriptDir", "");
    m_testScriptName = config.Get("testScriptName", "");

    std::cout << "Checking..." << std::endl
              << "m_pathToExampleFile: " << m_pathToExampleFile << std::endl
              << "m_commonHostnamePrefix: " << m_commonHostnamePrefix << std::endl
              << "m_piCounter: " << m_piCounter << std::endl
              << "m_localScriptDir: " << m_localScriptDir << std::endl
              << "m_testScriptName: " << m_testScriptName << std::endl;

    char formattedPiCounterChar[2];
    sprintf(formattedPiCounterChar, "%02d", m_piCounter);
    std::string formattedPiCounter(formattedPiCounterChar);
    std::string piHostAddress = m_commonHostnamePrefix + formattedPiCounter;
    
    EUDAQ_INFO(("Testing copying file to RPi number " + eudaq::to_string(m_piCounter) + " with host address " + piHostAddress).data());
    int copyAttempt = tryCopyFile(m_pathToExampleFile, "pi", piHostAddress, "test");
    if (copyAttempt != 0) {
      EUDAQ_ERROR(("Command to copy exited with error code " + eudaq::to_string(copyAttempt)).data());
      configurationSuccessful = false;
    }
    else {
      EUDAQ_INFO("Command to copy file successful!");
    }

    EUDAQ_INFO(("Testing executing script on RPi number " + eudaq::to_string(m_piCounter) + " with host address " + piHostAddress).data());
    int executeAttempt = tryExecuteScript(m_localScriptDir, m_testScriptName, "pi", piHostAddress, "test");
    if (executeAttempt != 0) {
      EUDAQ_ERROR(("Command to execute script exited with error code " + eudaq::to_string(executeAttempt)).data());
      configurationSuccessful = false;
    }
    else {
      EUDAQ_INFO("Command to execute script successful!");
    }

    if (configurationSuccessful) {
      EUDAQ_INFO(("Successfuly configured RPi number " + eudaq::to_string(m_piCounter)).data());
      // At the end, set the status that will be displayed in the Run Control.
      // SetStatus(eudaq::Status::LVL_OK, "Configured (" + config.Name() + ")");
      SetStatus(eudaq::Status::LVL_OK, "Configured pi number " + eudaq::to_string(m_piCounter) + " (" + config.Name() + ")");
    }
    else {
      EUDAQ_ERROR(("Could not configure RPi number " + eudaq::to_string(m_piCounter)).data());
      SetStatus(eudaq::Status::LVL_ERROR, "Error in configuring pi number " + eudaq::to_string(m_piCounter) + " (" + config.Name() + ")");
    }
  }

  // This gets called whenever a new run is started
  // It receives the new run number as a parameter
  virtual void OnStartRun(unsigned param) {
    m_run = param;
    m_ev = 0;
	  
    std::cout << "Start Run: " << m_run << std::endl;

    // It must send a BORE to the Data Collector
    eudaq::RawDataEvent bore(eudaq::RawDataEvent::BORE(EVENT_TYPE, m_run));
    // You can set tags on the BORE that will be saved in the data file
    // and can be used later to help decoding
    bore.SetTag("EXAMPLE", eudaq::to_string(m_exampleparam));
    // Send the event to the Data Collector
    SendEvent(bore);

    // At the end, set the status that will be displayed in the Run Control.
    SetStatus(eudaq::Status::LVL_OK, "Running");
    started=true;
  }

  // This gets called whenever a run is stopped
  virtual void OnStopRun() {
    std::cout << "Stopping Run" << std::endl;
    started=false;
    // Set a flag to signal to the polling loop that the run is over
    stopping = true;

    // wait until all events have been read out from the hardware
    while (stopping) {
      eudaq::mSleep(20);
    }

    // Send an EORE after all the real events have been sent
    // You can also set tags on it (as with the BORE) if necessary
    SendEvent(eudaq::RawDataEvent::EORE("Test", m_run, ++m_ev));
  }

  // This gets called when the Run Control is terminating,
  // we should also exit.
  virtual void OnTerminate() {
    std::cout << "Terminating..." << std::endl;
    done = true;
  }

  // This is just an example, adapt it to your hardware
  void ReadoutLoop() {
    // Loop until Run Control tells us to terminate
    while (!done) {
      if (!hardware.EventsPending()) {
        // No events are pending, so check if the run is stopping
        if (stopping) {
          // if so, signal that there are no events left
          stopping = false;
        }
        // Now sleep for a bit, to prevent chewing up all the CPU
        eudaq::mSleep(20);
        // Then restart the loop
        continue;
      }
      if (!started)
        {
          // Now sleep for a bit, to prevent chewing up all the CPU
          eudaq::mSleep(20);
          // Then restart the loop
          continue;
        }
      // If we get here, there must be data to read out
      // Create a RawDataEvent to contain the event data to be sent
      eudaq::RawDataEvent ev(EVENT_TYPE, m_run, m_ev);

      for (unsigned plane = 0; plane < hardware.NumSensors(); ++plane) {
        // Read out a block of raw data from the hardware
        std::vector<unsigned char> buffer = hardware.ReadSensor(plane);
        // Each data block has an ID that is used for ordering the planes later
        // If there are multiple sensors, they should be numbered incrementally

        // Add the block of raw data to the event
        ev.AddBlock(plane, buffer);
      }
      hardware.CompletedEvent();
      // Send the event to the Data Collector      
      SendEvent(ev);
      // Now increment the event number
      m_ev++;
    }
  }

private:
  // This is just a dummy class representing the hardware
  // It here basically that the example code will compile
  // but it also generates example raw data to help illustrate the decoder
  eudaq::ExampleHardware hardware;
  unsigned m_run, m_ev, m_exampleparam;
  std::string m_commonHostnamePrefix;
  std::string m_pathToExampleFile;
  unsigned m_piCounter;
  std::string m_localScriptDir;
  std::string m_testScriptName;
  bool stopping, done,started;
};

// The main function that will create a Producer instance and run it
int main(int /*argc*/, const char ** argv) {
  // You can use the OptionParser to get command-line arguments
  // then they will automatically be described in the help (-h) option
  eudaq::OptionParser op("EUDAQ Example Producer", "1.0",
                         "Just an example, modify it to suit your own needs");
  eudaq::Option<std::string> rctrl(op, "r", "runcontrol",
                                   "tcp://localhost:44000", "address",
                                   "The address of the RunControl.");
  eudaq::Option<std::string> level(op, "l", "log-level", "NONE", "level",
                                   "The minimum level for displaying log messages locally");
  eudaq::Option<std::string> name (op, "n", "name", "Example", "string",
                                   "The name of this Producer");
  try {
    // This will look through the command-line arguments and set the options
    op.Parse(argv);
    // Set the Log level for displaying messages based on command-line
    EUDAQ_LOG_LEVEL(level.Value());
    // Create a producer
    HGCalProducer producer(name.Value(), rctrl.Value());
    // And set it running...
    producer.ReadoutLoop();
    // When the readout loop terminates, it is time to go
    std::cout << "Quitting" << std::endl;
  } catch (...) {
    // This does some basic error handling of common exceptions
    return op.HandleMainException();
  }
  return 0;
}
