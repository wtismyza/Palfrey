#include "checkers/realloc_checker/realloc_checker.h"

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

class FindCalledExpr : public StmtVisitor<FindCalledExpr> {
private:
    std::vector<CallExpr*> reallocCalls;  
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
			if (FunctionDecl *FD = c->getDirectCallee()) {
				std::string fullName=common::getFullName(FD);
				if (fullName == "realloc") {
					reallocCalls.push_back(c);
				}
			}
        }
		else 
       		VisitChildren(expr);
    }

    std::vector<CallExpr*> getReallocCalls()
    {
    	return reallocCalls;
    }


};

void realloc_checker::check() {
	clock_t start = clock();
    for(auto F:resource->getFunctions()) {
		FunctionDecl *FD = manager->getFunctionDecl(F);
		std::unique_ptr<CFG> &function_cfg = manager->getCFG(F);
	/*
		FunctionDecl *Func = common::manager->getFunctionDecl(astFunc);
	std::unique_ptr<CFG> & myCFG= common::manager->getCFG(astFunc,Func);*/	
		const clang::SourceManager &SM = FD->getASTContext().getSourceManager();
		string filename = "";
		if(FD->hasBody())
			FD=FD->getDefinition();
		else
			continue;
		SourceLocation SL = FD->getBeginLoc();
		filename = SL.printToString(SM);
		if (filename.find(".h") != string::npos || filename.find("include") != string::npos)
			continue;
		common::printLog( "Start check function "+FD->getNameAsString()+"\n",common::CheckerName::stack_uaf_checker,4,*configure);
		CFG* cfg = function_cfg.get();
		for(CFG::iterator iter=cfg->begin();iter!=cfg->end();++iter){
			CFGBlock* block=*iter;
			for(CFGBlock::iterator iter=block->begin();iter!=block->end();iter++)
			{
				CFGElement element=*(iter);
				if(element.getKind()==CFGElement::Kind::Statement)
				{
					const Stmt* it=((CFGStmt*)&element)->getStmt();

				}
			}
		}		
		
    }
    
}
