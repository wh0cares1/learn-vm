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
    co->constants.push_back(ALLOC_STRING(value));           \
} while (false)

// Generic binary operator
#define GEN_BINARY_OP(op)   \
do {                        \
    gen(exp.list[1]);       \
    gen(exp.list[2]);       \
    emit(op);               \
} while (false)

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
      co = AS_CODE(ALLOC_CODE("main"));
      // Generate recursively from top level
      gen(exp)
      // Explicit VM-stop marker
      emit(OP_HALT);
      return co;
  }

  /**
   * Scope analysis.
   */
  void analyze(const Exp& exp, std::shared_ptr<Scope> scope) {
    // Implement here...
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
          // 1. Local vars
          auto localIndex = co->getLocalIndex(varName);
          if (localIndex != -1) {
              emit(OP_GET_LOCAL);
              emit(localIndex);
          }
          // 2. Global vars
          else {
              if (!global->exists(varName)) {
                  DIE << "[EvaCompiler]: Reference error: " << varName;
              }
              emit(OP_GET_GLOBAL);
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
              // Initializer
              gen(exp.list[2]);
              // 1. Global vars
              if (isGlobalScope()){
                  global->define(varName);
                  emit(OP_SET_GLOBAL);
                  emit(global->getGlobalIndex(varName));
              }
              // 2. Local vars :
              else {
                  co->addLocal(varName);
                  emit(OP_SET_LOCAL);
                  emit(co->getLocalIndex(varName));
              }
          }
          else if (op == "set") {
              auto varName = exp.list[1].string;
              
              // Initializer
              gen(exp.list[2]);

              // 1. Local vars
              auto localIndex = co->getLocalIndex(varName);

              if (localIndex != -1) {
                  emit(OP_SET_LOCAL);
                  emit(localIndex);
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
              scopeEnter();
              // Compile each expression within the block
              for (auto i = 1; i < exp.list.size(); i++) {
                  // The value of last expression is kept on the stack as the final result
                  bool isLast = i == exp.list.size() - 1;
                  // Local variable or function (should not pop)
                  auto isLocalDeclaration =
                      isDeclaration(exp.list[i]) && !isGlobalScope();
                  // Generate expression code
                  gen(exp.list[i]);
                  if (!isLast && !isLocalDeclaration) {
                      emit(OP_POP);
                  }
              }
              scopeExit();
          }
        }

        // --------------------------------------------
        // Lambda function calls:
        //
        // ((lambda (x) (* x x)) 2)

        else {
          // Implement here...
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
      if (varsCount > 0) {
          emit(OP_SCOPE_EXIT);
          emit(varsCount);
      }
      co->scopeLevel--; 
  }

  /**
   * Compiles a function.
   */
  void compileFunction(const Exp& exp, const std::string fnName,
                       const Exp& params, const Exp& body) {
    // Implement here...
  }

  /**
   * Creates a new code object.
   */
  EvaValue createCodeObjectValue(const std::string& name, size_t arity = 0) {
    // Implement here...
  }

  /**
   * Enters a new block.
   */
  void blockEnter() { co->scopeLevel++; }

  /**
   * Exits a block.
   */
  void blockExit() {
    // Implement here...
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