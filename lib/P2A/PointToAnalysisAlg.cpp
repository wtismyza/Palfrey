#include "P2A/PointToAnalysis.h"
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
std::set<const PTAVar *> PointToAnalysis::get_pointee_of_l1(CFGBlock *entry,
                                                            CFGBlock *exit,
                                                            unsigned elem_id,
                                                            const PTAVar *var) {
  std::set<const PTAVar *> result;
  return result;
}

// Level2: Field-, Context-(Object-), Flow-sensitive
std::set<const PTAVar *> PointToAnalysis::get_pointee_of_l2(CFGBlock *entry,
                                                            CFGBlock *exit,
                                                            unsigned elem_id,
                                                            const PTAVar *var) {
  std::set<const PTAVar *> result;
  return result;
}

// Level3: Field-, Context-(Object-), Flow-, Path-sensitive
// Assert there is only one path from entry to exit.
std::set<const PTAVar *>
PointToAnalysis::get_pointee_of_l3(std::list<const CFGBlock *> path,
                                   unsigned elem_id, const PTAVar *var, bool isForAliasAnalysis) {
  PTAUpdateMap *var_update_map = _pvm.getPTAUpdateMap(var);
  // var_update_map->dump();
  assert(var->isInstance());
  std::set<const PTAVar *> result;
  if (1) {
    auto current = *(path.rbegin());
    auto block_id = current->getBlockID();

    // auto decl = current->getParent()->getParentDecl();
    auto decl = manager->parentDecls[current->getParent()];
    // decl->dump();
    
    const FunctionDecl *funcDecl = dyn_cast<FunctionDecl>(decl);
    bool isMemMethod = funcDecl->getKind() == FunctionDecl::CXXMethod;
    ASTFunction *F =
        manager->getASTFunction(const_cast<FunctionDecl *>(funcDecl));
    auto func_id = F->getID();
    BlockNode *currentBlockNode =
        _blockToBlockMap.getBlockNode(func_id, block_id);
    // get "this->"
    std::list<std::string> instance = getInstanceInPath(path);
    // auto block_node = func_node->get_block_node(block_id);
    const PTAVar *pta_var_copy = var;

    // set data flow between two function
    if (block_id == current->getParent()->getNumBlockIDs() - 1 &&
        path.size() > 1) {
      std::vector<const Expr *> args = _callerInfo.getArgInfo(
          func_id, const_cast<CFGBlock *>(*(++path.rbegin()))->getBlockID());
      for (unsigned i = 0, paramNum = funcDecl->getNumParams(); i != paramNum;
           ++i) {
        auto param = funcDecl->getParamDecl(i);
        auto pta_var = _pvm.insertPTAVar(param);
        auto pta_update_map = _pvm.getPTAUpdateMap(pta_var);
        auto update_node = new PointerUpdateNode();
        HandleExpr(args[i], update_node);
        _func_nodes[func_id]->get_block_node(block_id)->add_var_update(pta_var);
        pta_update_map->clear_update_node(func_id, block_id);
        pta_update_map->insert_update_node(func_id, block_id, update_node);
      }
    }

    // set the return value
    if (current->size() > 0) {
      auto element = *(current->rbegin());
      if (element.getKind() == CFGStmt::Statement) {
        auto stmt = element.getAs<CFGStmt>()->getStmt();
        if (stmt->getStmtClass() == Stmt::ReturnStmtClass) {
          if (auto return_stmt = dyn_cast<ReturnStmt>(stmt)) {
            const Expr *return_value = return_stmt->getRetValue();
            if (return_value) {
              auto pta_var = _pvm.insertPTAVar(funcDecl);
              auto pta_update_map = _pvm.getPTAUpdateMap(pta_var);
              auto update_node = new PointerUpdateNode(INT_MAX);
              HandleExpr(return_value, update_node);
              _func_nodes[func_id]->get_block_node(block_id)->add_var_update(
                  pta_var);
              pta_update_map->clear_update_node(func_id, block_id);
              pta_update_map->insert_update_node(func_id, block_id,
                                                 update_node);
            }
          }
        }
      }
    }

    if ((isMemMethod &&
         currentBlockNode->is_update_var(var, pta_var_copy, instance)) ||
        (!isMemMethod && currentBlockNode->is_update_var(var))) {
      // get update_list
      auto update_list = var_update_map->get_update_nodes(func_id, block_id);
      if (isMemMethod && pta_var_copy->isVirtualField()) {
        assert(update_list->empty());
        auto field_var_update_map = _pvm.getPTAUpdateMap(pta_var_copy);
        update_list = field_var_update_map->get_update_nodes(func_id, block_id);
      }
      // traverse update_list
      for (auto update_node_iter = update_list->rbegin();
           update_node_iter != update_list->rend(); ++update_node_iter) {
        auto update_node = *update_node_iter;
        // check elem_id only the last block
        if (elem_id > update_node->get_elem_id()) {
          switch (update_node->get_kind()) {
          case PointerUpdateNode::_Declaration: {
            return result;
          } break;
          case PointerUpdateNode::_AddrOfAssign: {
            auto one_res = update_node->get_pointee();
            if (one_res->isInstance())
              result.insert(one_res);
            else if (one_res->isVirtualField()) {
              auto new_key = one_res->trans_to_instance_field_var_key(instance);
              auto new_one_res = _pvm.getPTAVar(new_key);
              if (new_one_res == nullptr) {
                new_one_res = new PTAVar(one_res->get_name(), new_key, true,
                                         one_res->get_type());
                _pvm.insertPTAVar(new_one_res);
              }
              result.insert(new_one_res);
            }
            return result;
          } break;
          case PointerUpdateNode::_ImplicitCast: {
            auto one_alias = update_node->get_alias();
            if (one_alias->isInstance()) {
              auto part_result = get_pointee_of_l3(
                  path, update_node->get_elem_id(), one_alias, isForAliasAnalysis);
              result.insert(part_result.begin(), part_result.end());
            } else if (one_alias->isVirtualField()) {
              auto new_key =
                  one_alias->trans_to_instance_field_var_key(instance);
              auto new_one_alias = _pvm.getPTAVar(new_key);
              if (new_one_alias == nullptr) {
                new_one_alias = new PTAVar(new_one_alias->get_name(), new_key,
                                           true, new_one_alias->get_type());
                _pvm.insertPTAVar(new_one_alias);
              }
              auto part_result = get_pointee_of_l3(
                  path, update_node->get_elem_id(), new_one_alias, isForAliasAnalysis);
              result.insert(part_result.begin(), part_result.end());
            }
            return result;
          } break;
          } // End of switch
        }
      } // End of for
    }
    path.pop_back();
  }
  while (!path.empty()) {
    auto current = *(path.rbegin());
    auto block_id = current->getBlockID();

    // auto decl = current->getParent()->getParentDecl();
    auto decl = manager->parentDecls[current->getParent()];

    const FunctionDecl *funcDecl = dyn_cast<FunctionDecl>(decl);
    bool isMemMethod = funcDecl->getKind() == FunctionDecl::CXXMethod;
    ASTFunction *F =
        manager->getASTFunction(const_cast<FunctionDecl *>(funcDecl));
    auto func_id = F->getID();
    // traverse update_list
    BlockNode *currentBlockNode =
        _blockToBlockMap.getBlockNode(func_id, block_id);
    std::list<std::string> instance = getInstanceInPath(path);
    // auto block_node = func_node->get_block_node(block_id);
    const PTAVar *pta_var_copy = var;
    currentBlockNode->is_update_var(var);

    // set data flow between two function
    if (block_id == current->getParent()->getNumBlockIDs() - 1 &&
        path.size() > 1) {
      std::vector<const Expr *> args = _callerInfo.getArgInfo(
          func_id, const_cast<CFGBlock *>(*(++path.rbegin()))->getBlockID());
      for (unsigned i = 0, paramNum = funcDecl->getNumParams(); i != paramNum;
           ++i) {
        auto param = funcDecl->getParamDecl(i);
        auto pta_var = _pvm.insertPTAVar(param);
        auto pta_update_map = _pvm.getPTAUpdateMap(pta_var);
        auto update_node = new PointerUpdateNode();
        HandleExpr(args[i], update_node);
        _func_nodes[func_id]->get_block_node(block_id)->add_var_update(pta_var);
        pta_update_map->clear_update_node(func_id, block_id);
        pta_update_map->insert_update_node(func_id, block_id, update_node);
      }
    }

    // set the return value
    if (current->size() > 0) {
      auto element = *(current->rbegin());
      if (element.getKind() == CFGStmt::Statement) {
        auto stmt = element.getAs<CFGStmt>()->getStmt();
        if (stmt->getStmtClass() == Stmt::ReturnStmtClass) {
          if (auto return_stmt = dyn_cast<ReturnStmt>(stmt)) {
            const Expr *return_value = return_stmt->getRetValue();
            if (return_value) {
              ASTFunction *F =
                  manager->getASTFunction(const_cast<FunctionDecl *>(funcDecl));
              auto pta_var = _pvm.insertPTAVar(funcDecl);
              auto pta_update_map = _pvm.getPTAUpdateMap(pta_var);
              auto update_node = new PointerUpdateNode(INT_MAX);
              HandleExpr(return_value, update_node);
              _func_nodes[func_id]->get_block_node(block_id)->add_var_update(
                  pta_var);
              pta_update_map->clear_update_node(func_id, block_id);
              pta_update_map->insert_update_node(func_id, block_id,
                                                 update_node);
            }
          }
        }
      }
    }

    if ((isMemMethod &&
         currentBlockNode->is_update_var(var, pta_var_copy, instance)) ||
        (!isMemMethod && currentBlockNode->is_update_var(var))) {
      // get update_list
      auto update_list = var_update_map->get_update_nodes(func_id, block_id);
      if (isMemMethod && pta_var_copy->isVirtualField()) {
        auto field_var_update_map = _pvm.getPTAUpdateMap(pta_var_copy);
        update_list = field_var_update_map->get_update_nodes(func_id, block_id);
      }

      for (auto update_node_iter = update_list->rbegin();
           update_node_iter != update_list->rend(); ++update_node_iter) {
        auto update_node = *update_node_iter;
        // check elem_id only the last block
        switch (update_node->get_kind()) {
        case PointerUpdateNode::_Declaration: {
          return result;
        } break;
        case PointerUpdateNode::_AddrOfAssign: {
          auto one_res = update_node->get_pointee();
          if (one_res->isInstance())
            result.insert(one_res);
          else if (one_res->isVirtualField()) {
            auto new_key = one_res->trans_to_instance_field_var_key(instance);
            auto new_one_res = _pvm.getPTAVar(instance);
            if (new_one_res == nullptr) {
              new_one_res = new PTAVar(one_res->get_name(), new_key, true,
                                       one_res->get_type());
            }
            result.insert(new_one_res);
          }
          return result;
        } break;
        case PointerUpdateNode::_ImplicitCast: {
          auto one_alias = update_node->get_alias();
          if (one_alias->isInstance()) {
            auto part_result =
                get_pointee_of_l3(path, update_node->get_elem_id(), one_alias, isForAliasAnalysis);
            result.insert(part_result.begin(), part_result.end());
          } else if (one_alias->isVirtualField()) {
            auto new_key = one_alias->trans_to_instance_field_var_key(instance);
            auto new_one_alias = _pvm.getPTAVar(instance);
            if (new_one_alias == nullptr) {
              new_one_alias = new PTAVar(new_one_alias->get_name(), new_key,
                                         true, new_one_alias->get_type());
            }
            auto part_result = get_pointee_of_l3(
                path, update_node->get_elem_id(), new_one_alias, isForAliasAnalysis);
            result.insert(part_result.begin(), part_result.end());
          }
          return result;
        } break;
        } // End of switch
      }   // End of for
    }
    path.pop_back();
    // ++pathIter;
  } // End of while
  if (isForAliasAnalysis && result.empty()){
    PTAVar* tempTointTo = new PTAVar(var->get_name() + "_temp", var->get_instance_var_key() + "_temp", var->get_type());
    result.insert(tempTointTo);
  }
  return result;
}

std::set<const PTAVar *>
PointToAnalysis::get_pointee_at_point(CFGBlock *pathEnd, unsigned elem_id,
                                      const PTAVar *var, unsigned pathLen) {
  if (nullptr == var) {
    std::string red = "\033[31m";
    std::string close = "\033[0m";
    std::cout << red << "[!] the query variable is nullptr!" << close
              << std::endl;
    std::set<const PTAVar *> res;
    return res;
  }
  std::vector<std::list<const CFGBlock *>> paths =
      _interProcedureCFG->getPathBeforePointWithLength(pathLen, pathEnd);
  std::set<const PTAVar *> result;
  for (auto it = paths.begin(), itend = paths.end(); it != itend; ++it) {
    std::set<const PTAVar *> res = get_pointee_of_l3(*it, elem_id, var);
    for (auto resit = res.begin(), resitend = res.end(); resit != resitend;
         ++resit) {
      result.insert(*resit);
    }
  }
  return result;
}

bool PointToAnalysis::is_alias_of(std::list<const CFGBlock *> path_1,
                                  unsigned elem_id_1, const PTAVar *var_1,
                                  std::list<const CFGBlock *> path_2,
                                  unsigned elem_id_2, const PTAVar *var_2) {
  if (nullptr == var_1 || nullptr == var_2) {
    std::string red = "\033[31m";
    std::string close = "\033[0m";
    std::cout << red << "[!] the query variable is nullptr!" << close
              << std::endl;
    return false;
  }
  std::set<const PTAVar *> res1 =
      PointToAnalysis::get_pointee_of_l3(path_1, elem_id_1, var_1, true);
  std::set<const PTAVar *> res2 =
      PointToAnalysis::get_pointee_of_l3(path_2, elem_id_2, var_2, true);
  for (auto it1 = res1.begin(), it1end = res1.end(); it1 != it1end; ++it1) {
    for (auto it2 = res2.begin(), it2end = res2.end(); it2 != it2end; ++it2) {
      if ((*it1)->get_instance_var_key() == (*it2)->get_instance_var_key() && (*it1)->get_name() == (*it1)->get_name()) {
        return true;
      }
    }
  }
  return false;
}

bool PointToAnalysis::is_alias_of_at_point(CFGBlock *pathEnd, unsigned elem_id,
                                           const PTAVar *var_1,
                                           const PTAVar *var_2, int pathLen) {
  if (nullptr == var_1 || nullptr == var_2) {
    std::string red = "\033[31m";
    std::string close = "\033[0m";
    std::cout << red << "[!] the query variable is nullptr!" << close
              << std::endl;
    return false;
  }
  auto cfg = pathEnd->getParent();
  std::vector<std::list<const CFGBlock *>> paths =
      _interProcedureCFG->getPathBeforePointWithLength(pathLen, pathEnd);

  std::set<const PTAVar *> result_1, result_2;
  for (auto it = paths.begin(), itend = paths.end(); it != itend; ++it) {
    std::set<const PTAVar *> res = get_pointee_of_l3(*it, elem_id, var_1, true);
    for (auto resit = res.begin(), resitend = res.end(); resit != resitend;
         ++resit) {
      result_1.insert(*resit);
    }

    res.clear();
    res = get_pointee_of_l3(*it, elem_id, var_2, true);
    for (auto resit = res.begin(), resitend = res.end(); resit != resitend;
         ++resit) {
      result_2.insert(*resit);
    }
  }

  for (auto it1 = result_1.begin(), it1end = result_1.end(); it1 != it1end;
       ++it1) {
    for (auto it2 = result_2.begin(), it2end = result_2.end(); it2 != it2end;
         ++it2) {
      if (*it1 == *it2) {
        return true;
      }
    }
  }
  return false;
}

std::set<const PTAVar *> PointToAnalysis::get_alias_in_func(ASTFunction* F,
                                                            const PTAVar *var, int pathlen, int pathnum) {
  if (nullptr == var) {
    std::string red = "\033[31m";
    std::string close = "\033[0m";
    std::cout << red << "[!] the query variable is nullptr!" << close
              << std::endl;
    std::set<const PTAVar *> res;
    return res;
  }
 // ASTFunction *F = manager->getASTFunction(FD);
  std::unique_ptr<CFG> &cfg = manager->getCFG(F);
  CFGBlock *entry = &(cfg->getEntry());
  CFGBlock *exit = &(cfg->getExit());

  // LangOptions LangOpts;
  // LangOpts.CPlusPlus = true;
  // cfg->dump(LangOpts, true);

  std::set<const PTAVar *> res;

  std::vector<std::list<const CFGBlock *>> paths = getPathInFunc(entry, exit,pathlen,pathnum);
  int visited=0;
  for (auto paths_it = paths.begin(), paths_itend = paths.end();
       paths_it != paths_itend; ++paths_it) {
    if ((*paths_it).size()>pathlen) continue;
    if (visited>pathnum) break;
    const PTAType* ty = var->get_type();
    if (!ty) continue;
    FindVariable findVariable(ty->getName());
    for (auto block_it = (*paths_it).begin(), blcok_itend = (*paths_it).end();
         block_it != blcok_itend; ++block_it) {
      const CFGBlock *currentBlock = *block_it;

      for (auto element_it = currentBlock->begin(),
                element_itend = currentBlock->end();
           element_it != element_itend; ++element_it) {
        if ((*element_it).getKind() == CFGStmt::Statement) {
          findVariable.TraverseStmt(
              const_cast<Stmt *>((*element_it).getAs<CFGStmt>()->getStmt()));
        }
      }
    }
    std::set<ValueDecl *> vars = findVariable.getVar();
    for (auto var_it = vars.begin(), var_itend = vars.end();
         var_it != var_itend; ++var_it) {
      std::string key = (*var_it)->getSourceRange().getBegin().printToString(
                            (*var_it)->getASTContext().getSourceManager()) +
                        ":" + (*var_it)->getNameAsString();

      const PTAVar *var_alias = _pvm.getPTAVar(key);
      if (!var_alias) continue;
 			std::string aliasStr = var_alias->get_instance_var_key();
			//cout<< "var_alias:"+aliasStr+"\n"<<endl;    
      if (var_alias->get_name() == var->get_name())
        continue;

      if (is_alias_of(*paths_it, 0, var, *paths_it, 0, var_alias)) {
        res.insert(var_alias);
      }
    }
    visited++;
  }

  return res;
}
/*
std::set<unsigned>
PointToAnalysis::get_pointee_of(std::string file_name, unsigned var_id,
                                unsigned func_id, unsigned block_id,
                                unsigned elem_id, unsigned flags) {
  std::set<unsigned> result;
  auto file_node_iter = _file_nodes.find(file_name);
  // if (file_node_iter == _file_nodes.end()) {
  //   std::cout << "Can not find file" << std::endl;
  //   return result;
  // }
  auto file_node = file_node_iter->second;
  auto func_node = file_node->get_func_node(func_id);
  // if (func_node == nullptr) {
  //   std::cout << "Can not find function" << std::endl;
  //   return result;
  // }
  auto var_node = file_node->get_var_node(var_id);
  // if (var_node == nullptr) {
  //   std::cout << "Can not find var" << std::endl;
  //   return result;
  // }

  // assert(VarNode::_PointerVar == var_node->get_kind());

  auto paths = func_node->get_all_path_to(func_node->get_exit_block_id());
  for (auto path : paths) {
    for (auto block_id : path) {
      std::cout << " " << block_id;
    }
    std::cout << std::endl;

    auto part_result =
        get_pointee_of(file_name, var_id, func_id, path, elem_id, flags);
    result.insert(part_result.begin(), part_result.end());
  }

  return result;
}

std::set<unsigned>
PointToAnalysis::get_pointee_of(std::string file_name, unsigned var_id,
                                unsigned func_id, std::list<unsigned> path,
                                unsigned elem_id, unsigned flags) {
  std::set<unsigned> result;
  auto file_node_iter = _file_nodes.find(file_name);
  // if (file_node_iter == _file_nodes.end()) {
  //   std::cout << "Can not find file" << std::endl;
  //   return result;
  // }
  auto file_node = file_node_iter->second;
  auto func_node = file_node->get_func_node(func_id);
  // if (func_node == nullptr) {
  //   std::cout << "Can not find function" << std::endl;
  //   return result;
  // }
  auto var_node = file_node->get_var_node(var_id);
  // if (var_node == nullptr) {
  //   std::cout << "Can not find var" << std::endl;
  //   return result;
  // }

  // assert(VarNode::_PointerVar == var_node->get_kind());
  auto pointer = (PointerVarNode *)var_node;
  // auto update_nodes = pointer->get_update_nodes(func_id, block_id);
  bool flow_sens = flags & _Flow_Sensitive;
  if (path.size()) {
    auto block_id = path.back();
    auto block_node = func_node->get_block_node(block_id);
    if (block_node->is_update_var(var_id)) {
      auto update_list = pointer->get_update_nodes(func_id, block_id);
      for (auto update_node_iter = update_list->rbegin();
           update_node_iter != update_list->rend(); ++update_node_iter) {
        auto update_node = *update_node_iter;
        // check elem_id only the last block
        if (elem_id > update_node->get_elem_id()) {
          switch (update_node->get_kind()) {
          case PointerUpdateNode::_Declaration: {
            return result;
          } break;
          case PointerUpdateNode::_AddrOfAssign: {
            result.insert(update_node->get_pointee_id());
            if (flow_sens)
              return result;
          } break;
          case PointerUpdateNode::_ImplicitCast: {
            auto part_result =
                get_pointee_of(file_name, update_node->get_alias_id(),
func_id, path, update_node->get_elem_id(), flags);
            result.insert(part_result.begin(), part_result.end());
            if (flow_sens)
              return result;
          } break;
          } // End of switch
        }
      } // End of for
    }
    path.pop_back();
  }
  while (path.size()) {
    auto block_id = path.back();
    auto block_node = func_node->get_block_node(block_id);
    if (block_node->is_update_var(var_id)) {
      auto update_list = pointer->get_update_nodes(func_id, block_id);
      for (auto update_node_iter = update_list->rbegin();
           update_node_iter != update_list->rend(); ++update_node_iter) {
        auto update_node = *update_node_iter;
        switch (update_node->get_kind()) {
        case PointerUpdateNode::_Declaration: {
          return result;
        } break;
        case PointerUpdateNode::_AddrOfAssign: {
          result.insert(update_node->get_pointee_id());
          if (flow_sens)
            return result;
        } break;
        case PointerUpdateNode::_ImplicitCast: {
          auto part_result =
              get_pointee_of(file_name, update_node->get_alias_id(), func_id,
                             path, update_node->get_elem_id(), flags);
          result.insert(part_result.begin(), part_result.end());
          if (flow_sens)
            return result;
        } break;
        } // End of switch
      }   // End of for
    }
    path.pop_back();
  } // End of while

  return result;
}

std::set<unsigned>
PointToAnalysis::get_alias_of(std::string file_name, unsigned var_id,
                              unsigned func_id, unsigned block_id,
                              unsigned elem_id, unsigned flags) {
  std::set<unsigned> result;
  return result;
}
*/
