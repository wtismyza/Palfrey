#ifndef PTATYPE_H
#define PTATYPE_H

#include <clang/AST/DeclCXX.h>
#include <map>
#include <string>
#include <vector>

using namespace clang;

class PTAType;

class PTAField {
public:
  PTAField() : _isField(false), _recordPTAType(nullptr) {}
  PTAField(std::string name, PTAType *recordPTAType)
      : _isField(true), _recordPTAType(recordPTAType) {}
  bool isField() const { return _isField; }
  const PTAType *getRecordPTAType() const { return _recordPTAType; }

private:
  bool _isField;
  PTAType *_recordPTAType;
};

class PTAType : public PTAField {
public:
  enum Kind { _BuiltinPTAType, _PointerPTAType, _ArrayPTAType, _RecordPTAType };

  PTAType(std::string name, Kind kind) : _name(name), _kind(kind){};
  std::string getName() const { return _name; }
  Kind getKind() const { return _kind; }

private:
  std::string _name;
  Kind _kind;
};

class BuiltinPTAType : public PTAType {
public:
  BuiltinPTAType(std::string name) : PTAType(name, _BuiltinPTAType){};

private:
};

class PointerPTAType : public PTAType {
public:
  PointerPTAType(std::string name) : PTAType(name, _PointerPTAType){};
  const PTAType *getPointeePTAType() const { return _pointeePTAType; }
  void setPointeePTAType(const PTAType *pointeePTAType) {
    _pointeePTAType = pointeePTAType;
  }

private:
  const PTAType *_pointeePTAType;
};

class ArrayPTAType : public PTAType {
public:
  ArrayPTAType(std::string name) : PTAType(name, _ArrayPTAType){};

private:
  PTAType *_elementPTAType;
  unsigned _size; // number of _elementPTAType
};

class RecordPTAType : public PTAType {
public:
  RecordPTAType(std::string name) : PTAType(name, _RecordPTAType){};
  void push_back(std::string fieldName, const PTAType *fieldPTAType) {
    _fieldName.push_back(fieldName);
    _fieldPTAType.push_back(fieldPTAType);
  }
  void resize(unsigned size) {
    _fieldName.resize(size, "");
    _fieldPTAType.resize(size, nullptr);
  }

private:
  std::vector<std::string> _fieldName;
  // _fields[FieldIndex]
  std::vector<const PTAType *> _fieldPTAType;
};

class PTATypeMap {
public:
  PTATypeMap() {
    BuiltinPTAType *pta_short = new BuiltinPTAType("short");
    BuiltinPTAType *pta_int = new BuiltinPTAType("int");
    BuiltinPTAType *pta_long = new BuiltinPTAType("long");
    BuiltinPTAType *pta_float = new BuiltinPTAType("float");
    BuiltinPTAType *pta_double = new BuiltinPTAType("double");
    insertPTAType(pta_short);
    insertPTAType(pta_int);
    insertPTAType(pta_long);
    insertPTAType(pta_float);
    insertPTAType(pta_double);
  };
  ~PTATypeMap() {
    for (auto ptaType : _ptaTypeMap) {
      delete ptaType.second;
    }
  }
  const PTAType *getPTAType(std::string name) {
    auto iter = _ptaTypeMap.find(name);
    if (iter == _ptaTypeMap.end())
      return nullptr;
    else
      return iter->second;
  };

  void insertPTAType(PTAType *ptaType) {
    _ptaTypeMap[ptaType->getName()] = ptaType;
  };

  const PTAType *insertPTAType(const CXXRecordDecl *cxxrecord_decl);
  const PTAType *insertPTAType(QualType type);

private:
  std::map<std::string, PTAType *> _ptaTypeMap;
};

#endif // PTATYPE_H