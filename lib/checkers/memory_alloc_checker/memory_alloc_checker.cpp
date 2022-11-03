#include "checkers/memory_alloc_checker/memory_alloc_checker.h"

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
#include <clang/AST/StmtVisitor.h>
#include <string>

using namespace std;

void memory_alloc_checker::check() {
	clock_t start = clock();
    for(auto F:resource->getFunctions()) {
		FunctionDecl *FD = manager->getFunctionDecl(F);
		std::unique_ptr<CFG> &function_cfg = manager->getCFG(F);
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
		common::printLog( "In function "+FD->getNameAsString()+"\n",common::CheckerName::memory_alloc_checker,2,*configure);

		CFG* cfg = function_cfg.get();
		for(CFG::iterator cfgiter=cfg->begin();cfgiter!=cfg->end();++cfgiter){
			CFGBlock* block=*cfgiter;
			for(CFGBlock::iterator iter=block->begin();iter!=block->end();iter++)
			{
				CFGElement element=*(iter);
				if(element.getKind()==CFGElement::Kind::Statement)
				{
					const Stmt* stmt=((CFGStmt*)&element)->getStmt();
					//const Stmt* it = stmt;
					/*if(const DeclStmt* decl = dyn_cast<DeclStmt>(stmt)) {
						if(const VarDecl* var = dyn_cast<VarDecl>(decl->getSingleDecl())){
							if(const Expr* assign = var->getInit()){
								it = assign;
							}
						}
					}
					if(const BinaryOperator* bo=dyn_cast<BinaryOperator>(stmt)){
						if (bo->getOpcode() == clang::BinaryOperatorKind::BO_Assign) {
							it = bo->getRHS()->IgnoreCasts()->IgnoreParens();
						}
					}*/
					if (const CallExpr *c = dyn_cast<CallExpr>(stmt)){
						if (const FunctionDecl *f = c->getDirectCallee()) {
							std::string fName=f->getNameAsString();
							if (fName == "malloc" || fName == "calloc" ) {
								common::printLog( fName+" call "+common::print(c)+" located\n",common::CheckerName::memory_alloc_checker,2,*configure);
								const Expr* szexpr = c->getArg(0)->IgnoreCasts()->IgnoreParens();//get first arg, namely size
								VarDecl* szdecl = nullptr;
								if (const DeclRefExpr *ref = dyn_cast<DeclRefExpr>(szexpr)) {
									szdecl = (VarDecl*)(ref->getDecl());
								}
								if (!szdecl) continue;
								bool checkedSize = false;
								Stmt* term = block->getTerminatorStmt();
								if (term) {
									if(term->getStmtClass()==Stmt::IfStmtClass){
										//check 0 in same block
										Expr* cond = ((IfStmt*)term)->getCond();
										if (checkSize(cond, szdecl)) {
											checkedSize = true;
											common::printLog( fName+" statement "+common::print(c)+" has been checked size==0\n",common::CheckerName::memory_alloc_checker,3,*configure);
											continue;
										}
									}
									if (!checkedSize) {
										for(CFGBlock::succ_iterator succ_iter=block->succ_begin();succ_iter!=block->succ_end();++succ_iter){	
											//check 0 in succ block
											CFGBlock* succ=*succ_iter;
											if(succ==NULL)continue;
											Stmt* term = succ->getTerminatorStmt();
											if (!term) continue;
											if(term->getStmtClass()==Stmt::IfStmtClass){
												Expr* cond = ((IfStmt*)term)->getCond();
												if (checkSize(cond, szdecl)) {
													checkedSize = true;
													common::printLog( fName+" statement "+common::print(c)+" has been checked size==0\n",common::CheckerName::memory_alloc_checker,3,*configure);
													continue;
												}
											}
										}									
									}	
									if (!checkedSize) {
										for(CFGBlock::pred_iterator pred_iter=block->pred_begin();pred_iter!=block->pred_end();++pred_iter){
											//check 0 in pred block
											CFGBlock* pred=*pred_iter;
											if(pred==NULL)continue;
											Stmt* term = pred->getTerminatorStmt();
											if (!term) continue;
											if(term->getStmtClass()==Stmt::IfStmtClass){
												Expr* cond = ((IfStmt*)term)->getCond();
												if (checkSize(cond, szdecl)) {
													checkedSize = true;
													common::printLog( fName+" statement "+common::print(c)+" has been checked size==0\n",common::CheckerName::memory_alloc_checker,3,*configure);
													continue;
												}
											}
										}									
									}																		
								}

								if (!checkedSize) {									
									common::printLog( "Warning: "+fName+" statement "+common::print(stmt)+" "+common::print(szexpr)+"==0 has not been checked at "+stmt->getBeginLoc().printToString(SM)+"\n",common::CheckerName::memory_alloc_checker,5,*configure);
								}							
							}
						}
					}
				}
			}
		}
		
		
    }
    
}

bool memory_alloc_checker::checkDeclRef(DeclRefExpr* D, VarDecl* szdecl) {
	VarDecl* v = (VarDecl*)(D->getDecl());
	return (v == szdecl);
}

bool memory_alloc_checker::checkSize(Expr* cond, VarDecl* szdecl) {
	if (DeclRefExpr *lref = dyn_cast<DeclRefExpr>(cond->IgnoreCasts()->IgnoreParens())) {
		//if (size)
		return checkDeclRef(lref, szdecl);
	}
	if(const UnaryOperator * uo=dyn_cast<UnaryOperator>(cond->IgnoreCasts()->IgnoreParens())) {
		//if (!size)
		if(uo->getOpcode()==clang::UnaryOperatorKind::UO_LNot) {
			Expr* expr=uo->getSubExpr()->IgnoreCasts()->IgnoreParens();
			if (DeclRefExpr *lref = dyn_cast<DeclRefExpr>(expr)) {
				//if (size)
				return checkDeclRef(lref, szdecl);
			}			
		}
	}

	if(const BinaryOperator* bo=dyn_cast<BinaryOperator>(cond->IgnoreCasts()->IgnoreParens())){
		if (bo->getOpcode()==clang::BinaryOperatorKind::BO_LAnd || bo->getOpcode()==clang::BinaryOperatorKind::BO_LOr) {
			return checkSize(bo->getLHS(),szdecl) || checkSize(bo->getRHS(),szdecl);
		}
		if (bo->getOpcode()==clang::BinaryOperatorKind::BO_EQ) {
			Expr* l = bo->getLHS()->IgnoreCasts()->IgnoreParens();
			Expr* r = bo->getRHS()->IgnoreCasts()->IgnoreParens();
			if (DeclRefExpr *lref = dyn_cast<DeclRefExpr>(l)) {
				if (checkDeclRef(lref, szdecl)) {
					if (IntegerLiteral* ILE = dyn_cast<IntegerLiteral>(r)) {
						uint64_t value=ILE->getValue().getLimitedValue();
						if (value == 0) 
							return true;
						else
							return false;
					}
					else
						return false;
				}
				else
					return false;
			}
			if (DeclRefExpr *rref = dyn_cast<DeclRefExpr>(r)) {
				if (checkDeclRef(rref, szdecl)) {
					if (IntegerLiteral* ILE = dyn_cast<IntegerLiteral>(l)) {
						uint64_t value=ILE->getValue().getLimitedValue();
						if (value == 0) 
							return true;
						else
							return false;
					}
					else
						return false;
				}
				else
					return false;
			}

		}
	}
	return false;
}
