#include "P2A/Node.h"
#include "P2A/PTAVar.h"
#include <iostream>

/// PointerUpdateNode
std::string PointerUpdateNode::get_kind_as_string() {
  switch (_kind) {
  case _Declaration:
    return "Declaration";
  case _AddrOfAssign:
    return "AddrOfAssign";
  case _ImplicitCast:
    return "ImplicitCast";
  }
  return "";
}

std::string PointerUpdateNode::get_update_as_string() {
  switch (_kind) {
  case _Declaration:
    return "No Init";
  case _AddrOfAssign:
    if (nullptr == _pointee) {
      return "= nullptr";
    } else {
      return "= &" + _pointee->get_name();
    }
  case _ImplicitCast:
    if (nullptr == _alias) {
      return "= nullptr";
    } else {
      return "= " + _alias->get_name();
    }
  }
  return "";
}
/// End of PointerUpdateNode

/// BuiltinVarNode
// void BuiltinVarNode::dump() {
//   std::string green = "\033[32m";
//   std::string yellow = "\033[33m";
//   std::string blue = "\033[34m";
//   std::string close = "\033[0m";
//   std::cout << "--------" << blue << _name << green << " \"" << _type << "\"
//   "
//             << yellow << "<id: " << _id << ">" << close << "--------"
//             << std::endl;
// }

/// End of BuiltinVarNode

/// PTAUpdateMap

PTAUpdateMap::PTAUpdateMap(const PTAUpdateMap *pum) {
  _pta_var = pum->_pta_var;
  for (auto update_node : pum->_outside_update_nodes) {
    _outside_update_nodes.push_back(new PointerUpdateNode(update_node));
  }
  for (auto func_map : pum->_inside_update_nodes) {
    for (auto block_map : func_map.second) {
      for (auto update_node : block_map.second) {
        _inside_update_nodes[func_map.first][block_map.first].push_back(
            new PointerUpdateNode(update_node));
      }
    }
  }
}
PTAUpdateMap::~PTAUpdateMap() {
  for (auto update_node : _outside_update_nodes) {
    delete update_node;
  }
  for (auto func_map : _inside_update_nodes) {
    for (auto block_map : func_map.second) {
      for (auto update_node : block_map.second) {
        delete update_node;
      }
    }
  }
}

void PTAUpdateMap::insert_update_node(int64_t func_id, unsigned block_id,
                                      PointerUpdateNode *update_node) {

  _inside_update_nodes[func_id][block_id].push_back(update_node);
}

void PTAUpdateMap::clear_update_node(int64_t func_id, unsigned block_id) {
  _inside_update_nodes[func_id][block_id].clear();
}
void PTAUpdateMap::dump() {
  std::string two_space = "  ";
  std::string four_space = "    ";
  std::string red = "\033[31m";
  std::string green = "\033[32m";
  std::string yellow = "\033[33m";
  std::string blue = "\033[34m";
  std::string purple = "\033[35m";
  std::string dgreen = "\033[36m";
  std::string close = "\033[0m";
  // std::cout << "--------" << blue << _name << green << " \"" << _type
  //           << "\"
  //              "
  //           << yellow << "<id: " << _id << ">" << close << "--------"
  //           << std::endl;
  std::cout << dgreen << "Outside Function" << std::endl;
  for (auto update_node : _outside_update_nodes) {
    std::cout << four_space << green << update_node->get_kind_as_string() << " "
              << update_node->get_update_as_string() << std::endl;
  }
  std::cout << close;
  for (auto func_map : _inside_update_nodes) {
    // std::cout << dgreen << "Inside Function " << blue
    //           << _file_node->get_func_node(func_map.first)->get_name() <<
    //           green
    //           << " \"" <<
    //           _file_node->get_func_node(func_map.first)->get_type()
    //           << "\" " << yellow << "<id: " << func_map.first << ">" << close
    //           << std::endl;
    std::cout << dgreen << "Inside Function " << yellow
              << "<id: " << func_map.first << ">" << close << std::endl;
    for (auto block_map : func_map.second) {
      std::cout << two_space << yellow << "[B" << block_map.first << "]"
                << close << std::endl;
      for (auto update_node : block_map.second) {
        std::cout << four_space << green << update_node->get_kind_as_string()
                  << " " << update_node->get_update_as_string() << close
                  << std::endl;
      }
    }
  }
}
/// End of PTAUpdateMap

/// BlockNode
// assert this block is part of instance's memmber function
bool BlockNode::is_update_var(const PTAVar *pta_var,
                              const PTAVar *&field_pta_var,
                              std::list<std::string> instance) {
  field_pta_var = pta_var;
  if (!pta_var->isField())
    return _vars.find(pta_var) != _vars.end();
  // assert instance == pta_var.get_Instance_var_key().front()
  for (auto var : _vars) {
    if (var->isVirtualField()) {
      if (pta_var->get_field_var_key() ==
          var->trans_to_instance_field_var_key(instance)) {
        field_pta_var = var;
        return true;
      }
    }
  }
  return false;
}
/// End of BlockNode

/// FunctionNode
FunctionNode::FunctionNode(unsigned id, unsigned block_count, std ::string name,
                           std::string type, unsigned entry_block_id,
                           unsigned exit_block_id)
    : _id(id), _name(name), _type(type), _entry_block_id(entry_block_id),
      _exit_block_id(exit_block_id) {
  _block_nodes.resize(block_count);
  for (unsigned i = 0; i < block_count; ++i) {
    _block_nodes[i].set_id(i);
  }
}

void FunctionNode::expand_loop(unsigned times) {
  // TODO
}

// Expand the loop only once, in other words, handle loop as if
void FunctionNode::topo_sort() {
  expand_loop(1);
  // Topological sort
  unsigned topo_id = 0;
  std::set<unsigned> handle_already;
  std::list<unsigned> work_list;
  work_list.push_back(_entry_block_id);
  while (work_list.size()) {
    auto block_id = work_list.back();
    work_list.pop_back();
    // std::cout << "Pop B" << block_id << std::endl;
    if (handle_already.find(block_id) != handle_already.end())
      continue;
    auto block_node = get_block_node(block_id);
    // Check if preds were handled already
    bool is_ready = true;
    for (auto pred : *block_node->get_preds()) {
      if (handle_already.find(pred->get_id()) == handle_already.end()) {
        is_ready = false;
        break;
      }
    }
    if (is_ready) {
      block_node->set_topo_id(topo_id++);
      for (auto succ : *block_node->get_succs()) {
        if (nullptr != succ)
          work_list.push_back(succ->get_id());
      }
      handle_already.insert(block_id);
    } // End if(is_ready)
  }
  // Set flag
  _is_topo_sort = true;
  return;
}

std::set<std::list<unsigned>> FunctionNode::get_all_path_to(unsigned block_id) {
  if (!_is_topo_sort)
    topo_sort();
  std::set<std::list<unsigned>> result;
  std::list<unsigned> work_list;
  std::list<unsigned> steps;
  std::list<unsigned> path;
  work_list.push_back(block_id);
  steps.push_back(0);
  unsigned step = 0;
  while (work_list.size()) {
    auto block_id = work_list.back();
    work_list.pop_back();
    path.push_front(block_id);
    ++step;
    if (block_id == _entry_block_id) {
      result.insert(path);
      for (unsigned i = 0; i < step; i++) {
        path.pop_front();
      }
      step = steps.back();
      steps.pop_back();
    }
    auto block = &_block_nodes[block_id];
    auto preds_size = block->get_pred_size();
    if (preds_size > 1) {
      steps.push_back(step);
      for (; preds_size > 2; --preds_size) {
        steps.push_back(0);
      }
      step = 0;
    }
    for (auto pred : *block->get_preds()) {
      work_list.push_back(pred->get_id());
    }
  }
  return result;
}

void FunctionNode::dump() {
  std::string two_space = "  ";
  std::string four_space = "    ";
  std::string red = "\033[31m";
  std::string green = "\033[32m";
  std::string yellow = "\033[33m";
  std::string blue = "\033[34m";
  std::string purple = "\033[35m";
  std::string dgreen = "\033[36m";
  std::string close = "\033[0m";
  std::cout << blue << _name << green << " \"" << _type << "\" " << yellow
            << "<id: " << _id << ">" << close << std::endl;
  for (auto &block : _block_nodes) {
    auto block_id = block.get_id();
    std::cout << two_space << yellow << "[B" << block_id;
    if (block_id == _entry_block_id)
      std::cout << " (ENTRY)";
    else if (block_id == _exit_block_id)
      std::cout << " (EXIt)";
    std::cout << "]" << yellow << " <size: " << block.get_elem_size() << ">"
              << close << std::endl;
    auto pred_size = block.get_pred_size();
    auto succ_size = block.get_succ_size();
    if (pred_size) {
      std::cout << four_space << blue << "Preds " << close << "(" << pred_size
                << "):" << blue;
      for (auto pred : *block.get_preds()) {
        std::cout << " B" << pred->get_id();
      }
      std::cout << close << std::endl;
    }
    if (succ_size) {
      std::cout << four_space << purple << "Succs " << close << "(" << succ_size
                << "):" << purple;
      for (auto succ : *block.get_succs()) {
        if (nullptr == succ)
          std::cout << " NULL";
        else
          std::cout << " B" << succ->get_id();
      }
      std::cout << close << std::endl;
    }
  }
}

/// End of FunctionNode

/// FileNode
FileNode::~FileNode() {
  // for (auto var : _var_nodes) {
  //   delete var.second;
  // }
  for (auto fn : _func_nodes) {
    delete fn.second;
  }
}

void FileNode::dump_var_nodes() {
  std::cout << "####----" << _name << "----####" << std::endl;
  // std::cout << "Pointer Var count " << _var_nodes.size() << std::endl;
  // for (auto var : _var_nodes) {
  //   var.second->dump();
  // }
}
/// End of FileNode