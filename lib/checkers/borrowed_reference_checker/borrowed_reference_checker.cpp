#include "checkers/borrowed_reference_checker/borrowed_reference_checker.h"

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

void borrowed_reference_checker::check() {
	clock_t start = clock();
    for(auto F:resource->getFunctions()) {
		FunctionDecl *FD = manager->getFunctionDecl(F);
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
		std::string FDName = FD->getNameAsString();
		//if (FDName != "element_setstate_from_attributes") continue;
		common::printLog( "In function "+FDName+"\n",common::CheckerName::borrowed_reference_checker,2,*configure);
		CFG* cfg = function_cfg.get();
		CFGBlock* Entry = nullptr;
		std::vector<CFGBlock*> unfinished;
		std::unordered_set<CFGBlock*> handled;
		for(CFG::iterator iter=cfg->begin();iter!=cfg->end();++iter){
			//get entry block
			CFGBlock* block=*iter;
			if(block->pred_begin()==block->pred_end())
			{
				Entry=block;
				unfinished.push_back(Entry);
				handled.insert(block);
			}
		}		
		while(unfinished.size()!=0){
			//analyze from entry of cfg
			CFGBlock* block=unfinished[0];
			//block->dump();
			unfinished.erase(unfinished.begin());
			for(CFGBlock::succ_iterator succ_iter=block->succ_begin();succ_iter!=block->succ_end();++succ_iter){
				CFGBlock *succ=*succ_iter;
				if(succ==NULL)continue;
				auto findb = handled.find(succ);
				if (findb != handled.end()) continue;
				unfinished.push_back(succ);
				handled.insert(block);
			}			
			for(CFGBlock::iterator iter=block->begin();iter!=block->end();iter++)
			{
				CFGElement element=*(iter);
				if(element.getKind()==CFGElement::Kind::Statement)
				{
					const Stmt* stmt=((CFGStmt*)&element)->getStmt();
					std::string stmtstr = common::print(stmt);
					if (hasSkipedFunctions(stmtstr)) {
						common::printLog( "SkipedFunctions callsite "+common::print(stmt)+" at "+stmt->getBeginLoc().printToString(SM)+"\n",common::CheckerName::borrowed_reference_checker,2,*configure);							
						continue;
					}
                    std::string s = hasCheckFunctions(stmtstr);//PyUnicode_Check PyList_Check
                    //eg: !PyUnicode_Check(doc)
                    if (s != "") {
                        if (const Expr* expr = dyn_cast<Expr>(stmt)) {
                            const Expr* arg0 = getFirstArg(s, expr);//get first arg
                            if (!arg0) continue;
                            if (const DeclRefExpr *ref = dyn_cast<DeclRefExpr>(arg0)) {
                                const VarDecl* v = (VarDecl*)(ref->getDecl());
                                auto findv = borrowedRefVars.find(v);
                                if (findv != borrowedRefVars.end()){
                                    borrowedRefVars.erase(findv);		
                                    common::printLog("CheckFunctions callsite "+common::print(stmt)+" at "+stmt->getBeginLoc().printToString(SM)+", erase from borrowedRefVars\n",common::CheckerName::borrowed_reference_checker,3,*configure);
                                }
                            }
                        }
                        continue;						
                    }
                    const VarDecl* borrowedrefdecl=nullptr;
					const Stmt* roperand = stmt;
					if(const DeclStmt* decl = dyn_cast<DeclStmt>(stmt)) {
						borrowedrefdecl = dyn_cast<VarDecl>(decl->getSingleDecl());
						if(borrowedrefdecl){
							if(const Expr* assign = borrowedrefdecl->getInit()){
								roperand = assign->IgnoreCasts()->IgnoreParens();
							}
						}
					}
					if(const BinaryOperator* bo=dyn_cast<BinaryOperator>(stmt)){
						if (bo->getOpcode() == clang::BinaryOperatorKind::BO_Assign) {
							roperand = bo->getRHS()->IgnoreCasts()->IgnoreParens();
							if (const DeclRefExpr *ref = dyn_cast<DeclRefExpr>(bo->getLHS()->IgnoreCasts()->IgnoreParens())) {
								borrowedrefdecl = (VarDecl*)(ref->getDecl());
							}
						}
                        else if (bo->getOpcode() == clang::BinaryOperatorKind::BO_EQ) {
                            continue;
                        }
					}	
					if (const UnaryOperator* uo = dyn_cast<UnaryOperator>(stmt)) {
						if (uo->getOpcode() == UO_LNot) {
							//!key
							continue;
						}
					}
					const CallExpr *c = dyn_cast<CallExpr>(roperand);
					if (borrowedrefdecl && !c) {
						//borrowedrefdecl=xxx, new reference, not borrowed, if exist in borrowedRefVars, rm it
						auto findv = borrowedRefVars.find(borrowedrefdecl);
						if (findv != borrowedRefVars.end()){
							borrowedRefVars.erase(findv);		
							common::printLog("new reference "+common::print(stmt)+" at "+stmt->getBeginLoc().printToString(SM)+", release "+borrowedrefdecl->getNameAsString()+"\n",common::CheckerName::borrowed_reference_checker,3,*configure);					
						}	
						continue;					
					}

					if (c){//handle call
						if (const FunctionDecl *callee = c->getDirectCallee()) {
							std::string fName = callee->getNameAsString();
							if (isSkipedFunctions(fName)) {//eg assert()
								common::printLog( "SkipedFunctions callsite "+common::print(stmt)+" at "+stmt->getBeginLoc().printToString(SM)+"\n",common::CheckerName::borrowed_reference_checker,2,*configure);							
								continue;
							}
                            /*
							if (isCheckFunction(fName)) {//fName == "Py_INCREF" || fName == "Py_XINCREF" 
								const Expr* arg0 = c->getArg(0)->IgnoreCasts()->IgnoreParens();//get first arg
								if (const DeclRefExpr *ref = dyn_cast<DeclRefExpr>(arg0)) {
									const VarDecl* v = (VarDecl*)(ref->getDecl());
									auto findv = borrowedRefVars.find(v);
									if (findv != borrowedRefVars.end()){
										borrowedRefVars.erase(findv);		
										common::printLog("CheckFunctions callsite "+common::print(stmt)+" at "+stmt->getBeginLoc().printToString(SM)+", erase from borrowedRefVars\n",common::CheckerName::borrowed_reference_checker,3,*configure);
									}
								}
								continue;
							}*/
							/*if (fName == "Py_DECREF") {
								const Expr* arg0 = c->getArg(0)->IgnoreCasts()->IgnoreParens();//get first arg
								if (const DeclRefExpr *ref = dyn_cast<DeclRefExpr>(arg0)) {
									const VarDecl* v = (VarDecl*)(ref->getDecl());
									auto findv = borrowedRefVars.find(v);
									if (findv != borrowedRefVars.end()){
										borrowedRefVars.erase(findv);		
										common::printLog(common::print(stmt)+" at "+stmt->getBeginLoc().printToString(SM)+"\n",common::CheckerName::borrowed_reference_checker,3,*configure);
									}
								}
							}	*/						
							if (borrowedrefdecl) {
								//borrowedrefdecl=Borrowedcall()
								if (isGetBorrowedRefFunctions(fName)){							
									common::printLog( "Find GetBorrowedRefFunction callsite "+common::print(stmt)+" at "+stmt->getBeginLoc().printToString(SM)+"\n",common::CheckerName::borrowed_reference_checker,3,*configure);															
									borrowedRefVars.insert(std::make_pair(borrowedrefdecl, stmt));				
									continue;
								}
								else {
									//borrowedrefdecl=call(), new ref?
									auto findv = borrowedRefVars.find(borrowedrefdecl);
									if (findv != borrowedRefVars.end()){
										borrowedRefVars.erase(findv);		
										common::printLog("new reference "+common::print(stmt)+" at "+stmt->getBeginLoc().printToString(SM)+", release "+borrowedrefdecl->getNameAsString()+"\n",common::CheckerName::borrowed_reference_checker,3,*configure);					
									}	
									continue;									
								}
								
							}	
							//else other call						
						}
					}
					//else 
                    
					//otherwise, if stmt uses borrowedRefVars, report warning
					for (auto& pair : borrowedRefVars){
						std::string stmts = common::print(stmt);
						std::string vs = pair.first->getNameAsString();
						if (stmts.find(vs) != std::string::npos) {
							//vs maybe string "vs" in stmts, double check
							FindVarDeclRef visitor;
							visitor.TraverseStmt((Stmt*)stmt);
							if (visitor.isVarDeclUsed(pair.first))
								common::printLog( "Warning: before "+stmts+" need Py_INCREF("+vs+"), since "+vs+" is borrowed reference in "+common::print(pair.second)+" at "+stmt->getBeginLoc().printToString(SM)+"\n",common::CheckerName::borrowed_reference_checker,5,*configure);
						}
					}	
				}			
			}

		}		
		
    }
    
}

bool borrowed_reference_checker::isGetBorrowedRefFunctions(std::string s) {
	auto finds = GetBorrowedRefFunctions.find(s);
	if (finds != GetBorrowedRefFunctions.end()) {
		return true;
	}
	else
		return false;
}

bool borrowed_reference_checker::isSkipedFunctions(std::string s) {
	auto finds = SkipedFunctions.find(s);
	if (finds != SkipedFunctions.end()) {
		return true;
	}
	else
		return false;
}

bool borrowed_reference_checker::isCheckFunction(std::string s) {
	auto finds = CheckFunctions.find(s);
	if (finds != CheckFunctions.end()) {
		return true;
	}
	else
		return false;
}

bool borrowed_reference_checker::hasSkipedFunctions(std::string f) {
	for(auto& s: SkipedFunctions) {
		if (f.find(s)!=std::string::npos)
			return true;
	}
	return false;
}

std::string borrowed_reference_checker::hasCheckFunctions(std::string f) {
	for(auto& s: CheckFunctions) {
		if (f.find(s)!=std::string::npos)
			return s;
	}
	return "";
}


const Expr* borrowed_reference_checker::getFirstArg(std::string funcName, const Expr* expr) {
	if (const DeclRefExpr *ref = dyn_cast<DeclRefExpr>(expr->IgnoreCasts()->IgnoreParens())) {
		return ref;
	}
	if (const UnaryOperator *ref = dyn_cast<UnaryOperator>(expr->IgnoreCasts()->IgnoreParens()))
		return getFirstArg(funcName, ref->getSubExpr());
	if (const BinaryOperator *ref = dyn_cast<BinaryOperator>(expr->IgnoreCasts()->IgnoreParens())) {
		const Expr* l = ref->getLHS();
		std::string lstmts = common::print(l);
		if (hasCheckFunctions(lstmts)!="") 
			return getFirstArg(funcName, l);
		const Expr* r = ref->getRHS();
		std::string rstmts = common::print(r);
		if (hasCheckFunctions(rstmts)!="") 
			return getFirstArg(funcName, r);
	}
	if (const MemberExpr *ref = dyn_cast<MemberExpr>(expr->IgnoreCasts()->IgnoreParens()))
		return getFirstArg(funcName, ref->getBase());	
	if (const CallExpr *c = dyn_cast<CallExpr>(expr)){
        if (c->getNumArgs() == 0) return nullptr;
		const Expr* arg0 = c->getArg(0)->IgnoreCasts()->IgnoreParens();//get first arg
        return getFirstArg(funcName,arg0);	
	}
    if (const CStyleCastExpr* ce = dyn_cast<CStyleCastExpr>(expr)) {
        return getFirstArg(funcName,ce->getSubExpr());
    }
	return nullptr;		
}
