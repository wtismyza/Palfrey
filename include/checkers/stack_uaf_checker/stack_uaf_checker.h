#ifndef _stack_uaf_checker_H
#define _stack_uaf_checker_H
#include "framework/BasicChecker.h"
#include "framework/Common.h"
#include "P2A/PointToAnalysis.h"

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


class FindReturnStmt : public StmtVisitor<FindReturnStmt>
{
private:
    std::vector<ReturnStmt *> retStmts;

public:
    void VisitChildren(Stmt *stmt)
    {
        for (Stmt *SubStmt : stmt->children())
        {
            if (SubStmt)
            {
                this->Visit(SubStmt);
            }
        }
    }
    void VisitStmt(Stmt *expr)
    {
        if (ReturnStmt *ret = dyn_cast<ReturnStmt>(expr))
        {
            retStmts.push_back(ret);
        }
        VisitChildren(expr);
    }
    std::vector<ReturnStmt *> &getReturnStmts()
    {
        return retStmts;
    }
};

class stack_uaf_checker : public BasicChecker {
private:
    std::unordered_map<std::string, std::string> ptrConfig;
    PointToAnalysis* PTA;
public:
	stack_uaf_checker(ASTResource *resource, ASTManager *manager,
                  CallGraph *call_graph, Config *configure, PointToAnalysis* pta)
      : BasicChecker(resource, manager, call_graph, configure){
        ptrConfig =configure->getOptionBlock("stack_uaf_checker");
        PTA = pta;
	}

    ~stack_uaf_checker() {

    }
	void check();
    bool isAliasOfLocalVar(ASTFunction* astF, DeclRefExpr* decl);
};

#endif
