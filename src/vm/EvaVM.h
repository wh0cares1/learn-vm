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
 * Eva Virtual Machine.
 */

#ifndef EvaVM_h
#define EvaVM_h

#include <array>
#include <stack>
#include <string>
#include <vector>

#include "../Logger.h"
#include "../bytecode/OpCode.h"
#include "../compiler/EvaCompiler.h"
#include "../gc/EvaCollector.h"
#include "../parser/EvaParser.h"
#include "EvaValue.h"
#include "Global.h"

using syntax::EvaParser;

/**
 * Reads the current byte in the bytecode
 * and advances the ip pointer.
 */
#define READ_BYTE()  *ip++

/**
 * Reads a short word (2 bytes).
 */
#define READ_SHORT()  (ip += 2, (uint16_t)((ip[-2] << 8)) | ip[-1])

/**
 * Converts bytecode index to a pointer.
 */
#define TO_ADDRESS(index) (&fn->co->code[index])

/**
 * Gets a constant from the pool.
 */
#define GET_CONST() (fn->co->constants[READ_BYTE()])

/**
 * Stack top (stack overflow after exceeding).
 */
#define STACK_LIMIT 512

/**
 * Memory threshold after which GC is triggered.
 */
#define GC_TRESHOLD 1024

/**
 * Runtime allocation, can call GC.
 */
#define MEM(allocator, ...)  // Implement here...

/**
 * Binary operation.
 */
#define BINARY_OP(op)               \
do {                                \
    auto op2 = AS_NUMBER(pop());    \
    auto op1 = AS_NUMBER(pop());    \
    push(NUMBER(op1 op op2));       \
} while(false)

/**
 * Generic values comparison.
 */
#define COMPARE_VALUES(op, v1, v2)  \
do {                                \
    bool res;                       \
    switch (op) {                   \
    case 0:                         \
        res = v1 < v2;              \
        break;                      \
    case 1:                         \
        res = v1 > v2;              \
        break;                      \
    case 2:                         \
        res = v1 == v2;             \
        break;                      \
    case 3:                         \
        res = v1 >= v2;             \
        break;                      \
    case 4:                         \
        res = v1 <= v2;             \
        break;                      \
    case 5:                         \
        res = v1 != v2;             \
        break;                      \
    }                               \
    push(BOOLEAN(res))              \
} while (false)

// --------------------------------------------------

/**
 * Stack frame for function calls.
 */
struct Frame {
  // Return address of the caller (ip of the caller)
  uint8_t* ra;
  // Base pointer of the caller
  EvaValue* bp;
  // Reference to the running function
  FunctionObject* fn;
};

// --------------------------------------------------

/**
 * Eva Virtual Machine.
 */
class EvaVM {
 public:
  EvaVM()
      : global(std::make_shared<Global>()),
        parser(std::make_unique<EvaParser>()),
        compiler(std::make_unique<EvaCompiler>(global)),
        collector(std::make_unique<EvaCollector>()) {
    setGlobalVariables();
  }

  /**
   * VM shutdown.
   */
  ~EvaVM() { Traceable::cleanup(); }

  //----------------------------------------------------
  // Stack operations:

  /**
   * Pushes a value onto the stack.
   */
  void push(const EvaValue& value) {
      if ((size_t)(sp - stack.begin()) == STACK_LIMIT) {
          DIE << "push(): stack overflow.\n";
      }
      *sp = value;
      sp++;
  }

  /**
   * Pops a value from the stack.
   */
  EvaValue pop() {
      if (sp == stack.begin()) {
          DIE << "pop(): empty stack.\n";
      }
      --sp;
      return *sp;
  }

  /**
   * Peeks an element from the stack.
   */
  EvaValue peek(size_t offset = 0) {
      if (stack.size() == 0) {
          DIE << "peek(): empty stack.\n";
      }
      return *(sp - 1 - offset);
  }

  /**
   * Pops multiple values from the stack.
   */
  void popN(size_t count) {
      if (stack.size() == 0) {
          DIE << "popN(): empty stack.\n";
      }
      sp -= count;
  }

  //----------------------------------------------------
  // GC operations:

  /**
   * Obtains GC roots: variables on the stack, globals, constants.
   */
  std::set<Traceable*> getGCRoots() {
    // Implement here...
  }

  /**
   * Returns stack GC roots.
   */
  std::set<Traceable*> getStackGCRoots() {
    // Implement here...
  }

  /**
   * Returns GC roots for constants.
   */
  std::set<Traceable*> getConstantGCRoots() {
    // Implement here...
  }

  /**
   * Returns global GC roots.
   */
  std::set<Traceable*> getGlobalGCRoots() {
    // Implement here...
  }

  /**
   * Spawns a potential GC cycle.
   */
  void maybeGC() {
    // Implement here...
  }

  //----------------------------------------------------
  // Program execution

  /**
   * Executes a program.
   */
  EvaValue exec(const std::string& program) {
    // 1. Parse the program
    auto ast = parser->parse("(begin " + program + ")");

    // 2. Compile program to Eva bytecode
    compiler->compile(ast);

    // Start from the main entry point:
    fn = compiler->getMainFunction();

    // Set instruction pointer to the beginning:
    ip = &fn->co->code[0];

    // Init the stack:
    sp = &stack[0];

    // Init the base (frame) pointer:
    bp = sp;

    // Debug disassembly:
    compiler->disassembleBytecode();

    return eval();
  }

  /**
   * Main eval loop.
   */
  EvaValue eval() {
    for (;;) {
        // dumpStack();
        auto opcode = READ_BYTE();
        switch (opcode) {
            case OP_HALT;
                return pop();
            case OP_CONST;
                push(GET_CONST());
                break();
            case OP_ADD;
                BINARY_OP(+);
                break;
            case OP_SUB;
                BINARY_OP(-);
                break;
            case OP_MUL;
                BINARY_OP(*);
                break;
            case OP_DIV;
                BINARY_OP(/);
                break;
            case OP_COMPARE: {
                auto op = READ_BYTE();
                auto op2 = pop();
                auto op1 = pop();
                if (IS_NUMBER(op1) && IS_NUMBER(op2)) {
                    auto v1 = AS_NUMBER(op1);
                    auto v2 = AS_NUMBER(op2);
                    COMPARE_VALUES(op, v1, v2);
                } else if (IS_STRING(op1) && IS_STRING(op2)) {
                    auto s1 = AS_STRING(op1);
                    auto s2 = AS_STRING(op2);
                    COMPARE_VALUES(op, s1, s2);
                }
            }
            case OP_JMP_IF_FALSE: {
                auto cond = AS_BOOLEAN(pop());
                auto address = READ_SHORT();
                if (!cond) {
                    ip = TO_ADDRESS(address);
                }
                break;
            }
            // Unconditional Jump
            case OP_JMP: {
                ip = TO_ADDRESS(READ_SHORT());
                break;
            }
            
            // Global variable value
            case OP_GET_GLOBAL: {
                auto globalIndex = READ_BYTE();
                push(global->get(globalIndex).value);
                break;
            }
            case OP_SET_GLOBAL: {
                auto globalIndex = READ_BYTE();
                auto value = pop(0);
                global->set(globalIndex, value);
                break;
            }
            // Stack manipulation
            case OP_POP:
                pop();
                break;
            // Local variable value
            case OP_GET_GLOBAL: {
                auto localIndex = READ_BYTE();
                if (localIndex < 0 || localIndex >= stack.size()) {
                    DIE << "OP_GET_LOCAL: invalid variable index: " << (int)localIndex;
                }
                push(bp[localIndex]);
                break;
            }
            case OP_SET_LOCAL: {
                auto localIndex = READ_BYTE();
                auto value = peek(0);
                if (localIndex < 0 || localIndex >= stack.size()) {
                    DIE << "OP_GET_LOCAL: invalid variable index: " << (int)localIndex;
                }
                bp[localIndex] = value;
                break;
            }
            case OP_SCOPE_EXIT: {
                // How many vars to pop
                auto count = READ_BYTE();
                // Move the result above the vars
                *(sp - 1 - count) = peek(0);
                // Pop the vars
                popN(count);
                break;
            }
            case OP_CALL: {
                auto argsCount = READ_BYTE();
                auto fnValue = peek(argsCount);
                // 1. Native function
                if (IS_NATIVE(fnValue)) {
                    AS_NATIVE(fnValue)->function();
                    auto result = pop();
                    // Pop args and function
                    popN(argsCount + 1);
                    // Put result back on top
                    push(result);
                    break;
                }
                // 2. User-defined function:
                auto callee = AS_FUNCTION(fnValue);
                // Save execution context, restored on OP_RETURN
                callStack.push(Frame(ip, bp, fn));
                // To access locals, etc:
                fn = callee;
                // Set the base (frame) pointer for the callee
                bp = sp - argsCount - 1;
                // Jump to the function code
                ip = &callee->co->code[0];
                break;
            }
            // Return from function
            case OP_RETURN: {
                //Restore the caller address
                auto callerFrame = callStack.top();
                // Restore ip, bp and fn for caller
                ip = callerFrame.ra;
                bp = callerFrame.bp;
                fn = callerFrame.fn;
                callStack.pop();
                break;
            }
            default:
                DIE << "Unkown Opcode: " << std::hex << opcode;
      }
    }
  }

  /**
   * Sets up global variables and function.
   */
  void setGlobalVariables() {
      // Native square function
      global->addNativeFunction(
          "native-square",
          [&]() {
              auto x = AS_NUMBER(peek(0));
              push(NUMBER(x * x));
          },
          1);
      // Native sum function
      global->addNativeFunction(
          "sum",
          [&]() {
              auto v2 = AS_NUMBER(peek(0));
              auto v1 = AS_NUMBER(peek(1));
              push(NUMBER(v1 + v2));
          },
          2);
      // Global variable
      global->addConst("VERSION", 1);
  }

  /**
   * Global object.
   */
  std::shared_ptr<Global> global;

  /**
   * Parser.
   */
  std::unique_ptr<EvaParser> parser;

  /**
   * Compiler.
   */
  std::unique_ptr<EvaCompiler> compiler;

  /**
   * Garbage collector.
   */
  std::unique_ptr<EvaCollector> collector;

  /**
   * Instruction pointer (aka Program counter).
   */
  uint8_t* ip;

  /**
   * Stack pointer.
   */
  EvaValue* sp;

  /**
   * Base pointer (aka Frame pointer).
   */
  EvaValue* bp;

  /**
   * Operands stack.
   */
  std::array<EvaValue, STACK_LIMIT> stack;

  /**
   * Separate stack for calls. Keeps return addresses.
   */
  std::stack<Frame> callStack;

  /**
   * Currently executing function.
   */
  FunctionObject* fn;

  // --------------------------------------------------
  // Debug functions:

  /**
   * Dumps current stack.
   */
  void dumpStack() {
      std::cout << "\n---------- Stack ----------\n";
      if (sp == stack.begin()) {
          std::cout << "(empty)";
      }
      auto csp = sp - 1;
      while (csp >= stack.begin()) {
          std::cout << *csp-- << "\n";
      }
      std::cout << "\n";
  }
};

#endif