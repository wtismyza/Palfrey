#ifndef CALL_GRAPH_H
#define CALL_GRAPH_H

#include <queue>
#include <unordered_map>

#include "ASTManager.h"

using std::queue;
using CallInfo = std::vector<std::pair<ASTFunction *, int64_t>>;
// using CallInfo = std::vector<std::pair<ASTFunction *, int64_t>>;
class CallGraphNode {
public:
  CallGraphNode(ASTFunction *F) { this->F = F; }
  ~CallGraphNode() {
    parents.clear();
    children.clear();
    parentsWithCallsite.clear();
    childrenWithCallsite.clear();
  }

  void addParent(ASTFunction *F) { parents.push_back(F); }

  void addChild(ASTFunction *F) { children.push_back(F); }

  ASTFunction *getFunction() const { return F; }
  //获取其caller
  const std::vector<ASTFunction *> &getParents() const { return parents; }
  //获取其callee
  const std::vector<ASTFunction *> &getChildren() const { return children; }
  //获取其caller的同时获取call site的ID
  const CallInfo &getParentsWithCallsite() const { return parentsWithCallsite; }
  //获取其callee的同时获取call site的ID
  const CallInfo &getChildrenWithCallsite() const {
    return childrenWithCallsite;
  }

  void addParentWithCallSite(ASTFunction *F, int64_t CS) {
    auto callElement = std::make_pair(F, CS);
    parentsWithCallsite.push_back(callElement);
  }

  void addChildWithCallSite(ASTFunction *F, int64_t CS) {
    auto callElement = std::make_pair(F, CS);
    childrenWithCallsite.push_back(callElement);
  }
  // for test only, this function should be delete in later version
  void dumpTest(ASTManager &manager);
  /* @brief      输出该节点的信息
     @param out  输出流
  */
  void printCGNode(std::ostream &os);

private:
  friend class NonRecursiveCallGraph;
  ASTFunction *F;

  std::vector<ASTFunction *> parents;
  std::vector<ASTFunction *> children;
  CallInfo parentsWithCallsite;
  CallInfo childrenWithCallsite;
};

class CallGraph {
public:
  // for test only, this function should be delete in later version
  void dumpTest();
  // CallGraph(ASTManager &manager, const ASTResource &resource);
  ~CallGraph();
  CallGraph(ASTManager &manager, ASTResource &resource,
            const std::unordered_map<std::string, std::string> &configure);

  const std::vector<ASTFunction *> &getTopLevelFunctions() const;

  ASTFunction *getFunction(FunctionDecl *FD) const;

  std::vector<ASTFunction *> getParents(ASTFunction *F) const;
  std::vector<ASTFunction *> getChildren(ASTFunction *F) const;
  CallInfo getParentsWithCallsite(ASTFunction *F) const;
  CallInfo getChildrenWithCallsite(ASTFunction *F) const;
  /* @brief      以 .dot文件的形式输出函数调用图
     @param out  输出流
  */
  void writeDotFile(std::ostream &out);
  /* @brief      在控制台中输出函数调用图
     @param out  输出流
  */
  void printCallGraph(std::ostream &out);
  /* @brief      往 .dot文件中写入CGNode信息
     @param out  输出流
  */
  void writeNodeDot(std::ostream &out, CallGraphNode *node);

  CallGraphNode *getNode(ASTFunction *f);

protected:
  std::unordered_map<std::string, CallGraphNode *> nodes;
  std::vector<ASTFunction *> topLevelFunctions;
  //防止递归调用导致拓扑序永远不结束
  std::unordered_map<CallGraphNode *, bool> visitInAll;
  std::unordered_map<CallGraphNode *, bool> visitInFunction;
  //函数指针的信息。表示ID表示的某一函数指针可能指向哪些函数。
  std::unordered_map<std::string, std::set<std::string>> mayPointTo;

  const std::unordered_map<std::string, std::string> &config;
  ASTManager &astManager;

  // judge if F will be included in call graph or not
  bool willIncludeInGraph(ASTFunction *F, FunctionDecl *FD);
  CallGraphNode *getOrInsertNode(ASTFunction *F);

  void removeNoCalledSystemHeader(ASTFunction *F);

  void Init() {
    mayPointTo.clear();
    nodes.clear();
    topLevelFunctions.clear();
    visitInAll.clear();
    visitInFunction.clear();
  }

private:
  queue<CallGraphNode *> topOrderQueue;

  void constructCallGraphInTopOrder(ASTFunction *in);
  void addEdge(CallGraphNode *parent, CallGraphNode *callee, int64_t callsite);
  //处理一个CallGraphNode的callee
  void processCallee(CallGraphNode *parent);
  //处理函数指针信息
  void processFunctionPtrInTopOrder(CallGraphNode *inFunction);
};

class NonRecursiveCallGraphNode {

public:
  NonRecursiveCallGraphNode() {}

  NonRecursiveCallGraphNode(ASTFunction *F) { this->F = F; }

  ASTFunction *getFunction() const { return F; }

  const std::set<ASTFunction *> &getParents() const { return parents; }

  const std::set<ASTFunction *> &getChildren() const { return children; }

  void dump(std::ostream &out);

  friend class NonRecursiveCallGraph;

private:
  ASTFunction *F;
  std::set<ASTFunction *> parents;
  std::set<ASTFunction *> children;
};

class NonRecursiveCallGraph {

public:
  NonRecursiveCallGraph(CallGraph *call_graph, const ASTResource *resource);

  const std::set<ASTFunction *> &getParents(ASTFunction *F) const {
    return nodes.at(F).getParents();
  }
  const std::set<ASTFunction *> &getChildren(ASTFunction *F) const {
    return nodes.at(F).getChildren();
  }

  void dump(std::ostream &out);

  std::vector<std::vector<ASTFunction *>>
  getReachablePath(ASTFunction *startFunc, ASTFunction *endFunc);

private:
  void spanningTree(ASTFunction *F,
                    std::unordered_map<ASTFunction *, int> &colors);

  std::unordered_map<ASTFunction *, NonRecursiveCallGraphNode> nodes;
  std::vector<ASTFunction *> topLevelFunctions;

  std::vector<std::pair<ASTFunction *, ASTFunction *>> removeEdges;

  std::vector<std::vector<ASTFunction *>> reachablePath;

  void traverseNonRecursiveCGForReachablePath(ASTFunction *startFunc,
                                              ASTFunction *endFunc,
                                              std::vector<ASTFunction *> path);
};

#endif