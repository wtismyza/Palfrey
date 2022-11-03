
#include "checkers/ReturnStackAddrChecker/ReturnStackAddrChecker.h"

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

class ASTCalledExprFind : public RecursiveASTVisitor<ASTCalledExprFind> {
    
public:
    bool VisitCallExpr(CallExpr *E) {
        if (FunctionDecl *FD = E->getDirectCallee()) {
	    std::string fullName=common::getFullName(FD);
            functions.insert(fullName);
            if(map.find(fullName)==map.end())
            {
            		std::vector<CallExpr*> tmp;
            		tmp.push_back(E);
            		map.insert(pair<string,std::vector<CallExpr* > >(fullName,tmp));
            }
            else
            		map[fullName].push_back(E);
        }
        return true;
    }

    bool getFunctions(FunctionDecl* FD) {
        return functions.find(common::getFullName(FD))!=functions.end();
    }

    std::vector<CallExpr*> getCallExpr(FunctionDecl* FD)
    {
    	return map[common::getFullName(FD)];
    }

private:
    std::set<std::string> functions;
    std::map<std::string,std::vector<CallExpr* > > map;
};

vector<ArraySubscript> ReturnStackAddrChecker::getFatherScript(vector<ArraySubscript>& list,ASTFunction * father, CallExpr* callsite, ASTFunction * current) {
    vector<ArraySubscript> result;
	FunctionDecl *cFunc = manager->getFunctionDecl(current);
    SourceManager *sm = &(cFunc->getASTContext().getSourceManager());
    std::string loc = callsite->getBeginLoc().printToString(*sm);
	if (loc == "<invalid>") return result;
	//cout<<"loc:"<<loc<<endl;
    int firstColonInBegin = loc.find(":");
    std::string fileName = loc.substr(0,firstColonInBegin);
    int secondColonInBegin = loc.find(":", firstColonInBegin + 1);
    std::string locLinesInBegin = loc.substr(firstColonInBegin + 1, secondColonInBegin - firstColonInBegin - 1);
    //cout<<"locLinesInBegin:"<<locLinesInBegin<<endl;
	int line = std::stoi(locLinesInBegin);
    CFGBlock* block = manager->getBlockWithLoc(fileName,line);
	if (!block) return result;
	FunctionDecl *fatherDecl = manager->getFunctionDecl(father);
	std::string fatherFuncName =  fatherDecl->getNameInfo().getName().getAsString();
    common::printLog( "handle script in caller "+fatherFuncName+" at callsite "+print(callsite)+"\n",common::CheckerName::arrayBound,3,*configure);
    
    for (auto& l : list) {
        if (l.depth >= depth) {
            reportWarning(l,-1);
            continue;
        }
        ArraySubscript sc(l);
       /* vector<z3::expr> idxexprs = translator->zc->clangExprToZ3Expr(sc.index,current);
        if (!idxexprs){
            continue;
        }*/
        int paramIdx = -1;
        for (unsigned i=0;i<cFunc->getNumParams();i++){
            ParmVarDecl* p = cFunc->getParamDecl(i);
            /*std::pair<string,z3::expr> pexpr = translator->zc->clangValueDeclToZ3Expr(p,current);
            if (pexpr.second.is_int())
                if (idxexprs.back().contains(pexpr.second)){
                    paramIdx = i;
                    result.push_back(sc);
                    break;

                }*/
            if (checkIndexHasVar(p,sc.index)){
                paramIdx = i;
                sc.updateCallStack("at "+loc+" "+fatherFuncName);
				for (auto s:sc.callStack) 
            		common::printLog("call stack: "+s+"\n",common::CheckerName::arrayBound,3,*configure);
                //for (auto s: sc.callStack) cout<<"getfatherscript sc 100 "<<s<<endl;
                result.push_back(sc);
                break;
            }
        }
        
    }   
    if (result.empty())
        return result;
    if (SolvePathConstraints) {
        z3::expr parm2FatherRealArg = translator->zc->TRUE;
        for (unsigned i=0;i<cFunc->getNumParams();i++){
            ParmVarDecl* p = cFunc->getParamDecl(i);
            std::pair<string,z3::expr> pexpr = translator->zc->clangValueDeclToZ3Expr(p,current);
            if (pexpr.second.is_array()){
                //                continue;
            }
            Expr* realArg = callsite->getArg(i);
            std::vector<z3::expr> realexprs = translator->zc->clangExprToZ3Expr(realArg, father);
            if (realexprs.empty() ){//|| realexprs.back().is_array()){
                continue;
            }
            //cout<<"pexpr:"<<pexpr.second<<",realexpr:"<<realexprs.back()<<endl;
            if (pexpr.second.is_array() && realexprs.back().is_int()){
                //parm: int* p; realarg: &n, p==&n => *p==n
                z3::expr p0 = select(pexpr.second,translator->zc->Int2Z3Expr(0));
                parm2FatherRealArg = parm2FatherRealArg && (p0==realexprs.back());

            }
            else {
                parm2FatherRealArg = parm2FatherRealArg && (pexpr.second==realexprs.back());
				//if (pexpr.second.is_array() && realexprs.back().is_array()) {
					std::vector<z3::expr> eqs = translator->zc->updateParameterRealArgument4StructElement(pexpr.second, realexprs.back());
					for (auto e : eqs) {
						parm2FatherRealArg = parm2FatherRealArg && e;
					}
				//}
			}
            //cout<<"parm2FatherRealArg:"<<parm2FatherRealArg<<endl; 
            //delete realexprs; realexprs=nullptr;
            }
            for (auto& sc: result){
                sc.updatePathConstraint(callsite, parm2FatherRealArg, true);
				common::printLog( "input path constraints:"+sc.pathConstraint.to_string()+"\n",common::CheckerName::arrayBound,2,*configure);
                sc.depth++;
                sc.block = block;
                sc.stmt = callsite;
               // for (auto s: sc.callStack) cout<<"getfatherscript sc 130 "<<s<<endl;
            }
            return result;
        }
    FunctionDecl *pFunc = manager->getFunctionDecl(father);
	std::unique_ptr<CFG> &pCFG= manager->getCFG(father);	
	for(CFG::iterator iter=pCFG->begin();iter!=pCFG->end();++iter){
		CFGBlock* block=*iter;
		for(CFGBlock::iterator iter=block->begin();iter!=block->end();iter++)
			{
				CFGElement element=*(iter);
				if(element.getKind()==CFGElement::Kind::Statement)
				{
					const Stmt* it=((CFGStmt*)&element)->getStmt();
					
					 ASTCalledExprFind load;
    					 load.TraverseStmt((Stmt*)it);
   					 if(load.getFunctions(cFunc))
   					 {
   					 	std::vector<CallExpr*> callexprs=load.getCallExpr(cFunc);
   					 	for(auto sc : list)
   					 	{
                            int flag;
   					 		for(auto ce : callexprs)
   					 		{
	   					 		string index=print(sc.index);
	   					 		bool hasP=false;
	   					 		if (MemberExpr *MRE = dyn_cast<MemberExpr>(sc.index))
								{
									Expr* tmp;
									if (CXXConstructExpr *CXXC = dyn_cast<CXXConstructExpr>(MRE->getBase()))
									{

										tmp=CXXC->getArg(0);
									}
									else
									{
										tmp=MRE->getBase();
									}
									if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(tmp->IgnoreCasts()->IgnoreParens()))
									{
										if (ParmVarDecl *pvd = dyn_cast<ParmVarDecl>(ref->getDecl()))
										{
											index=pvd->getNameAsString();
										}
									}
								}
	   					 		for(unsigned i=0;i<cFunc->getNumParams();i++)
								{
									auto p=cFunc->getParamDecl(i);
									string name=p->getNameAsString();
									if(name==index)
									{
										hasP=true;
										ArraySubscript tmp(sc);
			   					 		tmp.block=block;
			   					 		tmp.stmt=(Stmt*)it;
			   					 		tmp.isLoopBoundChecking=false;
			   					 		tmp.func=pFunc;
										Expr* rhs=ce->getArg(i)->IgnoreCasts()->IgnoreParens();
                                        translator->handleIndex(tmp, father);
                                        if (tmp.condition.empty()) {
                                            //erase
                                            mapToPath[tmp.ID].insert(callpath);
//                                            tmp.updateCallStack()
                                        }
                                        /*else if (flag == -1) {
                                            tmp.resetIndex(tmp.orignIndex);
                                            reportWarning(tmp);
                                        }*/
                                        else {
                                            result.push_back(tmp);
                                        }
                                        /*
                                        if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(rhs)) 
										{
											uint64_t value=ILE->getValue().getLimitedValue();
											bool flag=true;
											for(unsigned j=0;j<sc.condition.size();j++)
											{
												AtomicAPInt temp=sc.condition[j];
												if(temp.op==clang::BinaryOperatorKind::BO_LT)
												{
													if(value>=temp.rhs)flag=false;
												}
												if(temp.op==clang::BinaryOperatorKind::BO_GE)
												{
													if(value<temp.rhs)flag=false;
												}
											}
											if(flag)
											{
												erase.insert(sc.ID);
												break;
											}
										}
										else if(print(rhs).find("sizeof")!=std::string::npos)
										{
											erase.insert(sc.ID);
											break;
										}
										else if(FloatingLiteral *ILE = dyn_cast<FloatingLiteral>(rhs))//-ImplicitCastExpr
										{
											double value=ILE->getValue().convertToDouble();
											bool flag=true;
											for(unsigned j=0;j<sc.condition.size();j++)
											{
												AtomicAPInt temp=sc.condition[j];
												if(temp.op==clang::BinaryOperatorKind::BO_LT)
												{
													if(value>=temp.rhs)flag=false;
												}
												if(temp.op==clang::BinaryOperatorKind::BO_GE)
												{
													if(value<temp.rhs)flag=false;
												}
											}
											if(flag)
											{
												erase.insert(sc.ID);
												break;
											}
										}
										else if(DeclRefExpr * ref=dyn_cast<DeclRefExpr>(rhs)){
											
											if(EnumConstantDecl * EC=dyn_cast<EnumConstantDecl>(ref->getDecl())){
												uint64_t value=EC->getInitVal().getLimitedValue ();
												common::printLog( "EnumConstantDecl:"+int2string(value)+"\n",common::CheckerName::arrayBound,0,*configure);	
												bool flag=true;	
												for(unsigned j=0;j<sc.condition.size();j++)
												{
													AtomicAPInt temp=sc.condition[j];
													if(temp.op==clang::BinaryOperatorKind::BO_LT)
													{
														if(temp.op==clang::BinaryOperatorKind::BO_LT)
														{
															if(value>=temp.rhs)flag=false;
														}
														if(temp.op==clang::BinaryOperatorKind::BO_GE)
														{
															if(value<temp.rhs)flag=false;
														}

													}
													
												}
												if(flag)
												{
													erase.insert(sc.ID);
													break;
												}
											}
										}
                                    */
										tmp.changeIndex(ce->getArg(i),true);
										common::printLog( "get Expr:"+print(ce->getArg(i))+"\n",common::CheckerName::arrayBound,3,*configure);
   					 					//result.push_back(tmp);	
										break;
									}
								}
								if(!hasP)
   					 			{
   					 				//errors.insert(sc.ID);
   					 				sc.resetIndex(sc.orignIndex);
                                    reportWarning(sc,-1);
                                    mapToPath[sc.ID].insert(callpath);
   					 			}
   					 		}
   					 		
   					 	}
   					 }
				}
			}
	}
	return result;
}

vector<ArraySubscript> ReturnStackAddrChecker::DFS2func(ASTFunction *astFunc,int level,vector<ArraySubscript> list) {
    vector<ArraySubscript> result;
    if (list.empty()) return result;
   // for (auto& as:list)
     //   for (auto s: as.callStack) cout<<"dfs list 309 "<<s<<endl;
    translator->clearExprMap();
	FunctionDecl *f = manager->getFunctionDecl(astFunc);
	DeclarationName DeclName = f->getNameInfo().getName();
	std::string FuncName = DeclName.getAsString();
	callpath.push_back(FuncName);
	common::printLog( "DFS analyse into function "+FuncName+"\n",common::CheckerName::arrayBound,3,*configure);
	if(level<=0) {
/*		for(ArraySubscript s : list) {
			mapToPath[s.ID].insert(callpath);
		}
		callpath.pop_back();
		common::printLog( "DFS out:"+FuncName+"\n",common::CheckerName::arrayBound,3,*configure);*/
		return list;
	}

	//std::vector<ASTFunction *> parents=call_graph->getParents(astFunc);
     //using CallInfo = std::vector<std::pair<ASTFunction *, int64_t>>;
    CallInfo parents=call_graph->getParentsWithCallsite(astFunc); 
	if(parents.size()==0)
	{
	/*	for(ArraySubscript s : list)
		{
			mapToPath[s.ID].insert(callpath);
		}
		callpath.pop_back();
//		common::printLog( "DFS out:"+FuncName+"\n",common::CheckerName::arrayBound,3,*configure);*/
		return list;
	}
	else
	{
		for(auto father : parents)
		{
			FuncNow=manager->getFunctionDecl(father.first);
            Stmt* callsite = common::getStmtInFunctionWithID(FuncNow, father.second);
            CallExpr* call = dyn_cast<CallExpr>(callsite);
            if (!call)
                continue;
            std::unique_ptr<CFG> &pCFG= manager->getCFG(father.first);					
			unordered_set<int> erase;
            std::vector<ArraySubscript> r = getFatherScript(list,father.first,call,astFunc);
			
			if(r.size()>0)
			{
               // for (auto& as:r)
                //for (auto s: as.callStack) cout<<"fatherscript result 351 "<<s<<endl;
				vector<ArraySubscript> temp = backwardAnalyse(pCFG.get(), father.first, r);
               // for (auto& as:temp)
             //   for (auto s: as.callStack) cout<<"out backward temp 353"<<s<<endl;
				vector<ArraySubscript> rtemp = DFS2func(father.first,level-1,temp);
                pushAToB(rtemp, result);
			}
            /*
			else
			{
				for(ArraySubscript s : list)
				{
					if(erase.find(s.ID)!=erase.end())continue;
					mapToPath[s.ID].insert(callpath);
				}
			}
			for(ArraySubscript s : input)
			{
				s.resetIndex(true);
				
			}*/
		}
	}
//for (auto& as:result)
  //      for (auto s: as.callStack) cout<<"dfs ret 377 "<<s<<endl;
    callpath.pop_back();
	common::printLog( "DFS out function "+FuncName+"\n",common::CheckerName::arrayBound,3,*configure);
    return result;
}

void ReturnStackAddrChecker::check() {   
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
		common::printLog( "Start check function "+FD->getNameAsString()+"\n",common::CheckerName::arrayBound,4,*configure);
		if ( DebugMode&& DebugFunctionName!="None" && DebugFunctionName != "" && DebugFunctionName != FD->getNameAsString()) {
            continue;
		}

		clock_t start = clock();
	
		FuncNow=FD;

		if(DebugMode)
		{
			LangOptions L0;
			L0.CPlusPlus=1;
			function_cfg.get()->dump(L0,true);
		}

		callpath.clear();
		mapToPath.clear();	
		reportedID.clear();
		sameID.clear();
		
		vector<ArraySubscript> list = LocatingTaintExpr(F,function_cfg.get());
		if(list.size()==0) {
			common::printLog( "list size is 0, checkFunc return\n",common::CheckerName::arrayBound,1,*configure);
			continue;
		}
		common::printLog( "find "+int2string(list.size())+ " ArraySubscript in function\n",common::CheckerName::arrayBound,3,*configure);
		set<std::vector<string> > tt;
		for(unsigned i=0;i<list.size();i++)
		{
			list[i].ID=i;
			mapToPath[i]=tt;
			list[i].orignIndex=list[i].index;
		}
		vector<ArraySubscript> temp = backwardAnalyse(function_cfg.get(),F, list);
      // for (auto& as:temp)
        //for (auto s: as.callStack) cout<<"out backwardtemp 432 "<<s<<endl;     
		common::printLog( "check depth:"+int2string(depth)+"\n",common::CheckerName::arrayBound,4,*configure);
		
		if(temp.size()!=0)
		{
			//baocuo
			//unordered_set<int> result;
			temp = DFS2func(F,depth,temp);
            //std::unordered_map<int,vector<ArraySubscript>> id2as;
            for(auto lis:temp){
                lis.resetIndex(lis.orignIndex);
              //  for (auto s: lis.callStack) cout<<"lis 434 "<<s<<endl;
                reportWarning(lis,-1);
                //id2as[lis.ID].push_back(lis);
                /*
                auto findid = id2as.find(lis.ID);
                if (findid != id2as.end()){
                    findid->second, lis);    
                }
                else {
                    id2as.insert(std::make_pair(lis.ID,lis));
                }*/
            }			
           // for (auto idas: id2as) {
             //   reportWarning(idas.second); //TODO!
           // }
/*
            for(unsigned i=0;i<id2as.size();i++){
				if(result.find(id2as[i].ID)!=result.end())
				{
					map<int,vector<ArraySubscript>>::iterator find=sameID.find(temp[i].ID);
					temp[i].resetIndex(temp[i].orignIndex);
					if(find!=sameID.end()){
						vector<ArraySubscript> sameIDAS = find->second;
						reportWarningSameID(sameIDAS);
					}else{
						reportWarning(temp[i]);
					}
					result.erase(temp[i].ID);
				}
			}
*/
        }
		reportWarning();
        common::printLog( "End check function "+FD->getNameAsString()+"\n",common::CheckerName::arrayBound,4,*configure);
		callpath.clear();
		reportedID.clear();
		mapToPath.clear();
		sameID.clear();
		translator->clearExprMap();	
		run_time += clock() - start;
	}
}

clock_t ReturnStackAddrChecker::get_time() {
    return run_time;
}
/*
vector<ArraySubscript> ReturnStackAddrChecker::backwardAnalyse(CFG * cfg, ASTFunction* astF, vector<ArraySubscript> list,unordered_set<int>& set)
{
	common::printLog( "Begin backwardAnalyse:\n",common::CheckerName::arrayBound,3,*configure);
	CFGBlock* Entry;
	unfinished.clear();
	mapToBlockIn.clear();
	mapToBlockOut.clear();
	//prevous strp
	common::printLog( "initial map\n",common::CheckerName::arrayBound,1,*configure);
	for(CFG::iterator iter=cfg->begin();iter!=cfg->end();++iter){
		CFGBlock* block=*iter;
		if(block->pred_begin()==block->pred_end())
		{
			Entry=block;
		}
		vector<ArraySubscript> temp;
		mapToBlockIn.insert(pair<CFGBlock*, vector<ArraySubscript> >(block,temp));
		mapToBlockOut.insert(pair<CFGBlock*, vector<ArraySubscript> >(block,temp));
	}
	//add condition
	for (unsigned i = 0; i < list.size(); ++i)
	{

		CFGBlock* block=list[i].block;
		Stmt *stmt=list[i].stmt;
		stack<const Stmt*> s;
		if(list[i].isLoopBoundChecking)
		{
			for(CFGBlock::iterator iter=block->begin();iter!=block->end();iter++)
			{
				CFGElement element=*(iter);
				if(element.getKind()==CFGElement::Kind::Statement)
				{
					const Stmt* it=((CFGStmt*)&element)->getStmt();
					
					s.push(it);
				}
			}
		}
		else
		{
			for(CFGBlock::iterator iter=block->begin();iter!=block->end();iter++)
			{
				CFGElement element=*(iter);
				if(element.getKind()==CFGElement::Kind::Statement)
				{
					const Stmt* it=((CFGStmt*)&element)->getStmt();
					if(it==stmt)
					{
						break;
					}
					else
					{
						s.push(it);
					}
				}
			}
		}
		bool n=true;
		while(!s.empty())
		{
			const Stmt* it=s.top();
			s.pop();
			int result=throughStmt(astF, (Stmt*)it,list[i],block);
			if(result==1)
			{
				n=false;
				list.erase(list.begin()+i);
				i--;
				while(!s.empty()) s.pop();
				break;
			}
			else if(result==-1)
			{		
				set.insert(list[i].ID);		
				//reportWarning(list[i]);
				////////cerr<<"result==-1"<<endl;
				////////cerr<<"error："<<list[i].location<<endl;
				n=false;
				list.erase(list.begin()+i);
				i--;
				while(!s.empty()) s.pop();
				break;
			}
			else
			{
			}
		}
		if(n)
		{
			map<CFGBlock*, vector<ArraySubscript> >::iterator block_out_iter=mapToBlockOut.find(block);
		//	if(block_out_iter == mapToBlockOut.end()) {//////cerr<<"!!!Error!!!!!!!!!!!!!!mapToBlockOut not find block"<<endl;return list;}
			vector<ArraySubscript> temp=mapToBlockOut[block];
			temp.push_back(list[i]);
			mapToBlockOut.erase(block_out_iter);
			mapToBlockOut.insert(pair<CFGBlock*, vector<ArraySubscript> >(block,temp));
			for(CFGBlock::pred_iterator pred_iter=block->pred_begin();pred_iter!=block->pred_end();++pred_iter){
				CFGBlock *pred=*pred_iter;
				if(pred==NULL)continue;
				unfinished.push_back(pred);
			}
		}
	}
	//flow analyse
	while(unfinished.size()!=0){
		CFGBlock* block=unfinished[0];
		unfinished.erase(unfinished.begin());
		vector<ArraySubscript> temp_in;
		bool T=true;
		for(CFGBlock::succ_iterator succ_iter=block->succ_begin();succ_iter!=block->succ_end();++succ_iter){
			CFGBlock* succ=*succ_iter;
			if(succ==NULL)continue;
			vector<ArraySubscript> c_in=mapToBlockOut[succ];
			Stmt* it=(block->getTerminator().getStmt());
			if(it!=NULL)
			{
				vector<ArraySubscript> tmp_child=getScript(astF, block,it,c_in,T);
				temp_in=Union(temp_in,tmp_child);
			}
			else
			{
				temp_in=Union(temp_in,c_in);
			}
			T=false;
		}
		
		std::vector<ArraySubscript> temp_out=flowThrough(astF, block,temp_in);
		std::vector<ArraySubscript> orign_out=mapToBlockOut[block];
		
		if(!isEuql(temp_out,orign_out))
		{
			for(CFGBlock::pred_iterator pred_iter=block->pred_begin();pred_iter!=block->pred_end();++pred_iter){
				CFGBlock *pred=*pred_iter;
				if(pred==NULL)continue;
				unfinished.push_back(pred);
			}

            map<CFGBlock*, vector<ArraySubscript> >::iterator block_out_iter=mapToBlockOut.find(block);
            mapToBlockOut.erase(block_out_iter);
            mapToBlockOut.insert(pair<CFGBlock*, vector<ArraySubscript> >(block,Union(temp_out,orign_out)));
		}
	}
	return mapToBlockOut[Entry];
}
*/

vector<ArraySubscript> ReturnStackAddrChecker::backwardAnalyse(CFG * cfg, ASTFunction* astF, vector<ArraySubscript> list)
{
/*    z3::expr e1 = translator->ctx.bool_val(1);
    z3::expr e2 = e1;
    translator->zc->dump(e1);
    translator->zc->dump(e2);
    cout<<translator->zc->dump(e1==e2)<<endl;*/
    CFGBlock* Entry;
	unfinished.clear();
	mapToBlockIn.clear();
	mapToBlockOut.clear();
	//prevous process, get Entry block, intialize in and out maps
	for(CFG::iterator iter=cfg->begin();iter!=cfg->end();++iter){
		CFGBlock* block=*iter;
		if(block->pred_begin()==block->pred_end())
		{
			Entry=block;
		}
		vector<ArraySubscript> temp;
		mapToBlockIn.insert(pair<CFGBlock*, vector<ArraySubscript> >(block,temp));
		mapToBlockOut.insert(pair<CFGBlock*, vector<ArraySubscript> >(block,temp));
	}

    //save already in unfinished block, to avoid redundant push of the same block for multi array in the same block
    std::unordered_set<CFGBlock*> alreadyBlocks;

	//add condition,throughStmt in the block where the array is in, if the block dont check the idx cond, add to mapToBlockOut
	for (unsigned i = 0; i < list.size(); ++i)
	{
     //   for (auto s: list[i].callStack) cout<<"backward list i "<<s<<endl;
		CFGBlock* block=list[i].block;
		Stmt *stmt=list[i].stmt;
		stack<const Stmt*> s;
		//if(list[i].isLoopBoundChecking)
		//{
		//	for(CFGBlock::iterator iter=block->begin();iter!=block->end();iter++)
		//	{
		//		CFGElement element=*(iter);
		//		if(element.getKind()==CFGElement::Kind::Statement)
		//		{
		//			const Stmt* it=((CFGStmt*)&element)->getStmt();
		//			
		//			s.push(it);
		//		}
		//	}
		//}
		//else
		{
			for(CFGBlock::iterator iter=block->begin();iter!=block->end();iter++)
			{
				CFGElement element=*(iter);
				if(element.getKind()==CFGElement::Kind::Statement)
				{
					const Stmt* it=((CFGStmt*)&element)->getStmt();
					if(it==stmt)
					{
						break;
					}
					else
					{
						s.push(it);
					}
				}
			}
		}
		bool n=true;
		while(!s.empty())
		{
			const Stmt* it=s.top();
			s.pop();
			int result=throughStmt(astF, (Stmt*)it,list[i],block);
			if(result==1)
			{
				n=false;
				list.erase(list.begin()+i);
				i--;
				while(!s.empty()) s.pop();
				break;
			}
			else if(result==-1)
			{				
				reportWarning(list[i],-1);
				////////cerr<<"result==-1"<<endl;
				////////cerr<<"error："<<list[i].location<<endl;
				n=false;
				list.erase(list.begin()+i);
				i--;
				while(!s.empty()) s.pop();
				break;
			}
			else
			{
			}
		}
	//array idx need to be checked, save info
        if(n)
		{
            map<CFGBlock*, vector<ArraySubscript> >::iterator block_out_iter=mapToBlockOut.find(block);
            if(block_out_iter == mapToBlockOut.end()) {
                //////cerr<<"!!!Error!!!!!!!!!!!!!!mapToBlockOut not find block"<<endl;return list;
            }
			vector<ArraySubscript> temp=mapToBlockOut[block];
			temp.push_back(list[i]);
			mapToBlockOut.erase(block_out_iter);
			mapToBlockOut.insert(pair<CFGBlock*, vector<ArraySubscript> >(block,temp));
			for(CFGBlock::pred_iterator pred_iter=block->pred_begin();pred_iter!=block->pred_end();++pred_iter){
				CFGBlock *pred=*pred_iter;
				if(pred==NULL)continue;
                auto findb = alreadyBlocks.find(pred);
                if (findb != alreadyBlocks.end())
                    continue;
				unfinished.push_back(pred);
                alreadyBlocks.insert(pred);
				/* code */
			}
		}
	}
	//analyse the predecessors of the array block, each block's in_state is the uion of the successors' out_state, use getScript to rm idx cond that have been checked by the terminatros of the block, then call flowThrough to analyse block
	while(unfinished.size()!=0){
		CFGBlock* block=unfinished[0];
		unfinished.erase(unfinished.begin());
		vector<ArraySubscript> temp_in;
		//flag T, true means succ is the true branch of block, false means false branch
        bool T=true;
		for(CFGBlock::succ_iterator succ_iter=block->succ_begin();succ_iter!=block->succ_end();++succ_iter){
			CFGBlock* succ=*succ_iter;
			if(succ==NULL)continue;
			vector<ArraySubscript> c_in=mapToBlockOut[succ];
			Stmt* it=(block->getTerminator().getStmt());
			
			if(it!=NULL)
			{
                //getScript handle if,for,while
				vector<ArraySubscript> tmp_child=getScript(astF,block,it,c_in,T);
				temp_in=Union(temp_in,tmp_child);
			}
			else
			{
				temp_in=Union(temp_in,c_in);
			}
			T=false;
		}
        //flowThrogh will analyse all stmt in block
        //TODO avoid through terminator stmt again
		std::vector<ArraySubscript> temp_out=flowThrough(astF, block,temp_in);
		std::vector<ArraySubscript> orign_out=mapToBlockOut[block];
		
	//TODO only for while need fixpoint, avoid redundant analysis of if block	
		if(!isEuql(temp_out,orign_out))
        {
            for(CFGBlock::pred_iterator pred_iter=block->pred_begin();pred_iter!=block->pred_end();++pred_iter){
                CFGBlock *pred=*pred_iter;
                if(pred==NULL)continue;
                unfinished.push_back(pred);
            }
            map<CFGBlock*, vector<ArraySubscript> >::iterator block_out_iter=mapToBlockOut.find(block);
            mapToBlockOut.erase(block_out_iter);
            mapToBlockOut.insert(pair<CFGBlock*, vector<ArraySubscript> >(block,Union(temp_out,orign_out)));
		}
	}
	return mapToBlockOut[Entry];
}


//flowThrough will analyse all the stmt in block, to check whether stmt satisfy array idx cond
//called by backwardanalysis, call throughsmt to check
vector<ArraySubscript> ReturnStackAddrChecker::flowThrough(ASTFunction* astF, CFGBlock* block,vector<ArraySubscript> list) {
	for (unsigned i = 0; i < list.size(); ++i)
	{
		stack<const Stmt*> s;
		for(CFGBlock::iterator iter=block->begin();iter!=block->end();iter++)
		{
			CFGElement element=*(iter);
			if(element.getKind()==CFGElement::Kind::Statement)
			{
				const Stmt* it=((CFGStmt*)&element)->getStmt();
                if (block->getTerminatorCondition(true) ==it)
                    continue;
                if (block->succ_begin()!=block->succ_end()){
                    CFGBlock *succ=*(block->succ_begin());
					if (!succ) continue;
                    Stmt* term = succ->getTerminatorStmt();
                    if (term) {
						if(term->getStmtClass()==Stmt::ForStmtClass){
							if (iter+1 == block->end()){
								break;
							}
                   		 }
					}
                }
                s.push(it);
			}
		}
		bool n=true;
		while(!s.empty())
		{
			const Stmt* it=s.top();
			s.pop();
			int result=throughStmt(astF, (Stmt*)it,list[i],block);
			if(result==1)
			{
				n=false;
				list.erase(list.begin()+i);
				i--;
				while(!s.empty()) s.pop();
				break;
			}
			else if(result==-1)
			{
				reportWarning(list[i],-1);
				n=false;
				list.erase(list.begin()+i);
				i--;
				while(!s.empty()) s.pop();
				break;
			}
			else
			{
			}
		}

	}
	return list;
}
//vector<ArraySubscript> ReturnStackAddrChecker::getScript(Stmt* stmt,vector<ArraySubscript> input,bool T)
//{
//	////////cerr<<"getScript input size:"<<input.size()<<endl;
//	Expr* cond=NULL;
//	switch(stmt->getStmtClass())
//	{
//		case Stmt::ForStmtClass:
//			{
//				cond=((ForStmt*)stmt)->getCond();				
//				break;
//			}
//		case Stmt::IfStmtClass:
//			{
//				cond=((IfStmt*)stmt)->getCond();				
//				break;
//			}
//		case Stmt::WhileStmtClass:
//			{
//				cond=((WhileStmt*)stmt)->getCond();				
//				break;
//			}
//		//case Stmt::DoStmtClass:
//		//	Stmt* cond=((DoStmt*)it)->getCond();
//		//	if(cond!=NULL)
//		//	{
//		//		return checkConditionStmt(cond,input,T);
//		//	}
//		//	break;
//		default:return input;
//	}
//	if(cond!=NULL)
//	{
//		if(SimpleCheckEnable){
//			vector<ArraySubscript> result;
//			////////cerr<<"input size:"<<input.size()<<endl;
//			for(unsigned i=0;i<input.size();i++)
//			{				
//				//input[i].index->dump();
//				////////cerr<<"input[i].isLoopBoundChecking:"<<input[i].isLoopBoundChecking<<endl;
//				vector<AtomicAPInt> cons=input[i].condition;
//				if(input[i].isLoopBoundChecking){
//					////////cerr<<print(input[i].index)<<" isLoopBoundChecking"<<endl;
//					for(unsigned j=0;j<cons.size();j++)
//					{
//						if(checkConditionStmt(cond,input[i],cons[j],T))
//						{
//							cons.erase(cons.begin()+j);
//							j--;
//						}
//					}
//					if(cons.size()!=0)
//					{
//						input[i].condition=cons;
//						result.push_back(input[i]);	
//					}
//				}
//				else{
//					for(unsigned j=0;j<cons.size();j++)
//					{
//						if(checkConditionStmtHasIdx(cond,input[i],cons[j].getLHS(),T))
//						{
//							cons.erase(cons.begin()+j);
//							j--;
//						}
//					}
//					if(cons.size()!=0)
//					{
//						input[i].condition=cons;
//						result.push_back(input[i]);	
//					}
//				}
//			}
//			return result;
//		}
//		else{			
//			vector<ArraySubscript> result;
//			for(unsigned i=0;i<input.size();i++)
//			{
//				vector<AtomicAPInt> cons=input[i].condition;
//				for(unsigned j=0;j<cons.size();j++)
//				{
//					if(checkConditionStmt(cond,input[i],cons[j],T))
//					{
//						cons.erase(cons.begin()+j);
//						j--;
//					}
//				}
//				if(cons.size()!=0)
//				{
//					input[i].condition=cons;
//					result.push_back(input[i]);	
//				}
//			}
//			return result;
//		}
//	}
//	return input;
//}
vector<ArraySubscript> ReturnStackAddrChecker::ReplaceIdxLoopBound(ASTFunction* astF, CFGBlock* block,Stmt* cond,Expr* expr,ArraySubscript & input, int& AndOrFlag){
	//this function will replace index with loop bound when encountering a for or while statement
	//common::printLog( "ReplaceIdxLoopBound,index:"+print(input.condition[0].getLHS())+"\n",common::CheckerName::arrayBound,0,*configure);
	common::printLog( "ReplaceIdxLoopBound,condition:"+print(expr)+"\n",common::CheckerName::arrayBound,0,*configure);
	vector<ArraySubscript> inputVec;
	if(BinaryOperator * it=dyn_cast<BinaryOperator>(expr)){
		switch(it->getOpcode())
		{
		case clang::BinaryOperatorKind::BO_LAnd:
			{
				AndOrFlag=1;
                vector<ArraySubscript> input1=ReplaceIdxLoopBound(astF, block,cond,it->getLHS()->IgnoreCasts()->IgnoreParens(), input, AndOrFlag);
				vector<ArraySubscript> input2=ReplaceIdxLoopBound(astF, block,cond,it->getRHS()->IgnoreCasts()->IgnoreParens(), input, AndOrFlag);				
				for(auto i1:input1){
					if(i1.condition.size()!=0){
						inputVec.push_back(i1);
					}
				}
				for(auto i2:input2){
					if(i2.condition.size()!=0){
						inputVec.push_back(i2);
					}
				}
				break;
			}
		case clang::BinaryOperatorKind::BO_LOr:
			{
				AndOrFlag=-1;
				vector<ArraySubscript> input1=ReplaceIdxLoopBound(astF, block,cond,it->getLHS()->IgnoreCasts()->IgnoreParens(), input, AndOrFlag);
				vector<ArraySubscript> input2=ReplaceIdxLoopBound(astF, block,cond,it->getRHS()->IgnoreCasts()->IgnoreParens(), input, AndOrFlag);
				for(auto i1:input1){
					if(i1.condition.size()!=0){
						inputVec.push_back(i1);
					}
				}
				for(auto i2:input2){
					if(i2.condition.size()!=0){
						inputVec.push_back(i2);
					}
				}
				break;
			}
		default:
			{
				ArraySubscript inputtmp(input);
				Expr* lhs=it->getLHS()->IgnoreCasts()->IgnoreParens();
				Expr* rhs=it->getRHS()->IgnoreCasts()->IgnoreParens();
                //VarDecl* rd=getVarDecl(rhs);
                //rd->dump();
				for(auto con:input.condition){
					if(con.op==clang::BinaryOperatorKind::BO_LT)
					{
						//common::printLog( "ReplaceIdxLoopBound, index:"+print(con.getLHS())+"\n",common::CheckerName::arrayBound,0,*configure);
						VarDecl* idx=getVarDecl(con.getLHS());
						//if (!idx){
							//con index is not declrefexpr, maybe memberexpr etc.
						//}
						//idx->dump();
						VarDecl* lhsV=getVarDecl(lhs);
						VarDecl* rhsV=getVarDecl(rhs);
						bool idxEqlhs=false;
						bool idxEqrhs=false;
						if(SimpleExprCheckOnly){
							 if(dyn_cast<DeclRefExpr>(rhs) || dyn_cast<MemberExpr>(rhs) || dyn_cast<ArraySubscriptExpr>(rhs)) {
							 }
							 else{
							 	common::printLog( "SimpleExprCheckOnly, not DeclRefExpr:"+print(rhs)+",return\n",common::CheckerName::arrayBound,0,*configure);
							 	return inputVec;
							 }
							if(idx==NULL||lhsV==NULL||rhsV==NULL){
								idxEqlhs=print(con.getLHS())==print(lhs);
								idxEqrhs=print(con.getLHS())==print(rhs);
							}
							else{
								idxEqlhs=idx==lhsV;
								idxEqrhs=idx==rhsV;
							}
						}else{
							idxEqlhs=print(con.getLHS())==print(lhs);
							idxEqrhs=print(con.getLHS())==print(rhs);
							if (!idxEqlhs && !idxEqrhs) {
								bool checkIdxIncflag=false;
								const UnaryOperator* varIncStmt = NULL;
								//check whether has index++ in for(i<n) body
								for(CFGBlock::succ_iterator succ_iter=block->succ_begin();succ_iter!=block->succ_end();++succ_iter){
									CFGBlock* succ=*succ_iter;
									if(succ==NULL)break;  
									for(CFGBlock::iterator iter=succ->begin();iter!=succ->end();iter++)
									{
										CFGElement element=*(iter);
										if(element.getKind()==CFGElement::Kind::Statement)
										{
											const Stmt* succStmt = ((CFGStmt*)&element)->getStmt();
											if(const UnaryOperator * uo2=dyn_cast<UnaryOperator>(succStmt))
											{
												if(uo2->getOpcode()==clang::UnaryOperatorKind::UO_PostInc || uo2->getOpcode()==clang::UnaryOperatorKind::UO_PreInc) {
													Expr* varInc=uo2->getSubExpr()->IgnoreCasts()->IgnoreParens();
													checkIdxIncflag = print(con.getLHS())==print(varInc);
													varIncStmt=uo2;
												}
											}
										}
									}
									break;
								}
								if (!checkIdxIncflag)
								for(CFGBlock::pred_iterator pred_iter=block->pred_begin();pred_iter!=block->pred_end();++pred_iter){
									CFGBlock* pred=*pred_iter;
									if(pred==NULL)break;  
									for(CFGBlock::pred_iterator pred_iter2=pred->pred_begin();pred_iter2!=pred->pred_end();++pred_iter2){
										CFGBlock* pred2=*pred_iter2;
										if(pred2==NULL)break;  
										for(CFGBlock::iterator iter=pred2->begin();iter!=pred2->end();iter++)
										{
											CFGElement element=*(iter);
											if(element.getKind()==CFGElement::Kind::Statement)
											{
												const Stmt* predStmt = ((CFGStmt*)&element)->getStmt();
												if(const UnaryOperator * uo2=dyn_cast<UnaryOperator>(predStmt))
												{
													if(uo2->getOpcode()==clang::UnaryOperatorKind::UO_PostInc || uo2->getOpcode()==clang::UnaryOperatorKind::UO_PreInc) {
														Expr* varInc=uo2->getSubExpr()->IgnoreCasts()->IgnoreParens();
														checkIdxIncflag = print(con.getLHS())==print(varInc);
														varIncStmt=uo2;
													}
												}
											}
										}	
																			
									}							
									break;
								}
								if (checkIdxIncflag) {
									if (SolvePathConstraints) {
                                		vector<z3::expr> idxexprs = translator->zc->clangExprToZ3Expr(con.getLHS(), astF, false);//original expr
										if (idxexprs.empty())
											break;
										vector<z3::expr> rexprs = translator->zc->clangExprToZ3Expr(it->getRHS(), astF);
										if (rexprs.empty())
											break;
										z3::expr tmp=idxexprs.back() <  translator->zc->clangExprToZ3Expr(con.getLHS(), astF, true).back()+ rexprs.back();
										input.updatePathConstraint(it, tmp, true);
										common::printLog( "update path constraints "+tmp.to_string()+" at for stmt where "+print(it)+", since index increases ("+getInfo(varIncStmt)+") along with the loop\n",common::CheckerName::arrayBound,3,*configure);
										common::printLog( "input path constraints:"+input.pathConstraint.to_string()+"\n",common::CheckerName::arrayBound,2,*configure);
										inputVec.push_back(input);
										return inputVec;
									}
								}							
							} 
						}
						if(it->getOpcode()==BinaryOperatorKind::BO_GT||it->getOpcode()==BinaryOperatorKind::BO_GE){
							//n>i,i<size--n<=size
							//n>=i,i<size--n<size
							if(idxEqrhs){
								common::printLog( "ReplaceIdxLoopBound, index==rhs:"+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);								
								//common::printLog( "FuncNow:"+FuncNow->getNameAsString()+"\n",common::CheckerName::arrayBound,0,*configure);
								//common::printLog( "block id:"+int2string(block->getBlockID())+"\n",common::CheckerName::arrayBound,0,*configure);
								//common::printLog( "rhs:"+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);
								if((!CheckTaintArrayOnly && !dyn_cast<IntegerLiteral>(rhs)) || isTainted(FuncNow,block,cond,rhs))	{								
                                    //input.changeIndex(rhs);
									//input.condition.clear();
									//vector<AtomicAPInt>(input.condition).swap(input.condition);
									AtomicAPInt newCon;
									newCon.lhs=lhs;
									newCon.op=con.op;
									newCon.rhs=con.rhs;
									if(it->getOpcode()==BinaryOperatorKind::BO_GT) newCon.rhs=con.rhs+1;
									common::printLog( "ReplaceIdxLoopBound, from "+(inputtmp.lowBoundCondition).to_string()+"to "+print(newCon.lhs)+">="+int2string(newCon.rhs)+"\n",common::CheckerName::arrayBound,3,*configure);
									vector<AtomicAPInt> tmp;
									tmp.push_back(newCon);
									inputtmp.isLoopBoundChecking=true;
                                    if (SolvePathConstraints){
                                        vector<z3::expr> lhsexpr = translator->zc->clangExprToZ3Expr(newCon.lhs, astF);
                                        
                                        z3::expr rhsexpr = translator->zc->Int2Z3Expr(newCon.rhs);
                                        if (!lhsexpr.empty()){
                                            if (!inputtmp.isUpBoundChecked)
                                                inputtmp.upBoundCondition = lhsexpr.back() < rhsexpr; 
                                            if (!inputtmp.isUnsignedChecked) {
                                                if(it->getOpcode()==BinaryOperatorKind::BO_GT) {
                                                    inputtmp.lowBoundCondition = lhsexpr.back() >= translator->zc->Int2Z3Expr(1);
                                                }
                                                else{
                                                    inputtmp.lowBoundCondition = lhsexpr.back() >= translator->zc->Int2Z3Expr(0);
                                                }
                                                
                                            }
                                            //delete lhsexpr; lhsexpr=nullptr;
                                        }
                                        
                                    }
                                    inputtmp.condition=tmp;
									inputtmp.block=block;
									inputtmp.stmt=cond;
									SourceManager *sm;
									sm = &(FuncNow->getASTContext().getSourceManager()); 
									string loc = rhs->getBeginLoc().printToString(*sm);
									inputtmp.location=loc;
									inputtmp.index=lhs;
									inputVec.push_back(inputtmp);
								}
								else{
									common::printLog( "ReplaceIdxLoopBound,bound "+print(lhs)+" not tainted\n",common::CheckerName::arrayBound,0,*configure);
                                    if (input.isVectorArraySubscript)
                                        input.isLoopBoundChecking = true;
                                        inputVec.push_back(input);
                                }
							}
                            else {
                                inputVec.push_back(input);
                            }
						}
						if(it->getOpcode()==BinaryOperatorKind::BO_LT||it->getOpcode()==BinaryOperatorKind::BO_LE){
							//i<n,i<size--n<size+1
							//i<=n,i<size--n<size
							if(idxEqlhs){
								common::printLog( "ReplaceIdxLoopBound, index==lhs:"+print(lhs)+"\n",common::CheckerName::arrayBound,0,*configure);								
								common::printLog( "FuncNow:"+FuncNow->getNameAsString()+"\n",common::CheckerName::arrayBound,0,*configure);
								common::printLog( "block id:"+int2string(block->getBlockID())+"\n",common::CheckerName::arrayBound,0,*configure);
								common::printLog( "rhs:"+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);
								if((!CheckTaintArrayOnly && !dyn_cast<IntegerLiteral>(rhs)) || isTainted(FuncNow,block,cond,rhs))
								{								
									//input.changeIndex(rhs);
									//input.condition.clear();
									//vector<AtomicAPInt>(input.condition).swap(input.condition);
									AtomicAPInt newCon;
									newCon.lhs=rhs;
									newCon.op=con.op;
									newCon.rhs=con.rhs;
									if(it->getOpcode()==BinaryOperatorKind::BO_LT) 
                                        newCon.rhs=con.rhs+1;

									common::printLog( "ReplaceIdxLoopBound, from "+(inputtmp.upBoundCondition).to_string()+" to "+print(newCon.lhs)+"<"+(inputtmp.upBound_expr+1).to_string()+"\n",common::CheckerName::arrayBound,3,*configure);
									vector<AtomicAPInt> tmp;
									tmp.push_back(newCon);
									//inputtmp=input;
                                    if(it->getOpcode()==BinaryOperatorKind::BO_LT) 
                                        inputtmp.upBound_expr = inputtmp.upBound_expr+1;
                                    inputtmp.condition=tmp;
									inputtmp.isLoopBoundChecking=true;
                                    if (SolvePathConstraints){
                                        vector<z3::expr> lhsexpr = translator->zc->clangExprToZ3Expr(newCon.lhs, astF);

                                    //    z3::expr rhsexpr = translator->zc->Int2Z3Expr(newCon.rhs);
                                        if (!lhsexpr.empty()){
//                                            if (!inputtmp.isUpBoundChecked)
                                                inputtmp.upBoundCondition = lhsexpr.back() < inputtmp.upBound_expr; 
  //                                          if (!inputtmp.isUnsignedChecked) {
                                                if(it->getOpcode()==BinaryOperatorKind::BO_LT) {
                                                    inputtmp.lowBoundCondition = lhsexpr.back() >= translator->zc->Int2Z3Expr(1);
                                                }
                                                else{
                                                    inputtmp.lowBoundCondition = lhsexpr.back() >= translator->zc->Int2Z3Expr(0);
                                                }

//                                            }
                                                //delete lhsexpr; lhsexpr=nullptr;
                                        } 
                                    }
                                    inputtmp.block=block;
                                    inputtmp.stmt=cond;
									SourceManager *sm;
									sm = &(FuncNow->getASTContext().getSourceManager()); 
									string loc = rhs->getBeginLoc().printToString(*sm);
									//common::printLog( "use FuncNow,get location end\n",common::CheckerName::arrayBound,0,*configure);
									inputtmp.location=loc;
									inputtmp.ID=input.ID;
									inputtmp.orignIndex=rhs;
									inputtmp.indexCnt=input.indexCnt;
									inputtmp.arrayName=input.arrayName;
									inputtmp.arrayExpr=input.arrayExpr;
									inputtmp.index=rhs;
									inputtmp.arrayIdx=input.arrayIdx;
									inputtmp.func=input.func;
									inputtmp.originalLocation=input.originalLocation;
									//input.condition=tmp;
									inputVec.push_back(inputtmp);
								}
								else{
									common::printLog( "ReplaceIdxLoopBound,bound "+print(rhs)+" not tainted\n",common::CheckerName::arrayBound,0,*configure);
                                    if (input.isVectorArraySubscript)
                                        input.isLoopBoundChecking = true;
                                        inputVec.push_back(input);
								}
							}
                            else {
                                inputVec.push_back(input);
                            }
						}
					}
				}
			}
		}
	}
	
	sameID[input.ID]=inputVec;
	return inputVec;
}
vector<ArraySubscript> ReturnStackAddrChecker::getScript(ASTFunction* astF, CFGBlock* block,Stmt* stmt,vector<ArraySubscript> input,bool T)
{
	//this function deal with if, for, while statement
	common::printLog( "analyse conditon from child from condition stmt\n",common::CheckerName::arrayBound,1,*configure);
	//if(SimpleCheckEnable){

	//	////////cerr<<"getScript input size:"<<input.size()<<endl;
	//	Expr* cond=NULL;
	//	if(stmt->getStmtClass() == Stmt::IfStmtClass){		
	//		cond=((IfStmt*)stmt)->getCond();							
	//	}
	//	else{
	//		return input;
	//	}
	//	if(cond!=NULL)
	//	{				
	//		vector<ArraySubscript> result;
	//		////////cerr<<"input size:"<<input.size()<<endl;
	//		for(unsigned i=0;i<input.size();i++)
	//		{				
	//			vector<AtomicAPInt> cons=input[i].condition;
	//			for(unsigned j=0;j<cons.size();j++)
	//			{
	//				if(checkConditionStmtHasIdx(cond,input[i],cons[j].getLHS(),T))
	//				{
	//					cons.erase(cons.begin()+j);
	//					j--;
	//				}
	//			}
	//			if(cons.size()!=0)
	//			{
	//				input[i].condition=cons;
	//				result.push_back(input[i]);	
	//			}
	//		}
	//		return result;
	//	}
	//	return input;
	//}
	//else
	{
		////////cerr<<"getScript input size:"<<input.size()<<endl;
		Expr* cond=NULL;
		switch(stmt->getStmtClass())
		{
			case Stmt::ForStmtClass:
				{
					cond=((ForStmt*)stmt)->getCond();
					if(cond!=NULL)
					{	
						common::printLog( "encounter for stmt with condition: "+print(cond)+"\n",common::CheckerName::arrayBound,4,*configure);			
						vector<ArraySubscript> result;
						for(unsigned i=0;i<input.size();i++)
						{
							int AndOrFlag=0;
                            if (SolvePathConstraints) {
                                bool hasUnchecked =  updateAndSolveConstraints(astF,input[i], cond, T,block);
                                if (hasUnchecked){
									vector<ArraySubscript> newInput=ReplaceIdxLoopBound(astF, block,cond,cond,input[i], AndOrFlag);
                                    if (newInput.size()==1){
                                        result.push_back(newInput[0]);
                                    }
                                    else if(newInput.size()==0){

                                    }
                                    else {
                                        ArraySubscript tmp(newInput[0]);
                                        if (AndOrFlag == 1){
                                            tmp.upBoundCondition = translator->zc->TRUE;
                                            for (auto in:newInput){
                                                tmp.upBoundCondition = tmp.upBoundCondition && in.upBoundCondition;
                                            }
                                        }
                                        else if (AndOrFlag == -1) {
                                            tmp.upBoundCondition = translator->zc->FALSE;
                                            for (auto in:newInput){
                                                tmp.upBoundCondition = tmp.upBoundCondition && in.upBoundCondition;
                                            }
                                        }
                                        
                                        result.push_back(tmp);
                                    }
                                    
                                }
                            }
                            else{
                                vector<AtomicAPInt> cons=input[i].condition;
                                for(unsigned j=0;j<cons.size();j++)
                                {
                                    int r;
                                    if(SimpleCheckEnable){
                                        r=checkConditionStmtHasIdx(block,stmt,cond,input[i],cons[j],T,true);
                                        //the last argument is true, we check whehter index<untainted, if index<taint, return false,
									//to avoid for(i<m) making i<size checked.
								}
								else{r=checkConditionStmt(astF, cond,input[i],cons[j],T);}
								common::printLog( "checkConditionStmt result: "+int2string(r)+"\n",common::CheckerName::arrayBound,0,*configure);	
								if(r)
								{
									cons.erase(cons.begin()+j);
									j--;
								}
								if(!r){
									//common::printLog( "checkConditionStmt false\n",common::CheckerName::arrayBound,0,*configure);
									vector<ArraySubscript> newInput=ReplaceIdxLoopBound(astF, block,cond,cond,input[i],AndOrFlag);
									//common::printLog( "-------ReplaceIdxLoopBound end------\n",common::CheckerName::arrayBound,0,*configure);
									//common::printLog( "After ReplaceIdxLoopBound:\n",common::CheckerName::arrayBound,0,*configure);
									for(auto in:newInput){
										if(in.condition.size()!=0){
											vector<AtomicAPInt> cons2=in.condition;
											for(unsigned j=0;j<cons2.size();j++)
											{										
												common::printLog( print(cons2[j].getLHS())+" op "+int2string(cons2[j].getRHS())+"\n",common::CheckerName::arrayBound,0,*configure);	
											}
											cons.clear();
											result.push_back(in);
										}
									}
									break;
								}
							}
							if(cons.size()!=0)
							{
								input[i].condition=cons;
								result.push_back(input[i]);	
							}
                            
						}
                    }
						return result;
		
					}
					break;
				}
			case Stmt::IfStmtClass:
				{
					cond=((IfStmt*)stmt)->getCond();	
					vector<ArraySubscript> result;
					for(unsigned i=0;i<input.size();i++)
					{
                        if (SolvePathConstraints) {
                            bool hasUnchecked =  updateAndSolveConstraints(astF,input[i], cond, T,block);
                            if (hasUnchecked){
                                result.push_back(input[i]);
                            }
                        }
                        else{
                            vector<AtomicAPInt> cons=input[i].condition;
						for(unsigned j=0;j<cons.size();j++)
						{
							int r;
							if(SimpleCheckEnable){r=checkConditionStmtHasIdx(block,stmt,cond,input[i],cons[j],T,false);}
							else{r=checkConditionStmt(astF, cond,input[i],cons[j],T);}
							if(r)
							{
								common::printLog( "checkConditionStmt true,find if check: "+print(cond)+", "+int2string(T)+"\n",common::CheckerName::arrayBound,0,*configure);
								cons.erase(cons.begin()+j);
								j--;
							}
						}
						if(cons.size()!=0)
						{
							input[i].condition=cons;
							result.push_back(input[i]);	
						}
					}}
					return result;
					break;
				}
			case Stmt::WhileStmtClass:
				{
					cond=((WhileStmt*)stmt)->getCond();	
					if(cond!=NULL)
					{	
						vector<ArraySubscript> result;
						for(unsigned i=0;i<input.size();i++)
						{
                            if (SolvePathConstraints) {
                                bool hasUnchecked =  updateAndSolveConstraints(astF, input[i], cond, T,block);
                                if (hasUnchecked){
                                    result.push_back(input[i]);
                                }
                            }
                            else{
                                vector<AtomicAPInt> cons=input[i].condition;
							for(unsigned j=0;j<cons.size();j++)
							{
								int r;
								if(SimpleCheckEnable){
									r=checkConditionStmtHasIdx(block,stmt,cond,input[i],cons[j],T,true);
									//the last argument is true, we check whehter index<untainted, if index<taint, return false,
									//to avoid for(i<m) making i<size checked.
								}
								else{r=checkConditionStmt(astF, cond,input[i],cons[j],T);}								
								common::printLog( "checkConditionStmt result: "+int2string(r)+"\n",common::CheckerName::arrayBound,0,*configure);	
								if(r)
								{
									cons.erase(cons.begin()+j);
									j--;
								}
								if(!r){
									common::printLog( "checkConditionStmt false\n",common::CheckerName::arrayBound,0,*configure);
									int AndOrFlag = 0;
                                    vector<ArraySubscript> newInput=ReplaceIdxLoopBound(astF, block,cond,cond,input[i], AndOrFlag);
									//common::printLog( "After ReplaceIdxLoopBound:\n",common::CheckerName::arrayBound,0,*configure);
									for(auto in:newInput){
										if(in.condition.size()!=0){
											vector<AtomicAPInt> cons2=in.condition;
											for(unsigned j=0;j<cons2.size();j++)
											{										
												common::printLog( print(cons2[j].getLHS())+" op "+int2string(cons2[j].getRHS())+"\n",common::CheckerName::arrayBound,0,*configure);	
											}
											cons.clear();
											result.push_back(in);
										}
									}
									break;
								}
							}
							if(cons.size()!=0)
							{
								input[i].condition=cons;
								result.push_back(input[i]);	
							}
						}}
						return result;
		
					}
					break;
				}
			//case Stmt::DoStmtClass:
			//	Stmt* cond=((DoStmt*)it)->getCond();
			//	if(cond!=NULL)
			//	{
			//		return checkConditionStmt(cond,input,T);
			//	}
			//	break;
			default:return input;
		}
		return input;
	}
	
}

void ReturnStackAddrChecker::handleCallExprWithReturn(CallExpr* callexpr) {
    FunctionDecl* fun = callexpr->getDirectCallee();
    if(fun==nullptr || !fun->getBody()) return;
    DeclarationName DeclName = fun->getNameInfo().getName();
    std::string FuncName = DeclName.getAsString();
    if(FuncName=="__builtin_expect")
    {
    //    return checkConditionStmt(call->getArg(0)->IgnoreCasts()->IgnoreParens(),input,con,flag);

    }
    fun->dump();
    fun->getBody()->dump();
    FindReturnStmt returnFinder;
    returnFinder.Visit(fun->getBody());
    std::vector<ReturnStmt*> returnStmts = returnFinder.getReturnStmts();
    for (auto ret : returnStmts) {

    }

}

//return true if hasUnchecked as
bool ReturnStackAddrChecker::updateAndSolveConstraints(ASTFunction* astF, ArraySubscript& input, Stmt* stmt,bool flag, CFGBlock* block) {
     common::printLog( "updateAndSolveConstraints at stmt "+print(stmt)+(flag==1?" true":" false")+" branch\n",common::CheckerName::arrayBound,3,*configure);

	bool flagupdated = false;
    if (!flag) {
        //handle break in loops, the false branch
        //if (i==k) break; the false branch: i<k
        CFGBlock* succBlock = ((CFGBlock*)*(block->succ_begin()));
        Stmt* term = succBlock->getTerminatorStmt();
		if (term)
		{
			if (BreakStmt *breaks = dyn_cast<BreakStmt>(term))
			{
				// if stmt, break;
				if (BinaryOperator *it = dyn_cast<BinaryOperator>(stmt))
				{
					switch (it->getOpcode())
					{
					case clang::BinaryOperatorKind::BO_EQ:
					{
						flagupdated = true;
						vector<z3::expr> lexprs = translator->zc->clangExprToZ3Expr(it->getLHS(), astF);
						if (lexprs.empty())
							break;
						vector<z3::expr> rexprs = translator->zc->clangExprToZ3Expr(it->getRHS(), astF);
						if (rexprs.empty())
							break;
						z3::expr tmp=lexprs.back() >= rexprs.back();
						common::printLog( "updatePathConstraint: "+tmp.to_string()+"\n",common::CheckerName::arrayBound,3,*configure);			
						input.updatePathConstraint(stmt, tmp, false);
						common::printLog( "input path constraints:"+input.pathConstraint.to_string()+"\n",common::CheckerName::arrayBound,2,*configure);
						// delete lexprs; lexprs=nullptr;
						// delete rexprs; rexprs=nullptr;
						break;
					}
					default:
						break;
					}
				}
			}
		}
	}
    if(UnaryOperator * uo=dyn_cast<UnaryOperator>(stmt))
    {
        if(uo->getOpcode()==clang::UnaryOperatorKind::UO_PostInc || uo->getOpcode()==clang::UnaryOperatorKind::UO_PreInc) {
            Expr* expr=uo->getSubExpr()->IgnoreCasts()->IgnoreParens();
            vector<z3::expr> stmtexpr = translator->zc->clangExprToZ3Expr(expr,astF);
            if (stmtexpr.empty())
                return true;
            for(CFGBlock::pred_iterator pred_iter=block->pred_begin();pred_iter!=block->pred_end();++pred_iter){
                CFGBlock* pred = *pred_iter;
                if (pred==NULL) continue;
                Stmt* term = pred->getTerminatorStmt();
                if (term) {
                    if (term->getStmtClass()==Stmt::IfStmtClass) {
                        //handle if (cond) xx++
                        Stmt* termCond = ((IfStmt*)term)->getCond()->IgnoreCasts()->IgnoreParens();
                        if (termCond) {
                            //if (loop%c==0) i++, loop<loopCnt => i==loopCnt/c
                            if (BinaryOperator* bo = dyn_cast<BinaryOperator>(termCond)) {
                                if (bo->getOpcode()==clang::BinaryOperatorKind::BO_EQ) {
                                    //loop%c==0
                                    Stmt* subCond = bo->getLHS()->IgnoreCasts()->IgnoreParens();
                                    if (!subCond) continue;
                                    if (BinaryOperator* bo2 = dyn_cast<BinaryOperator>(subCond)) {
                                        if (bo2->getOpcode()==clang::BinaryOperatorKind::BO_Rem) {
                                            //loop%c
                                            Stmt* l = bo2->getLHS()->IgnoreCasts()->IgnoreParens();
                                            Stmt* r = bo2->getRHS()->IgnoreCasts()->IgnoreParens();
                                            vector<z3::expr> cexpr = translator->zc->clangExprToZ3Expr(r,astF);
                                            if (cexpr.empty())
                                                continue;

                                            for(CFGBlock::pred_iterator pred_iter2=pred->pred_begin();pred_iter2!=pred->pred_end();++pred_iter2){
                                                CFGBlock* pred2 = *pred_iter2;
                                                if (pred2==NULL) continue;
                                                Stmt* term2 = pred2->getTerminatorStmt();
                                                if (term2) {
                                                    if (term2->getStmtClass()==Stmt::ForStmtClass) {
                                                        //handle for(loop<loopCnt)
                                                        Stmt* forCond = ((ForStmt*)term2)->getCond()->IgnoreCasts()->IgnoreParens();
                                                        if (forCond) {
                                                            if (BinaryOperator* forcondbo = dyn_cast<BinaryOperator>(forCond)) {
                                                                if (forcondbo->getOpcode()==clang::BinaryOperatorKind::BO_LT) {
                                                                    Stmt* loopvar = forcondbo->getLHS()->IgnoreCasts()->IgnoreParens();
                                                                    Stmt* loopcnt = forcondbo->getRHS()->IgnoreCasts()->IgnoreParens();
                                                                    if (print(l)==print(loopvar)){
                                                                        vector<z3::expr> loopcntexpr = translator->zc->clangExprToZ3Expr(loopcnt,astF);
                                                                        if (loopcntexpr.empty())
                                                                            continue;
                                                                        //i==loopcont/c
                                                                        z3::expr myeq = translator->zc->clangOp2Z3Op(stmtexpr.back(), loopcntexpr.back()/cexpr.back(),BinaryOperatorKind::BO_EQ);
                                                                        common::printLog( "updatePathConstraint: "+myeq.to_string()+"\n",common::CheckerName::arrayBound,3,*configure);			
																		input.updatePathConstraint(stmt,myeq,true);
																		common::printLog( "input path constraints:"+input.pathConstraint.to_string()+"\n",common::CheckerName::arrayBound,2,*configure);
                                                                        flagupdated=true;
                                                                        //delete loopcntexpr; loopcntexpr=nullptr;
                                                                    }

                                                                }
                                                            }       
                                                        }
                                                    }
                                                }
                                            }
                                            //delete cexpr; cexpr=nullptr;
                                        }                                        
                                    }
                                }
                            }
                        }
                    }
                }
            }
            /*
    for(CFGBlock::succ_iterator succ_iter=block->succ_begin();succ_iter!=block->succ_end();++succ_iter){

                CFGBlock* succ=*succ_iter;
                if(succ==NULL)continue;  
                for(CFGBlock::iterator iter=succ->begin();iter!=succ->end();iter++)
                {
                    CFGElement element=*(iter);
                    if(element.getKind()==CFGElement::Kind::Statement)
                    {
                        const Stmt* succStmt = ((CFGStmt*)&element)->getStmt();
                        if(const UnaryOperator * uo2=dyn_cast<UnaryOperator>(succStmt))
                        {
                            if(uo2->getOpcode()==clang::UnaryOperatorKind::UO_PostInc || uo2->getOpcode()==clang::UnaryOperatorKind::UO_PreInc) {
                                Expr* loopVar=uo2->getSubExpr()->IgnoreCasts()->IgnoreParens();
                                vector<z3::expr> succexpr = translator->zc->clangExprToZ3Expr(loopVar,astF);
                                if (!succexpr)
                                    continue;
                                z3::expr eq = translator->zc->clangOp2Z3Op(stmtexpr.back(), succexpr.back(),BinaryOperatorKind::BO_EQ);
                                input.updatePathConstraint(stmt, eq, true);
                            }
                        }
                    }
                }
            }  */ 
            //delete stmtexpr; stmtexpr=nullptr;
        }
    }
    if (!flagupdated){
        vector<z3::expr> condv = translator->zc->clangExprToZ3Expr(stmt,astF);
        if (condv.empty()) {
            return true;
        }
		z3::expr r = translator->zc->TRUE;
		bool firstflag=true;
		int i=condv.size();
		while(i) {
			z3::expr e = condv.back();
			condv.pop_back();
			i--;
			if (e.is_int()) {
				e = (e!=translator->zc->ZERO);
			}
			if (!e.is_bool())
				continue;
			if (!flag && firstflag) {
				r = r && !e;
				firstflag = false;
			}
			else {
				if (e.is_true()) continue;
				r = r && e;					
			}		
		}
		if (r == translator->zc->TRUE)
			return input.isUpBoundChecked==0 || input.isUnsignedChecked==0;
		/*
        if (condv.size() > 1){
            condv.pop_back();
        }*/
		common::printLog( "updatePathConstraint: "+r.to_string()+"\n",common::CheckerName::arrayBound,3,*configure);			
        input.updatePathConstraint(stmt, r, true);
		common::printLog( "input path constraints:"+input.pathConstraint.to_string()+"\n",common::CheckerName::arrayBound,2,*configure);
        //delete condv; condv=nullptr;
    }
    if (translator->zc->isPermanentSat(!input.pathConstraint)) {
        //false will imply everything, making any bound condition checked, so return true to skip this?
    }
    //common::printLog( "input path constraints:"+input.pathConstraint.to_string()+"\n",common::CheckerName::arrayBound,2,*configure);
    if (!input.isUpBoundChecked) {    
//        common::printLog( "need check array index upbound:"+input.upBoundCondition.to_string()+"\n",common::CheckerName::arrayBound,1,*configure);
        if(translator->zc->implySolve(input.pathConstraint, input.upBoundCondition))
        {
            input.isUpBoundChecked = 1;//checked
            if (!translator->zc->isPermanentSat(!input.pathConstraint))
                common::printLog( "checked upbound "+input.upBoundCondition.to_string()+"\n",common::CheckerName::arrayBound,3,*configure);
        }
        else if (translator->zc->implySolve(input.pathConstraint, !(input.upBoundCondition))){
            //will make out bound permanent true
            input.isUpBoundChecked = -1;//overflow
            reportWarning(input,1);
            common::printLog( "upbound overflow "+input.upBoundCondition.to_string()+"\n",common::CheckerName::arrayBound,3,*configure);
        }
		else {
			common::printLog( "not check array upbound "+input.upBoundCondition.to_string()+", continue\n",common::CheckerName::arrayBound,3,*configure);
		}
    }
    if (!input.isUnsignedChecked) {
//        common::printLog( "need check array index lowbound:"+input.lowBoundCondition.to_string()+"\n",common::CheckerName::arrayBound,1,*configure);
        if (translator->zc->implySolve(input.pathConstraint, !(input.lowBoundCondition))){
            input.isUnsignedChecked = 1;
            common::printLog( "checked lowbound "+input.lowBoundCondition.to_string()+"\n",common::CheckerName::arrayBound,3,*configure);
        }
        else if (translator->zc->implySolve(input.pathConstraint, !(input.lowBoundCondition))){
            input.isUnsignedChecked = -1;
            reportWarning(input,0);
            common::printLog( "lowbound overlfow"+input.lowBoundCondition.to_string()+"\n",common::CheckerName::arrayBound,3,*configure);
        }
		else {
			common::printLog( "not check array lowbound "+input.lowBoundCondition.to_string()+", continue\n",common::CheckerName::arrayBound,3,*configure);
		}
    }
    return input.isUpBoundChecked==0 || input.isUnsignedChecked==0;
}

bool ReturnStackAddrChecker::checkConditionStmt(ASTFunction* astF, Expr* stmt,ArraySubscript input,AtomicAPInt con,bool flag)//true : remove this cons
{
	if (Z3CheckEnable) {
		bool r = checkConditionStmtUseZ3(astF, stmt,input,con,flag);
		return r;
	}
	if(CallExpr* call = dyn_cast<CallExpr>(stmt->IgnoreCasts()->IgnoreParens()) )
	{
		FunctionDecl* fun=call->getDirectCallee();
		if(fun==nullptr) return false;
		DeclarationName DeclName = fun->getNameInfo().getName();
		std::string FuncName = DeclName.getAsString();
		if(FuncName=="__builtin_expect")
		{
			return checkConditionStmt(astF, call->getArg(0)->IgnoreCasts()->IgnoreParens(),input,con,flag);

		}
	}
	bool result=false;
	if(BinaryOperator * it=dyn_cast<BinaryOperator>(stmt))
	{
		
		switch(it->getOpcode())
		{
		case clang::BinaryOperatorKind::BO_LAnd:
			{
				bool temp_lhs=checkConditionStmt(astF, it->getLHS()->IgnoreCasts()->IgnoreParens(),input,con,flag);
				bool temp_rhs=checkConditionStmt(astF, it->getRHS()->IgnoreCasts()->IgnoreParens(),input,con,flag);
				if(flag)
				{
					result=temp_lhs || temp_rhs;
				}
				else
				{
					result=temp_lhs && temp_rhs;
				}

				break;
			}
		case clang::BinaryOperatorKind::BO_LOr:
			{
				bool temp_lhs=checkConditionStmt(astF, it->getLHS()->IgnoreCasts()->IgnoreParens(),input,con,flag);
				bool temp_rhs=checkConditionStmt(astF, it->getRHS()->IgnoreCasts()->IgnoreParens(),input,con,flag);
				if(flag)
				{
					result=temp_lhs && temp_rhs;
				}
				else
				{
					result=temp_lhs || temp_rhs;
				}
				break;
			}
		default:
			{
				Expr* lhs=it->getLHS()->IgnoreImpCasts()->IgnoreCasts()->IgnoreParens();
				Expr* rhs=it->getRHS()->IgnoreImpCasts()->IgnoreCasts()->IgnoreParens();
				if(con.op==clang::BinaryOperatorKind::BO_LT)
				{
					string index=print(con.lhs->IgnoreCasts()->IgnoreParens());
					if(flag)
					{
						uint64_t value = con.rhs;
						common::printLog( int2string(flag)+"path\n",common::CheckerName::arrayBound,0,*configure);
						common::printLog( "checkConditionStmt rhs:"+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);
						if(index==print(lhs))
						{
							//cerr<<"Index match"<<endl;
							common::printLog( "Index match\n",common::CheckerName::arrayBound,0,*configure);
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_LT)
							{
								result=CheckConditionExpr(rhs,"<",value);
								if(print(rhs).find("sizeof")!=std::string::npos)
								{									
									common::printLog( print(lhs)+"< "+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);									
									result=1;
								}
							}
							else if(it->getOpcode()==clang::BinaryOperatorKind::BO_LE)
							{
								result=CheckConditionExpr(rhs,"<",value);//
								if(print(rhs).find("sizeof")!=std::string::npos)
								{									
									common::printLog( print(lhs)+"<="+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);									
									result=1;
								}
							}
						}
						else if(index==print(rhs))
						{
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_GT)
							{
								result=CheckConditionExpr(lhs,"<",value);
								if(print(lhs).find("sizeof")!=std::string::npos)
								{									
									common::printLog( print(lhs)+">"+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);									
									result=1;
								}
							}
							else if(it->getOpcode()==clang::BinaryOperatorKind::BO_GE)
							{
								result=CheckConditionExpr(lhs,"<",value);//	
								if(print(lhs).find("sizeof")!=std::string::npos)
								{									
									common::printLog( print(lhs)+">="+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);									
									result=1;
								}
							}
						}
					}
					else
					{
						common::printLog( int2string(flag)+"path\n",common::CheckerName::arrayBound,0,*configure);
						uint64_t value = con.rhs;
						if(index==print(lhs))
						{
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_GE)
							{
								result=CheckConditionExpr(rhs,"<",value);
								if(print(rhs).find("sizeof")!=std::string::npos)
								{									
									common::printLog( print(lhs)+"<"+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);									
									result=1;
								}
							}
							else if(it->getOpcode()==clang::BinaryOperatorKind::BO_GT)
							{
								result=CheckConditionExpr(rhs,"<",value);//
								if(print(rhs).find("sizeof")!=std::string::npos)
								{									
									common::printLog( print(lhs)+"<="+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);									
									result=1;
								}
							}
						}
						else if(index==print(rhs))
						{
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_LE)
							{
								result=CheckConditionExpr(lhs,"<",value);
								if(print(lhs).find("sizeof")!=std::string::npos)
								{									
									common::printLog( print(lhs)+">"+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);									
									result=1;
								}
							}
							else if(it->getOpcode()==clang::BinaryOperatorKind::BO_LT)
							{
								result=CheckConditionExpr(lhs,"<",value);//
								if(print(lhs).find("sizeof")!=std::string::npos)
								{									
									common::printLog( print(lhs)+">="+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);									
									result=1;
								}
							}
						}
					}
				}
				else if(con.op==clang::BinaryOperatorKind::BO_GE)
				{
					string index=print(con.lhs->IgnoreCasts()->IgnoreParens());
					
					if(flag)
					{
						//it->dump();
						uint64_t value = con.rhs;
						if(index==print(lhs))
						{
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_GE)
							{
								
								result=CheckConditionExpr(rhs,">=",value);
							}
							else if(it->getOpcode()==clang::BinaryOperatorKind::BO_GT)
							{
								result=CheckConditionExpr(rhs,">=",value);
							}
							////////cerr<<result<<endl;
						}
						else if(index==print(rhs))
						{
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_LT)
							{
								result=CheckConditionExpr(rhs,">=",value);
							}
							else if(it->getOpcode()==clang::BinaryOperatorKind::BO_LE)
							{
								result=CheckConditionExpr(rhs,">=",value);
							}
						}
					}
					else
					{
						uint64_t value = con.rhs;
						if(index==print(lhs))
						{
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_LT)
							{
								result=CheckConditionExpr(rhs,">=",value);
							}
							else if(it->getOpcode()==clang::BinaryOperatorKind::BO_LE)
							{
								result=CheckConditionExpr(rhs,">=",value);
							}
						}
						else if(index==print(rhs))
						{
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_GE)
							{
								result=CheckConditionExpr(rhs,">=",value);
							}
							else if(it->getOpcode()==clang::BinaryOperatorKind::BO_GT)
							{
								result=CheckConditionExpr(rhs,">=",value);
							}
						}
					}
				}
				if(!result )
				{
					//////cerr<<"Composed expr"<<endl;
					vector<PNode> tmp;
					common::printLog( "result false, call analyseIndex:\n",common::CheckerName::arrayBound,0,*configure);
					analyseIndex(input.index,tmp);
					//////cerr<<"analyse index"<<endl;
					//////cerr<<print(input.index)<<endl;
					common::printLog( "after analyseIndex:"+print(input.index)+"\n",common::CheckerName::arrayBound,0,*configure);
					if(tmp.size()>1)
					{
						if(con.op==clang::BinaryOperatorKind::BO_LT)
						{
							
							result=checkComposedExpr(it,tmp,input,con,flag);
							
						}
						else if(con.op==clang::BinaryOperatorKind::BO_GE)
						{
							
							result=checkComposedExpr(it,tmp,input,con,flag);
							
						}

					}
				}
			}
		}
	}
	else if(UnaryOperator * it=dyn_cast<UnaryOperator>(stmt))
	{
		//cerr<<"UnaryOperator"<<endl;
		if(it->getOpcode()==clang::UnaryOperatorKind::UO_LNot)
		{
			//cerr<<"Not"<<endl;
			Expr* expr=it->getSubExpr()->IgnoreCasts()->IgnoreParens();
			return checkConditionStmt(astF, expr,input,con,!flag);
		}
	}
	return result;//*/
}


void ReturnStackAddrChecker::initialize()
{
	unfinished.clear();
}


////接口函数
bool ReturnStackAddrChecker::isTainted(FunctionDecl* Func,CFGBlock* block,Stmt* stmt,Expr* expr)
{
	return taint::TaintChecker::is_tainted(Func,block,stmt,expr);
}

vector<ArraySubscript> ReturnStackAddrChecker::LocatingTaintExpr(ASTFunction* astFunc,CFG * myCFG)
{
	//this function will locate the targeted array index, and get the array boundary information
	vector<ArraySubscript> result;
	mapToBlockOutLoopExpr.clear();
	mapToBlockInLoopExpr.clear();
	mapToLoopTaintedExpr.clear();
	mapToBlockTaintedLoopExpr.clear();
	mapToCheckBlock.clear();
	unfinished.clear();
	//AnalyeLoopExpr(func,myCFG);
	//get array information,return to result
	ifAnalyeLoopExpr=false;
	result = getArraySubscript(astFunc,myCFG);
	mapToBlockOutLoopExpr.clear();
	mapToBlockInLoopExpr.clear();
	mapToLoopTaintedExpr.clear();
	mapToBlockTaintedLoopExpr.clear();
	mapToCheckBlock.clear();
	unfinished.clear();
	return result;
}

void ReturnStackAddrChecker::AnalyeLoopExpr(FunctionDecl *func,CFG * cfg)
{
	common::printLog( "begin AnalyeLoopExpr:\n",common::CheckerName::arrayBound,3,*configure);
	//init init value and block to visit
	for(CFG::iterator iter=cfg->begin();iter!=cfg->end();++iter){
		CFGBlock* block=*iter;
		vector<Expr*> vardecl;
		mapToBlockInLoopExpr.insert(pair<CFGBlock*, vector<Expr*>>(block,vardecl));
		vector<pair<CFGBlock*,vector<Expr*>>> BlockVar;
		
		for(CFGBlock::succ_iterator succ_iter=block->succ_begin();succ_iter!=block->succ_end();++succ_iter){

			CFGBlock* succ=*succ_iter;
			if(succ==NULL)continue;
			BlockVar.push_back(pair<CFGBlock*, vector<Expr*>>(succ,vardecl));
		}

		mapToBlockOutLoopExpr.insert(pair<CFGBlock*, vector<pair<CFGBlock*,vector<Expr*>>>>(block,BlockVar));
		
		vector<Expr*> taintedExpr;;
		Stmt* it=(block->getTerminator()).getStmt();

		if(it!=NULL)
		{
			Expr* cond=NULL;
			if(it->getStmtClass()==Stmt::ForStmtClass)
			{
				cond=((ForStmt*)it)->getCond();
				taintedExpr=checkTaintExpr(cond,block,func,false);
				unfinished.push_back(block);
			}
			else if(it->getStmtClass()==Stmt::WhileStmtClass)
			{
				cond=((WhileStmt*)it)->getCond();
				taintedExpr=checkTaintExpr(cond,block,func,false);
				unfinished.push_back(block);
			}
		}
		
		mapToBlockTaintedLoopExpr.insert(pair<CFGBlock*, vector<Expr*>>(block,taintedExpr));
	}
	common::printLog( "initial loop var map\n",common::CheckerName::arrayBound,1,*configure);	
	while(unfinished.size()!=0)
	{
		CFGBlock* block=unfinished[0];
		unfinished.erase(unfinished.begin());
		vector<Expr*> expr_in;
		vector<pair<CFGBlock*,vector<Expr*>>> expr_out;
		for(CFGBlock::pred_iterator piter=block->pred_begin();piter!=block->pred_end();piter++)
		{
			CFGBlock* pred=*piter;
			vector<pair<CFGBlock*,vector<Expr*>>> pred_out=mapToBlockOutLoopExpr[pred];
			vector<Expr*> temp_expr_in;
			for(unsigned i=0;i<pred_out.size();i++)
			{
				if(pred_out[i].first==block)
				{
					temp_expr_in=pred_out[i].second;
					break;
				}
			}
			if(piter==block->pred_begin())
			{
				expr_in=temp_expr_in;
			}
			else
			{
				expr_in=Union(expr_in,temp_expr_in);
			}
		}
		bool init = true;
		for(CFGBlock::succ_iterator succ_iter=block->succ_begin();succ_iter!=block->succ_end();succ_iter++)
		{
			CFGBlock* succ=*succ_iter;
			if(succ==NULL)continue;
			if(init)
			{
				init=false;
				expr_out.push_back(pair<CFGBlock*,vector<Expr*>>(succ,Union(expr_in,mapToBlockTaintedLoopExpr[block])));
			}
			else
			{
				expr_out.push_back(pair<CFGBlock*,vector<Expr*>>(succ,expr_in));
			}
		}
		
		vector<pair<CFGBlock*,vector<Expr*>>> pre_out=mapToBlockOutLoopExpr[block];
		if(!isEqual(pre_out,expr_out))
		{
			for(CFGBlock::succ_iterator succ_iter=block->succ_begin();succ_iter!=block->succ_end();++succ_iter){
				CFGBlock *succ=*succ_iter;
				if(succ==NULL)continue;
				if(find(unfinished.begin(),unfinished.end(),succ)==unfinished.end())
					unfinished.push_back(succ);	
			}
			map<CFGBlock*, vector<Expr*> >::iterator expr_in_iter;
			expr_in_iter=mapToBlockInLoopExpr.find(block);
			mapToBlockInLoopExpr.erase(expr_in_iter);
			mapToBlockInLoopExpr.insert(pair<CFGBlock*, vector<Expr*> >(block,expr_in));

			map<CFGBlock*, vector<pair<CFGBlock*,vector<Expr*> > > >::iterator expr_out_iter;
			expr_out_iter=mapToBlockOutLoopExpr.find(block);
			mapToBlockOutLoopExpr.erase(expr_out_iter);
			mapToBlockOutLoopExpr.insert(pair<CFGBlock*, vector<pair<CFGBlock*,vector<Expr*> > > >(block,expr_out));
		}

	}

	//llvm::errs() <<"AnalyeLoopExpr end\n";
	//print info
	/*LangOptions LO;
	LO.CPlusPlus=1; 
	cfg->dump(LO,true);
	for(CFG::iterator iter=cfg->begin();iter!=cfg->end();++iter){
		CFGBlock* block=*iter;
		////cerr<<"BLOCK:"<<block->getBlockID()<<endl;
		////cerr<<"Loop Expr"<<endl;
		 vector<Expr*> list=mapToBlockInLoopExpr[block];
		 for(Expr* e : list)
		 {
		 	////cerr<<"\tExpr:"<<print(e)<<endl;
		 	vector<Expr*> tlist=mapToLoopTaintedExpr[e];
		 	 for(Expr* te : tlist)
		 	{
		 		////cerr<<"\t\tTaintExpr:"<<print(te)<<endl;
		 		////cerr<<"\t\tCheckBlock:"<<mapToCheckBlock[te]->getBlockID()<<endl;
		 	}
		 	
		 }

	}*/
	ifAnalyeLoopExpr=true;
	common::printLog( "End AnalyeLoopExpr!\n",common::CheckerName::arrayBound,3,*configure);
}

bool ReturnStackAddrChecker::checkExprTaintedForLoop(FunctionDecl* Func,CFGBlock* block,Stmt* stmt,Expr* expr)
{
	if(expr==nullptr||stmt==nullptr||block==nullptr)return false;
	if(dyn_cast<IntegerLiteral>(expr))
	{
		return false;
	}
	CFGBlock* temp=block;
	
	while(temp!=nullptr)
	{
		for(CFGBlock::iterator iter=temp->begin();iter!=temp->end();iter++)
		{
			CFGElement element=*(iter);
			if(element.getKind()==CFGElement::Kind::Statement)
			{
				const Stmt* it=((CFGStmt*)&element)->getStmt();
				if(it==nullptr)continue;
				if(it==stmt)
				{
					bool t=  isTainted(Func,temp,stmt,expr);
					return t;
				}
			}
		}
		
		temp=*(temp->pred_begin());
		//clang::CFGTerminator cfgt=temp->getTerminator();
		//if(cfgt==NULL)break;
		
	}
	return false;
}

vector<Expr*> ReturnStackAddrChecker::checkTaintExpr(Expr* stmt,CFGBlock* block,FunctionDecl* func,bool flag)
{
	vector<Expr*> TaintedExpr;
	if(stmt==nullptr)
	{
		return TaintedExpr;
	}
	if(BinaryOperator * it=dyn_cast<BinaryOperator>(stmt))
	{
		switch(it->getOpcode())
		{
		case clang::BinaryOperatorKind::BO_LAnd:
			{
				vector<Expr*> temp_lhs=checkTaintExpr(it->getLHS()->IgnoreCasts()->IgnoreParens(),block,func,flag);
				vector<Expr*> temp_rhs=checkTaintExpr(it->getRHS()->IgnoreCasts()->IgnoreParens(),block,func,flag);
				for(unsigned i=0;i<temp_lhs.size();i++)
				{
					if(find(TaintedExpr.begin(),TaintedExpr.end(),temp_lhs[i])==TaintedExpr.end())
					{
						TaintedExpr.push_back(temp_lhs[i]);
					}
				}
				for(unsigned i=0;i<temp_rhs.size();i++)
				{
					if(find(TaintedExpr.begin(),TaintedExpr.end(),temp_rhs[i])==TaintedExpr.end())
					{
						TaintedExpr.push_back(temp_rhs[i]);
					}
				}
				break;
			}
		case clang::BinaryOperatorKind::BO_LOr:
			{
				vector<Expr*> temp_lhs=checkTaintExpr(it->getLHS()->IgnoreCasts()->IgnoreParens(),block,func,flag);
				vector<Expr*> temp_rhs=checkTaintExpr(it->getRHS()->IgnoreCasts()->IgnoreParens(),block,func,flag);
				for(unsigned i=0;i<temp_lhs.size();i++)
				{
					if(find(TaintedExpr.begin(),TaintedExpr.end(),temp_lhs[i])==TaintedExpr.end())
					{
						TaintedExpr.push_back(temp_lhs[i]);
					}
				}
				for(unsigned i=0;i<temp_rhs.size();i++)
				{
					if(find(TaintedExpr.begin(),TaintedExpr.end(),temp_rhs[i])==TaintedExpr.end())
					{
						TaintedExpr.push_back(temp_rhs[i]);
					}
				}
				break;
			}
		default:
			{
				Expr* lhs=it->getLHS();
				Expr* rhs=it->getRHS();
				vector<Expr*> texpr;
				if(flag)
				{
					if(it->getOpcode()==clang::BinaryOperatorKind::BO_LE||it->getOpcode()==clang::BinaryOperatorKind::BO_LT)
					{
						if(checkExprTaintedForLoop(func,block,stmt,lhs))
						{
							if(dyn_cast<IntegerLiteral>(rhs))
							{
							}
							else
							{
								TaintedExpr.push_back(rhs);
								texpr.push_back(lhs);
								mapToLoopTaintedExpr.insert(pair<Expr*,vector<Expr*>>(rhs,texpr));
								mapToCheckBlock.insert(pair<Expr*,CFGBlock*>(lhs,block));
							}
						}
					}
					else if(it->getOpcode()==clang::BinaryOperatorKind::BO_GE||it->getOpcode()==clang::BinaryOperatorKind::BO_GT)
					{
						if(checkExprTaintedForLoop(func,block,stmt,rhs))
						{
							if(dyn_cast<IntegerLiteral>(lhs))
							{
							}
							else
							{
								TaintedExpr.push_back(lhs);
								texpr.push_back(rhs);
								mapToLoopTaintedExpr.insert(pair<Expr*,vector<Expr*>>(lhs,texpr));
								mapToCheckBlock.insert(pair<Expr*,CFGBlock*>(rhs,block));
								
							}
						}
					}
				}
				else
				{
					if(it->getOpcode()==clang::BinaryOperatorKind::BO_LE||it->getOpcode()==clang::BinaryOperatorKind::BO_LT)
					{
						if(checkExprTaintedForLoop(func,block,stmt,rhs))
						{
							if(dyn_cast<IntegerLiteral>(lhs))
							{
							}
							else
							{
								TaintedExpr.push_back(lhs);
								texpr.push_back(rhs);
								mapToLoopTaintedExpr.insert(pair<Expr*,vector<Expr*>>(lhs,texpr));
								mapToCheckBlock.insert(pair<Expr*,CFGBlock*>(rhs,block));
								
							}
						}
					}
					else if(it->getOpcode()==clang::BinaryOperatorKind::BO_GE||it->getOpcode()==clang::BinaryOperatorKind::BO_GT)
					{
						if(checkExprTaintedForLoop(func,block,stmt,lhs))
						{
							if(dyn_cast<IntegerLiteral>(rhs))
							{
							}
							else
							{
								TaintedExpr.push_back(rhs);
								texpr.push_back(lhs);
								mapToLoopTaintedExpr.insert(pair<Expr*,vector<Expr*>>(rhs,texpr));
								mapToCheckBlock.insert(pair<Expr*,CFGBlock*>(lhs,block));
							}
						}
					}
				}
				break;
			}
		}
	}
	else if(UnaryOperator * it=dyn_cast<UnaryOperator>(stmt))
	{
		if(it->getOpcode()==clang::UnaryOperatorKind::UO_LNot)
		{
			Expr* expr=it->getSubExpr()->IgnoreCasts()->IgnoreParens();
			return checkTaintExpr(expr,block,func,!flag);
		}
	}
	return TaintedExpr;
}
std::string ReturnStackAddrChecker::replace_all(string str, const string old_value, const string new_value)
{     
    while(true){
		string::size_type pos(0);     
        if((pos=str.find(old_value))!=string::npos)     
            str.replace(pos,old_value.length(),new_value);     
        else 
			break;     
    }     
    return str;
}
//low 0, up 1, -1 check flag in as
void ReturnStackAddrChecker::reportWarning(ArraySubscript as, int upOrLowBound){
   // for (auto s: as.callStack) cout<<"reportWarning "<<s<<endl;
    uncheckedArrays[as.ID].insert(std::make_pair(upOrLowBound,as));
}

void ReturnStackAddrChecker::reportWarning(){
    for (auto id2lowupAs: uncheckedArrays) {
        ArraySubscript as(translator->zc,nullptr);
        bool firstloop = true;
        for (auto ass : id2lowupAs.second) {
            if (firstloop) {
                as = ass.second;
                firstloop = false;
            }
            else {
                as = merge(as, ass.second, SolvePathConstraints);
            }
        }
        reportWarning(as);
    }
}
void ReturnStackAddrChecker::reportWarning(ArraySubscript as){	
	string loc=as.location;
	string originalLoc=as.originalLocation;
	common::printLog( "reportWarning,asID:"+int2string(as.ID)+"\n",common::CheckerName::arrayBound,0,*configure);
	//if is not loopboundchecking, if as.ID not found in reportedID, we can report it, else found and reported, return
	if(!as.isLoopBoundChecking){
		if(find(reportedID.begin(),reportedID.end(),as.ID)==reportedID.end())
		{
			common::printLog( "Not find in reportedID,asID:"+int2string(as.ID)+"\n",common::CheckerName::arrayBound,0,*configure);
			common::printLog( "as originalLocation:"+originalLoc+"\n",common::CheckerName::arrayBound,0,*configure);
			reportedID.push_back(as.ID);
		}
		else
			return;
	}
	else{
		

	}
	warningCount++;
	
	string func=as.func->getNameAsString();

	//string originalfile,originalline;
	//string originalLoc0=originalLoc;
	//if(originalLoc.find("Spelling")!=std::string::npos){
	//	int sp=originalLoc.find("Spelling");
	//	common::printLog( "Spelling Position"+int2string(sp)+"\n",common::CheckerName::arrayBound,0,*configure);
	//	common::printLog( "Found Spelling\n",common::CheckerName::arrayBound,0,*configure);
	//	originalLoc0.assign(originalLoc.c_str(),sp-2);
	//	common::printLog( "Remove Spelling"+originalLoc0+"\n",common::CheckerName::arrayBound,0,*configure);
	//}
	//int opos = originalLoc0.find_last_of(':');
	//string ostr;
	//ostr.assign(originalLoc0.c_str(),opos);
	////.....test.c:20
	//int opos2=ostr.find_last_of(':');				
	//originalfile.assign(ostr.c_str(),opos2);
	//originalline = ostr.substr(opos2+1);	

	string file,desc="",line,idxCnt;
	string loc0=loc;
	if(loc.find("Spelling")!=std::string::npos){
		int sp=loc.find("Spelling");
		loc0.assign(loc.c_str(),sp-2);
	}
	reportWarnings+=loc0;
	reportWarnings+="\n";
	int pos = loc0.find_last_of(':');
	string str;
	str.assign(loc0.c_str(),pos);
	//.....test.c:20
	int pos2=str.find_last_of(':');				
	file.assign(str.c_str(),pos2);
	line = str.substr(pos2+1);	
	idxCnt = int2string(as.indexCnt+1);
	//if(as.isLoopBoundChecking) desc+="\n\t\tArray's Original Location line "+originalline+"\n\t\t"+originalfile;
	if (SolvePathConstraints)
		desc+="\n\t\tIn expression: "+as.arrayExpr+",\n\t\tthe index "+print(as.arrayIdx) +" of array"+as.arrayName+"(array size is "+as.original_upBound_expr.to_string()+")\n\t\tneeds bound checking:\n";
    else
		desc+="\n\t\tIn expression: "+as.arrayExpr+",\n\t\tthe index "+print(as.arrayIdx) +" of array"+as.arrayName+"(array size is "+int2string(as.upBound)+")\n\t\tneeds bound checking:\n";

	if (as.isVectorArraySubscript) {
        if (!as.isUpBoundChecked) {
            desc+="\t\t"+as.upBoundCondition.to_string();
            desc+=";\n";

        }
        if (!as.isUnsignedChecked) {
            desc+="\t\t"+as.lowBoundCondition.to_string();
            desc+=";\n";
        }       
    }
    else {
        for(auto con:as.condition){
            desc+="\t\t"+getInfo(con);
            desc+=";\n";
        }	
    }
	////////cerr<<desc<<endl;
	string checkerId;
	if(as.isLoopBoundChecking && !as.isVectorArraySubscript){
		checkerId="ARRAY.INDEX.OUT.BOUND.(LOOP)";
		desc+="\t\tArray Index "+print(as.arrayIdx)+" is a loop variable, its corresponding loop bound is "+print(as.index) ;
		desc+="\n";
	}else{
		checkerId="ARRAY.INDEX.OUT.BOUND";
	}
	desc+="\t\t";
	
//	set<vector<string> > tmp_path=mapToPath[as.ID];
	//if(tmp_path.size()>0)
	{
		desc+="stack:\n\t\t";
        /*set<string> callset;
		for(vector<string> path : as.callStack)
		{
            std::string tmp="";
			for(int i=path.size()-1;i>=0;i--)
			{
			    tmp+=path[i];
				if(i!=0) tmp+="->";
			}
            callset.insert(tmp);
		}*/
        for (auto s:as.callStack) {
           // cout<<s<<endl;
            desc+=s;
			desc+="\n";
        }
	}
  /* 
    if (SolvePathConstraints) {
        desc+="\t\tPath Constraints:\n\t\t"+as.pathConstraint.to_string();
        desc+=";\n";
    }*/
 /*
	else
		desc+="in Current Function\n\t\t";
        */
	file=replace_all(file, "\\", "/");
	writingToXML(as.isLoopBoundChecking,file, func, desc, line, checkerId, idxCnt, as.arrayExpr);
}
void ReturnStackAddrChecker::reportWarningSameID(vector<ArraySubscript> as){
	//this function will report the warnings which have the same ID
	//actually, this function is designed for the case that for(i<m&&i<n), in a loop bound checking,
	//we save m<size and n<size, they have the same ID as i<size.
	//at the end, we just report them togethor.
	if(as.size()==0) return;
	string loc=as[0].location;
	string originalLoc=as[0].originalLocation;
	common::printLog( "reportWarningSameID,asID:"+int2string(as[0].ID)+"\n",common::CheckerName::arrayBound,0,*configure);
	//if is not loopboundchecking, if as.ID not found in reportedID, we can report it, else found and reported, return
	warningCount++;
	
	string func=as[0].func->getNameAsString();	

	string file,desc="",line,idxCnt;
	string loc0=loc;
	if(loc.find("Spelling")!=std::string::npos){
		int sp=loc.find("Spelling");
		loc0.assign(loc.c_str(),sp-2);
	}
	reportWarnings+=loc0;
	reportWarnings+="\n";
	int pos = loc0.find_last_of(':');
	string str;
	str.assign(loc0.c_str(),pos);
	//.....test.c:20
	int pos2=str.find_last_of(':');				
	file.assign(str.c_str(),pos2);
	line = str.substr(pos2+1);	
	idxCnt = int2string(as[0].indexCnt+1);
	//if(as.isLoopBoundChecking) desc+="\n\t\tArray's Original Location line "+originalline+"\n\t\t"+originalfile;
	desc+="\n\t\tArray expression: "+as[0].arrayExpr+"\n\t\tneeds bound checking:\n";
	for(auto aas:as){
		for(auto con:aas.condition){
			desc+="\t\t"+getInfo(con);
			desc+=";\n";
		}	
	}
	////////cerr<<desc<<endl;
	string checkerId;
	if(as[0].isLoopBoundChecking){
		checkerId="ARRAY.INDEX.OUT.BOUND.(LOOP)";	
		string loopbound="";
		for(auto aas:as){loopbound=loopbound+", "+print(aas.index);}
		desc+="\t\tArray Index "+print(as[0].arrayIdx)+" is a loop variable,\n\t\tits corresponding loop bound is "+loopbound;
		desc+="\n";
	}else{
		checkerId="ARRAY.INDEX.OUT.BOUND";
	}
	desc+="\t\t";
	
	set<vector<string> > tmp_path=mapToPath[as[0].ID];
	if(tmp_path.size()>0)
	{
		desc+="stack:\n\t\t";
		for(vector<string> path : tmp_path)
		{
			for(int i=path.size()-1;i>=0;i--)
			{
				desc+=path[i];
				if(i!=0)desc+="->";
			}
			desc+="\n\t\t";
		}
	}
	else
		desc+="in Current Function\n\t\t";
	file=replace_all(file, "\\", "/");
	writingToXML(as[0].isLoopBoundChecking,file, func, desc, line, checkerId, idxCnt, as[0].arrayExpr);
}

/*const vector<uint64_t> ReturnStackAddrChecker::getArraySize(ASTFunction* astF, Expr* ex){
    if(DeclRefExpr* declRef=dyn_cast<DeclRefExpr>(ex->IgnoreCasts())){
        return getArraySize(astF, declRef->getDecl());
    }
    else if(MemberExpr *mem = dyn_cast<MemberExpr>(ex->IgnoreCasts())){
        return getArraySize(astF, mem->getMemberDecl());
	}
	else{
        vector<uint64_t> tmp;
        common::printLog( "Cannot get array size of "+ print(ex)+"\n",common::CheckerName::arrayBound,5,*configure);
		return tmp;
	}
}

const vector<uint64_t> ReturnStackAddrChecker::getArraySize(ASTFunction* astF, ValueDecl* valueDecl){
    vector<uint64_t> tmp;
    FunctionDecl* f = manager->getFunctionDecl(astF);
    //f->dump(); 
    //array declaration, eg char a[10];
    QualType qt = valueDecl->getType ();					
    //deal with a[][][]	to get array size													
    while(const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(qt)){													
        qt=CAT->getElementType();
        const llvm::APInt & size = CAT->getSize();	
        uint64_t v = size.getLimitedValue();
        tmp.push_back(v);												
    }
    if (tmp.size()>0)
        return tmp; 
    if (clang::ParmVarDecl* parm = dyn_cast<ParmVarDecl>(valueDecl)){
        //array is pointer, and is parameter of func, get real array in caller
        //using CallInfo = std::vector<std::pair<ASTFunction *, int64_t>>;
        CallInfo parents=call_graph->getParentsWithCallsite(astF);
        unsigned paramIdx = 0;
        for (unsigned i=0;i<f->getNumParams();i++){
            ParmVarDecl* p = f->getParamDecl(i);
            if (p == parm) {
                paramIdx = i;
                break; 
            }
        }
        if ( parents.size() == 0) {
            //main function?
            //assert(0);    
            if (f->getName() == "main"){
            }
            return tmp;
        }
        else {
            for(auto father : parents) {
                FunctionDecl* fatherdecl = manager->getFunctionDecl(father.first);
                Stmt* callsite = common::getStmtInFunctionWithID(fatherdecl, father.second);
                if (CallExpr* call = dyn_cast<CallExpr>(callsite)){
                    //call->dump();
                    Expr* realArg = call->getArg(paramIdx);
                    return getArraySize(father.first, realArg);
                }

            }
        }//endelse 
    }

    if (clang::VarDecl* varDecl = dyn_cast<VarDecl>(valueDecl)){
        std::string vStr = PTA->getPTAVarKey(varDecl);
        //cout<<"vStr:"<<vStr<<endl;
        const PTAVar* vPTA = PTA->getPTAVar(vStr);
        std::set<const PTAVar *> aliasSet = PTA->get_alias_in_func(f,vPTA);
        for (auto aliasVar : aliasSet) {
            std::string aliasStr = aliasVar->get_instance_var_key();
            //cout<<"aliasStr:"<<aliasStr<<endl;
            ///home/gfj/ESAF/tests/Arraybound/InterCheck/example.cpp:5:28:t
            int firstColonInBegin = aliasStr.find(":");
            std::string fileName = aliasStr.substr(0,firstColonInBegin);
            //cout<<"fileName:"<<fileName<<endl;
            int secondColonInBegin = aliasStr.find(":", firstColonInBegin + 1);
            std::string locLinesInBegin = aliasStr.substr(
                    firstColonInBegin + 1, secondColonInBegin - firstColonInBegin - 1
                    );
            int line = std::stoi(locLinesInBegin);
            Stmt* aliasStmt = manager->getStmtWithLoc(fileName,line);
            if (aliasStmt) {
                //aliasStmt->dump();
                CFGBlock* aliasBlock = manager->getBlockWithLoc(fileName,line);
                //if (aliasBlock)
                   // aliasBlock->dump();
                std::vector<Stmt*> aliasStmts = manager->getStmtWithLoc(line,aliasBlock);
                for(auto as : aliasStmts) {
                    //if (as)
                      //  as->dump();
                } 
            }
            else {
                //parameter?
                std::string paraName = aliasStr.substr(aliasStr.find_last_of(":"));
                std::string paraLoc = aliasStr.substr(0,aliasStr.find_last_of(":"));
                //cout<<"paraName:"<<paraName<<", paraloc:"<<paraLoc<<endl;
                SourceManager *sm;
                sm = &(f->getASTContext().getSourceManager()); 
                std::string floc = f->getBeginLoc().printToString(*sm);	
                //cout<<"f loc:"<<floc<<endl;
                if (floc.substr(0,floc.find_last_of(":")) == fileName+":"+locLinesInBegin) {
                    unsigned paramIdx = 0;
                    for (unsigned i=0;i<f->getNumParams();i++){
                        ParmVarDecl* p = f->getParamDecl(i);
                        std::string ploc = p->getBeginLoc().printToString(*sm);
                        if (ploc == paraLoc) {
                            paramIdx = i;
                  //          p->dump();
                            return getArraySize(astF,p);    
                        }
                    }
                }
            }
        }
    }
    return tmp; 


}*/
VarDecl* ReturnStackAddrChecker::getVarDecl(Expr* ex){
	VarDecl* tmp=NULL;
	Expr *exprTmp=ex->IgnoreCasts()->IgnoreParens();
	/*while(ImplicitCastExpr * it = dyn_cast<ImplicitCastExpr>(exprTmp)){
		exprTmp=it->IgnoreImpCasts();
	}*/
	if(DeclRefExpr* declRef=dyn_cast<DeclRefExpr>(exprTmp)){
		if (VarDecl *VD = dyn_cast<VarDecl>(declRef->getDecl())){
			tmp=VD;
		}
	}
	return tmp;
}
bool ReturnStackAddrChecker::isUnsigned(Expr *ex){
	if(ConditionalOperator *co=dyn_cast<ConditionalOperator>(ex->IgnoreCasts()->IgnoreParens()))
	{
		Expr* lhs=co->getLHS()->IgnoreCasts()->IgnoreParens();
		//Expr* rhs=co->getRHS()->IgnoreCasts()->IgnoreParens();
		if(dyn_cast<IntegerLiteral>(lhs)) 
		{
			return isUnsigned(co->getRHS());
		}
		else if(dyn_cast<FloatingLiteral>(lhs))//-ImplicitCastExpr
		{
			return isUnsigned(co->getRHS());
		}
		else
		{
			return isUnsigned(lhs);
		}
	}
	
	Expr *exprTmp=ex->IgnoreCasts()->IgnoreParens();
	QualType qt;
	if(DeclRefExpr *declRef = dyn_cast<DeclRefExpr>(exprTmp)){
		ValueDecl *valueDecl = declRef->getDecl();
		qt = valueDecl->getType ();		
	}
	else if(MemberExpr *mem = dyn_cast<MemberExpr>(exprTmp)){
		ValueDecl *valueDecl=mem->getMemberDecl();
		qt = valueDecl->getType ();	
	}
	else if(ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(exprTmp)){
		qt = ASE->getType ();		
	}
	else return false;	
	if(const BuiltinType *bt = dyn_cast<BuiltinType>(qt)){
		if(bt->isSignedInteger()){/*//////cerr<<"SignedInteger"<<endl;*/return false;}
		if(bt->isUnsignedInteger()){/*//////cerr<<"UnsignedInteger"<<endl;*/return true;}
	}
	if(const TypedefType *tt = dyn_cast<TypedefType>(qt)){
		const QualType t=tt->desugar();
		const QualType c = t.getCanonicalType();
		if(const BuiltinType *bt = dyn_cast<BuiltinType>(c)){
			if(bt->isSignedInteger()){/*//////cerr<<"SignedInteger"<<endl;*/return false;}
			if(bt->isUnsignedInteger()){/*//////cerr<<"UnsignedInteger"<<endl;*/return true;}
		} 

	}
	return false;
}
int ReturnStackAddrChecker::writingToXML(bool isLoopBoundChecking, string fileName, string funName, string descr, string locLine,string checkerId, string indexCnt, string arrayExpr)
{
    common::printLog( "Array out of bounds ",common::CheckerName::arrayBound,6,*configure);

    pugi::xml_node node;
	if(isLoopBoundChecking)
	{
		string report=fileName+funName+descr+locLine+checkerId+indexCnt+arrayExpr;
		if(find(loopReport.begin(),loopReport.end(),report)==loopReport.end()){
			common::printLog( "Not find in loopReport\n",common::CheckerName::arrayBound,0,*configure);
			loopReport.push_back(report);
		}
		else{
			common::printLog( "find in loopReport,return\n",common::CheckerName::arrayBound,0,*configure);
			return 0;
		}
		node= doc_loopbound.append_child("error");
		doc_loopbound_empty = false;
	}
	else{
		node= doc_arraybound.append_child("error");
		doc_arraybound_empty = false;
	}

    pugi::xml_node checker = node.append_child("checker");
	checker.append_child(pugi::node_pcdata).set_value(checkerId.c_str());

	pugi::xml_node domain = node.append_child("domain");
	domain.append_child(pugi::node_pcdata).set_value("STATIC_C");

	pugi::xml_node file = node.append_child("file");
	file.append_child(pugi::node_pcdata).set_value(fileName.c_str());

	pugi::xml_node function = node.append_child("function");
    function.append_child(pugi::node_pcdata).set_value(funName.c_str());

//	pugi::xml_node score = node.append_child("score");
  //  score.append_child(pugi::node_pcdata).set_value("100");

//	pugi::xml_node ordered = node.append_child("ordered");
  //  ordered.append_child(pugi::node_pcdata).set_value("false");

	pugi::xml_node event = node.append_child("event");

	pugi::xml_node main = event.append_child("main");
    main.append_child(pugi::node_pcdata).set_value("true");

	pugi::xml_node tag = event.append_child("tag");
    tag.append_child(pugi::node_pcdata).set_value("Error");

	pugi::xml_node description = event.append_child("description");
    description.append_child(pugi::node_pcdata).set_value(descr.c_str());

	pugi::xml_node line = event.append_child("line");
    line.append_child(pugi::node_pcdata).set_value(locLine.c_str());

	//pugi::xml_node arrayExpression = event.append_child("arrayExpression");
	//arrayExpression.append_child(pugi::node_pcdata).set_value(arrayExpr.c_str());


	//pugi::xml_node arrayIndex = event.append_child("arrayIndex");
    //arrayIndex.append_child(pugi::node_pcdata).set_value(indexCnt.c_str());

	pugi::xml_node extra = node.append_child("extra");
    extra.append_child(pugi::node_pcdata).set_value("none");

	pugi::xml_node subcategory = node.append_child("subcategory");
    subcategory.append_child(pugi::node_pcdata).set_value("none");

	return 0;
}
std::unordered_map<Expr*, vector<Expr*>> ReturnStackAddrChecker::getIdx(Expr *ex){
	std::unordered_map<Expr*, vector<Expr*>> tmp;	
	vector<Expr*> tmpIdx;
	Expr *root=ex;
	/*NOTE:
	m[i][j][k],getBase will get m[i][j];getIdx will get k,
	we get index:k,j,i in loop, 
	when ASE's left children is DeclRefExpr, we know there's not ArraySubscriptExpr in its children,so we set root=NULL, end the loop;
	In the last loop,getBase will get m;
	* */
	//root->dump();
	while(root!=NULL){
		//root->dump();
		if(ArraySubscriptExpr *ASE = dyn_cast<ArraySubscriptExpr>(root->IgnoreCasts()->IgnoreParens())){
			//ASE->dump();
			////////cerr<<"hi getIdx"<<endl;
			Expr *idx = ASE->getIdx()->IgnoreCasts()->IgnoreParens();	
			//idx->dump();		
			tmpIdx.push_back(idx);	
			root=ASE->getBase()->IgnoreCasts()->IgnoreParens();			
		}else{
			reverse(tmpIdx.begin(),tmpIdx.end());
			tmp.insert(std::make_pair(root,tmpIdx));
			root=NULL;
		}
	}
	return tmp;
}
vector<ArraySubscript> ReturnStackAddrChecker::getArraySubscriptInExpr(Expr* ex,ASTFunction* astF,CFGBlock* block,Stmt* stmt,CFG * myCFG){
	//this function can get array subscript information of a statement.	
    FunctionDecl* f = manager->getFunctionDecl(astF);	
	std::string funcName =  f->getNameInfo().getName().getAsString();
    vector<ArraySubscript> tmpAS;
    common::printLog( "Array located:"+print(ex)+" in "+funcName+", ",common::CheckerName::arrayBound,3,*configure);
	std::unordered_map<Expr*, vector<Expr*>> base_Idx = getIdx(ex);
	if(base_Idx.size()>0){
		for(auto content:base_Idx){
			Expr* base = content.first;
			const vector<uint64_t> arrSize = getArraySize(astF, base,manager,call_graph,PTA,configure);
			//if use extern,arrSize will benull.			
			common::printLog( "array size number"+int2string(arrSize.size())+" :"+print(arrSize)+"\n",common::CheckerName::arrayBound,1,*configure);
			common::printLog( "index number"+int2string(content.second.size())+"\n",common::CheckerName::arrayBound,1,*configure);
			//for(auto c:content.second) cerr<<print(c)<<endl;
			if(arrSize.size()!=content.second.size()){
				common::printLog( "array size number not equal index number!\n",common::CheckerName::arrayBound,1,*configure);
				return tmpAS;
			}
			if(arrSize.size()>0){
				int i=0;
				if(content.second.size()>0){
					for(auto idx:content.second){
						ArraySubscript as(translator->zc, stmt);
						if(dyn_cast<IntegerLiteral>(idx)) {
							i++;
							continue;
						}
						if(SimpleExprCheckOnly){
							if(dyn_cast<BinaryOperator>(idx)){
								common::printLog( "SimpleExprCheckOnly,BinaryOperator:"+print(idx)+",continue\n",common::CheckerName::arrayBound,0,*configure);
								i++;	
								continue;
							}
							else if(dyn_cast<UnaryOperator>(idx)){
								common::printLog( "SimpleExprCheckOnly,UnaryOperator:"+print(idx)+",continue\n",common::CheckerName::arrayBound,0,*configure);
								i++;	
								continue;
							}
							else if(dyn_cast<CallExpr>(idx)){
								common::printLog( "SimpleExprCheckOnly,callexpr:"+print(idx)+",continue\n",common::CheckerName::arrayBound,0,*configure);
								i++;	
								continue;
							}
							else if(dyn_cast<ConditionalOperator>(idx)){
								common::printLog( "SimpleExprCheckOnly,ConditionalOperator:"+print(idx)+",continue\n",common::CheckerName::arrayBound,0,*configure);
								i++;	
								continue;
							}
						}
						if(IndexIgnoreConditionalOperator == true){
							//config.txt IndexIgnoreConditionalOperator is true, if index is a conditionaloperator, we will ignore it
							if(dyn_cast<ConditionalOperator>(idx)){
								i++;
								continue;
							}
						}
						if(!CheckTaintArrayOnly || isTainted(f,block,stmt,idx))
						{
							vector<AtomicAPInt> tmpAA;
							common::printLog( "index is tainted or checkTaintArrayOnly is false, need check "+print(idx)+" ",common::CheckerName::arrayBound,3,*configure);							
							as.upBound = arrSize[i];
//                            as.upBoundExpr
                            uint64_t apInt=arrSize[i];	
							AtomicAPInt tempoperator;
							tempoperator.op=clang::BinaryOperatorKind(BO_LT);
							tempoperator.lhs=idx;
							tempoperator.rhs=apInt;							
							tmpAA.push_back(tempoperator);
                            vector<z3::expr> idxv = translator->zc->clangExprToZ3Expr(idx,astF);
                            if (idxv.empty()) continue;
                            as.upBound_expr = translator->zc->Int2Z3Expr(apInt);
							as.original_upBound_expr = as.upBound_expr;
							as.upBoundCondition = translator->zc->clangOp2Z3Op(idxv.back(), as.upBound_expr,BinaryOperatorKind::BO_LT);
                            common::printLog( "< "+int2string(apInt)+", ",common::CheckerName::arrayBound,3,*configure);
							//we need to determine whether there needs a checking like idx>=0. If idx is unsigned, then no; If idx is signed, then yes.
							if(!isUnsigned(idx)){
								common::printLog( "index is not unsigned, need check "+print(idx)+">=0, ",common::CheckerName::arrayBound,1,*configure);
								AtomicAPInt tempoperator;
								tempoperator.op=clang::BinaryOperatorKind(BO_GE);
								tempoperator.lhs=idx;
								tempoperator.rhs=0;
								tmpAA.push_back(tempoperator);
                                as.isUnsigned = false;
                                as.isUnsignedChecked = 0;
                                as.lowBoundCondition = translator->zc->clangOp2Z3Op(idxv.back(),translator->zc->Int2Z3Expr(0),BinaryOperatorKind::BO_GE);
							}
							//delete idxv; idxv=nullptr;
                            SourceManager *sm;
							sm = &(f->getASTContext().getSourceManager()); 
							string loc = idx->getBeginLoc().printToString(*sm);	
							common::printLog( "at "+loc+"\n",common::CheckerName::arrayBound,3,*configure);							
							as.func=f;
							as.block=block;
							as.stmt=stmt;
							as.index=idx;
							as.location=loc;
							as.originalLocation=loc;
							as.condition=tmpAA;
							as.isLoopBoundChecking=false;							
							as.indexCnt=i;
							as.arrayName=print(base);
							as.arrayExpr=print(ex);
							as.arrayIdx=idx;
						    as.orignIndex = idx;

                            translator->handleIndex(as, astF);
                            if (as.condition.empty()) {
                                //erase
                                return tmpAS;
                            }
                            /*else if (flag == -1) {
                                reportWarning(as);
                                return tmpAS;
                            }*/
                            else {
                                as.updateCallStack(funcName);
                                tmpAS.push_back(as);
                            }
                            if(ConditionalOperator *co=dyn_cast<ConditionalOperator>(idx->IgnoreCasts()->IgnoreParens()))
                            {
								Expr* cc=co->getCond();
								Expr* rhs=NULL;
								for(unsigned j=0;j<as.condition.size();j++)
								{
									bool flag=false;
									Expr* Indx=as.index;
									as.changeIndex(co->getLHS()->IgnoreCasts()->IgnoreParens());
									if(checkConditionStmt(astF, cc,as,as.condition[j],true))
									{
										rhs=co->getRHS()->IgnoreCasts()->IgnoreParens();
										flag=true;
									}
									else {
										as.changeIndex(co->getRHS()->IgnoreCasts()->IgnoreParens());
										if(checkConditionStmt(astF, cc,as,as.condition[j],false))
										{
											rhs=co->getLHS()->IgnoreCasts()->IgnoreParens();
											flag=true;
										}
									}
									as.changeIndex(Indx);
									if(rhs!=NULL)
									if(flag)
									{
										if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(rhs)) 
										{
											
											uint64_t value=ILE->getValue().getLimitedValue();
											bool ff=true;
											
											if(as.condition[j].op==clang::BinaryOperatorKind::BO_LT)
											{
												if(value>as.condition[j].rhs)ff=false;
											}
											
											
											if(ff)
											{
												as.condition.erase(as.condition.begin()+j);
                                                j--;
												//return 1;
											}
											
										}
										else if(FloatingLiteral *ILE = dyn_cast<FloatingLiteral>(rhs))//-ImplicitCastExpr
										{
											double value=ILE->getValue().convertToDouble();
											//////////cerr<<value<<endl;
											bool ff=true;
											if(as.condition[j].op==clang::BinaryOperatorKind::BO_LT)
											{
												if(value>as.condition[j].rhs)ff=false;
											}
											
											
											if(ff)
											{
												as.condition.erase(as.condition.begin()+j);
                                                j--;
												//return 1;
											}
										}
										else if(BinaryOperator *bb=dyn_cast<BinaryOperator>(rhs))
										{
											if(bb->getOpcode()==clang::BinaryOperatorKind::BO_Rem)
											{
												//////cerr<<"Rem after assign"<<endl;
												Expr* rr=bb->getRHS()->IgnoreCasts()->IgnoreParens();
												if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(rr)) 
												{
													
													uint64_t value=ILE->getValue().getLimitedValue();
													
													if(as.condition[j].op==clang::BinaryOperatorKind::BO_LT)
													{
														if(value<=as.condition[j].rhs)
														{
															
															as.condition.erase(as.condition.begin()+j);
                                                            j--;
														}
													}
													
													
												}
												//
											}
										}
									}
								}
								
								if(as.condition.size()==0)
									i++;
									continue;
							}
                            //as.updateCallStack(funcName);
							//tmpAS.push_back(as);
						}
						else{	
							common::printLog( "index " +print(idx)+" is not tainted, ",common::CheckerName::arrayBound,1,*configure);
							if(ifAnalyeLoopExpr==false) {
								common::printLog( "ifAnalyeLoopExpr false,call AnalyeLoopExpr\n",common::CheckerName::arrayBound,0,*configure);
								AnalyeLoopExpr(f,myCFG);
							}
							vector<AtomicAPInt> tmpBounds;							
							common::printLog( "mapToLoopTaintedExpr size:"+int2string(mapToLoopTaintedExpr.size())+"\n",common::CheckerName::arrayBound,0,*configure);
							vector<Expr*> loopBounds;
							std::vector<Expr*> vars=mapToBlockInLoopExpr[block];
							for(auto ex : vars){
								if(print(ex->IgnoreCasts()->IgnoreParens())==print(idx))
								{
									loopBounds=mapToLoopTaintedExpr[ex];
									break;
								}
								// VarDecl *tmp=getVarDecl(ex.first);
								// //tmp->dump();
								// if(tmp==NULL) continue;
								// if(idxdecl==tmp){
								// 	common::printLog( "idxdecl==tmp,idx is a loop variable\n",common::CheckerName::arrayBound,1,*configure);
								// 	loopBounds=ex.second;
								// }
							}
							if(loopBounds.size()>0){
								//for(auto loopvar:loopBounds){
								auto loopvar=loopBounds[0];
								
								{
									//now that loop variable is not tainted, 
									//we find the index is loop variable,
									//we also push the index into our array boundary information
									//and perform index<size in the loop
									//and perform loopbound<size out the loop
									uint64_t apInt=arrSize[i];										
									AtomicAPInt tempoperator;
									tempoperator.op=clang::BinaryOperatorKind(BO_LT);
									tempoperator.lhs=idx;
									tempoperator.rhs=apInt;
									
									tmpBounds.push_back(tempoperator);
									// if(!isUnsigned((Expr*)loopvar)){
									// 	////////cerr<<"loopbound is not unsigned"<<endl;
									// 	AtomicAPInt tempoperator;
									// 	tempoperator.op=clang::BinaryOperatorKind(BO_GE);
									// 	tempoperator.lhs=(Expr*)loopvar;
									// 	tempoperator.rhs=0;
									// 	tmpBounds.push_back(tempoperator);
									// }
									map<Expr*,CFGBlock*>::iterator itrblock;
									itrblock=mapToCheckBlock.find(loopvar);									
									if(itrblock!=mapToCheckBlock.end()){
										as.loopblock=itrblock->second;
									}
									else {
										common::printLog( "loopvar,mapToCheckBlock not find loopvar!\n",common::CheckerName::arrayBound,0,*configure);
										continue;
									}
																		
									SourceManager *sm;
									sm = &(f->getASTContext().getSourceManager()); 
									string loc = idx->getBeginLoc().printToString(*sm);	
									common::printLog( "get index location:"+loc+"\n",common::CheckerName::arrayBound,1,*configure);							
									as.func=f;
									as.block=block;
									as.stmt=stmt;
									as.index=idx;
									as.location=loc;
									as.originalLocation=loc;
									as.condition=tmpBounds;
									as.isLoopBoundChecking=false;
									as.indexCnt=i;
									as.arrayName=print(base);
									as.arrayExpr=print(ex);
									as.arrayIdx=idx;

									//loopvar->dump();
									//////cerr<<"**********************"<<endl;											
									//uint64_t apInt=arrSize[i];										
									//AtomicAPInt tempoperator;
									//tempoperator.op=clang::BinaryOperatorKind(BO_LT);
									//tempoperator.lhs=(Expr*)loopvar;
									//tempoperator.rhs=apInt;
									//
									//tmpBounds.push_back(tempoperator);
									//// if(!isUnsigned((Expr*)loopvar)){
									//// 	////////cerr<<"loopbound is not unsigned"<<endl;
									//// 	AtomicAPInt tempoperator;
									//// 	tempoperator.op=clang::BinaryOperatorKind(BO_GE);
									//// 	tempoperator.lhs=(Expr*)loopvar;
									//// 	tempoperator.rhs=0;
									//// 	tmpBounds.push_back(tempoperator);
									//// }
									//map<Expr*,CFGBlock*>::iterator itrblock;
									//itrblock=mapToCheckBlock.find(loopvar);									
									//if(itrblock!=mapToCheckBlock.end()){
									//	as.block=itrblock->second;
									//}
									//else {
									//	common::printLog( "loopvar,mapToCheckBlock not find loopvar!\n",common::CheckerName::arrayBound,1,*configure);
									//	continue;
									//}
									//SourceManager *sm;
									//sm = &(f->getASTContext().getSourceManager()); 
									//string loc = idx->getLocStart().printToString(*sm);
									//as.func=f;
									//as.stmt=as.block->getTerminatorCondition();
									//common::printLog( "loopvar,as.stmt=as.block->getTerminatorCondition() is:"+print(as.stmt)+"\n",common::CheckerName::arrayBound,1,*configure);
									//as.index=loopvar;
									//as.location=loc;
									//as.condition=tmpBounds;
									//as.isLoopBoundChecking=true;
									//as.indexCnt=i;
									//as.arrayExpr=print(ex);
									//as.arrayIdx=idx;
									//cerr<<"Loop Var"<<endl;
									//Expr* tmp=loopvar->IgnoreCasts()->IgnoreParens();
									//tmp->dump();
									//if(ConditionalOperator *co=dyn_cast<ConditionalOperator>(loopvar->IgnoreCasts()->IgnoreParens()))
									//{
									//	 
									//	// //////cerr<<print(rhs)<<endl;
									//	Expr* cc=co->getCond();
									//	
									//	Expr* rhs=NULL;
									//	//cerr<<"ConditionalOperator"<<endl;
									//	for(unsigned j=0;j<as.condition.size();j++)
									//	{
									//		bool flag=false;
									//		Expr* Indx=as.index;
									//		as.changeIndex(co->getLHS()->IgnoreCasts()->IgnoreParens());
									//		if(checkConditionStmt(cc,as,as.condition[j],true))
									//		{
									//			//cerr<<"1"<<endl;
									//			rhs=co->getRHS()->IgnoreCasts()->IgnoreParens();
									//			flag=true;
									//		}
									//		else {
									//			as.changeIndex(co->getRHS()->IgnoreCasts()->IgnoreParens());
									//			if(checkConditionStmt(cc,as,as.condition[j],false))
									//			{
									//				//cerr<<"2"<<endl;
									//				rhs=co->getLHS()->IgnoreCasts()->IgnoreParens();
									//				flag=true;
									//			}
									//		}
									//		as.changeIndex(Indx);
									//		if(rhs!=NULL)
									//		//cerr<<print(rhs)<<endl;
									//		if(flag)
									//		{
									//			if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(rhs)) 
									//			{
									//				
									//				uint64_t value=ILE->getValue().getLimitedValue();
									//				//////////cerr<<value<<endl;
									//				bool ff=true;
									//				
									//				if(as.condition[j].op==clang::BinaryOperatorKind::BO_LT)
									//				{
									//					if(value>as.condition[j].rhs)ff=false;
									//				}
									//				
									//				
									//				if(ff)
									//				{
									//					as.condition.erase(as.condition.begin()+j);
									//					//return 1;
									//				}
									//				
									//			}
									//			else if(FloatingLiteral *ILE = dyn_cast<FloatingLiteral>(rhs))//-ImplicitCastExpr
									//			{
									//				double value=ILE->getValue().convertToDouble();
									//				//////////cerr<<value<<endl;
									//				bool ff=true;
									//				if(as.condition[j].op==clang::BinaryOperatorKind::BO_LT)
									//				{
									//					if(value>as.condition[j].rhs)ff=false;
									//				}
									//				
									//				
									//				if(ff)
									//				{
									//					as.condition.erase(as.condition.begin()+j);
									//					//return 1;
									//				}
									//			}
									//			else if(BinaryOperator *bb=dyn_cast<BinaryOperator>(rhs))
									//			{
									//				if(bb->getOpcode()==clang::BinaryOperatorKind::BO_Rem)
									//				{
									//					//////cerr<<"Rem after assign"<<endl;
									//					Expr* rr=bb->getRHS()->IgnoreImpCasts();
									//					if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(rr)) 
									//					{
									//						
									//						uint64_t value=ILE->getValue().getLimitedValue();
									//						
									//						if(as.condition[j].op==clang::BinaryOperatorKind::BO_LT)
									//						{
									//							if(value<=as.condition[j].rhs)
									//							{
									//								
									//								as.condition.erase(as.condition.begin()+j);
									//							}
									//						}
									//						
									//						
									//					}
									//					//
									//				}
									//			}
									//		}
									//	}
									//	
									//	if(as.condition.size()==0)
									//		continue;
									//}
									//cerr<<"generate Script:"<<print(as.index)<<endl;
                                    as.updateCallStack(funcName);
									tmpAS.push_back(as);
									
								}
							}	
							else{
								common::printLog( "there's no loopvar,loopbound, index is not loopvar",common::CheckerName::arrayBound,1,*configure);
							}
						}
						
						i++;
					}
				}
			}
		}
	}
	common::printLog( "getArraySubscriptInExpr end!\n",common::CheckerName::arrayBound,1,*configure);
	return tmpAS;
}

vector<ArraySubscript> ReturnStackAddrChecker::getArraySubscript(ASTFunction* astF,CFG * myCFG){
	//this function will locate the targeted array index, and get the array boundary information
	common::printLog( "get array information begin:\n",common::CheckerName::arrayBound,0,*configure);
	vector<ArraySubscript> tmpAS;
	if(myCFG == nullptr)
		return tmpAS;
    for (CFGBlock *block : *myCFG) {
        for (auto& element : *block) {
            if (element.getKind() == CFGStmt::Statement) {
                Stmt *stmt = const_cast<Stmt*>(element.castAs<CFGStmt>().getStmt());
                FindArraySubscriptExpr visitor;
                if(stmt!=nullptr){
                    visitor.TraverseStmt(stmt);
                    const auto &arrayExprs = visitor.getArrayExprs();
                    if(!arrayExprs.empty()){
                        for(auto& arrayExpr:arrayExprs){  
                            //arrayPositions.push_back(ExprPosition(fd,block,stmt,arrayExpr));  
                            common::printLog( "arrayExpr:"+print(arrayExpr)+"\n",common::CheckerName::arrayBound,0,*configure);
                            vector<ArraySubscript> tmp = getArraySubscriptInExpr(arrayExpr, astF,block,stmt,myCFG);

                            if(tmp.size()>0){
                                vector<ArraySubscript>::iterator itrblock;
                                for(auto & as:tmp){
                                    bool iffind=false;
                                    for(auto& ass:tmpAS){
                                        if(ass.arrayName==as.arrayName&&print(ass.index)==print(as.index)&&ass.indexCnt==as.indexCnt){
                                            iffind=true;
                                            common::printLog( "as is found in tmpAS, just throw away\n",common::CheckerName::arrayBound,0,*configure);
                                            break;
                                        }
                                    }
                                    if(!iffind){
                                        common::printLog( "as is not found in tmpAS,push to tmpAS\n",common::CheckerName::arrayBound,0,*configure);
                                        //as  is not found in tmpAS
                                        tmpAS.push_back(as);
                                    }

                                }
                                //tmpAS.insert(tmpAS.begin(),tmp.begin(),tmp.end());
                            }
                        }
                    }
                    if (CheckVectorOutBoundEnable) {
                        FindCXXOperatorCallExpr visitcxx;
                        visitcxx.TraverseStmt(stmt);
                        const auto &cxxExprs = visitcxx.getArrayExprs();
                        if(!cxxExprs.empty()){
                            FunctionDecl* f = manager->getFunctionDecl(astF);	
                            std::string funcName =  f->getNameInfo().getName().getAsString();
                            for(auto& cxxExpr:cxxExprs){  
                                Expr* base = cxxExpr->getArg(0);
                                Expr* idx = cxxExpr->getArg(1);
                                common::printLog( "Vector located:"+print(cxxExpr)+" in "+funcName+", ",common::CheckerName::arrayBound,3,*configure);
                                ArraySubscript as(translator->zc, stmt);
                                if(!CheckTaintArrayOnly || isTainted(f,block,stmt,idx))
                                {
                                    vector<AtomicAPInt> tmpAA;
                                    common::printLog( "index is tainted or checkTaintArrayOnly is false, need check "+print(idx)+" ",common::CheckerName::arrayBound,3,*configure);							
                                    as.isVectorArraySubscript = true;
                                    AtomicAPInt tempoperator;
                                    tempoperator.op=clang::BinaryOperatorKind(BO_LT);
                                    tempoperator.lhs=idx;
                                    tempoperator.rhs=-1;							
                                    tmpAA.push_back(tempoperator);
                                    vector<z3::expr> idxv = translator->zc->clangExprToZ3Expr(idx,astF);
                                    if (idxv.empty()) continue;
                                    std::string name = print(base);
                                    as.upBound_expr = translator->zc->createExpr(name+".size()");
                                    as.upBoundCondition = translator->zc->clangOp2Z3Op(idxv.back(),as.upBound_expr,BinaryOperatorKind::BO_LT);
                                    common::printLog( "< "+name+".size()"+"\n",common::CheckerName::arrayBound,1,*configure);
                                    //we need to determine whether there needs a checking like idx>=0. If idx is unsigned, then no; If idx is signed, then yes.
                                    if(!isUnsigned(idx)){
                                        common::printLog( "index is not unsigned, need check "+print(idx)+">=0, ",common::CheckerName::arrayBound,3,*configure);
                                        
                                        AtomicAPInt tempoperator;
                                        tempoperator.op=clang::BinaryOperatorKind(BO_GE);
                                        tempoperator.lhs=idx;
                                        tempoperator.rhs=0;
                                        tmpAA.push_back(tempoperator);
                                        as.isUnsigned = false;
                                        as.isUnsignedChecked = 0;
                                        as.lowBoundCondition = translator->zc->clangOp2Z3Op(idxv.back(),translator->zc->Int2Z3Expr(0),BinaryOperatorKind::BO_GE);
                                    }
                                    //delete idxv; idxv=nullptr;
                                    SourceManager *sm;
                                    sm = &(f->getASTContext().getSourceManager()); 
                                    string loc = idx->getBeginLoc().printToString(*sm);	
                                    common::printLog( "at "+loc+"\n",common::CheckerName::arrayBound,3,*configure);							
                                    as.func=f;
                                    as.block=block;
                                    as.stmt=cxxExpr;
                                    as.index=idx;
                                    as.location=loc;
                                    as.originalLocation=loc;
                                    as.condition=tmpAA;
                                    as.isLoopBoundChecking=false;							
                                    as.arrayName=print(base);
                                    as.arrayExpr=print(cxxExpr);
                                    as.arrayIdx=idx;
                                    as.orignIndex = idx;

                                    translator->handleIndex(as, astF);
                                    as.updateCallStack(funcName);
                                    tmpAS.push_back(as);
                                }
                            }
                        } 
                    }
                }
            }
        }
        
    }
//#ifdef __DEBUG 
//	//////cerr<<"tmpAS size:"<<tmpAS.size()<<endl;
//	for(auto tt:tmpAS){
//		////////cerr<<"func:"<<print((Stmt*)tt.func)<<endl;
//		//////cerr<<"index:"<<print((Stmt*)tt.index)<<endl;
//		//////cerr<<"location:"<<tt.location<<endl;
//	}
//#endif
	common::printLog( "get array information end!\n",common::CheckerName::arrayBound,0,*configure);
	return tmpAS;
}

int ReturnStackAddrChecker::handleAssignStmt(ASTFunction* astF, Expr* rhs, ArraySubscript& con, CFGBlock* block) {
    //idx = cc?l:r;
    if(ConditionalOperator *co=dyn_cast<ConditionalOperator>(rhs))
    {
        /*
           Expr* cc=co->getCond()->IgnoreCasts()->IgnoreParens();
           Expr* r= nullptr;
           auto conitr = con.condition.begin();
           while(conitr!=con.condition.end()){
        //					for(unsigned j=0;j<con.condition.size();j++){
        AtomicAPInt temp=*conitr;
        bool flag=false;
        Expr* Indx=con.index;
        con.changeIndex(co->getLHS()->IgnoreCasts()->IgnoreParens());
        if(checkConditionStmt(cc,con,temp,true))
        {
        r=co->getRHS()->IgnoreCasts()->IgnoreParens();
        common::printLog( "true ConditionalOperator, co rhs:"+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);
        flag=true;
        }
        else {
        con.changeIndex(co->getRHS()->IgnoreCasts()->IgnoreParens());
        if(checkConditionStmt(cc,con,temp,false))
        {
        r=co->getLHS()->IgnoreCasts()->IgnoreParens();
        flag=true;
        common::printLog( "false ConditionalOperator, co rhs:"+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);
        }
        }
        con.changeIndex(Indx);
        if(flag)
        {
        if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(r)) 
        {

        uint64_t value=ILE->getValue().getLimitedValue();
        bool ff=true;

        if(temp.op==clang::BinaryOperatorKind::BO_LT)
        {
        //for(i<n) a[i], need n<=size
        //for(i<=n)a[i], need n<=size
        //if(value>=temp.rhs)ff=false;
        if(value>=temp.rhs)ff=false;
        }
        if(temp.op==clang::BinaryOperatorKind::BO_GE)
        {
        if(value<temp.rhs)ff=false;
        }

        if(ff)
        {
        conitr = con.condition.erase(conitr);
        //return 1;
        }
        else {
        conitr++;
        }

        }
        else if(FloatingLiteral *ILE = dyn_cast<FloatingLiteral>(r))//-ImplicitCastExpr
        {
        double value=ILE->getValue().convertToDouble();
        //////////cerr<<value<<endl;
        bool ff=true;
        if(temp.op==clang::BinaryOperatorKind::BO_LT)
        {
        if(value>=temp.rhs)ff=false;
        }
        if(temp.op==clang::BinaryOperatorKind::BO_GE)
        {
        if(value<temp.rhs)ff=false;
        }

        if(ff)
        {
            conitr = con.condition.erase(conitr);
            //return 1;
        }
        else {
            conitr++;
        }
    }
        else if(BinaryOperator *bb=dyn_cast<BinaryOperator>(r))
        {
            if(bb->getOpcode()==clang::BinaryOperatorKind::BO_Rem)
            {
                //////cerr<<"Rem after assign"<<endl;
                Expr* rr=bb->getRHS()->IgnoreCasts()->IgnoreParens();
                if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(rr)) 
                {
                    uint64_t value=ILE->getValue().getLimitedValue();
                    if(temp.op==clang::BinaryOperatorKind::BO_LT)
                    {
                        if(value<=temp.rhs)
                        {
                            conitr = con.condition.erase(conitr);
                        }
                        else {
                            conitr++;
                        }
                    }

                }
            }
            else if(bb->getOpcode()==clang::BinaryOperatorKind::BO_And)
            {
                Expr* rr=bb->getRHS()->IgnoreCasts()->IgnoreParens();
                if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(rr)) 
                {
                    uint64_t value=ILE->getValue().getLimitedValue();
                    if(temp.op==clang::BinaryOperatorKind::BO_LT)
                    {
                        if(value<=temp.rhs)
                        {
                            conitr = con.condition.erase(conitr);
                        }
                        else {
                            conitr++;
                        }
                    }

                }
            }
        }
    }//end if(flag)
    }//end while
    }
    if(con.condition.size()>0)return 0;
    else return 1;
    */
    }//end condtionaloperator
    if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(rhs)) 
    {
        //idx=const
        uint64_t value=ILE->getValue().getLimitedValue();
        auto itr = con.condition.begin();
        while(itr != con.condition.end()){
            AtomicAPInt temp=*itr;
            if(temp.op==clang::BinaryOperatorKind::BO_LT)
            {
                if (value<temp.rhs){
                    itr = con.condition.erase(itr);
                }
                else {
                    itr++;
                }
                //if(value>=temp.rhs)flag=false;
            }
            else if(temp.op==clang::BinaryOperatorKind::BO_GE)
            {
                if (value>=temp.rhs){
                    itr=con.condition.erase(itr);
                }
                else{
                    itr++;
                }
                //if(value<temp.rhs)flag=false;
            }
            else {
                assert(0);
            }
        }
        if(con.condition.size()==0)
        {
            return 1;//all sat, remove it
        }
        else
        {
            return -1;//report it
        }
    }
    else if(print(rhs).find("sizeof")!=std::string::npos)
    {
        auto itr = con.condition.begin();
        while(itr != con.condition.end()){
            AtomicAPInt temp=*itr;
            if(temp.op==clang::BinaryOperatorKind::BO_LT)
            {
                itr = con.condition.erase(itr);
            }
            else {
                itr++;
            }
        }
        if(con.condition.size()==0)
        {
            return 1;//all sat, remove it
        }
        else
        {
            return 0;//leave it saved in list for next check
        }
    }
    /*else if(UnaryOperator *ILE = dyn_cast<UnaryOperator>(rhs)) 
      {
      if(ILE->getOpcode()==clang::UnaryOperatorKind::UO_Minus)
      {
    //idx=-xxx
    //return -1;
    }
    }*/
      else if(FloatingLiteral *ILE = dyn_cast<FloatingLiteral>(rhs))//-ImplicitCastExpr
      {
          double value=ILE->getValue().convertToDouble();
          auto itr = con.condition.begin();
          while(itr != con.condition.end()){
              AtomicAPInt temp=*itr;
              if(temp.op==clang::BinaryOperatorKind::BO_LT)
              {
                  if (value<temp.rhs){
                      itr = con.condition.erase(itr);
                  }
                  else {
                      itr++;
                  }
                  //if(value>=temp.rhs)flag=false;
              }
              else if(temp.op==clang::BinaryOperatorKind::BO_GE)
              {
                  if (value>=temp.rhs){
                      itr=con.condition.erase(itr);
                  }
                  else{
                      itr++;
                  }
                  //if(value<temp.rhs)flag=false;
              }
              else {
                  assert(0);
              }
          }
          if(con.condition.size()==0)
          {
              return 1;//all sat, remove it
          }
          else
          {
              return -1;//report it
          }
      }
      else if(DeclRefExpr * ref=dyn_cast<DeclRefExpr>(rhs)){

          if(EnumConstantDecl * EC=dyn_cast<EnumConstantDecl>(ref->getDecl())){
              uint64_t value=EC->getInitVal().getLimitedValue ();
              auto itr = con.condition.begin();
              while(itr != con.condition.end()){
                  AtomicAPInt temp=*itr;
                  if(temp.op==clang::BinaryOperatorKind::BO_LT)
                  {
                      if (value<temp.rhs){
                          itr = con.condition.erase(itr);
                      }
                      else {
                          itr++;
                      }
                      //if(value>=temp.rhs)flag=false;
                  }
                  else if(temp.op==clang::BinaryOperatorKind::BO_GE)
                  {
                      if (value>=temp.rhs){
                          itr=con.condition.erase(itr);
                      }
                      else{
                          itr++;
                      }
                      //if(value<temp.rhs)flag=false;
                  }
                  else {
                      assert(0);
                  }
              }
              if(con.condition.size()==0)
              {
                  return 1;//all sat, remove it
              }
              else
              {
                  return -1;//report it
              }
          }
          //other declrefexpr
          else
          {
              if(SimpleExprCheckOnly){
                  if(dyn_cast<BinaryOperator>(rhs)){
                      common::printLog( "SimpleExprCheckOnly,BinaryOperator:"+print(rhs)+",continue\n",common::CheckerName::arrayBound,0,*configure);								
                      return 1;
                  }
                  else if(dyn_cast<UnaryOperator>(rhs)){
                      common::printLog( "SimpleExprCheckOnly,UnaryOperator:"+print(rhs)+",continue\n",common::CheckerName::arrayBound,0,*configure);
                      return 1;
                  }
                  else if(dyn_cast<CallExpr>(rhs)){
                      common::printLog( "SimpleExprCheckOnly,callexpr:"+print(rhs)+",continue\n",common::CheckerName::arrayBound,0,*configure);
                      return 1;
                  }						
              }
              con.changeIndex(rhs);
              return 0;
          }
      }
      else if(BinaryOperator *bb=dyn_cast<BinaryOperator>(rhs))
      {
          //idx=x%const
          if(bb->getOpcode()==clang::BinaryOperatorKind::BO_Rem)
          {
              //////cerr<<"Rem after assign"<<endl;
              Expr* rr=bb->getRHS()->IgnoreCasts()->IgnoreParens();
              if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(rr)) 
              {

                  uint64_t value=ILE->getValue().getLimitedValue();
                  auto itr = con.condition.begin();
                  while(itr != con.condition.end()){
                      AtomicAPInt temp=*itr;
                      if(temp.op==clang::BinaryOperatorKind::BO_LT)
                      {
                          if (value<=temp.rhs){
                              itr = con.condition.erase(itr);
                          }
                          else {
                              itr++;
                          }
                          //if(value>=temp.rhs)flag=false;
                      }
                      else if(temp.op==clang::BinaryOperatorKind::BO_GE)
                      {
                          if (value>=temp.rhs){
                              itr=con.condition.erase(itr);
                          }
                          else{
                              itr++;
                          }
                          //if(value<temp.rhs)flag=false;
                      }
                      else {
                          assert(0);
                      }
                  }
                  if(con.condition.size()==0)
                  {
                      return 1;//all sat, remove it
                  }
                  else
                  {
                      return -1;//report it
                  }
              }
          }
          else if(bb->getOpcode()==clang::BinaryOperatorKind::BO_And)
          {
              //cerr<<"And Operator"<<endl;
              //idx=x&const
              Expr* rr=bb->getRHS()->IgnoreCasts()->IgnoreParens();
              if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(rr)) 
              {

                  uint64_t value=ILE->getValue().getLimitedValue();
                  auto itr = con.condition.begin();
                  while(itr != con.condition.end()){
                      AtomicAPInt temp=*itr;
                      if(temp.op==clang::BinaryOperatorKind::BO_LT)
                      {
                          if (value<temp.rhs){
                              itr = con.condition.erase(itr);
                          }
                          else {
                              itr++;
                          }
                          //if(value>=temp.rhs)flag=false;
                      }
                      else if(temp.op==clang::BinaryOperatorKind::BO_GE)
                      {
                          if (value>=temp.rhs){
                              itr=con.condition.erase(itr);
                          }
                          else{
                              itr++;
                          }
                          //if(value<temp.rhs)flag=false;
                      }
                      else {
                          assert(0);
                      }
                  }
                  if(con.condition.size()==0)
                  {
                      return 1;//all sat, remove it
                  }
                  else
                  {
                      return -1;//report it
                  }
              }
          }
          //idx=other binary op
          else
          {
              con.changeIndex(rhs);
              return 0;
          }
      }
      //idx=other epxr
      else
      {

          auto rhs_value = calculateExpr(rhs);
          if(rhs_value.find(true) != rhs_value.end()){
              int value=rhs_value[true] ;
              auto itr = con.condition.begin();
              while(itr != con.condition.end()){
                  AtomicAPInt temp=*itr;
                  if(temp.op==clang::BinaryOperatorKind::BO_LT)
                  {
                      if (value<temp.rhs){
                          itr = con.condition.erase(itr);
                      }
                      else {
                          itr++;
                      }
                      //if(value>=temp.rhs)flag=false;
                  }
                  else if(temp.op==clang::BinaryOperatorKind::BO_GE)
                  {
                      if (value>=temp.rhs){
                          itr=con.condition.erase(itr);
                      }
                      else{
                          itr++;
                      }
                      //if(value<temp.rhs)flag=false;
                  }
                  else {
                      assert(0);
                  }
              }
              if(con.condition.size()==0)
              {
                  return 1;//all sat, remove it
              }
              else
              {
                  return -1;//report it
              }
          }
          else
          {
              if(SimpleExprCheckOnly){
                  if(dyn_cast<BinaryOperator>(rhs)){
                      common::printLog( "SimpleExprCheckOnly,BinaryOperator:"+print(rhs)+",continue\n",common::CheckerName::arrayBound,0,*configure);								
                      return 1;
                  }
                  else if(dyn_cast<UnaryOperator>(rhs)){
                      common::printLog( "SimpleExprCheckOnly,UnaryOperator:"+print(rhs)+",continue\n",common::CheckerName::arrayBound,0,*configure);
                      return 1;

                  }
                  else if(dyn_cast<CallExpr>(rhs)){
                      common::printLog( "SimpleExprCheckOnly,callexpr:"+print(rhs)+",continue\n",common::CheckerName::arrayBound,0,*configure);
                      return 1;

                  }						

              }
              con.changeIndex(rhs);
              return 0;
          }
      }
}
//return value 1:con been checked, erase from list; -1: con must out of bound, report it; 0: 
int ReturnStackAddrChecker::throughStmt(ASTFunction* astF, Stmt* it,ArraySubscript& con,CFGBlock* block)//0:next  1:ok   -1:error
{
	std::string tindex=print(con.index->IgnoreCasts()->IgnoreParens());
	std::string pathcond=con.pathConstraint.to_string();
	//cout<<pathcond<<endl;
    if(it->getStmtClass()==Stmt::CompoundAssignOperatorClass){
		const CompoundAssignOperator *co = cast<CompoundAssignOperator>(it);
		std::string top=print(co->getLHS()->IgnoreCasts()->IgnoreParens());
		if(tindex.find(top)==std::string::npos && pathcond.find(top)==std::string::npos ){
			common::printLog("assign to expr "+print(it)+" which is not related to index "+print(con.index)+", skip this stmt\n",common::CheckerName::arrayBound,2,*configure);	
			return 0;
		}
	}
	else if(it->getStmtClass()==Stmt::BinaryOperatorClass) {
		const BinaryOperator *bo = cast<BinaryOperator>(it);
		if(bo->getOpcode()== clang::BinaryOperatorKind::BO_Assign ) {
			std::string top=print(bo->getLHS()->IgnoreCasts()->IgnoreParens());
			if(tindex.find(top)==std::string::npos && pathcond.find(top)==std::string::npos){
				common::printLog("assign to expr "+print(it)+" which is not related to index "+print(con.index)+", skip this stmt\n",common::CheckerName::arrayBound,2,*configure);	
				return 0;
			}		
		}
	}
	else if(DeclStmt *decl = dyn_cast<DeclStmt>(it)) {
		if(VarDecl* var = dyn_cast<VarDecl>(decl->getSingleDecl())) {
			std::string top=var->getNameAsString();
			//cout<<top<<endl;
			if(tindex.find(top)==std::string::npos && pathcond.find(top)==std::string::npos){
				common::printLog("assign to expr "+print(it)+", in which "+top+" is not related to index "+print(con.index)+", skip this stmt\n",common::CheckerName::arrayBound,2,*configure);	
				return 0;
			}			
		}
	}
	if(SolvePathConstraints){
        bool r = updateAndSolveConstraints(astF,con, it, true,block);//1 has unchecked
		return !r;//1 all checked, 0 has unchecked (updateAndSolveConstraints report must overflow when solve)
        //return con.isUpBoundChecked;
    }

    if(it->getStmtClass()==Stmt::CompoundAssignOperatorClass)//+=,-=,*=,/=,%=
	{
		const CompoundAssignOperator *co = cast<CompoundAssignOperator>(it);
        Expr* lhs=co->getLHS()->IgnoreCasts()->IgnoreParens();
        Expr* rhs=co->getRHS()->IgnoreCasts()->IgnoreParens();
        if(print(lhs)==print(con.index->IgnoreCasts()->IgnoreParens())){
            if (Z3CheckEnable){
                translator->handleAssignment(astF, it,con);
                if (con.condition.size()==0){
                    return 1;
                }
            }
        }

        if(co->getOpcode()== clang::BinaryOperatorKind::BO_RemAssign )
        {
            //%=

			if(print(lhs)==print(con.index->IgnoreCasts()->IgnoreParens()))
			{
				
				if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(rhs)) 
				{
					//idx%=const
					uint64_t value=ILE->getValue().getLimitedValue();
                    auto itr = con.condition.begin();
                    while(itr != con.condition.end()){
                        AtomicAPInt temp=*itr;
                        if(temp.op==clang::BinaryOperatorKind::BO_LT)
                        {
                            if (value<=temp.rhs){
                                itr = con.condition.erase(itr);
                            }
                            else {
                                itr++;
                            }
                            //if(value>=temp.rhs)flag=false;
                        }
                        else {
                            itr++;
                        }
                    }
                    if(con.condition.size()==0)
                    {
                        return 1;//all sat, remove it
                    }
                    else
                    {
                        return -1;//report it
                    }
				}
				else if(print(rhs).find("sizeof")!=std::string::npos)
				{
                    //idx%=sizeof()
					//int a[5][6];
					//cerr<<"test sizeof:"<<sizeof(a)<<endl;
					common::printLog( print(lhs)+"%= "+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);	
					//rhs->dump();
					//con.condition.clear();
					return 1;
				}
				else if(DeclRefExpr * ref=dyn_cast<DeclRefExpr>(rhs)){
				//idx%=rhs	
					if(EnumConstantDecl * EC=dyn_cast<EnumConstantDecl>(ref->getDecl())){
						uint64_t value=EC->getInitVal().getLimitedValue ();
						common::printLog( "EnumConstantDecl:"+int2string(value)+"\n",common::CheckerName::arrayBound,0,*configure);	
                        auto itr = con.condition.begin();
                        while(itr != con.condition.end()){
                            AtomicAPInt temp=*itr;
                            if(temp.op==clang::BinaryOperatorKind::BO_LT)
                            {
                                if (value<temp.rhs){
                                    itr = con.condition.erase(itr);
                                }
                                else {
                                    itr++;
                                }
                                //if(value>=temp.rhs)flag=false;
                            }
                            else {
                                itr++;
                            }
                        }
                        if(con.condition.size()==0)
                        {
                            return 1;//all sat, remove it
                        }
                        else
                        {
                            return -1;//report it
                        }		
                    }
				}
			}
		}
	}
	else if(it->getStmtClass()==Stmt::BinaryOperatorClass)//
	{
		const BinaryOperator *bo = cast<BinaryOperator>(it);
        Expr* lhs=bo->getLHS()->IgnoreCasts()->IgnoreParens();
        Expr* rhs=bo->getRHS()->IgnoreCasts()->IgnoreParens();
        if(print(lhs)==print(con.index->IgnoreCasts()->IgnoreParens())){
            if (Z3CheckEnable){
                translator->handleAssignment(astF, it,con);
                if (con.condition.size()==0){
                    return 1;
                }
            }
        }
        if(bo->getOpcode()== clang::BinaryOperatorKind::BO_Assign )
        {
            //idx=x op y, idx=cc?x:y; idx=expr; idx=i++;....
            if(print(lhs)==print(con.index->IgnoreCasts()->IgnoreParens()))
            {
                return handleAssignStmt(astF, rhs, con, block);
            } 
            else if (checkIndexHasVar(lhs, con.condition[0].lhs)) {
                //con.changeIndex()
            }
			else if(print(rhs)==print(con.index->IgnoreCasts()->IgnoreParens()))
			{
                //x=idx
                con.changeIndex(lhs);
				CFGBlock* tmp=block;
				while(tmp!=nullptr&&tmp->getTerminator().getStmt()!=nullptr
					&&tmp->getTerminator().getStmt()->getStmtClass()!=Stmt::IfStmtClass
					&&tmp->getTerminator().getStmt()->getStmtClass()!=Stmt::ForStmtClass&&tmp->getTerminator().getStmt()->getStmtClass()!=Stmt::WhileStmtClass
					&&tmp->getTerminator().getStmt()->getStmtClass()!=Stmt::DoStmtClass)
				{
					if(tmp->succ_begin()!=nullptr)
						tmp=*(tmp->succ_begin());
					else
						break;
				}
				if(tmp!=nullptr&&tmp->getTerminator().getStmt() != nullptr&&tmp->getTerminator().getStmt()->getStmtClass()==Stmt::IfStmtClass)
				{
					Expr* cond=((IfStmt*)tmp->getTerminator().getStmt())->getCond();	
					bool T=true;
					for(CFGBlock::succ_iterator succ_iter=block->succ_begin();succ_iter!=block->succ_end();++succ_iter){
						CFGBlock* succ=*succ_iter;
						//common::printLog( "succ block ID"+int2string(succ->getBlockID())+"\n",common::CheckerName::arrayBound,1,*configure);
						if(succ==NULL)continue;
						std::vector<ArraySubscript> tmp=mapToBlockOut[succ];
						for(ArraySubscript s : tmp)
						{
							if(s.ID == con.ID)
							{
								T=succ_iter==block->succ_begin();
								break;
							}
						}					
					}
					vector<AtomicAPInt> cons=con.condition;
					for(unsigned j=0;j<cons.size();j++)
					{
						common::printLog( "cons j: "+int2string(j)+"\n",common::CheckerName::arrayBound,0,*configure);
						int r;
						if(SimpleCheckEnable){r=checkConditionStmtHasIdx(block,tmp->getTerminator().getStmt(),cond,con,cons[j],T,false);}
						else{r=checkConditionStmt(astF, cond,con,cons[j],T);}
						if(r)
						{
							common::printLog( "checkConditionStmt true,find if check: "+print(cond)+", "+int2string(T)+"\n",common::CheckerName::arrayBound,0,*configure);
							cons.erase(cons.begin()+j);
							j--;
						}
					}
					if(cons.size()==0)
					{
						return 1;	
					}
				}
				con.resetIndex();
			}
		}//end it is assign
    }//end it is binary
	//int n=m;
    else if(DeclStmt *decl = dyn_cast<DeclStmt>(it))
	{
		if(VarDecl* var = dyn_cast<VarDecl>(decl->getSingleDecl()))
		{
			if(Expr* assign = var->getInit())
			{
                
                if (const clang::DeclRefExpr *idx = dyn_cast<DeclRefExpr>(con.index->IgnoreImpCasts())){
                    if(var==idx->getDecl()){
                        if(SolvePathConstraints){
                            return updateAndSolveConstraints(astF,con, decl, true,block);
                            //return con.isUpBoundChecked;
                        }
                        if (Z3CheckEnable){
                            translator->handleAssignment(astF, it,con);
                            if (con.condition.size()==0){
                                return 1;
                            }
                        }
                    }
                }

            }
        }
	}
	else if(it->getStmtClass()==Stmt::CallExprClass)//Call
	{

	}
	return 0;
}

//bool ReturnStackAddrChecker::CheckConditionExpr(Expr* expr,string op,uint64_t cons) //true : remove this cons
//{
//	//cerr<<"op"<<op<<endl;
//	common::printLog( "CheckConditionExpr expr:"+print(expr)+"\n",common::CheckerName::arrayBound,0,*configure);
//	if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(expr)) 
//	{
//		
//		uint64_t value=ILE->getValue().getLimitedValue();
//		//cerr<<value<<endl;
//		//if(value<0)return false;
//		if(op=="<")
//		{
//			return value<=cons;
//		}
//		else if(op==">=")
//		{
//			return value>=cons;
//		}
//
//	}
//	else if(UnaryOperator *ILE = dyn_cast<UnaryOperator>(expr)) 
//	{
//		common::printLog( "CheckConditionExpr UnaryOperator:"+print(expr)+"\n",common::CheckerName::arrayBound,0,*configure);
//		return false;
//	}
//	else if(FloatingLiteral *ILE = dyn_cast<FloatingLiteral>(expr))//-ImplicitCastExpr
//	{
//		int value=(int)ILE->getValue().convertToDouble();
//		//if(value<0)return false;
//		if(op=="<")
//		{
//			return value<=cons;
//		}
//		else if(op==">=")
//		{
//			return value>=cons;
//		}
//	}
//	else 
//	{
//		common::printLog( "CheckConditionExpr:"+print(expr)+"\n",common::CheckerName::arrayBound,0,*configure);
//		auto rhs_value = calculateExpr(expr);
//		if(rhs_value.find(true) != rhs_value.end()){
//			common::printLog( "calculateExpr find true\n",common::CheckerName::arrayBound,0,*configure);
//			int value=rhs_value[true] ;
//			//cerr<<value<<endl;
//			if(op=="<")
//			{
//				return value<=cons;
//			}
//			else if(op==">=")
//			{
//				return value>=cons;
//			}
//		}
//	}
//	return false;
//}
bool ReturnStackAddrChecker::CheckConditionExpr(Expr* expr,string op,uint64_t cons) //true : remove this cons
{
	//cerr<<"op"<<op<<endl;
	common::printLog( "CheckConditionExpr expr:"+print(expr)+"\n",common::CheckerName::arrayBound,0,*configure);
	if(IntegerLiteral *ILE = dyn_cast<IntegerLiteral>(expr)) 
	{
		
		uint64_t value=ILE->getValue().getLimitedValue();
		//cerr<<value<<endl;
		//if(value<0)return false;
		if(op=="<")
		{
			return value<=cons;
		}
		else if(op==">=")
		{
			return value>=cons;
		}

	}
	else if(dyn_cast<UnaryOperator>(expr)) 
	{
		common::printLog( "CheckConditionExpr UnaryOperator:"+print(expr)+"\n",common::CheckerName::arrayBound,0,*configure);
		return false;
	}
	else if(FloatingLiteral *ILE = dyn_cast<FloatingLiteral>(expr))//-ImplicitCastExpr
	{
		int value=(int)ILE->getValue().convertToDouble();
		//if(value<0)return false;
		if(op=="<")
		{
			return value<=cons;
		}
		else if(op==">=")
		{
			return value>=cons;
		}
	}
	else if(DeclRefExpr * ref=dyn_cast<DeclRefExpr>(expr)){
					
		if(EnumConstantDecl * EC=dyn_cast<EnumConstantDecl>(ref->getDecl())){
			uint64_t value=EC->getInitVal().getLimitedValue ();
			if(op=="<")
			{
				return value<=cons;
			}
			else if(op==">=")
			{
				return value>=cons;
			}
		}
	}
	else 
	{
		common::printLog( "CheckConditionExpr:"+print(expr)+"\n",common::CheckerName::arrayBound,0,*configure);
		auto rhs_value = calculateExpr(expr);
		if(rhs_value.find(true) != rhs_value.end()){
			common::printLog( "calculateExpr find true\n",common::CheckerName::arrayBound,0,*configure);
			int value=rhs_value[true] ;
			//cerr<<value<<endl;
			if(op=="<")
			{
				return value<=cons;
			}
			else if(op==">=")
			{
				return value>=cons;
			}
		}
	}
	return false;
}
bool  ReturnStackAddrChecker::checkConditionStmtUseZ3(ASTFunction* astF, Expr* stmt,ArraySubscript input,AtomicAPInt con,bool flag){
    if(translator->implyFlag(astF, stmt,input,con,flag)) return true;
    else return false;
}
//vector<ArraySubscript> ReturnStackAddrChecker::checkConditionStmtUseZ3(Expr* stmt,vector<ArraySubscript> input,bool flag){
//	vector<ArraySubscript> result;
//	//if flag is false,it's if's false branch,stmt use !stmt
//	////////cerr<<"checkConditionStmtUseZ3"<<endl;
//	////////cerr<<"stmt:"<<endl;stmt->dump();
//	////////cerr<<"input size:"<<input.size()<<endl;
//	for(unsigned i=0;i<input.size();i++){
//		bool notEmpty=false;
//		////////cerr<<"index is:"<<endl;input[i].index->dump();
//		//for(vector<AtomicAPInt>::iterator itr=input[i].condition.begin();itr!=input[i].condition.end();itr++){		
//		for(unsigned j=0;j<input[i].condition.size();j++){
//		////////cerr<<"AtomicAPInt:"<<getInfo(*itr)<<endl;
//			if(implyFlag(stmt,input[i].condition[j],flag)){
//				input[i].condition.erase(input[i].condition.begin()+j);
//			}else{
//				//////cerr<<"notEmpty=true"<<endl;
//				notEmpty=true;
//			}
//		}
//		if(notEmpty){
//			result.push_back(input[i]);
//			//////cerr<<"checkConditionStmtUseZ3 result size:"<<result.size()<<endl;
//		}
//	}
//
//	return result;
//}
bool ReturnStackAddrChecker::checkIndexHasVar(ParmVarDecl* p, Stmt* idx){
    bool flag=false;
    for (auto child:idx->children()) {
        if (Expr* c = dyn_cast<Expr>(child)){
            if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(c->IgnoreCasts()->IgnoreParens())){
                if (ParmVarDecl* c = dyn_cast<ParmVarDecl>(ref->getDecl())){
                    if (c == p){
                        return true;
                    }
                    else {
                        flag = flag || false;
                    } 
                }
            } 
            else {
                flag = flag || checkIndexHasVar(p,child);
            }
        }
    }
    if (DeclRefExpr *d = dyn_cast<DeclRefExpr>(idx)){
        ValueDecl* idxval = d->getDecl();
        return p == idxval;
    }
    return flag;
}

bool ReturnStackAddrChecker::checkIndexHasVar(Expr* lhs, Expr* idx){
    if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(lhs->IgnoreCasts()->IgnoreParens())){
        ValueDecl* val = ref->getDecl();
        if (DeclRefExpr *d = dyn_cast<DeclRefExpr>(idx->IgnoreCasts()->IgnoreParens())){
            ValueDecl* idxval = d->getDecl();
            return val == idxval;
        }
        else if(BinaryOperator * bo = dyn_cast<BinaryOperator>(idx->IgnoreCasts()->IgnoreParens())){	
            return checkIndexHasVar(lhs, bo->getLHS()) || checkIndexHasVar(lhs, bo->getRHS());
        }

    }
    //assert(0);
    return false;
}

bool ReturnStackAddrChecker::checkConditionStmtHasIdx(CFGBlock* block,Stmt* stmt,Expr* cond,ArraySubscript input,AtomicAPInt con,bool flag,bool checkTaint){
	//if checkTaint is true, we check whether index<untainted, namely, if index<tainted, return false
	//if checkTaint is false, we check whether index<**.
	bool result=false;
	Expr *idx=con.getLHS();
	common::printLog( "In checkConditionStmtHasIdx:\n",common::CheckerName::arrayBound,0,*configure);
	common::printLog( "condition is:"+print(cond)+"\n",common::CheckerName::arrayBound,0,*configure);
	common::printLog( "input.stmt:"+print(input.stmt)+"\n",common::CheckerName::arrayBound,0,*configure);
	if(CallExpr* call = dyn_cast<CallExpr>(cond->IgnoreCasts()->IgnoreParens()) )
	{
		FunctionDecl* fun=call->getDirectCallee();
		if(fun==nullptr) return false;
		DeclarationName DeclName = fun->getNameInfo().getName();
		std::string FuncName = DeclName.getAsString();
		//cerr<<FuncName<<endl;
		if(FuncName=="__builtin_expect")
		{
			return checkConditionStmtHasIdx(block,stmt,call->getArg(0)->IgnoreCasts()->IgnoreParens(),input,con,flag,checkTaint);

		}
	}
	if(BinaryOperator * it=dyn_cast<BinaryOperator>(cond)){	
		switch(it->getOpcode())
		{
		case clang::BinaryOperatorKind::BO_LAnd:
			{
				bool temp_lhs=checkConditionStmtHasIdx(block,stmt,it->getLHS()->IgnoreCasts()->IgnoreParens(),input,con,flag,checkTaint);
				bool temp_rhs=checkConditionStmtHasIdx(block,stmt,it->getRHS()->IgnoreCasts()->IgnoreParens(),input,con,flag,checkTaint);
				if(flag)
				{
					result=temp_lhs || temp_rhs;
				}
				else
				{
					result=temp_lhs && temp_rhs;
				}

				break;
			}
		case clang::BinaryOperatorKind::BO_LOr:
			{
				bool temp_lhs=checkConditionStmtHasIdx(block,stmt,it->getLHS()->IgnoreCasts()->IgnoreParens(),input,con,flag,checkTaint);
				bool temp_rhs=checkConditionStmtHasIdx(block,stmt,it->getRHS()->IgnoreCasts()->IgnoreParens(),input,con,flag,checkTaint);
				if(flag)
				{
					result=temp_lhs && temp_rhs;
				}
				else
				{
					result=temp_lhs || temp_rhs;
				}
				break;
			}
		default:
			{
				Expr* lhs=it->getLHS()->IgnoreImpCasts()->IgnoreCasts()->IgnoreParens();
				Expr* rhs=it->getRHS()->IgnoreImpCasts()->IgnoreCasts()->IgnoreParens();
				
				if(flag){
					if(con.op==clang::BinaryOperatorKind::BO_LT){						
						if(print(idx)==print(lhs))
						{
							//common::printLog( "Index matches lhs\n",common::CheckerName::arrayBound,0,*configure);
							common::printLog( "Index matches lhs\nrhs:FuncNow-"+FuncNow->getNameAsString()+"\tStmt-"+print(stmt)+"\trhs-"+print(rhs)+"\n",common::CheckerName::arrayBound,0,*configure);
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_LT||
								it->getOpcode()==clang::BinaryOperatorKind::BO_LE)
							{
								if(checkTaint){
									if(!isTainted(FuncNow,block,stmt,rhs)){
										result=true;
									}
									else{
										result=false;
									}
								}
								else{ 
									result=true;
								}
							}							
						}
						else if(print(idx)==print(rhs))
						{
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_GT||
								it->getOpcode()==clang::BinaryOperatorKind::BO_GE)
							{
								result=true;								
							}
						}
					}
					else if(con.op==clang::BinaryOperatorKind::BO_GE){
						if(print(idx)==print(lhs))
						{
							common::printLog( "Index match\n",common::CheckerName::arrayBound,0,*configure);
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_GT||
								it->getOpcode()==clang::BinaryOperatorKind::BO_GE)
							{
								if(checkTaint){
									if(!isTainted(FuncNow,block,stmt,rhs)){
										result=true;
									}
									else{
										result=false;
									}
								}
								else{ 
									result=true;
								}								
							}							
						}
						else if(print(idx)==print(rhs))
						{
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_LT||
								it->getOpcode()==clang::BinaryOperatorKind::BO_LE)
							{
								if(checkTaint){
									if(!isTainted(FuncNow,block,stmt,lhs)){
										result=true;
									}
									else{
										result=false;
									}
								}
								else{ 
									result=true;
								}								
							}
						}
					}
				}
				else
				{
					common::printLog( int2string(flag)+"path\n",common::CheckerName::arrayBound,0,*configure);
					if(con.op==clang::BinaryOperatorKind::BO_LT){						
						if(print(idx)==print(lhs))
						{
							common::printLog( "Index match\n",common::CheckerName::arrayBound,0,*configure);
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_GT||
								it->getOpcode()==clang::BinaryOperatorKind::BO_GE)
							{
								if(checkTaint){
									if(!isTainted(FuncNow,block,stmt,rhs)){
										result=true;
									}
									else{
										result=false;
									}
								}
								else{ 
									result=true;
								}								
							}							
						}
						else if(print(idx)==print(rhs))
						{
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_LT||
								it->getOpcode()==clang::BinaryOperatorKind::BO_LE)
							{
								if(checkTaint){
									if(!isTainted(FuncNow,block,stmt,lhs)){
										result=true;
									}
									else{
										result=false;
									}
								}
								else{ 
									result=true;
								}								
							}
						}
					}
					else if(con.op==clang::BinaryOperatorKind::BO_GE){
						if(print(idx)==print(lhs))
						{
							common::printLog( "Index match\n",common::CheckerName::arrayBound,0,*configure);
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_LT||
								it->getOpcode()==clang::BinaryOperatorKind::BO_LE)
							{
								if(checkTaint){
									if(!isTainted(FuncNow,block,stmt,rhs)){
										result=true;
									}
									else{
										result=false;
									}
								}
								else{ 
									result=true;
								}								
							}							
						}
						else if(print(idx)==print(rhs))
						{
							if(it->getOpcode()==clang::BinaryOperatorKind::BO_GT||
								it->getOpcode()==clang::BinaryOperatorKind::BO_GE)
							{
								if(checkTaint){
									if(!isTainted(FuncNow,block,stmt,lhs)){
										result=true;
									}
									else{
										result=false;
									}
								}
								else{ 
									result=true;
								}								
							}
						}
					}
				}
			}
			}
			}
	//else if(UnaryOperator* it=dyn_cast<UnaryOperator>(idx)){
	//	bool temp_lhs=checkConditionStmtHasIdx(stmt,input,it->getSubExpr()->IgnoreCasts()->IgnoreParens(),flag);
	//	result=temp_lhs;
	//}
	////if(isTainted(input.func,input.block,input.stmt,idx)){
	//else if(print(stmt).find(print(idx))!=string::npos){ result=true;}
	else{result=false;}
	return result;
}
