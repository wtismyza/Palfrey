#include "P2A/PTAType.h"

#include <iostream>

const PTAType *PTATypeMap::insertPTAType(const CXXRecordDecl *cxxrecordDecl) {
  auto name = cxxrecordDecl->getNameAsString();
  auto iter = _ptaTypeMap.find(name);
  if (iter != _ptaTypeMap.end())
    return iter->second;

  RecordPTAType *rec = new RecordPTAType(name);
  // std::cout << "insertPTAType " << name << ":" << std::endl;
  for (auto field : cxxrecordDecl->fields()) {
    auto ptaType = insertPTAType(field->getType());
    std::cout << field->getNameAsString() << std::endl;
    rec->push_back(field->getNameAsString(), ptaType);
    // std::cout << field->getFieldIndex() << ": " <<
    // field->getNameAsString()
    //           << std::endl;
    // field->getFieldIndex()
    if (field->getType()->isPointerType()) {
      // ToDo
    } else {
    }
  }
  _ptaTypeMap[name] = rec;
  return rec;
}

const PTAType *PTATypeMap::insertPTAType(QualType type) {
  auto name = type.getAsString();
  auto iter = _ptaTypeMap.find(name);
  if (iter != _ptaTypeMap.end())
    return iter->second;

  switch (type->getTypeClass()) {
  case Type::Builtin: {
    BuiltinPTAType *bui = new BuiltinPTAType(name);
    _ptaTypeMap[name] = bui;
    return bui;
  }
  case Type::Pointer: {
    PointerPTAType *poi = new PointerPTAType(name);
    auto pointeeType = type->getPointeeType();
    auto iter = _ptaTypeMap.find(pointeeType.getAsString());
    const PTAType *pointeePTAType;
    if (iter == _ptaTypeMap.end()) {
      pointeePTAType = insertPTAType(pointeeType);
    } else {
      pointeePTAType = iter->second;
    }
    poi->setPointeePTAType(pointeePTAType);
    _ptaTypeMap[name] = poi;
    return poi;
  }
  case Type::Record: {
    auto recordDecl =
        static_cast<const CXXRecordDecl *>(type->getAsRecordDecl());
    // return insertPTAType(recordDecl);
    return nullptr;
  }
    // Array
  default:;
  }
  return nullptr;
}