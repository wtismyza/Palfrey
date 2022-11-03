#include "framework/CallGraph.h"

#include <ctime>
#include <iostream>

void CallGraph::addEdge(CallGraphNode *parent, CallGraphNode *callee,
                        int64_t callsite) {
  //避免一个函数调用某个函数多次时，其parent或children有重复情况
  if (!visitInFunction[callee]) {
    visitInFunction[callee] = true;
    parent->addChild(callee->getFunction());
    callee->addParent(parent->getFunction());
  }

  callee->addParentWithCallSite(parent->getFunction(), callsite);
  parent->addChildWithCallSite(callee->getFunction(), callsite);
}

void CallGraph::processCallee(CallGraphNode *parent) {

  FunctionDecl *FD = astManager.getFunctionDecl(parent->getFunction());
  if (!FD->hasBody()) {
    return;
  }

  visitInFunction.clear();
  const std::vector<std::pair<std::string, int64_t>> &calledFunctionsWithCS =
      common::getCalledFunctionsInfo(FD, config);
  // 处理其调用的函数
  // 不以FD作为处理依据，以fullname作为处理依据
  // fullname->cgnode->astfunction->fd，保证重新load也不会导致CG失效
  for (const auto &calledInfo : calledFunctionsWithCS) {
    std::string calledFullName = calledInfo.first;
    auto it = nodes.find(calledFullName);

    if (it != nodes.end()) {
      CallGraphNode *callee = nodes[calledFullName];

      addEdge(parent, callee, calledInfo.second);
    }
  }
}

void CallGraph::processFunctionPtrInTopOrder(CallGraphNode *inFunction) {

  mayPointTo.clear();
  visitInAll.clear();
  visitInAll[inFunction] = true;
  topOrderQueue.push(inFunction);
  //前者为call site的ID，后者为可能调用的函数
  using FuncPtrWithCS = std::vector<std::pair<int64_t, std::set<std::string>>>;

  while (!topOrderQueue.empty()) {
    CallGraphNode *parent = topOrderQueue.front();
    topOrderQueue.pop();
    visitInFunction.clear();

    FunctionDecl *FD = astManager.getFunctionDecl(parent->getFunction());
    if (!FD->hasBody()) {
      continue;
    }
    //处理parent的函数指针信息
    //前者为call site的ID，后者为可能指向的函数
    FuncPtrWithCS calledPtrWithCS =
        common::getFunctionPtrWithCS(FD, mayPointTo);

    for (auto calledInfo : calledPtrWithCS) {
      int64_t callSite = calledInfo.first;

      std::set<std::string> calledFunc = calledInfo.second;
      //指针在当前callsite可能指向的函数
      for (std::string funFullName : calledFunc) {
        auto it = nodes.find(funFullName);
        if (it != nodes.end()) {
          CallGraphNode *callee = nodes[funFullName];

          addEdge(parent, callee, callSite);
        }
      }
    }
    //将parent的未访问过的子节点入队
    auto children = getChildren(parent->getFunction());
    for (ASTFunction *child : children) {

      CallGraphNode *childNode = getNode(child);
      if (childNode != nullptr && !visitInAll[childNode]) {
        visitInAll[childNode] = true;
        topOrderQueue.push(childNode);
      }
    }
  }
}

//拓扑序构建函数调用图
void CallGraph::constructCallGraphInTopOrder(ASTFunction *in) {
  CallGraphNode *node = getNode(in);

  assert(node != nullptr && "Call graph node is not constructed for ");

  std::string IncludeFunPtr = config.at("showFunctionPtr");
  visitInAll[node] = true;
  topOrderQueue.push(node);
  //前者为call site的ID，后者为可能调用的函数
  using FuncPtrWithCS = std::vector<std::pair<int64_t, std::set<std::string>>>;

  while (!topOrderQueue.empty()) {
    CallGraphNode *parent = topOrderQueue.front();
    topOrderQueue.pop();
    visitInFunction.clear();
    //处理其调用的函数
    FunctionDecl *FD = astManager.getFunctionDecl(parent->getFunction());

    if (!FD->hasBody()) {
      continue;
    }

    const std::vector<std::pair<std::string, int64_t>> &calledFunctionsWithCS =
        common::getCalledFunctionsInfo(FD, config);
    // 处理其调用的函数
    // 不以FD作为处理依据，以fullname作为处理依据
    // fullname->cgnode->astfunction->fd，保证重新load也不会导致CG失效
    for (const auto &calledInfo : calledFunctionsWithCS) {
      std::string calledFullName = calledInfo.first;
      auto it = nodes.find(calledFullName);

      if (it != nodes.end()) {
        CallGraphNode *callee = nodes[calledFullName];

        addEdge(parent, callee, calledInfo.second);

        //避免因递归调用导致循环无法结束
        if (!visitInAll[callee]) {
          visitInAll[callee] = true;
          topOrderQueue.push(callee);
        }
      }
    }

    //函数指针相关处理
    if (IncludeFunPtr == "true") {
      //前者为call site的ID，后者为可能指向的函数
      FuncPtrWithCS calledPtrWithCS =
          common::getFunctionPtrWithCS(FD, mayPointTo);
      for (auto calledInfo : calledPtrWithCS) {
        int64_t callSite = calledInfo.first;
        std::set<std::string> calledFunc = calledInfo.second;
        //指针在当前callsite可能指向的函数
        for (std::string funFullName : calledFunc) {
          auto it = nodes.find(funFullName);
          if (it != nodes.end()) {
            CallGraphNode *callee = nodes[funFullName];

            addEdge(parent, callee, callSite);
          }
        }
      }
    }
  }
}

void CallGraph::removeNoCalledSystemHeader(ASTFunction *F) {
  FunctionDecl *FD = astManager.getFunctionDecl(F);

  SourceLocation SL = FD->getLocation();
  SourceManager &SM = FD->getASTContext().getSourceManager();
  //排除系统头文件中的，没被调用的函数
  if (SM.isInSystemHeader(SL) || SM.isInExternCSystemHeader(SL) ||
      SM.isInSystemMacro(SL)) {

    CallGraphNode *node = nodes[F->getFullName()];
    nodes.erase(F->getFullName());
    delete node;
  }
}

CallGraph::CallGraph(
    ASTManager &manager, ASTResource &resource,
    const std::unordered_map<std::string, std::string> &configure)
    : config(configure), astManager(manager) {

  //初始化
  Init();
  //配置项
  //是否忽略没有被调用的系统头文件的函数
  std::string ignoreSystemHeader = config.at("ignoreNoCalledSystemHeader");
  //是否包括函数指针
  std::string IncludeFunPtr = config.at("showFunctionPtr");

  //为从resource中获取的每一个函数创建相应的CallGraphNode
  for (ASTFunction *F : resource.getFunctions()) {
    FunctionDecl *FD = manager.getFunctionDecl(F);

    if (!FD || !willIncludeInGraph(F, FD)) {
      continue;
    }

    CallGraphNode *node = getOrInsertNode(F);
  }

  //函数调用图生成
  for (auto cgnode : nodes) {
    CallGraphNode *node = cgnode.second;
    processCallee(node);
  }

  //将顶层函数放入topLevelFunctions中
  for (ASTFunction *F : resource.getFunctions()) {

    std::string fullName = F->getFullName();
    if (nodes.find(fullName) != nodes.end()) {

      CallGraphNode *node = nodes[fullName];
      if (node->getParents().size() == 0) {
        topLevelFunctions.push_back(F);
        //以toplevel函数作为函数指针分析的入口
        if (IncludeFunPtr == "true") {
          processFunctionPtrInTopOrder(node);
        }
        // ignore isolated node
        // no parents and no children
        if (ignoreSystemHeader == "true" && node->getChildren().size() == 0) {
          removeNoCalledSystemHeader(F);
        }
      }
    }
  }
}

CallGraph::~CallGraph() {
  for (auto &content : nodes) {
    delete content.second;
  }
}

const std::vector<ASTFunction *> &CallGraph::getTopLevelFunctions() const {
  return topLevelFunctions;
}

ASTFunction *CallGraph::getFunction(FunctionDecl *FD) const {
  if (!FD)
      return nullptr;
  std::string fullName = common::getFullName(FD);
  auto it = nodes.find(fullName);
  if (it != nodes.end()) {
    return it->second->getFunction();
  }
  return nullptr;
}

std::vector<ASTFunction *> CallGraph::getParents(ASTFunction *F) const {
  auto it = nodes.find(F->getFullName());
  if (it != nodes.end()) {
    return it->second->getParents();
  }
  return {};
}

std::vector<ASTFunction *> CallGraph::getChildren(ASTFunction *F) const {
  auto it = nodes.find(F->getFullName());
  if (it != nodes.end()) {
    return it->second->getChildren();
  }
  return {};
}

CallInfo CallGraph::getParentsWithCallsite(ASTFunction *F) const {
  auto it = nodes.find(F->getFullName());
  if (it != nodes.end()) {
    return it->second->getParentsWithCallsite();
  }
  return {};
}

CallInfo CallGraph::getChildrenWithCallsite(ASTFunction *F) const {
  auto it = nodes.find(F->getFullName());
  if (it != nodes.end()) {
    return it->second->getChildrenWithCallsite();
  }
  return {};
}

CallGraphNode *CallGraph::getNode(ASTFunction *f) {
  if (f == nullptr) {
    return nullptr;
  }
  auto it = nodes.find(f->getFullName());

  if (it == nodes.end()) {
    return nullptr;
  }
  return it->second;
}

CallGraphNode *CallGraph::getOrInsertNode(ASTFunction *F) {
  CallGraphNode *node = getNode(F);
  if (!node) {
    node = new CallGraphNode(F);
    nodes.insert(std::make_pair(F->getFullName(), node));
  }

  return node;
}

bool CallGraph::willIncludeInGraph(ASTFunction *F, FunctionDecl *FD) {
  std::string Includedestructor = config.at("showDestructor");
  std::string IncludeInlineAndTemplate = config.at("inlineAndTemplate");
  std::string IncludeLibFunc = config.at("showLibFunc");
  std::string IncludeLambda = config.at("showLambda");
  //析构函数
  if (Includedestructor == "false" &&
      (F->getFullName().find("~") != std::string::npos)) {
    return false;
  }
  // lambda表达式(匿名函数)
  if (IncludeLambda == "false" &&
      F->getFunctionType() == ASTFunction::AnonymousFunction) {
    return false;
  }

  // inline 函数
  if (IncludeInlineAndTemplate == "false" && FD->isInlineSpecified()) {
    return false;
  }
  //库函数，即hasBody()为false的函数
  if (IncludeLibFunc == "false" && !FD->hasBody()) {
    return false;
  }

  /*
  todo: template function
  // We skip function template definitions, as their semantics is
  // only determined when they are instantiated.
  if (FD->isDependentContext())
       return false;
  */

  return true;
}

void CallGraphNode::printCGNode(std::ostream &os) {
  std::string functionName = F->getFullName();
  os << "Callee of ";
  switch (F->getFunctionType()) {
  case ASTFunction::NormalFunction: {
    os << "function " << functionName << std::endl;
    break;
  }
  case ASTFunction::LibFunction: {
    os << "libfunction " << functionName << std::endl;
    break;
  }
  case ASTFunction::AnonymousFunction: {
    os << "AnonymousFunction/Lambda " << functionName << std::endl;
    break;
  }
  }
  for (ASTFunction *function : children) {
    std::string calleeName = function->getFullName();
    os << " " << calleeName << std::endl;
  }
}

void CallGraphNode::dumpTest(ASTManager &manager) {
  std::string functionName = F->getFullName();
  FunctionDecl *parentFD = manager.getFunctionDecl(F);

  std::cout << "caller: " << functionName << std::endl;
  for (auto callinfo : childrenWithCallsite) {
    Stmt *cs = common::getStmtInFunctionWithID(parentFD, callinfo.second);
    std::cout << "callee: " << callinfo.first->getFullName() << "\n";

    if (cs != nullptr) {
      cs->dumpColor();
      if (common::isThisCallSiteAFunctionPointer(cs)) {
        std::cout << "a funptr\n";
      }
    } else {
      std::cout << "can not find call site for: ";
      std::cout << callinfo.second << std::endl;
      std::cout << "Position in: ";
      SourceManager &SM = parentFD->getASTContext().getSourceManager();
      parentFD->getLocation().dump(SM);
      std::cout << std::endl;
    }
  }
}

void CallGraph::dumpTest() {
  std::cout << "Call Site dump test" << std::endl;
  std::cout << "nodes size: " << nodes.size() << std::endl;
  auto it = nodes.begin();
  for (; it != nodes.end(); it++) {
    CallGraphNode *temp = it->second;
    temp->dumpTest(astManager);
  }
}

void CallGraph::printCallGraph(std::ostream &out) {
  auto it = nodes.begin();
  for (; it != nodes.end(); it++) {
    CallGraphNode *temp = it->second;
    temp->printCGNode(out);
  }
  out << std::endl;
  // dumpTest();
}

void CallGraph::writeDotFile(std::ostream &out) {
  std::string head = "digraph \"Call graph\" {";
  std::string end = "}";
  std::string label = "    label=\"Call graph\"";
  out << head << std::endl;
  out << label << std::endl << std::endl;
  auto it = nodes.begin();
  for (; it != nodes.end(); it++) {
    CallGraphNode *temp = it->second;
    writeNodeDot(out, temp);
  }
  out << end << std::endl;
}

void CallGraph::writeNodeDot(std::ostream &out, CallGraphNode *node) {
  ASTFunction *function = node->getFunction();
  out << "    Node" << function << " [shape=record,label=\"{";
  out << function->getFunctionName() << "}\"];" << std::endl;
  for (ASTFunction *func : node->getChildren()) {
    out << "    Node" << function << " -> "
        << "Node" << func << ";" << std::endl;
  }
}

void NonRecursiveCallGraph::spanningTree(
    ASTFunction *F, std::unordered_map<ASTFunction *, int> &colors) {

  //标记函数F是否访问过
  colors[F] = 1;

  for (ASTFunction *CF : getChildren(F)) {

    if (colors[CF] == 0) {
      spanningTree(CF, colors);
    }
    //若CF访问过，则将F->CF的边标记为待移除
    if (colors[CF] == 1) {
      removeEdges.push_back(std::make_pair(F, CF));
    }
  }
  colors[F] = 2;
}

NonRecursiveCallGraph::NonRecursiveCallGraph(CallGraph *call_graph,
                                             const ASTResource *resource) {

  topLevelFunctions = call_graph->getTopLevelFunctions();

  std::unordered_map<ASTFunction *, int> colors;

  for (ASTFunction *F : resource->getFunctions()) {

    colors[F] = 0;
    //受配置项影响，CallGraph不会为部分ASTFunction函数创建相应的CGNode，即
    //部分ASTFunction不会被考虑在CallGraph中
    if (nodes.count(F) == 0 && call_graph->getNode(F)) {
      nodes[F] = NonRecursiveCallGraphNode(F);
    }
    //访问F在Callgraph中的子节点
    for (ASTFunction *CF : call_graph->getChildren(F)) {
      if (nodes.count(CF) == 0 && call_graph->getNode(F)) {
        nodes[CF] = NonRecursiveCallGraphNode(CF);
      }
      //排除自递归调用，该情况下函数不会是topLevelFunction，因而之后的代码
      //对其可能无效
      if (F != CF) {
        nodes[F].children.insert(CF);
        nodes[CF].parents.insert(F);
      }
    }
  }

  for (ASTFunction *F : topLevelFunctions) {
    spanningTree(F, colors);
  }

  for (auto removeEdge : removeEdges) {

    ASTFunction *PF = removeEdge.first;
    ASTFunction *CF = removeEdge.second;

    nodes[PF].children.erase(CF);
    nodes[CF].parents.erase(PF);
  }
}

std::vector<std::vector<ASTFunction *>>
NonRecursiveCallGraph::getReachablePath(ASTFunction *startFunc,
                                        ASTFunction *endFunc) {
  reachablePath.clear();
  std::vector<ASTFunction *> path;
  path.push_back(startFunc);
  if (startFunc == endFunc) {
    reachablePath.push_back(path);
    return reachablePath;
  }
  auto childs = this->getChildren(startFunc);
  auto iter = childs.begin();
  while (iter != childs.end()) {
    traverseNonRecursiveCGForReachablePath(*iter, endFunc, path);
    iter++;
  }
  return reachablePath;
}

void NonRecursiveCallGraph::traverseNonRecursiveCGForReachablePath(
    ASTFunction *startFunc, ASTFunction *endFunc,
    std::vector<ASTFunction *> path) {
  path.push_back(startFunc);
  if (startFunc == endFunc) {
    reachablePath.push_back(path);
    return;
  }
  auto childs = this->getChildren(startFunc);
  auto iter = childs.begin();
  while (iter != childs.end()) {
    traverseNonRecursiveCGForReachablePath(*iter, endFunc, path);
    iter++;
  }
  path.pop_back();
}

void NonRecursiveCallGraph::dump(std::ostream &out) {
  auto it = nodes.begin();
  for (; it != nodes.end(); it++) {
    NonRecursiveCallGraphNode temp = it->second;
    temp.dump(out);
  }
}

void NonRecursiveCallGraphNode::dump(std::ostream &os) {
  std::string functionName = F->getFunctionName();
  os << "Callee of " << functionName << std::endl;
  for (ASTFunction *function : children) {
    std::string calleeName = function->getFunctionName();
    os << " " << calleeName << std::endl;
  }
}
