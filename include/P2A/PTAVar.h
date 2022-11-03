#ifndef PTAVAR_H
#define PTAVAR_H
#include "PTAType.h"
#include "framework/ASTElement.h"
#include "iostream"
#include <unordered_map>
class PTAUpdateMap;

class PTAVar {
public:
  //
  PTAVar(std::string name, std::string identifier, const PTAType *ptaType)
      : _name(name), _identifier(identifier), _isField(false),
        _isInstanceField(false), _ptaType(ptaType){};

  PTAVar(std::string name, std::list<std::string> key, bool isInstanceField,
         const PTAType *ptaType)
      : _name(name), _varMapKey(key), _isField(true),
        _isInstanceField(isInstanceField), _ptaType(ptaType){};
  std::string get_name() const { return _name; }
  const PTAType *get_type() const { return _ptaType; }
  std::string get_instance_var_key() const {
    assert(!_isField);
    return _identifier;
  }
  std::list<std::string> get_field_var_key() const {
    assert(_isField);
    return _varMapKey;
  }
  std::list<std::string>
  trans_to_instance_field_var_key(std::list<std::string> instance) const {
    assert(_isField && !_isInstanceField);
    std::list<std::string> key(_varMapKey);
    key.pop_front();
    for (auto iter = instance.rbegin(); iter != instance.rend(); ++iter) {
      key.push_front(*iter);
    }
    return key;
  }
  std::list<std::string>
  trans_to_virtual_field_var_key(std::list<std::string> instance) const {
    assert(_isInstanceField);
    std::list<std::string> key(_varMapKey);
    key.pop_front();
    for (auto iter = instance.rbegin(); iter != instance.rend(); ++iter) {
      key.push_front(*iter);
    }
    return key;
  }
  bool isField() const { return _isField; }
  bool isVirtualField() const { return _isField && !_isInstanceField; }
  bool isInstanceField() const { return _isInstanceField; }
  bool isInstance() const { return !_isField || _isInstanceField; }

  bool operator==(const PTAVar &v) {
    return _identifier == v._identifier ? true : false;
  }

private:
  /// in PTAVarMap::_fieldVarMap , key == _varMapKey
  /// in PTAVarMap::_instanceFieldVarMap, key == _varMapKey
  ///       can call get_field_map_key()
  /// in PTAVarMap::_instacnceVarMap, key == _globalId
  std::string _name;
  std::string _identifier;
  const bool _isField;
  const bool _isInstanceField;
  unsigned _globalId;
  const PTAType *_ptaType;
  std::list<std::string> _varMapKey;
};

// class BuiltinPTAVar : public PTAVar {
// public:
// private:
// };

// class PointerPTAVar : public PTAVar {
// public:
// private:
// };

// class RecordPTAVar : public PTAVar {
// public:
// private:
// };

class PTAVarMap {
public:
  PTAVarMap(PTATypeMap &ptm) : _ptm(ptm) { _memoryAddressCount = 0; }
  ~PTAVarMap() {
    for (auto var : _virtualFieldVarMap) {
      delete (var.second);
    }
    for (auto var : _instanceFieldVarMap) {
      delete (var.second);
    }
    for (auto var : _instanceVarMap) {
      delete (var.second);
    }
  }
  // used in PointToAnalysis.cpp to modify info
  const PTAVar *insertPTAVar(const VarDecl *var_decl);
  const PTAVar *insertPTAVar(const MemberExpr *mem_expr);
  // used to insert the function return value, the name is function name, id is
  // function id
  const PTAVar *insertPTAVar(const FunctionDecl *func_decl);

  // used to insert malloc, new
  const PTAVar *insertPTAVar(std::string mallocFunc);
  void insertPTAVar(const PTAVar *pta_var);
  PTAUpdateMap *getPTAUpdateMap(const PTAVar *pta_var);

  // used in PointToAnalysisAlg.cpp and other file
  const PTAVar *getPTAVar(std::list<std::string> key);
  const PTAVar *getPTAVar(std::string key);
  // const PTAUpdateMap *getPTAUpdateMap(std::list<unsigned> key);
  const PTAUpdateMap *getPTAUpdateMap(std::string key);
  void dump() const;

private:
  // <RecordDecl.global_id + list of FieldDecl.global_id, >
  // std::map<std::list<int64_t>, const PTAVar *> _virtualFieldVarMap;
  std::map<std::list<std::string>, const PTAVar *> _virtualFieldVarMap;

  // <VarDecl.global_id + list of FieldDecl.global_id, >
  // std::map<std::list<int64_t>, const PTAVar *> _instanceFieldVarMap;
  std::map<std::list<std::string>, const PTAVar *> _instanceFieldVarMap;

  // <VarDecl.global_id, >
  // std::unordered_map<int64_t, const PTAVar *> _instanceVarMap;
  std::unordered_map<std::string, const PTAVar *> _instanceVarMap;

  //
  std::unordered_map<const PTAVar *, PTAUpdateMap *> _pum;
  PTATypeMap &_ptm;

  int _memoryAddressCount;
};

#endif // PTAVAR_H