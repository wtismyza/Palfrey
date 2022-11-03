# Point-to analysis


#### Get pointer information for a pointer in a given path

`std::set<const PTAVar *> PointToAnalysis::get_pointee_of_l3(std::list<const CFGBlock *> path, unsigned elem_id, const PTAVar *var)`

#### Get the pointer information at the given path end

`std::set<const PTAVar *> PointToAnalysis::get_pointee_at_point(CFGBlock *pathEnd, unsigned elem_id, const PTAVar *var, unsigned pathLen = 10)`

#### Check if two variables are aliases

`bool PointToAnalysis::is_alias_of(std::list<const CFGBlock *> path_1, unsigned elem_id_1, const PTAVar *var_1, std::list<const CFGBlock *> path_2, unsigned elem_id_2, const PTAVar *var_2)`

#### Get the alias relationship of a given variable within a given function

`std::set<const PTAVar *> PointToAnalysis::get_alias_in_func(FunctionDecl *FD, const PTAVar *var)`  

#### Determine if a variable is an alias at a point in the program

`bool PointToAnalysis::is_alias_of_at_point(CFGBlock *pathEnd, unsigned elem_id, const PTAVar *var_1, const PTAVar *var_2, int pathLen)`  

#### Gets the internal data structure pointing to the analysis variable by identifier

`const PTAVar *PTAVarMap::getPTAVar(std::string key)`
