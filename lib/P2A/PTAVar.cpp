#include "P2A/PTAVar.h"
#include "P2A/Node.h"
#include <iostream>

const PTAVar *PTAVarMap::insertPTAVar(const FunctionDecl *func_decl) {
  auto type = _ptm.insertPTAType(func_decl->getDeclaredReturnType());
  std::string declLoc = func_decl->getSourceRange().getBegin().printToString(
                            func_decl->getASTContext().getSourceManager()) +
                        ":" + func_decl->getNameAsString();
  auto iter = _instanceVarMap.find(declLoc);
  if (iter != _instanceVarMap.end()) {
    if (iter->second->get_name() != func_decl->getNameAsString()) {
      std::string red = "\033[31m";
      std::string close = "\033[0m";
      std::cout << red << "[!] there is a duplicate in PTAVarMap" << std::endl
                << "[!] loc is: " << declLoc << std::endl
                << "[!] var in var map is: " << iter->second->get_name()
                << std::endl
                << "[!] current var is: " << func_decl->getNameAsString()
                << close << std::endl;
      assert(0);
    }
    return iter->second;
  }
  auto var = new PTAVar(func_decl->getNameAsString(), declLoc, type);
  _instanceVarMap[declLoc] = var;
  return var;
}

const PTAVar *PTAVarMap::insertPTAVar(const VarDecl *var_decl) {
  auto type = _ptm.insertPTAType(var_decl->getType());
  std::string declLoc = var_decl->getSourceRange().getBegin().printToString(
                            var_decl->getASTContext().getSourceManager()) +
                        ":" + var_decl->getNameAsString();
  auto iter = _instanceVarMap.find(declLoc);
  if (iter != _instanceVarMap.end()) {
    if (iter->second->get_name() != var_decl->getNameAsString()) {
      std::string red = "\033[31m";
      std::string close = "\033[0m";
      std::cout << red << "[!] there is a duplicate in PTAVarMap" << std::endl
                << "[!] loc is: " << declLoc << std::endl
                << "[!] var in var map is: " << iter->second->get_name()
                << std::endl
                << "[!] current var is: " << var_decl->getNameAsString()
                << close << std::endl;
      assert(0);
    }
    return iter->second;
  }
  auto var = new PTAVar(var_decl->getNameAsString(), declLoc, type);
  _instanceVarMap[declLoc] = var;
  return var;
}

const PTAVar *PTAVarMap::insertPTAVar(const MemberExpr *mem_expr) {
  auto type = _ptm.insertPTAType(mem_expr->getType());

  std::list<std::string> bases;
  // auto field_decl = dyn_cast<FieldDecl>(mem_expr->getMemberDecl());
  // bases.push_front(field_decl->getID());
  // std::cout << field_decl->getID() << std::endl;
  // field_decl->dumpColor();
  bool isInstance = true;
  const Expr *base = mem_expr;
  while (true) {
    if (auto me = dyn_cast<MemberExpr>(base)) {
      auto fd = dyn_cast<FieldDecl>(me->getMemberDecl());
      std::string declLoc = fd->getSourceRange().getBegin().printToString(
                                fd->getASTContext().getSourceManager()) +
                            ":" + fd->getNameAsString();
      bases.push_front(declLoc);
      base = me->getBase();
    } else if (auto ice = dyn_cast<ImplicitCastExpr>(base)) {
      base = ice->getSubExpr();
    } else if (Stmt::CXXThisExprClass == base->getStmtClass()) {
      if (base->getType()->getPointeeCXXRecordDecl() == nullptr)
        break;
      isInstance = false;
      std::string declLoc =
          base->getType()
              ->getPointeeCXXRecordDecl()
              ->getSourceRange()
              .getBegin()
              .printToString(base->getType()
                                 ->getPointeeCXXRecordDecl()
                                 ->getASTContext()
                                 .getSourceManager()) +
          ":" + base->getType()->getPointeeCXXRecordDecl()->getNameAsString();
      bases.push_front(declLoc);
      break;
    } else if (auto dre = dyn_cast<DeclRefExpr>(base)) {
      std::string declLoc =
          dre->getDecl()->getSourceRange().getBegin().printToString(
              dre->getDecl()->getASTContext().getSourceManager()) +
          ":" + dre->getDecl()->getNameAsString();
      bases.push_front(declLoc);
      break;
    } else {
      break;
    }
  }
  if (isInstance) {
    auto iter = _instanceFieldVarMap.find(bases);
    if (iter != _instanceFieldVarMap.end()) {
      return iter->second;
    }
  } else {
    auto iter = _virtualFieldVarMap.find(bases);
    if (iter != _virtualFieldVarMap.end()) {
      return iter->second;
    }
  }
  auto var = new PTAVar(mem_expr->getMemberDecl()->getNameAsString(), bases,
                        isInstance, type);
  if (isInstance) {
    _instanceFieldVarMap[bases] = var;
  } else {
    _virtualFieldVarMap[bases] = var;
  }
  return var;
}

void PTAVarMap::insertPTAVar(const PTAVar *pta_var) {
  if (pta_var->isField()) {
    if (pta_var->isInstanceField()) {
      _instanceFieldVarMap[pta_var->get_field_var_key()] = pta_var;
    } else {
      _virtualFieldVarMap[pta_var->get_field_var_key()] = pta_var;
    }
  } else {
    _instanceVarMap[pta_var->get_instance_var_key()] = pta_var;
  }
}

const PTAVar *PTAVarMap::insertPTAVar(std::string mallocFunc) {
  const PTAType *type = nullptr;
  std::string declLoc = "MemoryAddress" + std::to_string(_memoryAddressCount);
  _memoryAddressCount++;
  auto iter = _instanceVarMap.find(declLoc);
  if (iter != _instanceVarMap.end()) {
    return iter->second;
  }
  auto var = new PTAVar(declLoc, declLoc, type);
  _instanceVarMap[declLoc] = var;
  return var;
}

PTAUpdateMap *PTAVarMap::getPTAUpdateMap(const PTAVar *pta_var) {
  auto iter = _pum.find(pta_var);
  if (iter == _pum.end()) {
    return _pum[pta_var] = new PTAUpdateMap(pta_var);
  } else {
    return iter->second;
  }
}

const PTAVar *PTAVarMap::getPTAVar(std::list<std::string> key) {
  auto iter = _instanceFieldVarMap.find(key);
  if (iter != _instanceFieldVarMap.end()) {
    return iter->second;
  }
  return nullptr;
}

const PTAVar *PTAVarMap::getPTAVar(std::string key) {
  auto iter = _instanceVarMap.find(key);
  if (iter != _instanceVarMap.end()) {
    return iter->second;
  }
  return nullptr;
}

const PTAUpdateMap *PTAVarMap::getPTAUpdateMap(std::string key) {
  auto pta_var = getPTAVar(key);
  if (nullptr == pta_var) {
    return nullptr;
  }
  auto iter = _pum.find(pta_var);
  if (iter == _pum.end()) {
    return nullptr;
  } else {
    return iter->second;
  }
}

void PTAVarMap::dump() const {
  std::cout << "######## PATUpdateMap ########" << std::endl;
  std::cout << "      -- VirtualFieldVar --" << std::endl;
  for (auto fv : _virtualFieldVarMap) {
    auto iter = fv.first.begin();
    std::cout << *iter;
    ++iter;
    while (iter != fv.first.end()) {
      std::cout << "." << *iter;
      ++iter;
    }
    std::cout << " : " << fv.second->get_name() << std::endl;
    auto pum = _pum.find(fv.second);
    if (pum != _pum.end()) {
      pum->second->dump();
    }
  }
  std::cout << "      -- InstanceFieldVar -- " << std::endl;
  for (auto ifv : _instanceFieldVarMap) {
    auto iter = ifv.first.begin();
    std::cout << *iter;
    ++iter;
    while (iter != ifv.first.end()) {
      std::cout << "." << *iter;
      ++iter;
    }
    std::cout << " : " << ifv.second->get_name() << std::endl;
    auto pum = _pum.find(ifv.second);
    if (pum != _pum.end()) {
      pum->second->dump();
    }
  }
  std::cout << "      -- InstanceVar --" << std::endl;
  for (auto iv : _instanceVarMap) {
    std::cout << iv.first << " : " << iv.second->get_name() << std::endl;
    auto pum = _pum.find(iv.second);
    if (pum != _pum.end()) {
      pum->second->dump();
    }
  }
}