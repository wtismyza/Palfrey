#ifndef NODE_H
#define NODE_H

#include <set>
#include <unordered_map>
// #include <utility>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "framework/ASTManager.h"

/// Structures below are designed to replace those in ASTManager to reduce
/// storage overhead.
class PTAVar;
/// Pointer update info
class PointerUpdateNode;
class PTAUpdateMap;
/// Path info
class BlockNode;
class FunctionNode;
class FileNode;

/// Location is var's <line, column> in source file.
class Location {
public:
  Location() {}
  Location(unsigned ln, unsigned col) : _ln(ln), _col(col) {}
  unsigned get_ln() { return _ln; }
  unsigned get_col() { return _col; }

  bool is_before(const Location &loc) {
    if (_ln < loc._ln)
      return true;
    else if (_ln > loc._ln)
      return false;
    return (_col < loc._col);
  }

private:
  unsigned _ln;
  unsigned _col;
};

class Path {
public:
private:
};

class PathManager {
public:
private:
  // Block
};

/// for each ValDecl
/// VarNode just as an interface
// class VarNode {
// public:
//   enum Kind {
//     _Var,
//     _MallocMem,
//     _BuiltinVar,
//     _PointerVar,
//   };
//   virtual Kind get_kind() { return _Var; }
//   virtual std::string get_name() { return ""; }
//   virtual unsigned get_id() { return 0; }
//   virtual void dump() {}

// private:
// };

// class BuiltinVarNode : public VarNode {
// public:
//   BuiltinVarNode(unsigned id, FileNode *file_node, std::string name,
//                  std::string type)
//       : _id(id), _file_node(file_node), _name(name), _type(type) {}
//   Kind get_kind() override { return _BuiltinVar; }
//   std::string get_name() override { return _name; }
//   unsigned get_id() override { return _id; }
//   void dump() override;

// private:
//   unsigned _id;
//   std::string _name;
//   std::string _type;
//   FileNode *_file_node;
// };

// PointerUpdateNode is a node that changes ptr's value including declaration,
// definition and assignment.
class PointerUpdateNode {
public:
  enum Kind {
    _Declaration,
    _AddrOfAssign,
    _ImplicitCast,
    _NullPtr,
  };

  PointerUpdateNode(unsigned elem_id = 0)
      : _kind(_Declaration), _elem_id(elem_id), _pointee(nullptr),
        _alias(nullptr) {}
  PointerUpdateNode(const PointerUpdateNode *pun)
      : _kind(pun->_kind), _elem_id(pun->_elem_id), _pointee(pun->_pointee),
        _alias(pun->_alias) {}

  ~PointerUpdateNode() {}
  Kind get_kind() { return _kind; }
  // void set_kind(Kind kind) { _kind = kind; }
  void set_pointee(const PTAVar *var) {
    _kind = _AddrOfAssign;
    _pointee = var;
  }
  void set_alias(const PTAVar *var) {
    _kind = _ImplicitCast;
    _alias = var;
  }
  const PTAVar *get_pointee() { return _pointee; }
  const PTAVar *get_alias() { return _alias; }
  // unsigned get_pointee_id() { return _pointee->get_id(); }
  // unsigned get_alias_id() { return _alias->get_id(); }
  unsigned get_elem_id() { return _elem_id; }
  std::string get_kind_as_string();
  std::string get_update_as_string();

private:
  Kind _kind;
  const PTAVar *_pointee;
  const PTAVar *_alias;
  unsigned _elem_id;
  // Location _begin_loc;
  // Location _end_loc;
};

class PTAUpdateMap {
public:
  PTAUpdateMap(const PTAVar *pta_var) : _pta_var(pta_var) {}
  PTAUpdateMap(const PTAUpdateMap *pum);
  ~PTAUpdateMap();

  void insertVirtualFieldPTAUpdateMap(const PTAUpdateMap *pum);
  // Kind get_kind() override { return _PointerVar; }
  // std::string get_name() override { return _name; }
  // unsigned get_id() override { return _id; }
  const PTAVar *get_pta_var() { return _pta_var; }
  void insert_update_node(PointerUpdateNode *update_node) {
    _outside_update_nodes.push_back(update_node);
  }
  void insert_update_node(int64_t func_id, unsigned block_id,
                          PointerUpdateNode *update_node);
  void clear_update_node(int64_t func_id, unsigned block_id);

  std::list<PointerUpdateNode *> *get_update_nodes(int64_t func_id,
                                                   unsigned block_id) {
    return &(_inside_update_nodes[func_id][block_id]);
  }
  void dump();

private:
  // unsigned _id;
  // std::string _name;
  // std::string _type;
  // FileNode *_file_node;
  const PTAVar *_pta_var;
  // outside function
  std::list<PointerUpdateNode *> _outside_update_nodes;
  // inside function
  // <FunctionNode::_id, <BlockNode::_id, list<> > >
  std::unordered_map<
      int64_t, std::unordered_map<unsigned, std::list<PointerUpdateNode *>>>
      _inside_update_nodes;
};

class BlockNode {
public:
  enum Kind {
    _Normal,
    _WhileLoop,
    _ForLoop,
    _DoWhileLoop,
    _If,
  };
  int64_t get_id() { return _id; }
  void set_id(int64_t id) { _id = id; }
  unsigned get_topo_id() { return _topo_id; }
  void set_topo_id(unsigned topo_id) { _topo_id = topo_id; }
  unsigned get_elem_size() { return _elem_size; }
  void set_elem_size(unsigned elem_size) { _elem_size = elem_size; }
  Kind get_kind() { return _kind; }
  void set_kind(Kind kind) { _kind = kind; }
  void add_var_update(const PTAVar *pta_var) { _vars.insert(pta_var); }
  bool is_update_var(const PTAVar *pta_var) {
    return _vars.find(pta_var) != _vars.end();
  }
  // assert this block is part of instance's memmber function
  bool is_update_var(const PTAVar *pta_var, const PTAVar *&field_pta_var,
                     std::list<std::string> instance);
  std::vector<BlockNode *> *get_preds() { return &_preds; }
  std::vector<BlockNode *> *get_succs() { return &_succs; }
  unsigned get_pred_size() { return _preds.size(); }
  unsigned get_succ_size() { return _succs.size(); }

private:
  int64_t _id;
  unsigned _topo_id;
  unsigned _elem_size;
  Kind _kind = _Normal;
  std::set<const PTAVar *> _vars;
  std::vector<BlockNode *> _preds;
  std::vector<BlockNode *> _succs;
};

class BlockCopyNode {
public:
private:
  std::set<unsigned> _topo_preds;
  std::set<unsigned> _topo_succs;
  BlockNode *_src;
};

class FunctionNode {
public:
  FunctionNode(unsigned id, unsigned block_count, std ::string name,
               std::string type, unsigned entry_block_id,
               unsigned exit_block_id);
  BlockNode *get_block_node(unsigned id) {
    if (_block_nodes.size() <= id) {
      std::cout << "_block_node not found" << std::endl;
    }
    return &(_block_nodes[id]);
  }
  std::string get_name() { return _name; }
  std::string get_type() { return _type; }
  unsigned get_entry_block_id() { return _entry_block_id; }
  unsigned get_exit_block_id() { return _exit_block_id; }
  void expand_loop(unsigned times);
  void topo_sort(); // expand loop once
  std::set<std::list<unsigned>> get_all_path_to(unsigned block_id);
  void dump();
  int getBlockNodeSize() { return _block_nodes.size(); }

private:
  int64_t _id;
  std::string _name;
  std::string _type;
  unsigned _entry_block_id;
  unsigned _exit_block_id;
  // Vector index is block id.
  std::vector<BlockNode> _block_nodes;
  std::vector<BlockCopyNode *> _block_topo_sort;
  bool _is_topo_sort = false;
};

class FileNode {
public:
  FileNode(std::string name) : _name(name) {}
  ~FileNode();
  // std::unordered_map<PTAVar *, PointerUpdateMap *> *get_var_nodes() {
  //   return &_var_nodes;
  // }
  std::unordered_map<int64_t, FunctionNode *> *get_func_nodes() {
    return &_func_nodes;
  }

  // PointerUpdateMap *getPointerUpdateNode(PTAVar *pta_var) {
  //   auto iter = _var_nodes.find(pta_var);
  //   return iter != _var_nodes.end() ? iter->second : nullptr;
  // }

  FunctionNode *get_func_node(int64_t global_id) {
    auto iter = _func_nodes.find(global_id);
    return iter != _func_nodes.end() ? iter->second : nullptr;
  }

  // void insert_var(PTAVar *pta_var, PointerUpdateMap *pum) {
  //   _var_nodes.insert({pta_var, pum});
  // }
  void insert_func(int64_t global_id, FunctionNode *func) {
    _func_nodes.insert({global_id, func});
  }

  // bool is_var_exist(PTAVar *pta_var) {
  //   return _var_nodes.find(pta_var) != _var_nodes.end();
  // }

  void dump_var_nodes();

private:
  // ast file name in astList.txt
  std::string _name;
  // <Decl::getID(), Class *>
  // std::unordered_map<PTAVar *, PointerUpdateMap *> _var_nodes;
  std::unordered_map<int64_t, FunctionNode *> _func_nodes;
};

// from CFGBlock to BlockNode
class BToBMap {
public:
  void putBlockNode(int64_t funcID, unsigned blockID, BlockNode *bn) {
    if (b2b.find(funcID) == b2b.end()) {
      std::map<unsigned, BlockNode *> blockInfo = {{blockID, bn}};
      b2b[funcID] = blockInfo;
    } else {
      b2b[funcID][blockID] = bn;
    }
  }

  BlockNode *getBlockNode(int64_t funcID, unsigned blockID) {
    if (b2b.find(funcID) == b2b.end())
      return nullptr;
    if (b2b[funcID].find(blockID) == b2b[funcID].end())
      return nullptr;
    return b2b[funcID][blockID];
  }

private:
  std::map<int64_t, std::map<unsigned, BlockNode *>> b2b;
};

class CallerInfo {
public:
  void putCallerInfo(int64_t funcID, unsigned blockID,
                     std::list<std::string> var) {
    if (callerMap.find(funcID) == callerMap.end()) {
      std::map<unsigned, std::list<std::string>> calleeInfo = {{blockID, var}};
      callerMap[funcID] = calleeInfo;
    } else {
      callerMap[funcID][blockID] = var;
    }
  }

  std::list<std::string> getCallerInfo(int64_t funcID, unsigned blockID) {
    std::list<std::string> res;
    if (callerMap.find(funcID) == callerMap.end())
      return res;
    if (callerMap[funcID].find(blockID) == callerMap[funcID].end())
      return res;
    return callerMap[funcID][blockID];
  }

  void putArgInfo(int64_t funcID, unsigned blockID, const Expr *args) {
    if (argMap.find(funcID) == argMap.end()) {
      std::vector<const Expr *> argsList;
      argsList.push_back(args);
      std::map<unsigned, std::vector<const Expr *>> argInfo = {
          {blockID, argsList}};
      argMap[funcID] = argInfo;
      return;
    }
    if (argMap[funcID].find(blockID) == argMap[funcID].end()) {
      std::vector<const Expr *> argsList;
      argsList.push_back(args);
      argMap[funcID][blockID] = argsList;
      return;
    } else {
      argMap[funcID][blockID].push_back(args);
      return;
    }
  }

  std::vector<const Expr *> getArgInfo(int64_t funcID, unsigned blockID) {
    std::vector<const Expr *> result;
    if (argMap.find(funcID) == argMap.end())
      return result;
    if (argMap[funcID].find(blockID) == argMap[funcID].end())
      return result;
    return argMap[funcID][blockID];
  }

private:
  // caller var map
  std::map<unsigned, std::map<unsigned, std::list<std::string>>> callerMap;
  std::map<unsigned, std::map<unsigned, std::vector<const Expr *>>> argMap;
};
#endif // NODE_H