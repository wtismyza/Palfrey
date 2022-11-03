#ifndef SAFE_HW_INTRAPROCEDURE_INTERPROCEDURE_CFG_H
#define SAFE_HW_INTRAPROCEDURE_INTERPROCEDURE_CFG_H

// #include "CFG/CFG.h"
#include <clang/Analysis/CFG.h>
#include "framework/ASTManager.h"
#include "framework/CallGraph.h"

#include <unordered_map>
#include <vector>

class InterProcedureBlockNode {
public:
  const ASTFunction *F;
  unsigned blockID;

  InterProcedureBlockNode(const ASTFunction *astF, unsigned id)
      : F(astF), blockID(id) {}
};

class InterProcedureCFGNode {
public:
  void addPredecessor(InterProcedureBlockNode *pred) {
    predecessor.push_back(pred);
  }

  void addSuccessor(InterProcedureBlockNode *succ) {
    successor.push_back(succ);
  }

  std::vector<InterProcedureBlockNode *> getPred() { return predecessor; }

  std::vector<InterProcedureBlockNode *> getSucc() { return successor; }

private:
  std::vector<InterProcedureBlockNode *> predecessor;
  std::vector<InterProcedureBlockNode *> successor;
};

class InterProcedureCFG {
public:
  InterProcedureCFG(ASTManager *manager, const ASTResource *resource,
                    CallGraph *callgraph, Config *configure) {
    callGraph = callgraph;
    astManager = manager;
    nonRecursiveCallGraph = new NonRecursiveCallGraph(callgraph, resource);

    auto cfgOption = configure->getOptionBlock("CFG");
    assert(cfgOption.find("SplitBasicBlockwithFunCall")->second == "true");

    for (const ASTFunction *F : resource->getFunctions()) {

      FunctionDecl *FD = manager->getFunctionDecl(const_cast<ASTFunction *>(F));

      std::unique_ptr<CFG> &functionCFG =
          manager->getCFG(const_cast<ASTFunction *>(F));

      if (nullptr == functionCFG || !FD->hasBody())
        continue;

      for (CFG::iterator cfg_it = functionCFG->begin(),
                         cfg_it_e = functionCFG->end();
           cfg_it != cfg_it_e; cfg_it++) {
        CFGBlock *block = *cfg_it;
        if (block->empty()) {
          continue;
        }
        CFGElement element = block->back();

        // CFGBlock::reverse_iterator block_it = block->rbegin();
        if (element.getKind() == CFGStmt::Statement) {
          const Stmt *stmt = element.getAs<CFGStmt>()->getStmt();
          if (const CallExpr *call = dyn_cast<CallExpr>(stmt)) {
            const FunctionDecl *callee = call->getDirectCallee();
            if (nullptr != callee && callee->hasBody()) {

              ASTFunction *calleeF =
                  callgraph->getFunction(const_cast<FunctionDecl *>(callee));
              if (nullptr == calleeF) {
                continue;
              }
              std::unique_ptr<CFG> &calleeCFG = manager->getCFG(calleeF);
              if (nullptr == calleeCFG)
                continue;
              CFGBlock &entry = calleeCFG->getEntry();
              CFGBlock &exit = calleeCFG->getExit();

              InterProcedureBlockNode *blockNode =
                  new InterProcedureBlockNode(F, block->getBlockID());

              InterProcedureBlockNode *entryNode =
                  new InterProcedureBlockNode(calleeF, entry.getBlockID());

              InterProcedureBlockNode *exitNode =
                  new InterProcedureBlockNode(calleeF, exit.getBlockID());

              // add succ for block
              if (ICFG.find(F) != ICFG.end()) {
                if (ICFG[F].find(block->getBlockID()) != ICFG[F].end())
                  ICFG[F][block->getBlockID()].addSuccessor(entryNode);
                else {
                  InterProcedureCFGNode ICFGNode;
                  ICFGNode.addSuccessor(entryNode);
                  ICFG[F].insert(std::make_pair(block->getBlockID(), ICFGNode));
                }
              } else {
                InterProcedureCFGNode ICFGNode;
                ICFGNode.addSuccessor(entryNode);
                std::unordered_map<unsigned, InterProcedureCFGNode> tem;
                tem[block->getBlockID()] = ICFGNode;
                ICFG[F] = tem;
              }

              // add pred for entry
              if (ICFG.find(calleeF) != ICFG.end()) {
                if (ICFG[calleeF].find(entry.getBlockID()) !=
                    ICFG[calleeF].end())
                  ICFG[calleeF][entry.getBlockID()].addPredecessor(blockNode);
                else {
                  InterProcedureCFGNode ICFGNode;
                  ICFGNode.addPredecessor(blockNode);
                  ICFG[calleeF].insert(
                      std::make_pair(entry.getBlockID(), ICFGNode));
                }
              } else {
                InterProcedureCFGNode ICFGNode;
                ICFGNode.addPredecessor(blockNode);
                std::unordered_map<unsigned, InterProcedureCFGNode> tem;
                tem[entry.getBlockID()] = ICFGNode;
                ICFG[calleeF] = tem;
              }

              CFGBlock *intraProcedureSucc = *(block->succ_begin());
              InterProcedureBlockNode *intraProcedureSucckNode =
                  new InterProcedureBlockNode(F,
                                              intraProcedureSucc->getBlockID());

              // add pred for intraProcedureSucc
              if (ICFG.find(F) != ICFG.end()) {
                if (ICFG[F].find(intraProcedureSucc->getBlockID()) !=
                    ICFG[F].end())
                  ICFG[F][intraProcedureSucc->getBlockID()].addPredecessor(
                      exitNode);
                else {
                  InterProcedureCFGNode ICFGNode;
                  ICFGNode.addPredecessor(exitNode);
                  ICFG[F].insert(std::make_pair(
                      intraProcedureSucc->getBlockID(), ICFGNode));
                }
              } else {
                InterProcedureCFGNode ICFGNode;
                ICFGNode.addPredecessor(exitNode);
                std::unordered_map<unsigned, InterProcedureCFGNode> tem;
                tem[intraProcedureSucc->getBlockID()] = ICFGNode;
                ICFG[F] = tem;
              }

              // add succ for exit
              if (ICFG.find(calleeF) != ICFG.end()) {
                if (ICFG[calleeF].find(exit.getBlockID()) !=
                    ICFG[calleeF].end())
                  ICFG[calleeF][exit.getBlockID()].addSuccessor(
                      intraProcedureSucckNode);
                else {
                  InterProcedureCFGNode ICFGNode;
                  ICFGNode.addSuccessor(intraProcedureSucckNode);
                  ICFG[calleeF].insert(
                      std::make_pair(exit.getBlockID(), ICFGNode));
                }
              } else {
                InterProcedureCFGNode ICFGNode;
                ICFGNode.addSuccessor(intraProcedureSucckNode);
                std::unordered_map<unsigned, InterProcedureCFGNode> tem;
                tem[exit.getBlockID()] = ICFGNode;
                ICFG[calleeF] = tem;
              }

              // std::cout<<"the entry is: "<<std::endl;
              // entry.dump();
            }
          }
        }
      }
    }
  }

  std::vector<CFGBlock *> getPred(CFGBlock *block,
                                  bool isNoneRecursive = false) {
    unsigned blockID = block->getBlockID();
    // const Decl *D = block->getParent()->getParentDecl();
    const Decl *D = astManager->parentDecls[block->getParent()];
    const FunctionDecl *FD = dyn_cast<const FunctionDecl>(D);
    ASTFunction *F = astManager->getASTFunction(const_cast<FunctionDecl *>(FD));
    if (ICFG.find(F) != ICFG.end() && ICFG[F].find(blockID) != ICFG[F].end() &&
        ICFG[F][blockID].getPred().size() != 0) {
      std::vector<CFGBlock *> result;
      auto p = ICFG[F][blockID].getPred();

      auto noneRecursivePred = nonRecursiveCallGraph->getParents(F);
      auto noneRecursiveSucc = nonRecursiveCallGraph->getChildren(F);
      for (auto it = p.begin(); it != p.end(); ++it) {
        InterProcedureBlockNode *ipbn = *it;

        if (isNoneRecursive) {
          if (block->getParent()->getEntry().getBlockID() == blockID) {
            if (noneRecursivePred.find(const_cast<ASTFunction *>(ipbn->F)) ==
                noneRecursivePred.end())
              continue;
          } else {
            if (noneRecursiveSucc.find(const_cast<ASTFunction *>(ipbn->F)) ==
                noneRecursiveSucc.end())
              continue;
          }
        }

        std::unique_ptr<CFG> &CFG =
            astManager->getCFG(const_cast<ASTFunction *>(ipbn->F));
        result.push_back(
            // const_cast<CFGBlock *>(CFG->getBlockWithID(ipbn->blockID)));
            const_cast<CFGBlock *>(common::getBlockWithID(CFG,ipbn->blockID)));
      }
      if (result.size() == 0) {
        for (auto it = block->pred_begin(); it != block->pred_end(); ++it) {
          result.push_back(*it);
        }
      }
      return result;
    } else {
      std::vector<CFGBlock *> result;
      for (auto it = block->pred_begin(); it != block->pred_end(); ++it) {
        result.push_back(*it);
      }
      return result;
    }
  }

  std::vector<CFGBlock *> getSucc(CFGBlock *block,
                                  bool isNoneRecursive = false) {
    unsigned blockID = block->getBlockID();
    // const Decl *D = block->getParent()->getParentDecl();
    const Decl *D = astManager->parentDecls[block->getParent()];
    const FunctionDecl *FD = dyn_cast<const FunctionDecl>(D);
    ASTFunction *F = astManager->getASTFunction(const_cast<FunctionDecl *>(FD));
    if (ICFG.find(F) != ICFG.end() && ICFG[F].find(blockID) != ICFG[F].end() &&
        ICFG[F][blockID].getSucc().size() != 0) {
      std::vector<CFGBlock *> result;
      auto p = ICFG[F][blockID].getSucc();

      auto noneRecursivePred = nonRecursiveCallGraph->getParents(F);
      auto noneRecursiveSucc = nonRecursiveCallGraph->getChildren(F);
      for (auto it = p.begin(); it != p.end(); ++it) {
        InterProcedureBlockNode *ipbn = *it;

        if (isNoneRecursive) {
          if (block->getParent()->getExit().getBlockID() == blockID) {
            if (noneRecursivePred.find(const_cast<ASTFunction *>(ipbn->F)) ==
                noneRecursivePred.end())
              continue;
          } else {
            if (noneRecursiveSucc.find(const_cast<ASTFunction *>(ipbn->F)) ==
                noneRecursiveSucc.end())
              continue;
          }
        }

        std::unique_ptr<CFG> &CFG =
            astManager->getCFG(const_cast<ASTFunction *>(ipbn->F));
        result.push_back(
            // const_cast<CFGBlock *>(CFG->getBlockWithID(ipbn->blockID)));
            const_cast<CFGBlock *>(common::getBlockWithID(CFG,ipbn->blockID)));
      }
      if (result.size() == 0) {
        for (auto it = block->succ_begin(); it != block->succ_end(); ++it) {
          result.push_back(*it);
        }
      }
      return result;
    } else {
      std::vector<CFGBlock *> result;
      for (auto it = block->succ_begin(); it != block->succ_end(); ++it) {
        result.push_back(*it);
      }
      return result;
    }
  }

  // std::vector< std::vector<CFGBlock* > > getPath(CFGBlock *pathStart,
  // CFGBlock *pathEnd) {
  //   assert(nullptr == callGraph);

  //   const FunctionDecl* startFuncDecl =
  //   pathStart->getParent()->getParentDecl(); assert(nullptr ==
  //   startFuncDecl); ASTFunction* startASTFunc =
  //   callGraph->getFunction(const_cast<FunctionDecl*>(startFuncDecl));
  //   assert(nullptr == startASTFunc);

  //   const FunctionDecl* endFuncDecl = pathEnd->getParent()->getParentDecl();
  //   assert(nullptr == endFuncDecl);
  //   ASTFunction* endASTFunc =
  //   callGraph->getFunction(const_cast<FunctionDecl*>(endFuncDecl));
  //   assert(nullptr == endASTFunc);

  //   std::vector< std::vector<ASTFunction *> > reachablePathInCG =
  //       nonRecursiveCallGraph->getReachablePath(startASTFunc, endASTFunc);

  //   // auto iter = reachablePathInCG.begin();
  //   // while (iter != reachablePathInCG.end()) {
  //   //   std::cout<<"the first path: "<<std::endl;
  //   //   auto iter2 = (*iter).begin();
  //   //   while ( iter2 != (*iter).end() ) {
  //   //     std::cout<<(*iter2)->getFullName()<<" ";
  //   //     iter2++;
  //   //   }
  //   //   std::cout<<std::endl;
  //   //   iter++;
  //   // }
  //   // std::cout<<"find path done!!!"<<std::endl;

  //   std::vector< std::vector<CFGBlock* > > result;
  //   if (reachablePathInCG.size() == 0) {
  //     return result;
  //   }
  //   else if (reachablePathInCG.size() == 1) {
  //     std::unique_ptr<CFG>& cfg = astManager->getCFG(startASTFunc);
  //     return cfg->getReachablePath(pathStart, pathEnd);
  //   }
  //   else {
  //     auto pathIter1 = reachablePathInCG.begin();
  //     auto pathIter2 = reachablePathInCG.begin() + 1;
  //     while ( pathIter2 != reachablePathInCG.end() ) {

  //       ++pathIter1;
  //       ++pathIter2;
  //     }
  //   }
  //   return result;
  // }

  std::vector<std::list<const CFGBlock *>>
  getPathBeforePointWithLength(unsigned pathLen, CFGBlock *pathEnd) {
    std::vector<std::list<const CFGBlock *>> res;
    std::list<const CFGBlock *> currentPath;
    if (pathEnd == nullptr)
      return res;
    currentPath.push_front(pathEnd);
    auto pred_blocks = getPred(pathEnd);
    if (pred_blocks.size() == 0) {
      res.push_back(currentPath);
      return res;
    }
    for (auto it = pred_blocks.begin(), itend = pred_blocks.end(); it != itend;
         ++it) {
      getSinglePathwithDFS(res, currentPath, --pathLen, *it);
    }
    return res;
  }

  void getSinglePathwithDFS(std::vector<std::list<const CFGBlock *>> &res,
                            std::list<const CFGBlock *> currentPath,
                            unsigned len, CFGBlock *currentBlock) {
    if (len <= 0 || currentBlock == nullptr) {
      res.push_back(currentPath);
      return;
    }

    // currentPath.insert(currentPath.begin(), currentBlock);
    currentPath.push_front(currentBlock);

    auto pred_blocks = getPred(currentBlock);
    if (pred_blocks.size() == 0) {
      res.push_back(currentPath);
      return;
    }

    // TODO: there may be a path explore because we do not handle the loop
    for (auto it = pred_blocks.begin(), itend = pred_blocks.end(); it != itend;
         ++it) {
      getSinglePathwithDFS(res, currentPath, len - 1, *it);
    }
  }

  CFGBlock *getCFGBlock(InterProcedureBlockNode *ICFGBblockNode) {
    std::unique_ptr<CFG> &CFG =
        astManager->getCFG(const_cast<ASTFunction *>(ICFGBblockNode->F));
    // return const_cast<CFGBlock *>(CFG->getBlockWithID(ICFGBblockNode->blockID));
    return const_cast<CFGBlock *>(common::getBlockWithID(CFG,ICFGBblockNode->blockID));
  }

private:
  std::unordered_map<const ASTFunction *,
                     std::unordered_map<unsigned, InterProcedureCFGNode>>
      ICFG;
  CallGraph *callGraph;
  ASTManager *astManager;
  NonRecursiveCallGraph *nonRecursiveCallGraph;
};
#endif // SAFE_HW_INTRAPROCEDURE_INTERPROCEDURE_CFG_H