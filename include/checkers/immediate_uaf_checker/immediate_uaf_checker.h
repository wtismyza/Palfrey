#ifndef _immediate_uaf_checker_H
#define _immediate_uaf_checker_H
#include "framework/BasicChecker.h"
#include "framework/Common.h"

#include <vector>

#include <clang/AST/Expr.h>
#include <clang/Analysis/CFG.h>

#include <sstream>
#include <string>
#include <iostream>
#include <map>
#include <time.h>
#include <utility>
#include <unordered_set>
#include <set>
#include <stack>

using namespace std;

class immediate_uaf_checker : public BasicChecker {
private:
	FindFreeMethod* finder;
    int checkDepth;
	std::unordered_set<const Stmt*> freeCallCites;
public:

	immediate_uaf_checker(ASTResource *resource, ASTManager *manager,
                  CallGraph *call_graph, Config *configure)
      : BasicChecker(resource, manager, call_graph, configure){
        std::unordered_map<std::string, std::string> ptrConfig =configure->getOptionBlock("immediate_uaf_checker");
        if (ptrConfig.find("checkDepth") != ptrConfig.end()){
            checkDepth = stoi(ptrConfig.find("checkDepth")->second);
        }
		finder = new FindFreeMethod();
	}

    ~immediate_uaf_checker() {
		delete finder;
		finder = nullptr;
    }
	void check();
	void lableAllFreeMethod();
	void checkFreeMethod(ASTFunction* astF);
	const VarDecl* getVarDecl(const Expr* e);	
	const ParmVarDecl* isParameter(const Expr* e);
	int getParameterPosition(FunctionDecl* FD, const ParmVarDecl* p);
	bool isFreedArgsReferenced(const Stmt* stmt, std::unordered_set<const Expr*>& freeargs);
	bool checkSuccessor(const Stmt* stmt,CFGBlock* block, std::unordered_set<const Expr*>& freeargs, const clang::SourceManager& SM,int depth);
};

#endif
