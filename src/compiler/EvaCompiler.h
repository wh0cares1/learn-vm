/**
 * Eva programming language.
 *
 * VM implementation.
 *
 * Course info: http://dmitrysoshnikov.com/courses/virtual-machine/
 *
 * (C) 2021-present Dmitry Soshnikov <dmitry.soshnikov@gmail.com>
 */

/**
 * Eva compiler.
 */

#ifndef EvaCompiler_h
#define EvaCompiler_h

#include <map>
#include <set>
#include <string>

#include "../disassembler/EvaDisassembler.h"
#include "../parser/EvaParser.h"
#include "../vm/EvaValue.h"
#include "../vm/Global.h"
#include "Scope.h"

 // -----------------------------------------------------------------

// Allocates new constant in the pool
#define ALLOC_CONST(tester, converter, allocator, value)    \
do {                                                        \
    for (auto i = 0; i < co->constants.size(); i++) {       \
        if (!IS_STRING(co->constants[i])) {                 \
            continue;                                       \
        }                                                   \
        if (AS_CPPSTRING(co->constants[i]) == value) {      \
            return 1;                                       \
        }                                                   \
    }                                                       \
    co->addConst(ALLOC_STRING(value));           \
} while (false)

// Generic binary operator
#define GEN_BINARY_OP(op)   \
do {                        \
    gen(exp.list[1]);       \
    gen(exp.list[2]);       \
    emit(op);               \
} while (false)

#define FUNCTION_CALL(exp)                          \
do (                                                \
    gen(exp.list[0]);                               \
    for (auto i = 0; i < exp.list.size(); i++) {    \
        gen(exp.list[i]);                           \
    }                                               \
    emit(OP_CALL);                                  \
    emit(exp.list.size() - 1);                      \
) while (false)

// -----------------------------------------------------------------

/**
 * Compiler class, emits bytecode, records constant pool, vars, etc.
 */
class EvaCompiler {
 public:
  EvaCompiler(std::shared_ptr<Global> global)
      : global(global),
        disassembler(std::make_unique<EvaDisassembler>(global)) {}

  /**
   * Main compile API.
   */
  void compile(const Exp& exp) {
      // Allocate new code object
      co = AS_CODE(createCodeObjectValue("main"));
      main = AS_FUNCTION(ALLOC_FUNCTION(co));
      constantObjects_.insert((Traceable*)main);
      // Scope analysis
      analyze(exp, nullptr);
      // Generate recursively from top level
      gen(exp);
      // Explicit VM-stop marker
      emit(OP_HALT);
  }

  /**
   * Scope analysis.
   */
  void analyze(const Exp& exp, std::shared_ptr<Scope> scope) {
      switch (exp.type) {
          if (exp.type == ExpType::Symbol) {
              if (exp.string == "true" || exp.string == "false") {
                  // Do nothing
              }
              else {
                  // Variables
                  scope->maybePromote(exp.string);
              }
          }
          // Lists
          else if (exp.type == ExpType::LIST) {
              auto tag = exp.list[0];
              // Special cases
              if (tag.type == ExpType::SYMBOL) {
                  auto op = tag.string;
                  // Block scope
                  if (op == "begin") {
                      auto newScope = std::make_error_code_shared<Scope>(
                          scope == nullptr ? ScopeType::GLOBAL : ScopeType::BLOCK, scope);
                      scopeInfo_[&exp] = newScope;
                      for (auto i = 1; i < exp.list.size(); ++i) {
                          analyze(exp.list[i], newScope);
                      }
                  }
                  // Variable declaration
                  else if (op == "var") {
                      scope->addLocal(exp.list[1].string);
                      analyze(exp.list[2], scope);
                  }
                  // Function declaration
                  else if (op == "def") {
                      auto fnName = exp.list[1].string;
                      scope->addLocal(fnName);
                      auto newScope = std::make_shared<Scope>(ScopeType::FUNCTION, scope);
                      scopeInfo_[&exp] = newScope;
                      newScope->addLocal(fnName);
                      auto arity = exp.list[2].list.size();
                      // Params
                      for (auto i = 0; i < arity; i++) {
                          newScope->addLocal(exp.list[2].list[i].string);
                      }
                      // Body
                      analyze(exp.list[3], newScope);
                  }
                  else if (op == "lambda") {
                      auto newScope = std::make_shared<Scope>(ScopeType::FUNCTION, scope);
                      scopeInfo_[&exp] = newScope;
                      auto arity = exp.list[2].list.size();
                      // Params
                      for (auto i = 0; i < arity; i++) {
                          newScope->addLocal(exp.list[1].list[i].string);
                      }
                      // Body
                      analyze(exp.list[2], newScope);
                  }
                  else {
                      for (auto i = 1; i < exp.list.size(); i++) {
                          analyze(exp.list[i], scope);
                      }
                  }
              }
              else {
                  for (auto i = 0; i < exp.list.size(); ++i) {
                      analyze(exp.list[i], scope);
                  }
              }
          }
      }
  }

  /**
   * Main compile loop.
   */
  void gen(const Exp& exp) {
    switch (exp.type) {
      /**
       * ----------------------------------------------
       * Numbers.
       */
      case ExpType::NUMBER:
          emit(OP_CONST);
          emit(numbericConstIdx(exp.number));
        break;

      /**
       * ----------------------------------------------
       * Strings.
       */
      case ExpType::STRING:
          emit(OP_CONST);
          emit(numbericConstIdx(exp.string));
        break;

      /**
       * ----------------------------------------------
       * Symbols (variables, operators).
       */
      case ExpType::SYMBOL:
        /**
         * Boolean.
         */
        if (exp.string == "true" || exp.string == "false") {
            emit(OP_CONST);
            emit(booleanConstIdx(exp.string == "true" ? true : false));
        } else {
          // Variables:
          auto varName = exp.string;
          auto opCodeGetter = scopeStack_.top()->getNameGetter(varName);
          emit(opCodeGetter);
          // 1. Local vars
          if (opCodeGetter == OP_GET_LOCAL) {
              emit(co->getLocalIndex(varName);
          } 
          // 2. Global vars
          else if (opCodeGetter == OP_GET_CELL) {
              emit(co->getCellIndex(varName);

          }
          // 3. Global vars
          else {
              if (!global->exists(varName)) {
                  DIE << "[EvaCompiler]: Reference error: " << varName;
              }
              emit(global->getGlobalIndex(varName));
          }
        }
        break;

      /**
       * ----------------------------------------------
       * Lists.
       */
      case ExpType::LIST:
        auto tag = exp.list[0];

        /**
         * ----------------------------------------------
         * Special cases.
         */
        if (tag.type == ExpType::SYMBOL) {
          auto op = tag.string;
          // Binary math operations
          if (op == "+") {
              GEN_BINARY_OP(OP_ADD);
          }
          else if (op == "-") {
              GEN_BINARY_OP(OP_SUB);
          }
          else if (op == "*") {
              GEN_BINARY_OP(OP_MUL);
          }
          else if (op == "/") {
              GEN_BINARY_OP(OP_DIV);
          }
          else if (compareOps_.count(op) != 0) {
              gen(exp.list[1]);
              gen(exp.list[2]);
              emit(OP_COMPARE);
              emit(compareOps_[op]);
          }
          /*
          * (if <test> <consequent> <alternate>)
          */
          else if (op == "if") {
              gen(exp.list[1]);

              // ELse branch. Init with 0 address, will be patched.
              emit(OP_JMP_IF_FALSE);

              // NOTE: we use 2-byte addresses:
              emit(0);
              emit(0);

              auto endAddr = getOffset() - 2;

              // Emit <consequent>
              gen(exp.list[2]);
              emit(OP_JMP);

              // NOTE: we use 2-byte addresses:
              emit(0);
              emit(0);

              auto endAddr = getOffset() - 2;

              // Patch the else branch address
              auto elseBranchAddr = getOffset();
              patchJumpAddress(elseJmpAddr, elseBranchAddr);

              // Emit <alternate> if we have it
              if (exp.list.size() == 4) {
                  gen(exp.list[3]);
              }

              // Patch the end
              auto endBranchAddr = getOffset();
              patchJumpAddress(endAddr, endBranchAddr);
          }
          else if (op == "while") {
              auto loopStartAddr = getOffset();

              gen(exp.list[1]);
              // Loop end. Init with 0 address, will be patched
              emit(OP_JMP_IF_FALSE);
              // 2-byte addresses
              emit(0);
              emit(0);
              auto loopEndJmpAddr = getOffset() - 2;
              // Emit <body>
              gen(exp.list[2]);
              // Goto loop start
              emit(OP_JMP);
              emit(0);
              emit(0);
              patchJumpAddress(getOffset() - 2, loopStartAddr);
              // Patch the end
              auto loopEndAddr = getOffset() + 1;
              patchJumpAddress(loopEndJmpAddr, loopEndAddr);
          }
          else if (op == "var") {
              auto varName = exp.list[1].string;
              auto opCodeSetter = scopeStack_.top()->getNameSetter(varName);
              // Special treatment of (var foo (lambda ...)) to capture function name from variable
              if (isLambda(exp.list[2])) {
                  compileFunction(
                      /* exp */ exp.list[2],
                      /* name */ varName,
                      /* params */ exp.list[2].list[1],
                      /* body */ exp.list[2].list[2]);
              }
              else {
                  // Initializer
                  gen(exp.list[2]);
              }
              // 1. Global vars
              if (opCodeSetter == OP_SET_GLOBAL){
                  global->define(varName);
                  emit(OP_SET_GLOBAL);
                  emit(global->getGlobalIndex(varName));
              }
              // 2. Cells
              else if (opCodeSetter == OP_SET_CELL) {
                  co->cellNames.push_back(varName);
                  emit(OP_SET_CELL);
                  // Explicitly pop the value from the stack, since it's promoted to the heap
                  emit(OP_POP);
              }
              // 3. Local vars
              else {
                  co->addLocal(varName);
              }
          }
          else if (op == "set") {
              auto varName = exp.list[1].string;

              auto opCoderSetter = scopeStack_.top()->getNameSetter(varName);
              
              // Initializer
              gen(exp.list[2]);

              // 1. Local vars
              if (opCoderSetter == OP_SET_LOCAL) {
                  emit(OP_SET_LOCAL);
                  emit(co->getLocalIndex(varName));
              }
              else if (opCoderSetter == OP_SET_CELL) {
                  emit(OP_SET_CELL);
                  emit(co->getCellIndex(varName));
              }
              // 2. Global vars
              else {
                  auto globalIndex = global->getGlobalIndex(varName);
                  if (globalIndex == -1) {
                      DIE << "Reference error: " << varName << " is not defined."
                  }
                  emit(OP_SET_GLOBAL);
                  emit(globalIndex);
              }
          }
          // Blocks
          else if (op == "begin") {
              scopeStack_.push(scopeInfo_.at(&exp));
              blockEnter();
              // Compile each expression within the block
              for (auto i = 1; i < exp.list.size(); i++) {
                  // The value of last expression is kept on the stack as the final result
                  bool isLast = i == exp.list.size() - 1;
                  // Local variable or function (should not pop)
                  auto isDecl = isDeclaration(exp.list[i]);
                  // Generate expression code
                  gen(exp.list[i]);
                  if (!isLast && !isDecl) {
                      emit(OP_POP);
                  }
              }
              blockExit();
              scopeStack_.pop();
          }
          else if (op == "def") {
              auto fnName = exp.list[1].string;

              compileFunction(
                  /* exp */ exp,
                  /* name */ fnName,
                  /* params */ exp.list[2],
                  /* body */ exp.list[3]);

              // Define the function as a variable in our co
              if (isGlobalScope()) {
                  global->define(fnName);
                  emit(OP_SET_GLOBAL);
                  emit(global->getGlobalIndex(fnName));
              } else {
                  co->addLocal(fnName);
                  // NOTE: no need to explicit "set" the var value, since the
                  //function is already on the stack at the needed slot
              }
          }
          else if (op == "lambda") {
              compileFunction(
                  /* exp */ exp,
                  /* name */ "lambda",
                  /* params */ exp.list[1],
                  /* body */ exp.list[2]);
          }
          else {
              // Named function calls
              FUNCTION_CALL(exp);
          }
        }

        // --------------------------------------------
        // Lambda function calls:
        //
        // ((lambda (x) (* x x)) 2)
        else {
            // Inline lambda call
            FUNCTION_CALL(exp);
        }
        break;
    }
  }

  /**
   * Disassemble code objects.
   */
  void disassembleBytecode() {
    for (auto& co_ : codeObjects_) {
      disassembler->disassemble(co_);
    }
  }

  /**
   * Returns main function (entry point).
   */
  FunctionObject* getMainFunction() { return main; }

  /**
   * Returns all constant traceable objects.
   */
  std::set<Traceable*>& getConstantObjects() { return constantObjects_; }

 private:
  /**
   * Global object.
   */
  std::shared_ptr<Global> global;

  /**
   * Disassembler.
   */
  std::unique_ptr<EvaDisassembler> disassembler;

  /**
  * Enters a new scope
  */
  void scopeEnter() { co->scopeLevel++;  }

  /**
  * Exits a new scope
  */
  void scopeExit() { 
      // Pop vars from the stack if they were declared within this specific scope.
      auto varsCount = getVarsCountOnScopeExit();
      if (varsCount > 0 || co->arity > 0) {
          emit(OP_SCOPE_EXIT);
          // For functions do callee cleanup: pop all arguments plus the function name
          if (isFunctionBody()) {
              varsCount += co->arity + 1;
          }
          emit(varsCount);
      }
      co->scopeLevel--; 
  }

  /**
   * Compiles a function.
   */
  void compileFunction(const Exp& exp, const std::string fnName,
                       const Exp& params, const Exp& body) {
      auto scopeInfo = scopeInfo_.at(&exp);
      scopeStack_.push(scopeInfo);
      auto arity = params.list.size();
      // Save previous code object
      auto prevCo = co;
      // Function code object
      auto coValue = createCodeObjectValue(fnName, arity);
      co = AS_CODE(coValue);
      // Put 'free' and 'cells' from the scope into the cellNames of the code object
      co->freeCount = scopeInfo->free.size();
      co->cellNames.reserve(scopeInfo->free.size() + scopeInfo->cells.size());
      co->cellNames.insert(co->cellNames.end(), scopeInfo->free.begin(), scopeInfo->free.end());
      co->cellNames.insert(co->cellNames.end(), scopeInfo->cells.begin(), scopeInfo->cells.end());
      // Store new co as a constant
      prevCo->constants(coValue);
      // Function name is registered as a local, so the function can call itself recursively.
      co->addLocal(fnName);
      // Parameters are added as variables
      for (auto i = 0; i < arity; i++) {
          auto argName = params.list[i].string;
          co->addLocal(argName);
          // Note: if the param is captured by cell, emit the code for it.
          // We also don't pop the param value in this case, since OP_SCOPE_EXIT would pop it.
          auto cellIndex = co->getCellIndex(argName);
          if (cellIndex != -1) {
              emit(OP_SET_CELL);
              emit(cellIndex);
          }
      }
      // Compile body in the new code object
      gen(body);
      // If we don't have explicit block which pops locals, we should pop arguments (if any) - callee cleanup
      // + 1 is for the function itself which is set as a local
      if (!isBlock(body)) {
          emit(OP_SCOPE_EXIT);
          emit(arity + 1);
      }
      // Explicit return to restore caller address
      emit(OP_RETURN);

      // 1. Simple function allocated at compile time
      // If it's not a closure (doesn't have free variables) allocate it at compile time and store as a constant
      // Closure are allocated at runtime, but reuse the same code object
      if (scopeInfo->free.size() == 0) {
          // Create the function
          auto fn = ALLOC_FUNCTION(co);
          constantObjects_.insert((Traceable*)AS_OBJECT(fn));
          // Restore the code object
          co = prevCo;
          // Add function as a constant to our co
          co->addConst(fn);
          // And emit code for this new constant
          emit(OP_CONST);
          emit(co->constants.size() - 1);
      }
      // 2. Closures 
      // - Load all free vars to capture (indices are taken from the 'cells' of the parent co)
      // - Load code object for the current function
      // - Make function
      else {
          // Restore the code object
          co = prevCo;
          for (const auto& freeVar : scopeInfo->free) {
              emit(OP_LOAD_CELL);
              emit(prevCo->getCellIndex(freeVar));
          }
          // Load code object
          emit(OP_CONST);
          emit(co->constants.size() - 1);
          // Create the function
          emit(OP_MAKE_FUNCTION);
          // How many cells to capture
          emit(scopeInfo->free.size());
      }
      scopeStack_.pop();
  }

  /**
   * Creates a new code object.
   */
  EvaValue createCodeObjectValue(const std::string& name, size_t arity = 0) {
      auto coValue = ALLOC_CODE(name, arity);
      auto co = AS_CODE(codeValue);
      codeObjects_.push_back(co);
      constantObjects_.insert((Traceable*)co);
      return coValue;
  }

  /**
   * Enters a new block.
   */
  void blockEnter() { co->scopeLevel++; }

  /**
   * Exits a block.
   */
  void blockExit() {
      // Pop vars from the stack if they were declared within this specific scope
      auto varsCount = getVarsCountOnScopeExit();
      if (varsCount > 0 || co->arity > 0) {
          emit(OP_SCOPE_EXIT);
          // For functions do callee cleanup: pop all arguments plus the function name
          if (isFunctionBody()) {
              varsCount += co->arity + 1;
          }
          emit(varsCount);
      }
      co->scopeLevel--;
  }

  /**
   * Whether it's the global scope.
   */
  bool isGlobalScope() { return co->name == "main" && co->scopeLevel == 1; }

  /**
   * Whether it's the global scope.
   */
  bool isFunctionBody() { return co->name != "main" && co->scopeLevel == 1; }

  /**
   * Whether the expression is a declaration.
   */
  bool isDeclaration(const Exp& exp) {
    return isVarDeclaration(exp) || isFunctionDeclaration(exp) ||
           isClassDeclaration(exp);
  }

  /**
   * (class ...)
   */
  bool isClassDeclaration(const Exp& exp) { return isTaggedList(exp, "class"); }

  /**
   * (prop ...)
   */
  bool isProp(const Exp& exp) { return isTaggedList(exp, "prop"); }

  /**
   * (var <name> <value>)
   */
  bool isVarDeclaration(const Exp& exp) { return isTaggedList(exp, "var"); }

  /**
   * (lambda ...)
   */
  bool isLambda(const Exp& exp) { return isTaggedList(exp, "lambda"); }

  /**
   * (def <name> ...)
   */
  bool isFunctionDeclaration(const Exp& exp) {
    return isTaggedList(exp, "def");
  }

  /**
   * (begin ...)
   */
  bool isBlock(const Exp& exp) { return isTaggedList(exp, "begin"); }

  /**
   * Tagged lists.
   */
  bool isTaggedList(const Exp& exp, const std::string& tag) {
    return exp.type == ExpType::LIST && exp.list[0].type == ExpType::SYMBOL &&
           exp.list[0].string == tag;
  }

  /**
   * Pop the variables of the current scope,
   * returns number of vars used.
   */
  size_t getVarsCountOnScopeExit() {
      auto varsCount = 0;
      if (co->locals.size() > 0) {
          while (co->locals.back().scopeLevel == co->scopeLevel) {
              co->locals.pop_back();
              varsCount++;
          }
      }
      return varsCount;
  }

  /**
   * Returns current bytecode offset.
   */
  uint16_t getOffset() { return (uint16_t)co->code.size(); }

  /**
   * Allocates a numeric constant.
   */
  size_t numericConstIdx(double value) {
      ALLOC_CONST(IS_NUMBER, AS_NUMBER, NUMBER, value);
      return co->constants.size() - 1;
  }

  /**
   * Allocates a string constant.
   */
  size_t stringConstIdx(const std::string& value) {
      ALLOC_CONST(IS_STRING, AS_CPPSTRING, ALLOC_STRING, value);
      constantObjects_.insert((Traceable*)co->constants.back().object);
      return co->constants.size() - 1;
  }

  /**
   * Allocates a boolean constant.
   */
  size_t booleanConstIdx(bool value) {
      ALLOC_CONST(IS_BOOLEAN, AS_BOOLEAN, BOOLEAN, value);
      return co->constants.size() - 1;
  }

  /**
   * Emits data to the bytecode.
   */
  void emit(uint8_t code) { co->code.push_back(code); }

  /**
   * Writes byte at offset.
   */
  void writeByteAtOffset(size_t offset, uint8_t value) {
    co->code[offset] = value;
  }

  /**
   * Patches jump address.
   */
  void patchJumpAddress(size_t offset, uint16_t value) {
    writeByteAtOffset(offset, (value >> 8) & 0xff);
    writeByteAtOffset(offset + 1, value & 0xff);
  }

  /**
   * Returns a class object by name.
   */
  ClassObject* getClassByName(const std::string& name) {
    // Implement here...
  }

  /**
   * Scope info.
   */
  std::map<const Exp*, std::shared_ptr<Scope>> scopeInfo_;

  /**
   * Scopes stack.
   */
  std::stack<std::shared_ptr<Scope>> scopeStack_;

  /**
   * Compiling code object.
   */
  CodeObject* co;

  /**
   * Main entry point (function).
   */
  FunctionObject* main;

  /**
   * All code objects.
   */
  std::vector<CodeObject*> codeObjects_;

  /**
   * All objects from the constant pools of all code objects.
   */
  std::set<Traceable*> constantObjects_;

  /**
   * Currently compiling class object.
   */
  ClassObject* classObject_;

  /**
   * All class objects.
   */
  std::vector<ClassObject*> classObjects_;

  /**
   * Compare ops map.
   */
  static std::map<std::string, uint8_t> compareOps_;
};

/**
 * Compare ops map.
 */
std::map<std::string, uint8_t> EvaCompiler::compareOps_ = {
    {"<", 0}, {">", 1}, {"==", 2}, {">=", 3}, {"<=", 4}, {"!=", 5},
};

#endif