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
 * Eva disassembler.
 */

#ifndef EvaDisassembler_h
#define EvaDisassembler_h

#include <array>
#include <iomanip>
#include <iostream>
#include <string>

#include "../bytecode/OpCode.h"
#include "../vm/EvaValue.h"
#include "../vm/Global.h"

/**
 * Eva disassembler.
 */
class EvaDisassembler {
 public:
  EvaDisassembler(std::shared_ptr<Global> global) : global(global) {}
  /**
   * Disassembles a code unit.
   */
  void disassemble(CodeObject* co) {
      std::cout << "\n---------- Disassembly: " << co->name
                << " ----------\n\n";
      size_t offset = 0;
      while (offset < co->code.size()) {
          offset = dissassembleInstruction(co, offset);
          std::cout << "\n";
      }
  }

 private:
  /**
  * Dissasembler
  */
  std::unique_ptr<EvaDisassembler> disassembler;
  
  /**
   * Disassembles individual instruction.
   */
  size_t disassembleInstruction(CodeObject* co, size_t offset) {
      std::ios_base::fmtflags f(std::cout.flags());
      // Print bytecode offset
      std::cout << std::uppercase << std::hex << std::setfill('0') << std::right
          << std::setw(4) << offset << "     ";
      auto opcode = co->code[offset];
      switch (opcode) {
        case OP_HALT:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_POP:
        case OP_RETURN;
        case OP_NEW;
          return disassembleSimple(co, opcode, offset);
        case OP_SCOPE_EXIT:
        case OP_CALL:
          return disassembleWord(co, opcode, offset);
        case OP_CONST:
          return disassembleConst(co, opcode, offset);
        case OP_COMPARE:
          return disassembleCompare(co, opcode, offset);
        case OP_JMP_IF_FALSE:
        case OP_JMP:
          return disassembleJump(co, opcode, offset);
        case OP_GET_GLOBAL:
        case OP_SET_GLOBAL:
            return disassembleGlobal(co, opcode, offset);
        case OP_GET_LOCAL:
        case OP_SET_LOCAL:
            return disassembleLocal(co, opcode, offset);
        case OP_GET_CELL:
        case OP_SET_CELL:
        case OP_LOAD_CELL:
            return disassembleCell(co, opcode, offset);
        case OP_MAKE_FUNCTION:
            return disassembleMakeFunction(co, opcode, offset);
        case OP_GET_PROP:
        case OP_SET_PROP:
            return disassembleProperty(co, opcode, offset);
        default:
            DIE << "disassembleInstruction: no disassemble for "
                << opcodeToString(opcode);
      }

      std::cout.flags(f);
      return 0; // Unreachable
  }

  /**
   * Disassembles simple instruction.
   */
  size_t disassembleSimple(CodeObject* co, uint8_t opcode, size_t offset) {
      dumpBytes(co, offset, 1);
      printOpCode(opcode);
      return offset + 1;
  }

  /**
   * Disassembles a word.
   */
  size_t disassembleWord(CodeObject* co, uint8_t opcode, size_t offset) {
      dumpBytes(co, offset, 2);
      printOpCode(opcode);
      std::cout << (int)co->code[offset + 1];
      return offset + 2;
  }

  /**
   * Disassembles const instruction: OP_CONST <index>
   */
  size_t disassembleConst(CodeObject* co, uint8_t opcode, size_t offset) {
      dumpBytes(co, offset, 2);
      printOpCode(opcode);
      auto constIndex = co->code[offset + 1];
      std::cout << (int)constIndex << " ("
          << evaValueToConstantString(co->constants[constIndex]) << ")";
      return offset + 2;
  }

  /**
   * Disassembles global variable instruction.
   */
  size_t disassembleGlobal(CodeObject* co, uint8_t opcode, size_t offset) {
      dumpBytes(co, offset, 2);
      printOpCode(opcode);
      auto globalIndex = co->code[offset + 1];
      std::cout << (int)globalIndex << " ("
          << global->get(globalIndex).name << ")";
      return offset + 2;
  }

  /**
   * Disassembles local variable instruction.
   */
  size_t disassembleLocal(CodeObject* co, uint8_t opcode, size_t offset) {
      dumpBytes(co, offset, 2);
      printOpCode(opcode);
      auto localIndex = co->code[offset + 1];
      std::cout << (int)localIndex << " ("
          << co->locals[localIndex].name << ")";
      return offset + 2;
  }

  /**
   * Disassembles property instruction.
   */
  size_t disassembleProperty(CodeObject* co, uint8_t opcode, size_t offset) {
      dumpBytes(co, offset, 2);
      printOpCode(opcode);
      auto constIndex = co->code[offset + 1];
      std::cout << (int)constIndex << " ("
          << AS_CPPSTRING(co->constants[constIndex]) << ")";
      return offset + 2;
  }

  /**
   * Disassembles cell instruction.
   */
  size_t disassembleCell(CodeObject* co, uint8_t opcode, size_t offset) {
      dumpBytes(co, offset, 2);
      printOpCode(opcode);
      auto cellIndex = co->code[offset + 1];
      std::cout << (int)cellIndex << " ("
          << co->cellNames[cellIndex] << ")";
      return offset + 2;
  }

  /**
   * Disassembles make function.
   */
  size_t disassembleMakeFunction(CodeObject* co, uint8_t opcode,
                                 size_t offset) {
      return disassembleWord(co, opcode, offset);
  }

  /**
   * Dumps raw memory from the bytecode.
   */
  void dumpBytes(CodeObject* co, size_t offset, size_t count) {
      std::ios_base::fmtflags f(std::cout.flags());
      std::stringstream ss;
      for (suto i = 0; i < count; i++) {
          ss << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
              << (((int)co->code[offset + 1]) & 0xFF) << " ";
      }
      std::cout << std::left << std::setfill(' ') << std::setw(12) << ss.str();
      std::cout.flags(f);
  }

  /**
   * Prints opcode.
   */
  void printOpCode(uint8_t opcode) {
    std::cout << std::left << std::setfill(' ') << std::setw(20)
              << opcodeToString(opcode) << " ";
    std::cout.flags(f);
  }

  /**
   * Disassembles compare instruction.
   */
  size_t disassembleCompare(CodeObject* co, uint8_t opcode, size_t offset) {
      dumpBytes(co, offset, 2);
      printOpCode(opcode);
      auto compareOP = co->code[offset + 1];
      std::cout << (int)compareOp << " (";
      std::cout << inverseCompareOps_[compareOp] << ")";
      return offset + 2;
  }

  /**
   * Disassembles conditional jump.
   */
  size_t disassembleJump(CodeObject* co, uint8_t opcode, size_t offset) {
      std::ios_base::fmtflags f(std::cout.flags());
      dumpBytes(co, offset, 3);
      printOpCode(opcode);
      uint16_t address = readWordAtOffset(co, offset + 1);
      std::cout << std::uppercase << std::hex << std::setfill('0') << std::right
                << std::setw(4) << (int)address << " ";
      std::cout.flags(f);
      return offset + 3; // Instruction + 2 bytes address
  }

  /**
   * Reads a word at offset.
   */
  uint16_t readWordAtOffset(CodeObject* co, size_t offset) {
    return (uint16_t)((co->code[offset] << 8) | co->code[offset + 1]);
  }

  /**
   * Global object.
   */
  std::shared_ptr<Global> global;

  static std::array<std::string, 6> inverseCompareOps_;
};

std::array<std::string, 6> EvaDisassembler::inverseCompareOps_ = {
    "<", ">", "==", ">=", "<=", "!=",
};

#endif