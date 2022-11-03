#include "checkers/immediate_uaf_checker/immediate_uaf_checker.h"

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

void immediate_uaf_checker::check() {
	lableAllFreeMethod();
    for(auto F:resource->getFunctions()) {
		FunctionDecl *FD = manager->getFunctionDecl(F);
		std::unique_ptr<CFG> &function_cfg = manager->getCFG(F);
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
		common::printLog( "Start check function "+FD->getNameAsString()+"\n",common::CheckerName::immediate_uaf_checker,2,*configure);
		CFG* cfg = function_cfg.get();
		for(CFG::iterator cfgiter=cfg->begin();cfgiter!=cfg->end();++cfgiter){
			CFGBlock* block=*cfgiter;
			for(CFGBlock::iterator blockiter=block->begin();blockiter!=block->end();blockiter++)
			{
				CFGElement element=*(blockiter);
				if(element.getKind()==CFGElement::Kind::Statement)
				{
					const Stmt* stmt=((CFGStmt*)&element)->getStmt();
					const FunctionDecl *callee = nullptr;
					std::unordered_set<const Expr*> freeargs;
					bool uafFlag=false;
					if (const CXXDeleteExpr* deletexpr = dyn_cast<CXXDeleteExpr>(stmt)) {
						//callee = deletexpr->getOperatorDelete();
						//std::string funcName =  callee->getNameInfo().getName().getAsString();
						//cout<<funcName<<endl;
						const Expr* arg = deletexpr->getArgument();
						common::printLog( "free stmt "+common::print(stmt)+" at "+stmt->getBeginLoc().printToString(SM)+"\n",common::CheckerName::immediate_uaf_checker,3,*configure);
						freeCallCites.insert(stmt);
						freeargs.insert(arg->IgnoreCasts()->IgnoreParens());					
					}
					if (const CallExpr* call = dyn_cast<CallExpr>(stmt)) {
						callee = call->getDirectCallee();
						std::vector<bool> freeparms = finder->isFreeMethod(callee);
						if (!freeparms.empty()){
							common::printLog( "free stmt "+common::print(stmt)+" at "+stmt->getBeginLoc().printToString(SM)+"\n",common::CheckerName::immediate_uaf_checker,3,*configure);
							freeCallCites.insert(stmt);					
							for(int i=0;i<freeparms.size();i++){
								if (freeparms[i]){
									freeargs.insert(call->getArg(i)->IgnoreCasts()->IgnoreParens());
								}
							}
							
						}
					}
					if (freeargs.empty()) continue;
					CFGBlock::iterator tmpitr = blockiter+1;
					while(tmpitr != block->end()){
						CFGElement element2=*(tmpitr);
						const Stmt* stmt2=((CFGStmt*)&element2)->getStmt();							
						if (isFreedArgsReferenced(stmt2,freeargs)){
							common::printLog( "Warning: use after free, "+common::print(stmt2)+" uses variable at "+stmt2->getBeginLoc().printToString(SM)+", which is freed in "+common::print(stmt)+" at "+stmt->getBeginLoc().printToString(SM)+" \n",common::CheckerName::immediate_uaf_checker,5,*configure);
							uafFlag = true;
							break;
						}
						tmpitr++;
					}
					if (!uafFlag) {
						//continue check succeed block
						checkSuccessor(stmt,block,freeargs,SM,checkDepth);
					}					
				}
			}
		}		
    } 
}

bool immediate_uaf_checker::checkSuccessor(const Stmt* stmt, CFGBlock* block,  std::unordered_set<const Expr*>& freeargs, const clang::SourceManager &SM, int depth) {
    if (depth <= 0) return false;
    bool uafFlag = false;
    for(CFGBlock::succ_iterator succ_iter=block->succ_begin();succ_iter!=block->succ_end();++succ_iter){
        CFGBlock* succblock=*succ_iter;
        for(CFGBlock::iterator succblockiter=succblock->begin();succblockiter!=succblock->end();succblockiter++) {
            CFGElement element3=*(succblockiter);
            if(element3.getKind()==CFGElement::Kind::Statement)
            {
                const Stmt* stmt3=((CFGStmt*)&element3)->getStmt();	
                if (isFreedArgsReferenced(stmt3,freeargs)){
                    common::printLog( "Warning: use after free, "+common::print(stmt3)+" uses variable at "+stmt3->getBeginLoc().printToString(SM)+", which is freed in "+common::print(stmt)+" at "+stmt->getBeginLoc().printToString(SM)+" \n",common::CheckerName::immediate_uaf_checker,5,*configure);
                    uafFlag = true;
                    break;
                }											
            }						
        }
        if (uafFlag) break;
        else {
            uafFlag = checkSuccessor(stmt,succblock,freeargs,SM,depth-1);
            if (uafFlag) return true;
        }
    }
    return uafFlag;
}

bool immediate_uaf_checker::isFreedArgsReferenced(const Stmt* stmt, std::unordered_set<const Expr*>& freeargs) {
	//cout<<"stmt: "<<endl;stmt->dump();
	std::string stmts = common::print(stmt);
	//getVarDecl()
	if (const DeclStmt* decls = dyn_cast<DeclStmt>(stmt)) {
	//handle declstmt, it will appear twice in ast, eg int r = foo(k, ptr); will become foo(); int r=foo(); to avoid second time being reported as uaf
		const Expr* init = nullptr;
		const VarDecl* decl = dyn_cast<VarDecl>(decls->getSingleDecl());
		if(decl){
			init = decl->getInit();
			if(init){
				if (const CallExpr* call = dyn_cast<CallExpr>(init->IgnoreCasts()->IgnoreParens())) {
					for (auto s : freeCallCites) {
						if (call == s) {
							common::printLog( stmts+" is a labled free method call site "+common::print(s)+"\n",common::CheckerName::immediate_uaf_checker,1,*configure);
							return false;
						}
					}
				}				
			}
		}	
	}
    if (const BinaryOperator* bo = dyn_cast<BinaryOperator>(stmt)) {
        //p = xx, erase free p
        if (bo->getOpcode() == clang::BinaryOperatorKind::BO_Assign) {
            const Expr* lhs = bo->getLHS()->IgnoreCasts()->IgnoreParens();
            if (const DeclRefExpr *ref = dyn_cast<DeclRefExpr>(lhs)) {
                const VarDecl* v = (VarDecl*)(ref->getDecl());
                auto itr = freeargs.begin();
                while(itr != freeargs.end()) {
                    const VarDecl* freevar = getVarDecl(*itr);
                    if (freevar == v) {
                        itr = freeargs.erase(itr);
						return false;
                    }
                    else
                        itr++;
                }
            }
            
        }
        else if (bo->getOpcode() == clang::BinaryOperatorKind::BO_EQ) {
            //p==xx , skip
            return false;
        }
    }
    for (auto e:freeargs) {
        //cout<<"freearg:"<<endl;e->dump();
		const VarDecl* freevar = getVarDecl(e);
		if (!freevar) continue;
		std::string vs = common::print(e);
		if (stmts.find(vs) != std::string::npos) {
			//vs maybe string "vs" in stmts, double check
			FindUAFVarDeclRef visitor;
			visitor.TraverseStmt((Stmt*)stmt);
			if (visitor.isVarDeclUsed(freevar)) {
				return true;
			}
		}
	}
	return false;
}

void immediate_uaf_checker::lableAllFreeMethod() {
	for(auto F:resource->getFunctions()) {
		finder->clearFreeCalls();
		checkFreeMethod(F);
	}
}

void immediate_uaf_checker::checkFreeMethod(ASTFunction* astF) {
	std::vector<bool> parms;
	FunctionDecl* FD = manager->getFunctionDecl(astF);
	if (!FD) return;

	std::string funcName =  FD->getNameInfo().getName().getAsString();
	if (funcName == "free") {
		common::printLog(FD->getNameAsString()+" is free method\n",common::CheckerName::immediate_uaf_checker,2,*configure);
		//parms.push_back(true);
		//finder->updateFreeMethods(FD,parms);
		return;
	}
	
	for (int i=0;i<FD->getNumParams();i++){
		ParmVarDecl* p = FD->getParamDecl(i);
		parms.push_back(false);

	}
	//FindCalledExpr finder;
	if (!FD->getBody()) return;
	finder->Visit(FD->getBody());
	bool flag = false;
	std::vector<const CallExpr*> freeCalls = finder->getFreeCalls();
	for(auto c : freeCalls) {
		//finder->AllFreeMethods.insert(c->getDirectCallee());
		for (int i=0;i<c->getNumArgs();i++){
			const Expr* arg = c->getArg(i)->IgnoreCasts()->IgnoreParens();
			const ParmVarDecl* p = isParameter(arg);
			if (p){
				int pos = getParameterPosition(FD,p);
				if (pos == -1) continue;
				parms[pos]=true;
				flag = true;
				common::printLog(common::print(arg)+" in "+common::print(c)+" is a parameter, therefore "+FD->getNameAsString()+" is a free method\n",common::CheckerName::immediate_uaf_checker,2,*configure);
				//return true;
			}
		}
	}
	if (flag){
		finder->updateFreeMethods(FD,parms);
	}
}

int immediate_uaf_checker::getParameterPosition(FunctionDecl* FD, const ParmVarDecl* p){
	for (int i=0;i<FD->getNumParams();i++){
		ParmVarDecl* pp = FD->getParamDecl(i);
		if (p == pp) return i;
	}
	return -1;
}

const VarDecl* immediate_uaf_checker::getVarDecl(const Expr* e) {
	if (const DeclRefExpr *ref = dyn_cast<DeclRefExpr>(e)) {
		const VarDecl* v = (VarDecl*)(ref->getDecl());
		return v;
	}
	else if (const UnaryOperator* uo = dyn_cast<UnaryOperator>(e)){
		return getVarDecl(uo->getSubExpr()->IgnoreCasts()->IgnoreParens());
	}	
	else if (const MemberExpr* m = dyn_cast<MemberExpr>(e)) {
		return getVarDecl(m->getBase()->IgnoreCasts()->IgnoreParens());
	}
	return nullptr;
}

const ParmVarDecl* immediate_uaf_checker::isParameter(const Expr* e) {
	const VarDecl* v = getVarDecl(e);
	if (!v) return nullptr;
	if (const ParmVarDecl* p = dyn_cast<ParmVarDecl>(v)) {
		// common::printLog(v->getNameAsString()+" is a parameter\n",common::CheckerName::immediate_uaf_checker,2,*configure);
		return p;
	}
	return nullptr;
}
