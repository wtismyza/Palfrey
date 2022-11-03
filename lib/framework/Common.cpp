#include "framework/Common.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/StmtVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>

#include <fstream>
#include <iostream>
#include <queue>
#include <regex>

using namespace std;

std::string trim(std::string);

namespace {

class ASTFunctionLoad : public ASTConsumer,
                        public RecursiveASTVisitor<ASTFunctionLoad> {
public:
  void HandleTranslationUnit(ASTContext &Context) override {
    TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
    TraverseDecl(TUD);
  }

  void setSourceLoc(SourceLocation SL) { AULoc = SL; }

  bool TraverseDecl(Decl *D) {
    if (!D)
      return true;
    bool rval = true;
    SourceManager &SM = D->getASTContext().getSourceManager();
    //该Decl位于Main file中
    if (SM.isInMainFile(D->getLocation()) ||
        D->getKind() == Decl::TranslationUnit) {
      rval = RecursiveASTVisitor<ASTFunctionLoad>::TraverseDecl(D);
    } else if (D->getLocation().isValid()) {
      std::pair<FileID, unsigned> XOffs = SM.getDecomposedLoc(AULoc);
      std::pair<FileID, unsigned> YOffs = SM.getDecomposedLoc(D->getLocation());
      //判断该Decl是否与主文件在同一个TranslateUnit中
      std::pair<bool, bool> InSameTU =
          SM.isInTheSameTranslationUnit(XOffs, YOffs);
      //判断是否位于同一个TranslationUnit中，该Decl
      //是否应该被包含于分析过程中
      if (InSameTU.first) {
        rval = RecursiveASTVisitor<ASTFunctionLoad>::TraverseDecl(D);
      }
    }
    return rval;
  }

  bool TraverseFunctionDecl(FunctionDecl *FD) {
    if (FD) {
      functions.push_back(FD);
    }
    return true;
  }

  //类内成员函数
  bool TraverseCXXMethodDecl(CXXMethodDecl *D) {
    if (D) {
      if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
        TraverseFunctionDecl(FD);
      }
    }
    return true;
  }

  //构造函数
  bool TraverseCXXConstructorDecl(CXXConstructorDecl *CCD) {
    if (CCD) {
      FunctionDecl *FD = CCD->getDefinition();
      TraverseFunctionDecl(FD);
    }
    return true;
  }

  //析构函数
  bool TraverseCXXDestructorDecl(CXXDestructorDecl *CDD) {
    if (CDD) {
      FunctionDecl *FD = CDD->getDefinition();
      TraverseFunctionDecl(FD);
    }
    return true;
  }

  bool TraverseStmt(Stmt *S) { return true; }

  const std::vector<FunctionDecl *> &getFunctions() const { return functions; }

private:
  bool includeOrNot(const Decl *D) {
    assert(D);
    if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      // We skip function template definitions, as their semantics is
      // only determined when they are instantiated.
      if (FD->isDependentContext()) {
        return false;
      }
    }
    return true;
  }

  std::vector<FunctionDecl *> functions;
  SourceLocation AULoc;
};

class ASTGlobalVarLoad : public ASTConsumer,
                         public RecursiveASTVisitor<ASTGlobalVarLoad> {
public:
  void HandleTranslationUnit(ASTContext &Context) override {
    TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
    TraverseDecl(TUD);
  }

  /*
   * do not visit Function body!
   * so VisitVarDecl(VarDecl *VD) JUST visit global var!
   */
  bool TraverseFunctionDecl(FunctionDecl *FD) { return true; }

  /*
   * Collect global vars, whether or not they are initialized/extern/...
   * Its correctness depends on TraverseFunctionDecl do not
   * call their father
   * (RecursiveASTVisitor<ASTGlobalVarLoad>::TraverseFunctionDecl)
   * And we do not collect ParamVarDecl, so use Traverse not Visit!
   */
  bool TraverseVarDecl(VarDecl *VD) {
    if (VD) {
      SourceManager &srcMgr = VD->getASTContext().getSourceManager();
      if (srcMgr.isInMainFile(VD->getLocation())) {
        globalvds.push_back(VD);
      }
    }
    return true;
  }

  const std::vector<VarDecl *> &getGlobalVars() const { return globalvds; }

private:
  std::vector<VarDecl *> globalvds;
};

class ASTVariableLoad : public RecursiveASTVisitor<ASTVariableLoad> {
public:
  bool VisitDeclStmt(DeclStmt *S) {
    for (auto D = S->decl_begin(); D != S->decl_end(); D++) {
      if (VarDecl *VD = dyn_cast<VarDecl>(*D)) {
        variables.push_back(VD);
      }
    }
    return true;
  }

  const std::vector<VarDecl *> &getVariables() { return variables; }

private:
  std::vector<VarDecl *> variables;
};

/*class ASTFieldVariableLoad : public RecursiveASTVisitor<ASTFieldVariableLoad> {
public:
  // get original VarDecl(e.g. a.b.c get(c)=a)
  VarDecl *getVarDecl(MemberExpr *S){
    VarDecl *VD = nullptr;
    
    Expr* base = S->getBase();

    while(1) {
      if(CastExpr *E = dyn_cast<CastExpr>(base)) {
          base = E->getSubExpr();
          continue;
      }
      if(UnaryOperator *E = dyn_cast<UnaryOperator>(base)) {
          base = E->getSubExpr();
          continue;
      }
      if(ParenExpr *E = dyn_cast<ParenExpr>(base)) {
          base = E->getSubExpr();
          continue;
      }
      break;
    }

    if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(base)) {
      VD = dyn_cast<VarDecl>(DRE->getDecl());
    }

    if(MemberExpr *ME = dyn_cast<MemberExpr>(base)) {
      return getVarDecl(ME);
    }

    return VD;   
  }

  bool VisitMemberExpr(MemberExpr *S) {
    VarDecl *VD = nullptr;
    FieldDecl *FD = dyn_cast<FieldDecl>(S->getMemberDecl());

    Expr* base = S->getBase();

    while(1) {
      if(CastExpr *E = dyn_cast<CastExpr>(base)) {
          base = E->getSubExpr();
          continue;
      }
      if(UnaryOperator *E = dyn_cast<UnaryOperator>(base)) {
          base = E->getSubExpr();
          continue;
      }
      if(ParenExpr *E = dyn_cast<ParenExpr>(base)) {
          base = E->getSubExpr();
          continue;
      }
      break;
    }

    if(DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(base)) {
      VD = dyn_cast<VarDecl>(DRE->getDecl());
    }

    if(CXXThisExpr *CXXTE = dyn_cast<CXXThisExpr>(base)) {
      //todo
    }

    //deal with nesting fields
    if(MemberExpr *ME = dyn_cast<MemberExpr>(base)) {
      FieldDecl *fatherFD = dyn_cast<FieldDecl>(ME->getMemberDecl());
      if(FD && fatherFD) {
        VD = getVarDecl(ME);
        if(VD){
          if(find(nesting_fields.begin(), nesting_fields.end(), std::make_tuple(VD, fatherFD, FD))
            == nesting_fields.end()) {
              nesting_fields.push_back(std::make_tuple(VD, fatherFD, FD));  
            }
        }
      }
      VD = getVarDecl(ME);
    }

    //normal MemberExpr (which maintain the mapping VarDecl* -> FieldDecl*)
    if(VD && FD){
      if(find(field_variables.begin(), field_variables.end(), std::make_pair(VD, FD)) 
          == field_variables.end()) {
        field_variables.push_back(std::make_pair(VD, FD));
      }
    }
    return true;
  }

  const std::vector<std::pair<VarDecl *, FieldDecl *>> &getFieldVariables() { 
    return field_variables; 
  }

  const std::vector<std::tuple<VarDecl *, FieldDecl *, FieldDecl *>> &getNestingFields() {
    return nesting_fields;
  }

private:
  std::vector<std::pair<VarDecl *, FieldDecl *>> field_variables;
  std::vector<std::tuple<VarDecl *, FieldDecl *, FieldDecl *>> nesting_fields;
};
*/
class ASTCalledFunctionLoad : public StmtVisitor<ASTCalledFunctionLoad> {
public:
  void VisitStmt(Stmt *stmt) {
    // lambda相关处理
    if (LambdaExpr *Lambda = dyn_cast<LambdaExpr>(stmt)) {
      return;
    }
    VisitChildren(stmt);
  }

  void VisitCallExpr(CallExpr *E) {
    // lambda表达式特殊处理。一般的operator call按一般形式处理
    if (CXXOperatorCallExpr *COC = dyn_cast<CXXOperatorCallExpr>(E)) {
      if (FunctionDecl *F = COC->getDirectCallee()) {
        if (CXXMethodDecl *CMD = dyn_cast<CXXMethodDecl>(F)) {
          CXXRecordDecl *CRD = CMD->getParent();
          //判断是否为lambda
          if (CRD && CRD->isLambda()) {
            if (optionBlock["showLambda"] == "true") {
              std::string lambdaName = common::getLambdaName(CMD);
              functions.insert(lambdaName);
              addCallInfo(lambdaName, E);
              // lambda表达式中的默认参数
              // problem here.
              // 会导致lambda表达式中调用的函数，lambda的caller仍然
              // 显示调用（应该不显示）
              // 此部分代码仍然需要，考虑碰到lambdaexpr直接return
              for (auto arg = E->arg_begin(); arg != E->arg_end(); arg++) {
                Visit(*arg);
              }
            }
            return;
          }
        }
      }
    }

    if (FunctionDecl *FD = E->getDirectCallee()) {
      addFunctionDeclCallInfo(FD, E);
    }
    VisitChildren(E);
  }
  // E可能是CXXTemporaryObjectExpr，为CXXConstructExpr的子类
  //可以使用getConstructor()方法获取实际的构造函数
  void VisitCXXConstructExpr(CXXConstructExpr *E) {
    CXXConstructorDecl *Ctor = E->getConstructor();
    CXXRecordDecl *CRD = Ctor->getParent();
    // lambda相关处理
    if (CRD && CRD->isLambda()) {
      return;
    }

    if (FunctionDecl *Def = Ctor->getDefinition()) {
      addFunctionDeclCallInfo(Def, E);
    }

    VisitChildren(E);
  }
  // CXXNewExpr一般都伴随着CXXConstructExpr，因此调用的构造函数的
  //相关信息可在VisitCXXConstructExpr中处理完成。
  void VisitCXXNewExpr(CXXNewExpr *E) {
    if (FunctionDecl *FD = E->getOperatorNew()) {
      addFunctionDeclCallInfo(FD, E);
    }

    VisitChildren(E);
  }

  void VisitCXXDeleteExpr(CXXDeleteExpr *E) {
    if (optionBlock["showDestructor"] == "false") {
      return;
    }
    std::string deleteType = E->getDestroyedType().getAsString();
    auto pos = deleteType.find("class");
    if (pos != std::string::npos) {
      deleteType = deleteType.substr(pos + 6);
      std::string FunctionName = deleteType + "::~" + deleteType;

      functions.insert(FunctionName);
      addCallInfo(FunctionName, E);
    }

    VisitChildren(E);
  }

  //函数默认参数。被调用函数A如果以其他函数B的返回值为默认参数，则该
  //函数A的调用者C会在调用图中显示调用了函数B。默认参数如果未使用则不会显示
  //只有函数A被正常的调用时，函数B才会显示被函数C调用，如果函数A也是作为函数C的
  //默认参数，则函数B不会显示被调用。
  void VisitCXXDefaultArgExpr(CXXDefaultArgExpr *E) { Visit(E->getExpr()); }

  // clang为VisitCXXDefaultArgExpr的方式
  //函数默认参数的另一种实现,配合修改过的getCalledFunction使用
  void VisitFunctionParm(ParmVarDecl *PV) {
    if (PV->hasDefaultArg()) {
      if (CallExpr *CE = dyn_cast<CallExpr>(PV->getDefaultArg())) {
        // VisitCallExpr(CE);
      }
    }
  }
  //类内成员的默认初始化，如果使用的构造函数未对所有成员进行初始化，则
  // AST会出现该节点。可通过访问该节点获取类内成员的默认初始化信息
  //如果未被构造函数初始化的成员的默认初始化调用了函数，则该函数将
  //作为该构造函数的子节点加入CG中
  void VisitCXXDefaultInitExpr(CXXDefaultInitExpr *CDI) {
    Visit(CDI->getExpr());
  }

  void VisitChildren(Stmt *stmt) {
    for (Stmt *SubStmt : stmt->children()) {
      if (SubStmt) {
        this->Visit(SubStmt);
      }
    }
  }

  const std::vector<std::string> getFunctions() {
    return std::vector<std::string>(functions.begin(), functions.end());
  }

  void setConfig(std::unordered_map<std::string, std::string> config) {
    optionBlock = config;
  }

  void setASTContext(FunctionDecl *FD) { parent = FD; }

  const std::vector<std::pair<std::string, int64_t>> getCalleeInfo() {
    return callInfo;
  }

private:
  std::set<std::string> functions;
  std::vector<std::pair<std::string, int64_t>> callInfo;
  std::unordered_map<std::string, std::string> optionBlock;

  FunctionDecl *parent;

  void addFunctionDeclCallInfo(FunctionDecl *FD, Stmt *callsite) {
    std::string fullName = common::getFullName(FD);

    functions.insert(fullName);
    addCallInfo(fullName, callsite);
  }

  int64_t getStmtID(Stmt *st) { return st->getID(parent->getASTContext()); }
  //(anonymous class)::operator()
  void addCallInfo(std::string fullName, Stmt *callSite) {
    int64_t callSiteID = getStmtID(callSite);

    auto element = std::make_pair(fullName, callSiteID);
    callInfo.push_back(element);
  }
};

class ASTCallExprLoad : public RecursiveASTVisitor<ASTCallExprLoad> {
public:
  bool VisitCallExpr(CallExpr *E) {
    call_exprs.push_back(E);
    return true;
  }

  const std::vector<CallExpr *> getCallExprs() { return call_exprs; }

private:
  std::vector<CallExpr *> call_exprs;
};

class ASTFunctionPtrLoad : public RecursiveASTVisitor<ASTFunctionPtrLoad> {
public:
  using FuncPtrInfo = std::unordered_map<std::string, std::set<std::string>>;

  bool VisitVarDecl(VarDecl *VD) {
    if (isFunctionPointer(VD)) {
      //探寻函数指针是否有初始化赋值
      if (VD->hasInit()) {
        Expr *init = VD->getInit();
        std::set<DeclRefExpr *> DREs = getDeclRefs(init);
        for (DeclRefExpr *DRE : DREs) {
          if (FunctionDecl *FD = dyn_cast<FunctionDecl>(DRE->getDecl())) {
            // functionPtrs[VD].insert(FD);
            //　赋值为某一个函数
            addPointToInfo(VD, FD, parent);
          }
          //其他函数指针赋值
          else if (VarDecl *right = dyn_cast<VarDecl>(DRE->getDecl())) {
            if (isFunctionPointer(right)) {
              pointTo[getVarDeclIdentifier(VD, parent)] =
                  pointTo[getVarDeclIdentifier(right, parent)];
            }
          }
        }
      }
    }
    return true;
  }

  bool VisitBinaryOperator(BinaryOperator *BO) {
    //查看是否为赋值语句
    if (BO->getOpcodeStr().str() != "=") {
      return true;
    }
    Expr *lvalue = BO->getLHS();
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(lvalue)) {
      if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
        //该语句是否为对函数指针的赋值语句
        if (isFunctionPointer(VD)) {
          Expr *rvalue = BO->getRHS();
          std::set<DeclRefExpr *> DREs = getDeclRefs(rvalue);
          for (DeclRefExpr *DRE : DREs) {
            Decl *arguement = DRE->getDecl();
            //函数赋值给函数指针
            if (FunctionDecl *FD = dyn_cast<FunctionDecl>(arguement)) {
              addPointToInfo(VD, FD, parent);
            }
            //函数指针间赋值
            else if (VarDecl *var = dyn_cast<VarDecl>(arguement)) {
              if (isFunctionPointer(var)) {
                pointTo[getVarDeclIdentifier(VD, parent)] =
                    pointTo[getVarDeclIdentifier(var, parent)];
              }
            }
          }
        }
      }
    }
    return true;
  }

  bool VisitCallExpr(CallExpr *CE) {
    //一般函数调用
    if (FunctionDecl *FD = CE->getDirectCallee()) {
      std::vector<ParmVarDecl *> funPtrParams;
      //查看被调用函数的参数列表中是否有函数指针
      for (auto param = FD->param_begin(); param != FD->param_end(); ++param) {
        ParmVarDecl *parm = *param;
        //处理函数的函数指针参数
        if (isFunctionPointer(parm)) {
          funPtrParams.push_back(parm);
          //默认形参
          if (parm->hasDefaultArg()) {
            Expr *defaultArg = parm->getDefaultArg();
            std::set<DeclRefExpr *> DREs = getDeclRefs(defaultArg);
            for (DeclRefExpr *DRE : DREs) {
              if (FunctionDecl *defFD =
                      dyn_cast<FunctionDecl>(DRE->getDecl())) {
                addPointToInfo(parm, defFD, FD);
              }
            }
          }
        }
      }
      // CallExpr传参处理
      //考虑到函数的默认形参，使用此变量防止数组越界
      int cnt = 0;
      unsigned size = CE->getNumArgs();
      for (unsigned i = 0; i < size && cnt < funPtrParams.size(); ++i) {
        Expr *arg = CE->getArg(i);
        //对调用参数中类型为函数指针的参数进行处理
        //将caller中函数指针的相关信息复制到callee的形参中
        std::set<DeclRefExpr *> DREs = getDeclRefs(arg);
        bool needMove = false;
        for (DeclRefExpr *DRE : DREs) {
          if (Decl *arguement = DRE->getDecl()) {
            //参数是一个函数指针
            if (VarDecl *VD = dyn_cast<VarDecl>(arguement)) {
              if (isFunctionPointer(VD) && cnt < funPtrParams.size()) {
                needMove = true;
                ParmVarDecl *para = funPtrParams[cnt];

                pointTo[getVarDeclIdentifier(para, FD)] =
                    pointTo[getVarDeclIdentifier(VD, parent)];
              }
            }
            //参数是一个函数
            else if (FunctionDecl *paramFD =
                         dyn_cast<FunctionDecl>(arguement)) {
              needMove = true;
              ParmVarDecl *para = funPtrParams[cnt];
              addPointToInfo(para, paramFD, FD);
            }
          }
        }
        if (needMove) {
          cnt++;
        }
      }
      return true;
    }
    //函数指针调用
    else {
      Expr *callee = CE->getCallee();
      std::set<DeclRefExpr *> DREs = getDeclRefs(callee);
      for (DeclRefExpr *DRE : DREs) {
        if (VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
          if (isFunctionPointer(VD)) {
            addPointerCallSite(VD, CE);
          }
        }
      }
    }
    return true;
  }
  //获取call site的ID与此处可能调用的函数
  std::vector<std::pair<int64_t, std::set<std::string>>> getcalledPtrWithCS() {
    return calledPtrWithCS;
  }

  ASTFunctionPtrLoad(FuncPtrInfo &mayPointTo, FunctionDecl *FD)
      : pointTo(mayPointTo), parent(FD) {}

private:
  FuncPtrInfo &pointTo;
  FunctionDecl *parent;
  //调用的指针以及call site
  //前者为call site的ID，后者为可能指向的函数
  std::vector<std::pair<int64_t, std::set<std::string>>> calledPtrWithCS;

  std::string getVarDeclIdentifier(VarDecl *VD, FunctionDecl *belong) {
    std::string res;
    res = common::getFullName(belong) + std::to_string(VD->getID());
    return res;
  }

  void addPointToInfo(VarDecl *pointer, FunctionDecl *called,
                      FunctionDecl *belong) {
    pointTo[getVarDeclIdentifier(pointer, belong)].insert(
        common::getFullName(called));
  }

  void addPointerCallSite(VarDecl *pointer, Stmt *callsite) {
    // int64_t pointerID = pointer->getID();
    std::string pointerID = getVarDeclIdentifier(pointer, parent);
    int64_t callSiteID = callsite->getID(parent->getASTContext());

    auto calledInfo = std::make_pair(callSiteID, pointTo[pointerID]);

    calledPtrWithCS.push_back(calledInfo);
  }

  bool isFunctionPointer(VarDecl *D) {
    return D->getType()->isFunctionPointerType() ||
           D->getType()->isMemberFunctionPointerType();
  }
  /**
   * 寻找node节点中的第一个DeclRefExpr类型的子节点。用于搜索函数指针的定义、赋值以及调用
   * 语句中，函数指针及其可能指向的被调用函数的信息
   * node : 搜寻的起点
   */
  DeclRefExpr *getDeclRef(Expr *node) {
    // todo 需要修改为层级遍历
    //返回一个数组而不是单个指针
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(node)) {
      return DRE;
    }
    auto child = node->child_begin();
    auto end = node->child_end();
    while (child != end && !(isa<DeclRefExpr>(*child))) {
      end = (*child)->child_end();
      child = (*child)->child_begin();
    }
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(*child)) {
      return DRE;
    }
    return nullptr;
  }
  //层级遍历搜索DeclRefExpr节点
  std::set<DeclRefExpr *> getDeclRefs(Expr *node) {
    std::set<DeclRefExpr *> res;
    if (node == nullptr || node == NULL) {
      return res;
    }

    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(node)) {
      res.insert(DRE);
    }
    queue<Stmt *> q;
    q.push(node);
    while (!q.empty()) {
      auto top = q.front();
      q.pop();
      if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(top)) {
        res.insert(DRE);
      }
      auto it = top->child_begin();
      for (; it != top->child_end(); ++it) {
        q.push(*it);
      }
    }
    return res;
  }
};

class ASTLambdaLoad : public RecursiveASTVisitor<ASTLambdaLoad> {

public:
  bool VisitLambdaExpr(LambdaExpr *LE) {
    if (FunctionTemplateDecl *FTD = getDependentCallOperator(LE)) {
      for (FunctionDecl *FD : FTD->specializations()) {
        functions.insert(FD);
        addLambdaWithCS(FD, LE);
      }
    } else if (CXXMethodDecl *MD = LE->getCallOperator()) {
      functions.insert(MD);
      addLambdaWithCS(MD, LE);
    }
    return true;
  }

  std::vector<FunctionDecl *> getFunctions() {
    return {functions.begin(), functions.end()};
  }

  std::vector<std::pair<FunctionDecl *, Stmt *>> getLambdaWithCS() {
    return lambdaWithCS;
  }

  void addLambdaWithCS(FunctionDecl *FD, Stmt *CS) {
    auto element = std::make_pair(FD, CS);
    lambdaWithCS.push_back(element);
  }

private:
  std::vector<std::pair<FunctionDecl *, Stmt *>> lambdaWithCS;
  std::set<FunctionDecl *> functions;
  // clang 10文档中定义但clang 9中没有的函数
  FunctionTemplateDecl *getDependentCallOperator(LambdaExpr *LE) const {
    CXXRecordDecl *Record = LE->getLambdaClass();
    return getDependentLambdaCallOperator(Record);
  }

  FunctionTemplateDecl *
  getDependentLambdaCallOperator(CXXRecordDecl *CRD) const {
    NamedDecl *CallOp = getLambdaCallOperatorHelper(*CRD);
    return dyn_cast_or_null<FunctionTemplateDecl>(CallOp);
  }

  NamedDecl *getLambdaCallOperatorHelper(const CXXRecordDecl &RD) const {
    if (!RD.isLambda())
      return nullptr;
    DeclarationName Name =
        RD.getASTContext().DeclarationNames.getCXXOperatorName(OO_Call);
    DeclContext::lookup_result Calls = RD.lookup(Name);

    assert(!Calls.empty() && "Missing lambda call operator!");
    assert(allLookupResultsAreTheSame(Calls) &&
           "More than one lambda call operator!");
    return Calls.front();
  }

  bool allLookupResultsAreTheSame(const DeclContext::lookup_result &R) const {
    for (auto *D : R)
      if (!declaresSameEntity(D, R.front()))
        return false;
    return true;
  }
};

class ASTStmtFinder : public StmtVisitor<ASTStmtFinder> {
public:
  /*
  void HandleTranslationUnit(ASTContext &Context) {
    TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
    TraverseDecl(TUD);
  }
  */
  bool checkStmt(Stmt *stmt) {
    if (stmt && stmt->getID(parent->getASTContext()) == targetID) {
      res = stmt;
      return true;
    }

    return false;
  }

  void VisitStmt(Stmt *stmt) {
    if (checkStmt(stmt))
      return;
    VisitChildren(stmt);
  }

  void VisitCallExpr(CallExpr *E) {
    if (checkStmt(E))
      return;

    VisitChildren(E);
  }

  void VisitCXXConstructExpr(CXXConstructExpr *E) {
    CXXConstructorDecl *Ctor = E->getConstructor();
    CXXRecordDecl *CRD = Ctor->getParent();
    // lambda相关处理
    if (CRD && CRD->isLambda()) {
      return;
    }

    if (FunctionDecl *Def = Ctor->getDefinition()) {
      if (checkStmt(E))
        return;
    }

    VisitChildren(E);
  }

  void VisitCXXNewExpr(CXXNewExpr *E) {
    if (FunctionDecl *FD = E->getOperatorNew()) {
      if (checkStmt(E))
        return;
    }

    VisitChildren(E);
  }

  void VisitCXXDeleteExpr(CXXDeleteExpr *E) {
    if (checkStmt(E))
      return;

    VisitChildren(E);
  }

  void VisitCXXDefaultArgExpr(CXXDefaultArgExpr *E) { Visit(E->getExpr()); }

  void VisitCXXDefaultInitExpr(CXXDefaultInitExpr *CDI) {
    Visit(CDI->getExpr());
  }

  void setParentAndID(FunctionDecl *parent, int64_t ID) {
    this->parent = parent;
    targetID = ID;
  }

  Stmt *getResult() { return res; }

  void VisitChildren(Stmt *stmt) {
    for (Stmt *SubStmt : stmt->children()) {
      if (SubStmt) {
        this->Visit(SubStmt);
      }
    }
  }

private:
  FunctionDecl *parent;
  int64_t targetID;
  Stmt *res = nullptr;
};

} // end of anonymous namespace

namespace common {

/*
 *获取FD中所有函数指针以及其可能指向的函数的信息
 *同时包括CallSite信息
 *返回的结果前者为call site的ID后者为函数指针在当前call site可能指向的函数。
 */
std::vector<std::pair<int64_t, std::set<std::string>>> getFunctionPtrWithCS(
    FunctionDecl *FD,
    std::unordered_map<std::string, std::set<std::string>> &mayPointTo) {

  // using resultType = std::vector<std::pair<int64_t,
  // std::set<std::string>>>;
  ASTFunctionPtrLoad load(mayPointTo, FD);
  load.TraverseStmt(FD->getBody());

  // resultType res = load->getcalledPtrWithCS();
  // delete load;
  return load.getcalledPtrWithCS();
}

/*
 *获取FD中调用的lambda表达式的信息
 */
std::vector<FunctionDecl *> getCalledLambda(FunctionDecl *FD) {
  if (!FD || !FD->hasBody()) {
    return {};
  }

  ASTLambdaLoad load;
  load.TraverseStmt(FD->getBody());
  return load.getFunctions();
}

/**
 * load an ASTUnit from ast file.
 * AST : the name of the ast file.
 */
std::unique_ptr<ASTUnit> loadFromASTFile(std::string AST) {

  FileSystemOptions FileSystemOpts;
  IntrusiveRefCntPtr<DiagnosticsEngine> Diags =
      CompilerInstance::createDiagnostics(new DiagnosticOptions());
  std::shared_ptr<PCHContainerOperations> PCHContainerOps;
  PCHContainerOps = std::make_shared<PCHContainerOperations>();
  return std::unique_ptr<ASTUnit>(
      ASTUnit::LoadFromASTFile(AST, PCHContainerOps->getRawReader(),
                               ASTUnit::LoadEverything, Diags, FileSystemOpts));
}

/**
 * get all functions's decl from an ast context.
 */
std::vector<FunctionDecl *> getFunctions(ASTContext &Context) {
  ASTFunctionLoad load;
  load.HandleTranslationUnit(Context);
  return load.getFunctions();
}

/**
 * get all functions's decl from ast context.
 * need extra infomation that ast context does not provide
 */
std::vector<FunctionDecl *> getFunctions(ASTContext &Context,
                                         SourceLocation SL) {
  ASTFunctionLoad load;
  load.setSourceLoc(SL);
  load.HandleTranslationUnit(Context);
  return load.getFunctions();
}

/**
 * get all Global Variables' decl from an ast context.
 */
std::vector<VarDecl *> getGlobalVars(ASTContext &Context) {
  ASTGlobalVarLoad load;
  load.HandleTranslationUnit(Context);
  return load.getGlobalVars();
}

/**
 * get all variables' decl of a function
 * FD : the function decl.
 */
std::vector<VarDecl *> getVariables(FunctionDecl *FD) {
  std::vector<VarDecl *> variables;
  variables.insert(variables.end(), FD->param_begin(), FD->param_end());

  ASTVariableLoad load;
  load.TraverseStmt(FD->getBody());
  variables.insert(variables.end(), load.getVariables().begin(),
                   load.getVariables().end());

  return variables;
}

std::vector<std::pair<std::string, int64_t>> getCalledFunctionsInfo(
    FunctionDecl *FD,
    const std::unordered_map<std::string, std::string> &configure) {

  ASTCalledFunctionLoad load;
  load.setConfig(configure);
  load.setASTContext(FD);
  //函数体
  load.Visit(FD->getBody());

  //如果此函数为构造函数，需要对其可能存在的初始化列表以及默认初始化做额外处理
  if (CXXConstructorDecl *CCD = dyn_cast<CXXConstructorDecl>(FD)) {
    for (CXXCtorInitializer *init : CCD->inits()) {
      load.Visit(init->getInit());
    }
  }

  return load.getCalleeInfo();
}

std::vector<std::string> getCalledFunctions(
    FunctionDecl *FD,
    const std::unordered_map<std::string, std::string> &configure) {

  ASTCalledFunctionLoad load;
  load.setConfig(configure);
  //函数体
  load.Visit(FD->getBody());

  //如果此函数为构造函数，需要对其可能存在的初始化列表以及默认初始化做额外处理
  if (CXXConstructorDecl *CCD = dyn_cast<CXXConstructorDecl>(FD)) {
    for (CXXCtorInitializer *init : CCD->inits()) {
      load.Visit(init->getInit());
    }
  }

  return load.getFunctions();
}

std::vector<CallExpr *> getCallExpr(FunctionDecl *FD) {
  ASTCallExprLoad load;
  load.TraverseStmt(FD->getBody());
  return load.getCallExprs();
}

Stmt *getStmtInFunctionWithID(FunctionDecl *parent, int64_t id) {
  if (parent == nullptr || !parent->hasBody())
    return nullptr;

  ASTStmtFinder finder;

  finder.setParentAndID(parent, id);
  finder.Visit(parent->getBody());

  if (CXXConstructorDecl *D = dyn_cast<CXXConstructorDecl>(parent)) {
    for (CXXCtorInitializer *Cinit : D->inits()) {
      finder.Visit(Cinit->getInit());
    }
  }

  return finder.getResult();
}

std::string getParams(const FunctionDecl *FD) {
  std::string params = "";
  for (auto param = FD->param_begin(); param != FD->param_end(); param++) {
    params = params + (*param)->getOriginalType().getAsString() + " ";
  }
  return params;
}

bool isThisCallSiteAFunctionPointer(Stmt *callsite) {
  if (CallExpr *CE = dyn_cast<CallExpr>(callsite)) {
    if (FunctionDecl *FD = CE->getDirectCallee()) {
      return false;
    } else {
      Expr *callee = CE->getCallee();
      Expr *fixPoint = callee->IgnoreParenImpCasts();
      if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(fixPoint)) {
        Decl *calleeD = DRE->getDecl();
        if (VarDecl *VD = dyn_cast<VarDecl>(calleeD)) {
          if (VD->getType()->isFunctionPointerType() ||
              VD->getType()->isMemberFunctionPointerType())
            return true;
        }
      }
    }
  }
  return false;
}

std::string getLambdaName(FunctionDecl *FD) {
  std::string funName = "Lambda " + FD->getType().getAsString();

  int64_t ID = FD->getID();
  funName += " " + std::to_string(ID);
  return funName;
}

std::string getFullName(const FunctionDecl *FD) {
  std::string name = FD->getQualifiedNameAsString();
  //对于无参数的函数，避免其fullname后出现不必要的空格
  name = trim(name + " " + getParams(FD));
  return name;
}

const CFGBlock *getBlockWithID(const std::unique_ptr<CFG> &cfg,unsigned id) {
    auto iter = cfg->begin();
    auto iterEnd = cfg->end();
    for (; iter != iterEnd; ++iter) {
      if (id == (*iter)->getBlockID()) {
        return (*iter);
      }
    }
    return nullptr;
}

std::vector<CFGBlock *> getNonRecursiveSucc(CFGBlock *curBlock) {
    std::vector<CFGBlock *> result;
    // Do stmt
    if (curBlock->pred_size() == 1) {
      CFGBlock *pred_block = *(curBlock->pred_begin());
      if (nullptr != pred_block &&
          pred_block->getBlockID() < curBlock->getBlockID()) {
        if (nullptr != pred_block->getTerminatorStmt()) {
          if (pred_block->getTerminatorStmt()->getStmtClass() ==
              Stmt::DoStmtClass) {
            auto iter = pred_block->succ_begin();
            while (iter != pred_block->succ_end()) {
              if ((*iter)->getBlockID() < curBlock->getBlockID()) {
                result.push_back(*iter);
              }
              ++iter;
            }
            return result;
          }
        }
      }
    }
    if (curBlock->succ_size() == 1) {
      CFGBlock *succ_block = *(curBlock->succ_begin());
      if (nullptr != succ_block) {
        if (succ_block->getBlockID() > curBlock->getBlockID()) {
          if (nullptr != succ_block->getTerminatorStmt()) {
            if (succ_block->getTerminatorStmt()->getStmtClass() ==
                    Stmt::ForStmtClass ||
                succ_block->getTerminatorStmt()->getStmtClass() ==
                    Stmt::WhileStmtClass ||
                succ_block->getTerminatorStmt()->getStmtClass() ==
                    Stmt::CXXForRangeStmtClass) {
              auto iter = succ_block->succ_begin();
              while (iter != succ_block->succ_end()) {
                if(*iter)
                  if ((*iter)->getBlockID() < curBlock->getBlockID()) {
                  result.push_back(*iter);
                }
                ++iter;
              }
            } else {
             // assert(0 && "There are other loop we not analysis");
            }
          } else {
            while (nullptr == succ_block->getTerminatorStmt() &&
                   succ_block->succ_size() > 0) {
              succ_block = *(succ_block->succ_begin());
            }
            assert(nullptr != succ_block->getTerminatorStmt() &&
                   "Do not find terminator stmt");
            auto iter = succ_block->succ_begin();
            while (iter != succ_block->succ_end()) {
              if ((*iter)->getBlockID() < curBlock->getBlockID()) {
                result.push_back(*iter);
              }
              ++iter;
            }
          }
        } else {
          auto iter = curBlock->succ_begin();
          while (iter != curBlock->succ_end()) {
            result.push_back(*iter);
            ++iter;
          }
        }

      } else {
        return result;
      }
    } else {
      auto iter = curBlock->succ_begin();
      while (iter != curBlock->succ_end()) {
        result.push_back(*iter);
        ++iter;
      }
    }
    return result;
  }

} // end of namespace common

std::string trim(std::string s) {
  std::string result = s;
  result.erase(0, result.find_first_not_of(" \t\r\n"));
  result.erase(result.find_last_not_of(" \t\r\n") + 1);
  return result;
}

std::vector<std::string> initialize(std::string astList) {
  std::vector<std::string> astFiles;

  std::ifstream fin(astList);
  std::string line;
  while (getline(fin, line)) {
    line = trim(line);
    if (line == "")
      continue;
    std::string fileName = line;
    astFiles.push_back(fileName);
  }
  fin.close();

  return astFiles;
}

void common::printLog(std::string logString, common::CheckerName cn, int level,
                      Config &c) {
  auto block = c.getOptionBlock("PrintLog");
  int l = atoi(block.find("level")->second.c_str());
  switch (cn) {
  case common::CheckerName::stack_uaf_checker:
    if (block.find("stack_uaf_checker")->second == "true" && level >= l) {
      llvm::errs() << logString;
    }
    break;
  case common::CheckerName::immediate_uaf_checker:
    if (block.find("immediate_uaf_checker")->second == "true" && level >= l) {
      llvm::errs() << logString;
    }
    break;
  case common::CheckerName::loop_doublefree_checker:
    if (block.find("loop_doublefree_checker")->second == "true" && level >= l) {
      llvm::errs() << logString;
    }
    break;        
  case common::CheckerName::realloc_checker:
    if (block.find("realloc_checker")->second == "true" && level >= l) {
      llvm::errs() << logString;
    }
    break;   
  case common::CheckerName::memory_alloc_checker:
    if (block.find("memory_alloc_checker")->second == "true" && level >= l) {
      llvm::errs() << logString;
    }
    break;       
  case common::CheckerName::borrowed_reference_checker:
    if (block.find("borrowed_reference_checker")->second == "true" && level >= l) {
      llvm::errs() << logString;
    }
    break;     
  }
}

std::string common::print(const Stmt* stmt)
{
    LangOptions L0;
    L0.CPlusPlus=1;
    std::string buffer1;
    llvm::raw_string_ostream strout1(buffer1);
    stmt->printPretty(strout1,nullptr,PrintingPolicy(L0));
    return ""+strout1.str()+"";

}

std::unordered_set<std::string> common::split(std::string text) {
  std::regex ws_re("\\s+"); 
  std::unordered_set<std::string> v(sregex_token_iterator(text.begin(), text.end(), ws_re, -1),sregex_token_iterator());
  return v;
}

void prcoess_bar(float progress) {
  int barWidth = 70;
  std::cout << "[";
  int pos = progress * barWidth;
  for (int i = 0; i < barWidth; ++i) {
    if (i < pos)
      std::cout << "|";
    else
      std::cout << " ";
  }
  std::cout << "] " << int(progress * 100.0) << "%\r";
  std::cout.flush();
}

namespace fix {
bool contains(const Stmt *a, const Stmt *b) {
  if (a == b)
    return true;
  auto s = a->child_begin();
  auto e = a->child_end();
  for (; s != e; s++) {
    if (contains(*s, b))
      return true;
  }
  return false;
}

bool isCharKind(string type) {
  return type.find("wchar_t") == type.npos && type.find("char") != type.npos;
}

string Trim(string s) {
  std::string result = s;
  result.erase(0, result.find_first_not_of(" \t\r\n"));
  result.erase(result.find_last_not_of(" \t\r\n*") + 1);
  return result;
}

std::pair<std::string, std::string> getTypeLenFromStr(std::string str) {
  int i = 0;
  int j = str.length() - 1;
  for (; i < str.length() && str[i] != '['; i++)
    ;
  for (; j >= 0 && str[j] != ']'; j--)
    ;
  if (i >= str.length() || j < 0 || i >= j)
    return make_pair("", "");
  string type = Trim(str.substr(0, i));
  string len = str.substr(i + 1, j - i - 1);
  return make_pair(type, len);
}

pair<string, string> getArrayTypeLen(FunctionDecl *funcDecl, Stmt *pointer) {
  if (!pointer)
    return make_pair("", "");
  if (isa<ImplicitCastExpr>(pointer))
    pointer = *pointer->child_begin();
  if (auto e = dyn_cast<MemberExpr>(pointer)) {
    string str = e->getType().getAsString();
    return getTypeLenFromStr(str);
  }
  int len = funcDecl->getNumParams();
  auto ref = dyn_cast<DeclRefExpr>(pointer);
  if (!ref)
    return make_pair("", "");
  auto res = getTypeLenFromStr(ref->getType().getAsString());
  if (res.first != "" && res.second != "")
    return res;
  for (int i = 0; i < len; i++) {
    auto paramDecl = funcDecl->getParamDecl(i);
    auto decl = ref->getDecl();
    if (i < len - 1 && paramDecl == decl) {
      string type = Trim(paramDecl->getType().getAsString());
      string bufLen = funcDecl->getParamDecl(i + 1)->getQualifiedNameAsString();
      return make_pair(type, bufLen);
    }
  }
  return make_pair("", "");
}

void split(vector<string> &res, string str, string delimiter) {
  unsigned int posBegin = 0;
  int posSeperator = str.find(delimiter);

  while (posSeperator != str.npos) {
    res.push_back(str.substr(posBegin, posSeperator - posBegin));
    posBegin = posSeperator + delimiter.size();
    posSeperator = str.find(delimiter, posBegin);
  }
  if (posBegin != str.length())
    res.push_back(str.substr(posBegin));
}

string getSourceCode(const Stmt *stmt) {
  clang::LangOptions LangOpts;
  LangOpts.CPlusPlus = true;
  clang::PrintingPolicy Policy(LangOpts);
  std::string TypeS;
  llvm::raw_string_ostream s(TypeS);
  stmt->printPretty(s, 0, Policy);
  return s.str();
}

SourceLocation getEnd(SourceManager &sm, const Stmt *expr) {
  clang::LangOptions op;
  return Lexer::getLocForEndOfToken(expr->getEndLoc(), 0, sm, op);
}

Stmt *findChild(Stmt *parent, string child) {
  if (getSourceCode(parent) == child)
    return parent;
  auto s = parent->child_begin();
  auto e = parent->child_end();
  for (; s != e; s++) {
    if (auto res = findChild(*s, child))
      return res;
  }
  return nullptr;
}

std::string BufCalculator::calculateBufLen(string arrayLen, POffset bufOffset,
                                           string type) {
  // string u = type=="char"?"":"*sizeof("+type+")";
  string u = isCharKind(type) ? "" : "*sizeof(" + type + ")";
  if (bufOffset.val == "")
    return arrayLen + u;
  if (bufOffset.needBracket)
    bufOffset.val = "(" + bufOffset.val + ")";
  return arrayLen + u + " - " + bufOffset.val;
}

POffset BufCalculator::calculateExpr(Stmt *stmt) {
  if (auto e = dyn_cast<BinaryOperator>(stmt)) {
    auto L = calculateExpr(e->getLHS());
    auto R = calculateExpr(e->getRHS());
    auto op = e->getOpcodeStr();

    POffset res;
    if (L.val == "") {
      res = L + R;
      res.needBracket = R.needBracket;
      return res;
    } else if (R.val == "") {
      res = L + R;
      res.needBracket = L.needBracket;
      return res;
    }

    res = L + op.str() + R;
    switch (e->getOpcode()) {
    case BO_Add:
    case BO_Sub:
      res.needBracket = true;
      break;
    default:
      res.needBracket = false;
    }

    return res;
  } else if (auto e = dyn_cast<ParenExpr>(stmt)) {
    auto a = calculateExpr(e->IgnoreParens());
    if (a.needBracket)
      a.val = "(" + a.val + ")";
    a.needBracket = false;
    return a;
  } else if (auto e = dyn_cast<MemberExpr>(stmt)) {
    if (getSourceCode(e) == pName)
      return POffset(pType, "");
    return POffset("", getSourceCode(e));
  } else if (auto e = dyn_cast<DeclRefExpr>(stmt)) {
    if (e->getDecl()->getNameAsString() == pName)
      return POffset(pType, "");
    return POffset("", e->getDecl()->getNameAsString());
  } else if (auto e = dyn_cast<ImplicitCastExpr>(stmt)) {
    return calculateExpr(e->getSubExprAsWritten());
  } else if (auto e = dyn_cast<CStyleCastExpr>(stmt)) {
    string destType = Trim(e->getTypeAsWritten().getAsString());
    if (destType == "void")
      destType = "char";
    auto res = calculateExpr(e->getSubExprAsWritten());
    res.type = destType;

    return res;
  } else {
    for (auto iter = stmt->child_begin(); iter != stmt->child_end(); iter++)
      return calculateExpr(*iter);
    return POffset("", getSourceCode(stmt));
  }
}

POffset POffset::operator+(const POffset &s) {
  // i + j
  if (this->type == "" && s.type == "")
    return POffset("", this->val + s.val);

  // p + i
  else if (s.type == "") {
    string r;
    if (isCharKind(this->type))
      r = s.val;
    else
      r = s.val + "*sizeof(" + this->type + ")";

    return POffset(this->type, this->val + r);
  }
  // i + p
  else {
    string l;
    if (isCharKind(s.type))
      l = this->val;
    else
      l = this->val + "*sizeof(" + s.type + ")";

    return POffset(s.type, s.val + l);
  }
}

POffset POffset::operator+(const string &s) {
  if (this->val == "")
    return POffset(this->type, "");
  return POffset(this->type, this->val + s);
}
} // namespace fix
