#ifndef _loop_doublefree_checker_H
#define _loop_doublefree_checker_H
#include "framework/BasicChecker.h"
#include "framework/Common.h"

#include <vector>
#include <clang/AST/StmtVisitor.h>
#include <clang/AST/Expr.h>
#include <clang/Analysis/CFG.h>
#include <clang/AST/RecursiveASTVisitor.h>
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

class loop_doublefree_checker : public BasicChecker {
private:
	FindFreeMethod* finder;
	std::unordered_set<const Stmt*> freeCallCites;
public:

	loop_doublefree_checker(ASTResource *resource, ASTManager *manager,
                  CallGraph *call_graph, Config *configure)
      : BasicChecker(resource, manager, call_graph, configure){
        std::unordered_map<std::string, std::string> ptrConfig =configure->getOptionBlock("loop_doublefree_checker");
		finder = new FindFreeMethod();
	}

    ~loop_doublefree_checker() {
		delete finder;
		finder = nullptr;
    }
	void check();
	void lableAllFreeMethod();
	void checkFreeMethod(ASTFunction* astF);
	const VarDecl* getVarDecl(const Expr* e);	
	const ParmVarDecl* isParameter(const Expr* e);
	int getParameterPosition(FunctionDecl* FD, const ParmVarDecl* p);
	bool isFreedArgsFreed(const Stmt* stmt, std::unordered_set<const Expr*>& freeargs,const clang::SourceManager& SM);
	bool checkSucceedBlocks(CFGBlock* block, std::unordered_set<const Expr*>& freeargs, const clang::SourceManager &SM, const Stmt* stmt,int depth);

};

#endif
