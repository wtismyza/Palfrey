#ifndef POINTER_TO_ANALYSIS_H
#define POINTER_TO_ANALYSIS_H

#include "CFG/InterProcedureCFG.h"
#include "Node.h"
#include "PTAVar.h"
#include <clang/AST/RecursiveASTVisitor.h>

#include "framework/ASTManager.h"
#include "framework/Common.h"

#include <list>
#include <string>
#include <unordered_map>
#include <vector>

class PointToAnalysis {
public:
  enum Flag {
    _Flow_Sensitive = 0x01,
    _Path_Sensitive = 0x02,
  };

  PointToAnalysis(ASTManager *astmanager, InterProcedureCFG *interProcedureCFG,
                  std::unordered_map<std::string, std::string> configure)
      : config(configure), manager(astmanager), _ptm(), _pvm(_ptm),
        _interProcedureCFG(interProcedureCFG) {
    funcNums = manager->getFunctions().size();

    memoryMallocFunc.insert("malloc");
    memoryMallocFunc.insert("calloc");
    memoryMallocFunc.insert("realloc");
    memoryMallocFunc.insert("new");

    manager->setMaxSize(100);

    std::string green = "\033[32m";
    std::string close = "\033[0m";
    std::cout << green << "[+] there are " << funcNums << " functions at all!"
              << close << std::endl;
  }
  ~PointToAnalysis() {
    for (auto fn : _func_nodes) {
      delete fn.second;
    }
  }

  // Analysis range is [Entry.1, Exit.elem_id].
  // [Entry]
  //    1:
  //    2:
  //    ...
  // [Bi]
  //    1:
  //    2:
  //    ...
  // ...
  // [Bj]
  //    1:
  //    2:
  //    ...
  // [Exit]
  //    1:
  //    2:
  //    ...
  //    elem_id:
  //    ...
  //
  // Level1: Field-, Context-(Object-)sensitive
  // Level2: Field-, Context-(Object-), Flow-sensitive
  // Level3: Field-, Context-(Object-), Flow-, Path-sensitive
  std::set<const PTAVar *> get_pointee_of_l1(CFGBlock *entry, CFGBlock *exit,
                                             unsigned elem_id,
                                             const PTAVar *var);
  std::set<const PTAVar *> get_pointee_of_l2(CFGBlock *entry, CFGBlock *exit,
                                             unsigned elem_id,
                                             const PTAVar *var);
  std::set<const PTAVar *> get_pointee_of_l3(std::list<const CFGBlock *> path,
                                             unsigned elem_id,
                                             const PTAVar *var, bool isForAliasAnalysis = false);

  std::set<const PTAVar *> get_pointee_at_point(CFGBlock *pathEnd,
                                                unsigned elem_id,
                                                const PTAVar *var,
                                                unsigned pathLen = 10);
  bool is_alias_of(std::list<const CFGBlock *> path_1, unsigned elem_id_1,
                   const PTAVar *var_1, std::list<const CFGBlock *> path_2,
                   unsigned elem_id_2, const PTAVar *var_2);

  std::set<const PTAVar *> get_alias_in_func(ASTFunction* F,
                                             const PTAVar *var, int pathLen=10, int pathnum=10);

  bool is_alias_of_at_point(CFGBlock *pathEnd, unsigned elem_id,
                            const PTAVar *var_1, const PTAVar *var_2,
                            int pathLen);

  // // before block[id].elem[id]
  // std::set<unsigned> get_pointee_of(std::string file_name, unsigned var_id,
  //                                   unsigned func_id, unsigned block_id,
  //                                   unsigned elem_id,
  //                                   unsigned flags = _Flow_Sensitive);
  // // before block[id].elem[id]
  // std::set<unsigned> get_pointee_of(std::string file_name, unsigned var_id,
  //                                   unsigned func_id, std::list<unsigned>
  //                                   path, unsigned elem_id, unsigned flags =
  //                                   _Flow_Sensitive);
  // // before block[id].elem[id]
  // std::set<unsigned> get_alias_of(std::string file_name, unsigned var_id,
  //                                 unsigned func_id, unsigned block_id,
  //                                 unsigned elem_id,
  //                                 unsigned flags = _Flow_Sensitive);

  void ConstructPartialCFG();
  std::string getPTAVarKey(VarDecl *vd);

  void dump();

  PTAVarMap &get_pvm() { return _pvm; };
  const PTAVar *getPTAVar(std::string key) { return _pvm.getPTAVar(key); }

private:
  // Pointer VarDecl outside function and no-pointer VarDecl
  void HandleVarDecl(const VarDecl *var_decl, bool pointer_only = true);
  // Pointer VarDecl inside function
  void HandleVarDecl(const VarDecl *var_decl, unsigned func_id,
                     unsigned block_id, unsigned elem_id);

  void HandleFuncDecl(ASTFunction *F);

  void HandleStmt(const Stmt *stmt, CFGBlock *block, unsigned func_id,
                  unsigned block_id, unsigned elem_id);
  void HandleAssignStmt(const BinaryOperator *stmt, unsigned func_id,
                        unsigned block_id, unsigned elem_id);
  void HandleExpr(const Expr *expr, PointerUpdateNode *update_node);

  std::list<std::string> TraverseMemberExpr(MemberExpr *memberExpr);

  std::vector<std::list<const CFGBlock *>> getPathInFunc(CFGBlock *startBlock,
                                                         CFGBlock *endBlock, int pathlen, int pathnum);
  void traverseCFGForGetReachablePath(
      std::vector<std::list<const CFGBlock *>> &reachablePath,
      CFGBlock *startBlock, CFGBlock *endBlock,
      std::list<const CFGBlock *> path, int pathlen, int pathnum);

  // std::unordered_map<std::string, FileNode *> _file_nodes;
  std::unordered_map<unsigned, FunctionNode *> _func_nodes;
  PTATypeMap _ptm;
  PTAVarMap _pvm;
  InterProcedureCFG *_interProcedureCFG;
  ASTManager *manager;
  BToBMap _blockToBlockMap;
  CallerInfo _callerInfo;
  const std::unordered_map<std::string, std::string> config;

  unsigned funcNums;
  std::set<std::string> memoryMallocFunc;

  std::list<std::string> getInstanceInPath(std::list<const CFGBlock *> path);
};

class FindVariable : public RecursiveASTVisitor<FindVariable> {
private:
  std::set<ValueDecl *> variables;
  std::string varType;

public:
  FindVariable(std::string type) { varType = type; }
  bool VisitDeclRefExpr(DeclRefExpr *dre) {
    if (dre) {
      if (dre->getDecl()->getType().getAsString() == varType) {
        variables.insert(dre->getDecl());
      }
    }
    return true;
  }
  bool VisitDeclStmt(DeclStmt *S) {
    for (auto D = S->decl_begin(); D != S->decl_end(); D++) {
      if (VarDecl *VD = dyn_cast<VarDecl>(*D)) {
        if (VD->getType().getAsString() == varType)
          variables.insert(VD);
      }
    }
    return true;
  }
  std::set<ValueDecl *> &getVar() { return variables; }
};

#endif // POINTER_TO_ANALYSIS_H
