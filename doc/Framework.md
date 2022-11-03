# StaticChecker Documentation

## Content provided by Framework（in the BasicChecker）


1.  ASTManager manager:

    1. `FunctionDecl *getFunctionDecl(ASTFunction *F)`：Through `ASTFunction`（the function structure in the Framework）to obtain `FD`（the function structure in Clang）

    2. `ASTVariable *getASTVariable(VarDecl *VD)`，`VarDecl *getVarDecl(ASTVariable *V)`：mutual conversion between `VarDecl` and `ASTVariable`

    3. `std::unique_ptr<CFG> &getCFG(ASTFunction *F)`：Through `ASTFunction` to obtain CFG.
    `std::unique_ptr<CFG> functionCFG = std::unique_ptr<CFG>(CFG::buildCFG(FD, FD->getBody(), &FD->getASTContext(), CFG::BuildOptions()));` is another method to get CFG，in which FD belongs to FunctionDecl.
    
    4. `VarDecl *getVarDecl(ASTVariable *V)` and `ASTUnit *getASTUnit(ASTFile *AF)` are used for class conversion.
    
    5. `std::vector<ASTGlobalVariable *> getGlobalVars(bool uninit = false)`：When uninit is false, get all global variables `ASTGlobalVariable`; when it is true, get global variables without explicit initialization
        + The global variable represented by `ASTGlobalVariable` refers to a variable that is not declared in a function body. If a static variable is declared in a function body, although the variable symbol is also in the global area after compilation in a general environment, it is not an `ASTGlobalVariable`, but an `ASTVariable`
    
    6. `ASTGlobalVariable *getASTGlobalVariable(VarDecl *GVD)`，`VarDecl *getGlobalVarDecl(ASTGlobalVariable *GV)`： `VarDecl` and `ASTGlobalVariable` convert to each other, note that `VarDecl` here is the `VarDecl` of global variables.

2. the functions in namespace Common

    1. `Stmt *getStmtInFunctionWithID(FunctionDecl *parent, int64_t id)`。Used to get the actual `Stmt` of the call site whose parent's ID is `id`. It can also be used in other situations, but when using it, please ensure that the `Stmt` represented by the `id` is the child node in the `FunctionDecl` represented by the parent. Returns nullptr if not found.

    2. `bool isThisCallSiteAFunctionPointer(Stmt *callsite)` Determines whether a call site is a function call caused by a function pointer. Please **ensure that the incoming parameters are valid**, that is, not empty and indeed a call site

3. CallGraph call_graph：
   
    1. `std::vector<ASTFunction *> &getTopLevelFunctions()`：Parse out all top-level functions, i.e. functions without callers (eg main)

    2.  `ASTFunction *getASTFunction(FunctionDecl *FD) `：Obtain ASTFunction (the Framework's function structure) through FD (Clang's function structure)

    3.  `std::vector<ASTFunction *> getParents(ASTFunction *F)`, `std::vector<ASTFunction *> getChildren(ASTFunction *F)`：Get the callee and caller of a function

    4.  `std::vector<std::pair<ASTFunction *, int64_t>> getParentsWithCallsite()(ASTFunction *F)`, `std::vector<std::pair<ASTFunction *, int64_t>> getChildrenWithCallsite(ASTFunction *F)`：Get the callee and caller of a function, and get the corresponding call point information. The call point is given in the form of `Stmt` `ID`. You can use the `getStmtInFunctionWithID(FunctionDecl *parent, int64_t id)` method in `Common.cpp` to get the actual `Stmt`

    5. Before using the functions in 3 and 4 above, please make sure that the incoming `ASTFunction` has corresponding nodes in `CallGraph`. Please specify the relevant configuration items of `CallGraph` before use; **Please ensure that the incoming `ASTFunction *` is valid**.

    6. **After obtaining FunctionDecl through ASTFunction, please perform bounds check**, such as judging whether it is `nullptr`, using `hasBody()` to detect whether the FunctionDecl has a function body, etc.

    7. For the function pointer `FP`, the functions it may point to will be added to the `Call graph`. For a particular call site, if the function pointer `FP` may point to functions `funA` and `funB` at this time, the result of `getChildrenWithCallsite` will include `<funA, call site>` and `<funB, call site>`.

    8. The result of the call site getting the actual `Stmt *` via `getStmtInFunctionWithID` is not necessarily `CallExpr *`. May be:
        1. `CXXConstructExpr`: This call site calls the constructor
        2. `CXXDeleteExpr`: This call site calls the destructor
        3. `CXXTemporaryObjectExpr`: A subclass of `CXXConstructExpr`, which can be processed in the way of `CXXConstructExpr`
        4. `CallExpr`: General function call
        5. `CXXOperatorCallExpr`: Operator overloading. Subclass of `CallExpr`. The call site of a lambda expression (anonymous function) is also in this form. After obtaining the `FunctionDecl` called by `getDirectCallee`, it can be judged whether it is a `CXXMethodDecl`. If so, use `getPatent` to get the `CXXRecordDecl` where it is located. , use the `isLambda` function to judge.
        6. `CXXMemberCallExpr`: Class member function calls. Subclass of `CallExpr`.

    9. The meanings of each configuration item in CallGraph are as follows:

        - `showDestructor`: Whether to include destructors. If false, the corresponding `CallGraph` node will not be created for the `ASTFunction` corresponding to the destructor and added to the `Callgraph`; if true, the corresponding `CallGraph` node will be created for the corresponding `ASTFunction` and added to the `Callgraph`

        - `showFunctionPtr`: Whether to display information about function pointers. If true, the function pointer information will be considered in the `Callgraph` construction process; if false, the `CallGraph` will not include any function pointer related information

        - `showLambda`: Whether to include lambda expressions. If true, `CallGraph` will include lambda expressions (information about anonymous functions; if false, lambda expressions are ignored)

        - `inlineAndTemplate`: Whether to include inline functions and Template functions. If it is true, the CallGraph will include the information of the inline function and the Template function; if it is false, these functions will be ignored during the CallGraph generation process.

        - `showLibFunc`: Whether to include third-party library functions (functions not defined in the source code). If it is true, CallGraph will retain the information of these third-party library functions **called**; if it is false, CallGraph will ignore these information; whether the configuration item is true or false, CallGraph will not consider these third parties **Call information** of the library function.

        - `ignoreNoCalledSystemHeader`: Whether to ignore functions in system header files that are not called. If true, a function in the system header file (such as `printf` in `stdio.h`), that is not called in the entire Call graph, will be eliminated after the Call graph is constructed.

4. NonRecursiveCallGraph NoRCallGraph: The ring in the existing CallGraph, that is, the CallGraph after the recursive call relationship is eliminated. If the recursive call form is A->B->C->A->B->C..., only one layer of loop will be retained, that is, A->B->C

    1.  `std::set<ASTFunction *> &getParents(ASTFunction *F)`, `std::set<ASTFunction *> &getChildren(ASTFunction *F)`：Get the callee and caller of a function


5. Config configure: Used to read configuration files. For specific use, please refer to `TemplateChecker::readConfig().`
6. ASTResource resource：

    1. `std::vector<ASTFunction *> &getFunctions(bool use)`：Get all `ASTFunctions`

    2. `std::vector<ASTFile *> getASTFiles()`：Get all `ASTFiles`
    
    3. `std::vector<ASTGlobalVariable *> &getGlabalVars() `：Get all global variables `ASTGlobalVariable`
7. **Point-to/alias analysis**: Please refer to [Readme.md](../include/README.md) **(still editing)**

## BasicChecker

To put it simply, `BasicChecker` stores the classes provided by the `Framework` so that we can call it during analysis. All its subclasses can be analyzed based on the functions provided by this `Framework`. 

## Framework-independent analysis

The example is as follows (only serves as illustration, and some APIs need to be replaced)：
``` C++
class GlobalVarDecl : public ASTConsumer, public RecursiveASTVisitor<GlobalVarDecl> {

    public :
    // TraverseDecl tells the frontend library's ASTConsumer to 
    // visit declarations recursively from the AST. Then VisitDecl is 
    // called where you can extract the relevant information.
    void HandleTranslationUnit(ASTContext &Context) override {
        TranslationUnitDecl *decl = Context.getTranslationUnitDecl();
        TraverseDecl(decl); 
    }
    bool VisitDecl(Decl *D)
    {
        if(ParmVarDecl *E = dyn_cast<ParmVarDecl>(D))
            return true;
        if(VarDecl *E = dyn_cast<VarDecl>(D))
        {
            list<VarInfo>::iterator varIterator;
            if(!isStaticVar(E->getQualifiedNameAsString(), &varIterator) && !E->isLocalVarDecl())
            {
                // ...
            }

        }
        return true;
    }
    list<VarInfo>* getVars(){
        // ...
    }
};

void Foo::check(){
    // getASTs is to get all ASTContexts
    for (const ASTContext &AST : getASTs()) {
        GlobalVarDecl globVar;
        globVar.HandleTranslationUnit(*AST);
        list<VarInfo>* tmp = globVar.getVars();
    }
```

For APIs similar to `TraverseDecl` and `VisitDecl`, please refer to [RecursiveASTVisitor](http://clang.llvm.org/doxygen/classclang_1_1RecursiveASTVisitor.html). Other APIs such as[FunctionDecl](https://clang.llvm.org/doxygen/classclang_1_1FunctionDecl.html) can refer to the corresponding documentation.
