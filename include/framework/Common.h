#ifndef BASE_COMMON_H
#define BASE_COMMON_H

#include <vector>
#include <unordered_map>
#include <clang/AST/StmtVisitor.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Analysis/CFG.h>
#include "Config.h"

using namespace clang;

std::vector<std::string> initialize(std::string astList);

void prcoess_bar(float progress);

class FindFreeMethod : public StmtVisitor<FindFreeMethod> {
private:
    std::vector<const CallExpr*> freeCalls;  
	//eg, freecall(xx0,xx1), xx1 is freed
	std::unordered_map<const FunctionDecl*, std::vector<bool>> AllFreeMethods;
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
        if (CallExpr *c = dyn_cast<CallExpr>(expr))
        {
			if (FunctionDecl *callee = c->getDirectCallee()) {
				std::string funcName =  callee->getNameInfo().getName().getAsString();
				if (funcName == "free") {
					std::vector<bool> t;
					t.push_back(true);
					AllFreeMethods.insert(std::make_pair(callee,t));
					freeCalls.push_back(c);
					return;
				}
				else{
					auto findfd = AllFreeMethods.find(callee);
					if (findfd != AllFreeMethods.end()){
						freeCalls.push_back(c);					
					}
				}
			}
        }
		else 
       		VisitChildren(expr);
    }

    std::vector<const CallExpr*> getFreeCalls()
    {
    	return freeCalls;
    }
	void clearFreeCalls(){
		freeCalls.clear();
	}
	void updateFreeMethods(const FunctionDecl* F, std::vector<bool> parms) {
		AllFreeMethods.insert(std::make_pair(F,parms));
	}
	
	std::vector<bool> isFreeMethod(const FunctionDecl* F) {
		std::vector<bool> r;
		auto findfd = AllFreeMethods.find(F);
		if (findfd != AllFreeMethods.end()){
			return findfd->second;		
		}	
		return r;
	}
};


class FindUAFVarDeclRef : public RecursiveASTVisitor<FindUAFVarDeclRef> {
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

namespace common {

enum CheckerName {
  stack_uaf_checker,
  immediate_uaf_checker,
  loop_doublefree_checker,
  borrowed_reference_checker,
  realloc_checker,
  memory_alloc_checker
};
/**
 * 判断某一个call site是不是一个函数指针引起
 */
bool isThisCallSiteAFunctionPointer(Stmt *callsite);
/**
 * 根据ID获取实际的Stmt。主要应用于获取call site
 * @param  parent 于call site处调用了某一函数的函数。
 * @param  id     Stmt的ID，通过Stmt->getID(ASTContext&)获取
 */
Stmt *getStmtInFunctionWithID(FunctionDecl *parent, int64_t id);

std::string getLambdaName(FunctionDecl *FD);

std::unique_ptr<ASTUnit> loadFromASTFile(std::string AST);

std::vector<FunctionDecl *> getFunctions(ASTContext &Context);
std::vector<FunctionDecl *> getFunctions(ASTContext &Context,
                                         SourceLocation SL);

std::vector<VarDecl *> getGlobalVars(ASTContext &Context);

std::vector<VarDecl *> getVariables(FunctionDecl *FD);

std::vector<std::pair<VarDecl *, FieldDecl *>> getFieldVariables(FunctionDecl *FD);

//获取FD调用的函数
std::vector<std::string> getCalledFunctions(
    FunctionDecl *FD,
    const std::unordered_map<std::string, std::string> &configure);

//获取FD调用的函数与函数调用点(callsite)
std::vector<std::pair<std::string, int64_t>> getCalledFunctionsInfo(
    FunctionDecl *FD,
    const std::unordered_map<std::string, std::string> &configure);

std::vector<std::pair<int64_t, std::set<std::string>>> getFunctionPtrWithCS(
    FunctionDecl *FD,
    std::unordered_map<std::string, std::set<std::string>> &mayPointTo);

std::vector<FunctionDecl *> getCalledLambda(FunctionDecl *FD);

std::vector<CallExpr *> getCallExpr(FunctionDecl *FD);

std::string getFullName(const FunctionDecl *FD);

void printLog(std::string, CheckerName cn, int level, Config &c);

const CFGBlock *getBlockWithID(const std::unique_ptr<CFG> &cfg,unsigned id);
std::vector<CFGBlock *> getNonRecursiveSucc(CFGBlock *curBlock);


template <class T> void dumpLog(T &t, CheckerName cn, int level, Config &c) {
  auto block = c.getOptionBlock("PrintLog");
  int l = atoi(block.find("level")->second.c_str());
  switch (cn) {
  case common::CheckerName::stack_uaf_checker:
    if (block.find("stack_uaf_checker")->second == "true" && level >= l) {
      t.dump();
    }
    break;
  case common::CheckerName::immediate_uaf_checker:
    if (block.find("immediate_uaf_checker")->second == "true" && level >= l) {
      t.dump();
    }
    break;
  case common::CheckerName::loop_doublefree_checker:
    if (block.find("loop_doublefree_checker")->second == "true" && level >= l) {
      t.dump();
    }
    break;        
  case common::CheckerName::borrowed_reference_checker:
    if (block.find("borrowed_reference_checker")->second == "true" && level >= l) {
      t.dump();
    }
    break;  
  case common::CheckerName::realloc_checker:
    if (block.find("realloc_checker")->second == "true" && level >= l) {
      t.dump();
    }
    break;
  case common::CheckerName::memory_alloc_checker:
    if (block.find("memory_alloc_checker")->second == "true" && level >= l) {
      t.dump();
    }
    break;    
  }  
}
std::string print(const Stmt* stmt);
std::unordered_set<std::string> split(std::string text);
} // end of namespace common

namespace fix {
class POffset {
public:
  std::string type;
  std::string val;
  bool needBracket;
  POffset(std::string t, std::string v) : type(t), val(v), needBracket(false) {}
  POffset() {}
  POffset operator+(const POffset &s);
  POffset operator+(const std::string &s);
};

class BufCalculator {
public:
  std::string pType;
  std::string pName;
  // string unit = "char";
  POffset calculateExpr(Stmt *stmt);
  BufCalculator(std::string type, std::string name)
      : pType(type), pName(name){};
  std::string calculateBufLen(std::string bufSize, POffset bufOffset,
                              std::string type);
  // POffset cast(POffset p, string type);
};

std::string Trim(std::string s);
// Context getContext(ASTContext* context, Stmt *expr);
std::string getSourceCode(const Stmt *stmt);
// string rmBrackets(string str);
SourceLocation getEnd(SourceManager &sm, const Stmt *expr);
void split(std::vector<std::string> &res, std::string str,
           std::string delimiter = ",");
bool contains(const Stmt *a, const Stmt *b);
Stmt *findChild(Stmt *parent, std::string child);
std::pair<std::string, std::string> getArrayTypeLen(FunctionDecl *funcDecl,
                                                    Stmt *pointer);
bool isCharKind(std::string type);

std::pair<std::string, std::string> getTypeLenFromStr(std::string str);

} // end of namespace fix

#endif
