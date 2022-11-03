#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <llvm-c/Target.h>
#include <llvm/Support/CommandLine.h>

//#include "checkers/TemplateChecker.h"
#include "framework/ASTManager.h"
#include "framework/BasicChecker.h"
#include "framework/CallGraph.h"
#include "framework/Config.h"
#include "framework/Logger.h"

#include "CFG/InterProcedureCFG.h"
#include "P2A/PointToAnalysis.h"

#include "checkers/stack_uaf_checker/stack_uaf_checker.h"
#include "checkers/immediate_uaf_checker/immediate_uaf_checker.h"
#include "checkers/loop_doublefree_checker/loop_doublefree_checker.h"
#include "checkers/borrowed_reference_checker/borrowed_reference_checker.h"
#include "checkers/realloc_checker/realloc_checker.h"
#include "checkers/memory_alloc_checker/memory_alloc_checker.h"

using namespace clang;
using namespace llvm;
using namespace clang::tooling;

int main(int argc, const char *argv[])
{
  ofstream process_file("time.txt");
  if (!process_file.is_open())
  {
    cerr << "can't open time.txt\n";
    return -1;
  }
  clock_t startCTime, endCTime;
  startCTime = clock();

  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmParser();

  std::vector<std::string> ASTs = initialize(argv[1]);

  Config configure(argv[2]);

  ASTResource resource;
  ASTManager manager(ASTs, resource, configure);
  CallGraph *call_graph = nullptr;
  PointToAnalysis *pta = nullptr;
  
  auto enable = configure.getOptionBlock("CheckerEnable");

// point to analysis
  if (enable.find("PointToAnalysis")->second == "true")
  {
    process_file << "Starting InterProcedureCFG Construct" << endl;
    clock_t start_icfg, end_icfg;
    start_icfg = clock();
    InterProcedureCFG *icfgPtr = nullptr;
    if (call_graph)
      icfgPtr = new InterProcedureCFG(&manager, &resource, call_graph, &configure);
    end_icfg = clock();
    unsigned sec_icfg = unsigned((end_icfg - start_icfg) / CLOCKS_PER_SEC);
    unsigned min_icfg = sec_icfg / 60;
    process_file << "Time: " << min_icfg << "min" << sec_icfg % 60 << "sec"
                 << endl;
    process_file
        << "End of InterProcedureCFG "
           "Construct\n------------------------------------------------"
           "-----------"
        << endl;

    process_file << "Starting Point-to Analysis" << endl;
    clock_t start, end;
    start = clock();
    pta = new PointToAnalysis(&manager, icfgPtr, configure.getOptionBlock("P2A"));
    pta->ConstructPartialCFG();

    end = clock();
    unsigned sec = unsigned((end - start) / CLOCKS_PER_SEC);
    unsigned min = sec / 60;
    process_file << "Time: " << min << "min" << sec % 60 << "sec" << endl;
    process_file << "End of Point-to "
                    "Analysis\n------------------------------------------------"
                    "-----------"
                 << endl;
  }

  Logger::configure(configure);
  if (enable.find("stack_uaf_checker")->second == "true")
  {
    std::cout << "Starting stack_uaf_checker" << endl;
    clock_t start, end;
    // record the time of generating call graph
    start = clock();
    stack_uaf_checker checker1(&resource, &manager, call_graph, &configure, pta);
    checker1.check();
    end = clock();

    unsigned sec = unsigned((end - start) / CLOCKS_PER_SEC);
    unsigned min = sec / 60;
    std::cout << "Time: " << min << "min" << sec % 60 << "sec" << endl;
    std::cout
        << "End of stack_uaf_checker "
           "check\n-----------------------------------------------------------"
        << endl;
  }

  if (enable.find("immediate_uaf_checker")->second == "true")
  {
    std::cout << "Starting immediate_uaf_checker" << endl;
    clock_t start, end;
    // record the time of generating call graph
    start = clock();
    immediate_uaf_checker checker2(&resource, &manager, call_graph, &configure);
    checker2.check();
    end = clock();

    unsigned sec = unsigned((end - start) / CLOCKS_PER_SEC);
    unsigned min = sec / 60;
    std::cout << "Time: " << min << "min" << sec % 60 << "sec" << endl;
    std::cout
        << "End of immediate_uaf_checker "
           "check\n-----------------------------------------------------------"
        << endl;
  }

  if (enable.find("loop_doublefree_checker")->second == "true")
  {
    std::cout << "Starting loop_doublefree_checker" << endl;
    clock_t start, end;
    // record the time of generating call graph
    start = clock();
    loop_doublefree_checker checker3(&resource, &manager, call_graph, &configure);
    checker3.check();
    end = clock();

    unsigned sec = unsigned((end - start) / CLOCKS_PER_SEC);
    unsigned min = sec / 60;
    std::cout << "Time: " << min << "min" << sec % 60 << "sec" << endl;
    std::cout
        << "End of loop_doublefree_checker "
           "check\n-----------------------------------------------------------"
        << endl;
  }

  

  if (enable.find("borrowed_reference_checker")->second == "true")
  {
    std::cout << "Starting borrowed_reference_checker" << endl;
    clock_t start, end;
    // record the time of generating call graph
    start = clock();
    borrowed_reference_checker checker4(&resource, &manager, call_graph, &configure);
    checker4.check();
    end = clock();

    unsigned sec = unsigned((end - start) / CLOCKS_PER_SEC);
    unsigned min = sec / 60;
    std::cout << "Time: " << min << "min" << sec % 60 << "sec" << endl;
    std::cout
        << "End of borrowed_reference_checker "
           "check\n-----------------------------------------------------------"
        << endl;
  }

  if (enable.find("realloc_checker")->second == "true")
  {
    std::cout << "Starting realloc_checker" << endl;
    clock_t start, end;
    // record the time of generating call graph
    start = clock();
    realloc_checker realloc_checker(&resource, &manager, call_graph, &configure);
    realloc_checker.check();
    end = clock();

    unsigned sec = unsigned((end - start) / CLOCKS_PER_SEC);
    unsigned min = sec / 60;
    std::cout << "Time: " << min << "min" << sec % 60 << "sec" << endl;
    std::cout
        << "End of realloc_checker "
           "check\n-----------------------------------------------------------"
        << endl;
  }

  if (enable.find("memory_alloc_checker")->second == "true")
  {
    std::cout << "Starting memory_alloc_checker" << endl;
    clock_t start, end;
    // record the time of generating call graph
    start = clock();
    memory_alloc_checker memory_alloc_checker(&resource, &manager, call_graph, &configure);
    memory_alloc_checker.check();
    end = clock();

    unsigned sec = unsigned((end - start) / CLOCKS_PER_SEC);
    unsigned min = sec / 60;
    std::cout << "Time: " << min << "min" << sec % 60 << "sec" << endl;
    std::cout
        << "End of memory_alloc_checker "
           "check\n-----------------------------------------------------------"
        << endl;
  }  

  endCTime = clock();
  unsigned sec = unsigned((endCTime - startCTime) / CLOCKS_PER_SEC);
  unsigned min = sec / 60;
  std::cout << "-----------------------------------------------------------"
                  "\nTotal time: "
               << min << "min" << sec % 60 << "sec" << endl;
  return 0;
}
