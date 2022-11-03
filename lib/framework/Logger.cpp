#include "framework/Logger.h"

TLogLevel Logger::configurationLevel = LOG_DEBUG_3;
TCheck Logger::checkType = ReturnStackAddr_Checker;
bool Logger::options[6] = {false,false,false,false,false,false};
//bool Logger::options[6] = {false, false, false, false, false, false};
void Logger::configure(Config &c) {
  auto block = c.getOptionBlock("PrintLog");
  int level = atoi(block.find("level")->second.c_str());
  switch (level) {
  case 1:
    configurationLevel = LOG_DEBUG_1;
    break;
  case 2:
    configurationLevel = LOG_DEBUG_2;
    break;
  case 3:
    configurationLevel = LOG_DEBUG_3;
    break;
  case 4:
    configurationLevel = LOG_DEBUG_4;
    break;
  case 5:
    configurationLevel = LOG_DEBUG_5;
    break;
  default:
    configurationLevel = LOG_DEBUG_3;
  }

  if (block.find("stack_uaf_checker")->second == "true")
    options[ReturnStackAddr_Checker] = true;
  if (block.find("immediate_uaf_checker")->second == "true")
    options[UAFChecker_2] = true;    
  if (block.find("loop_doublefree_checker")->second == "true")
    options[DoubleFreeChecker_3] = true;        
  if (block.find("borrowed_reference_checker")->second == "true")
    options[RefCountChecker_4] = true;    
  if (block.find("realloc_checker")->second == "true")
    options[Checker_5] = true;
  if (block.find("memory_alloc_checker")->second == "true")
    options[Checker_alloc] = true;
}
