#ifndef _borrowed_reference_checker_H
#define _borrowed_reference_checker_H
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

class FindVarDeclRef : public RecursiveASTVisitor<FindVarDeclRef> {
private:
	std::unordered_set<const VarDecl*> vars;
public:
    bool VisitDeclRefExpr(const DeclRefExpr *expr) {
        if (expr) {
            VarDecl* v = (VarDecl*)(expr->getDecl());
			vars.insert(v);
        }
        return true;
    }	
    bool isVarDeclUsed(const VarDecl* v) {
        auto findv = vars.find(v);
		return findv != vars.end();
    }
};

class borrowed_reference_checker : public BasicChecker {
private:
	std::unordered_set<std::string> GetBorrowedRefFunctions;
	std::unordered_set<std::string> SkipedFunctions;
	std::unordered_set<std::string> CheckFunctions;
	std::unordered_map<const VarDecl*, const Stmt*> borrowedRefVars;

public:
	borrowed_reference_checker(ASTResource *resource, ASTManager *manager,
                  CallGraph *call_graph, Config *configure)
      : BasicChecker(resource, manager, call_graph, configure){

        std::unordered_map<std::string, std::string> ptrConfig =configure->getOptionBlock("borrowed_reference_checker");
		if(ptrConfig.find("GetBorrowedRefFunctions") != ptrConfig.end()){
    		std::string s = ptrConfig.find("GetBorrowedRefFunctions")->second;
			GetBorrowedRefFunctions = common::split(s);
		}
		if(ptrConfig.find("SkipedFunctions") != ptrConfig.end()){
    		std::string s = ptrConfig.find("SkipedFunctions")->second;
			SkipedFunctions = common::split(s);
		}		
		if(ptrConfig.find("CheckFunctions") != ptrConfig.end()){
    		std::string s = ptrConfig.find("CheckFunctions")->second;
			CheckFunctions = common::split(s);
		}						
	}

    ~borrowed_reference_checker() {

    }
	void check();
	bool isGetBorrowedRefFunctions(std::string s);
	bool isSkipedFunctions(std::string s);
	bool hasSkipedFunctions(std::string f);
	bool isCheckFunction(std::string s);
	std::string hasCheckFunctions(std::string f);
	const Expr* getFirstArg(std::string funcName, const Expr* expr);
};

#endif
