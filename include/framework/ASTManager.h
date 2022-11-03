#ifndef AST_MANAGER_H
#define AST_MANAGER_H

#include <list>
#include <unordered_map>

// #include <clang/Analysis/CFG.h>
#include <clang/Frontend/ASTUnit.h>

#include "ASTElement.h"
#include "Config.h"

// #include "../CFG/CFG.h"
#include <clang/Analysis/CFG.h>

using namespace clang;

class ASTManager;
/**
 * the resource of AST.
 * contains AST, function, function-local variables, global variables.
 */
class ASTResource {
public:
  ~ASTResource();

  const std::vector<ASTFunction *> &getFunctions(bool use = true) const;
  std::vector<ASTFile *> getASTFiles() const;
  std::vector<ASTGlobalVariable *> &getGlabalVars() { return ASTGlobalVars; };

  friend class ASTManager;

private:
  std::unordered_map<std::string, ASTFile *> ASTs;

  std::vector<ASTFunction *> ASTFunctions;
  std::vector<ASTFunction *> useASTFunctions;
  std::vector<ASTGlobalVariable *> ASTGlobalVars;
  std::vector<ASTVariable *> ASTVariables;

  void buildUseFunctions();

  ASTFile *addASTFile(std::string AST);
  ASTFunction *addASTFunction(FunctionDecl *FD, ASTFile *AF, bool use = true);
  ASTFunction *addLambdaASTFunction(FunctionDecl *FD, ASTFile *AF,
                                    std::string fullName, bool use = true);
  ASTGlobalVariable *addASTGlobalVar(VarDecl *VD, ASTFile *AF);
  ASTVariable *addASTVariable(VarDecl *VD, ASTFunction *F);
};

/**
 * a bidirectional map.
 * You can get a pointer from an id or get an id from a pointer.
 */
class ASTBimap {
public:
  friend class ASTManager;

private:
  void insertFunction(ASTFunction *F, FunctionDecl *FD);
  void insertVariable(ASTVariable *V, VarDecl *VD);
  void insertGlobalVariable(ASTGlobalVariable *V, VarDecl *VD);

  FunctionDecl *getFunctionDecl(ASTFunction *F);
  ASTVariable *getASTVariable(VarDecl *VD);
  VarDecl *getVarDecl(ASTVariable *V);
  ASTGlobalVariable *getASTGlobalVariable(VarDecl *VD);
  VarDecl *getGlobalVarDecl(ASTGlobalVariable *GV);

  void removeFunction(ASTFunction *F);
  void removeVariable(ASTVariable *V);
  void removeGlobalVariable(ASTGlobalVariable *GV);

  std::unordered_map<ASTFunction *, FunctionDecl *> functionMap;
  std::unordered_map<ASTVariable *, VarDecl *> variableLeft;
  std::unordered_map<VarDecl *, ASTVariable *> variableRight;
  std::unordered_map<ASTGlobalVariable *, VarDecl *> astgv2gvd;
  std::unordered_map<VarDecl *, ASTGlobalVariable *> gvd2astgv;
};

class FunctionLoc {
public:
  FunctionDecl *FD;
  std::string fileName;
  int beginLoc;
  int endLoc;
  bool operator<(const FunctionLoc &a) const { return a.beginLoc < beginLoc; }

  FunctionLoc(FunctionDecl *D, std::string name, int begin, int end)
      : FD(D), fileName(name), beginLoc(begin), endLoc(end) {}
};
/**
 * a class that manages all ASTs.
 */
class ASTManager {
public:
  ASTManager(std::vector<std::string> &ASTs, ASTResource &resource,
             Config &configure);

  ASTUnit *getASTUnit(ASTFile *AF);
  FunctionDecl *getFunctionDecl(ASTFunction *F);
  ASTFunction *getASTFunction(FunctionDecl *FD);
  std::vector<ASTFunction *> getFunctions(bool use = true);

  ASTGlobalVariable *getASTGlobalVariable(VarDecl *GVD);
  VarDecl *getGlobalVarDecl(ASTGlobalVariable *GV);
  std::vector<ASTGlobalVariable *> getGlobalVars(bool uninit = false);

  ASTVariable *getASTVariable(VarDecl *VD);
  VarDecl *getVarDecl(ASTVariable *V);

  std::unique_ptr<CFG> &getCFG(ASTFunction *F);
  std::vector<ASTFunction *> getASTFunction(const std::string &funcName);

  void insertFunction(ASTFunction *F, FunctionDecl *FD);

  std::map<std::string, std::set<FunctionLoc>> funcLocInfo;
  void saveFuncLocInfo(FunctionLoc FL);
  CFGBlock *getBlockWithLoc(std::string fileName, int line);
  Stmt *getStmtWithLoc(std::string fileName, int line);
  std::vector<CFGBlock *> getBlocksWithLoc(std::string fileName, int line);
  std::vector<std::pair<CFGBlock *, Stmt *>>
  getCandidatePair(std::string fileName, int line);
  std::vector<Stmt *> getStmtWithLoc(int line, CFGBlock *block);

  void setMaxSize(unsigned size);

  std::unordered_map<std::string, ASTUnit *> &getASTs();
  std::unordered_map<CFG *, Decl *> parentDecls;
private:
  ASTResource &resource;
  Config &c;

  ASTBimap bimap;
  std::unordered_map<std::string, ASTUnit *> ASTs;
  std::unordered_map<ASTFunction *, std::unique_ptr<CFG>> CFGs;
  

  unsigned max_size;
  std::list<std::unique_ptr<ASTUnit>> ASTQueue;

  void pop();
  void move(ASTUnit *AU);
  void push(std::unique_ptr<ASTUnit> AU);

  void loadASTUnit(std::unique_ptr<ASTUnit> AU);
};

#endif