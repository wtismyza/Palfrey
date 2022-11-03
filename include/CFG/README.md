# about this CFG

If using this CFG, add the change 
`#include <clang/Analysis/CFG.h>`
to
`#include “CFG/SACFG.h”`

#### Get the `Decl` corresponding to CFG, this `Decl` is the `Decl` passed in when calling `buildCFG`

`const Decl* CFG::getParentDecl()`

#### Get CFG topological sort

`std::vector<CFGBlock *> CFG::getTopoOrder()`

#### Get the acyclic basic block successor, and the cycle splitting method is the same as the topological sort

`std::vector<CFGBlock *> CFG::getNonRecursiveSucc(CFGBlock *curBlock)`

#### Get the basic block for a function call

`std::set<CFGBlock *> CFG::getFuncCallBlock(FunctionDecl *funcCall)`  
Must use function call as basic block split

#### Split by function calls as basic blocks

Set `SplitBasicBlockwithFunCall` to `true` in `CFG` in the configuration file

#### Splitting New Statements as Basic Blocks

Set `SplitBasicBlockwithCXXNew` to `true` in `CFG` in the configuration file

#### Divide the Construct statement as the basic block

Set `SplitBasicBlockwithCXXConstruct` to `true` in `CFG` in the configuration file

#### Get the reachable path between two basic blocks

`std::vector< std::vector<CFGBlock *> > CFG::getReachablePath(CFGBlock *startBlock, CFGBlock* endBlock)`


# InterProcedureCFG

Please include the header file if you want to use it
`#include "CFG/InterProcedureCFG.h"`
Must use function call as basic block split

#### Get the predecessor and successor of the basic block in the global control flow graph

`std::vector<CFGBlock *> InterProcedureCFG::getPred(CFGBlock *block, bool isNoneRecursive = false)`  
`std::vector<CFGBlock *> InterProcedureCFG::getSucc(CFGBlock *block, bool isNoneRecursive = false)`
