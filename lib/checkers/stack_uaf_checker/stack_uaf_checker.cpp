#include "checkers/stack_uaf_checker/stack_uaf_checker.h"

#include "clang/Frontend/FrontendAction.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/Decl.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/MemoryBuffer.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Frontend/ASTUnit.h"

using namespace std;

void stack_uaf_checker::check() {
	clock_t start = clock();
    for(auto F:resource->getFunctions()) {
		FunctionDecl *FD = manager->getFunctionDecl(F);
		if (!FD) continue;
		std::unique_ptr<CFG> &function_cfg = manager->getCFG(F);
	/*
		FunctionDecl *Func = common::manager->getFunctionDecl(astFunc);
	std::unique_ptr<CFG> & myCFG= common::manager->getCFG(astFunc,Func);*/	
		const clang::SourceManager& SM = FD->getASTContext().getSourceManager();
		string filename = "";
		if(FD->hasBody())
			FD=FD->getDefinition();
		else
			continue;
		SourceLocation SL = FD->getBeginLoc();
		filename = SL.printToString(SM);
		if (filename.find(".h") != string::npos || filename.find("include") != string::npos)
			continue;
		common::printLog( "In function "+FD->getNameAsString()+"\n",common::CheckerName::stack_uaf_checker,2,*configure);
		FindReturnStmt returnFinder;
		returnFinder.Visit(FD->getBody());
		std::vector<ReturnStmt*> returnStmts = returnFinder.getReturnStmts();
		for (auto ret : returnStmts) {
			if (!ret->getRetValue()) continue;
            if (UnaryOperator* uo = dyn_cast<UnaryOperator>(ret->getRetValue()->IgnoreCasts()->IgnoreParens())) {
				if (uo->getOpcode() == UO_AddrOf) {//return &xx
					Expr* sub = uo->getSubExpr();
					common::printLog( "return address of "+common::print(sub)+"\n",common::CheckerName::stack_uaf_checker,2,*configure);
					if (DeclRefExpr* decl = dyn_cast<DeclRefExpr>(sub)) {
						VarDecl* vardecl = (VarDecl*)(decl->getDecl());
						if (vardecl->isStaticLocal() ) continue;
						if (vardecl->isLocalVarDecl()) {						
							common::printLog( "Warning: return address of local declared variable "+common::print(ret)+" at "+ret->getBeginLoc().printToString(SM)+"\n",common::CheckerName::stack_uaf_checker,5,*configure);
						}
					}
				}
			}
		}
		
		
    }
    
}

bool stack_uaf_checker::isAliasOfLocalVar(ASTFunction* astF, DeclRefExpr* decl){
    if(ptrConfig.find("PointToAnalysis") != ptrConfig.end() && ptrConfig. find("PointToAnalysis")->second == "false"){
        return false;
    }    
    FunctionDecl* f = manager->getFunctionDecl(astF);
    //    int* p = &value; return p;
   if (clang::VarDecl* varDecl = dyn_cast<VarDecl>(decl->getDecl())){
        std::string vStr = PTA->getPTAVarKey(varDecl);
        const PTAVar* vPTA = PTA->getPTAVar(vStr);
        std::set<const PTAVar *> aliasSet = PTA->get_alias_in_func(astF,vPTA);
        for (auto aliasVar : aliasSet) {
            std::string aliasStr = aliasVar->get_instance_var_key();
            ///home/gfj/ESAF/tests/Arraybound/InterCheck/example.cpp:5:28:t
            int firstColonInBegin = aliasStr.find(":");
            std::string fileName = aliasStr.substr(0,firstColonInBegin);
            int secondColonInBegin = aliasStr.find(":", firstColonInBegin + 1);
            std::string locLinesInBegin = aliasStr.substr(firstColonInBegin + 1, secondColonInBegin - firstColonInBegin - 1 );
            int line = std::stoi(locLinesInBegin);
            Stmt* aliasStmt = manager->getStmtWithLoc(fileName,line);
            if (aliasStmt) {
                CFGBlock* aliasBlock = manager->getBlockWithLoc(fileName,line);
                std::vector<Stmt*> aliasStmts = manager->getStmtWithLoc(line,aliasBlock);
                for(auto as : aliasStmts) {
                    if (!as) continue;
                    as->dump();
                } 
            }
            /*
            else {
                std::string paraName = aliasStr.substr(aliasStr.find_last_of(":"));
                std::string paraLoc = aliasStr.substr(0,aliasStr.find_last_of(":"));
                SourceManager *sm;
                sm = &(f->getASTContext().getSourceManager()); 
                std::string floc = f->getBeginLoc().printToString(*sm);
                if (floc.substr(0,floc.find_last_of(":")) == fileName+":"+locLinesInBegin) {
                    unsigned paramIdx = 0;
                    for (unsigned i=0;i<f->getNumParams();i++){
                        ParmVarDecl* p = f->getParamDecl(i);
                        std::string ploc = p->getBeginLoc().printToString(*sm);
                        if (ploc == paraLoc) {
                            paramIdx = i;
                              
                        }
                    }
                }
            }*/
        }
    }
}
