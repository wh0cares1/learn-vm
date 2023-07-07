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
 * Scope analysis.
 */

#ifndef Scope_h
#define Scope_h

#include <map>
#include <set>

/**
 * Scope type.
 */
enum class ScopeType {
  GLOBAL,
  FUNCTION,
  BLOCK,
  CLASS,
};

/**
 * Allocation type.
 */
enum class AllocType {
  GLOBAL,
  LOCAL,
  CELL,
};

/**
 * Scope structure.
 */
struct Scope {
  Scope(ScopeType type, std::shared_ptr<Scope> parent)
      : type(type), parent(parent) {}

  /**
   * Registers a local.
   */
  void addLocal(const std::string& name) {
      allocInfo[name] = type == ScopeType::GLOBAL ? AllocType::GLOBAL : AllocType::LOCAL;
  }

  /**
   * Registers an own cell.
   */
  void addCell(const std::string& name) {
      allocInfo[name] = AllocType::CELL;
  }

  /**
   * Registers a free var (parent cell).
   */
  void addFree(const std::string& name) {
      free.insert(name);
      allocInfo[name] = AllocType::CELL;
  }

  /**
   * Potentially promotes a variable from local to cell.
   */
  void maybePromote(const std::string& name) {
      auto initAllocType = scopeType == ScopeType::GLOBAL ? AllocType::GLOBAL : AllocType::LOCAL;
      if (allocInfo.count(name) != 0) {
          initAllocType = allocInfo[name];
      }
      // Already promoted
      if (initAllocType == AllocType::CELL) {
          return;
      }
      auto [ownerScope, allocType] = resolve(name, initAllocType);
      // Update the alloc type based on resolution
      allocInfo[name] = allocType;
      // If we resolve it as a cell, promote to heap
      if (allocType == AllocType::CELL) {
          promote(name, ownerScope);
      }
  }

  /**
   * Promotes a variable from local (stack) to cell (heap).
   */
  void promote(const std::string& name, Scope* ownerScope) {
      ownerScope->addCell(name);
      // Thread the variable as free in all parent scopes so it's propagated down to our scope
      auto scope = this;
      while (scope != ownerScope) {
          scope->addFree(name);
          scope = scope->parent.get();
      }
  }

  /**
   * Resolves a variable in the scope chain.
   *
   * Initially a variable is treated as local, however if during
   * the resolution we passed the own function boundary, it is
   * free, and hence should be promoted to a cell, unless global.
   */
  std::pair<Scope*, AllocType> resolve(const std::string& name,
                                       AllocType allocType) {
    // Found in the current scope
    if (allocInfo.count(name) != 0) {
        return std::make_pair(this, allocType);
    }
    // We crossed the boundary of the function and still didn't
    // resolve a local variable - further resolution should be free
    if (type == ScopeType::FUNCTION) {
        allocType = AllocType::CELL;
    }
    if (parent == nullptr) {
        DIE << "[Scope] Reference error: " << name << " is not defined.";
    }
    // If we resolve in the Global scope, the resolution is GLOBAL
    if (parent->type == ScopeType::GLOBAL) {
        allocType = AllocType::GLOBAL;
    }
    return parent->resolve(name, allocType);
  }

  /**
   * Returns get opcode based on allocation type.
   */
  int getNameGetter(const std::string& name) {
    switch (allocInfo[name]) {
        case AllocType::GLOBAL:
            return OP_GET_GLOBAL;
        case AllocType::LOCAL:
            return OP_GET_LOCAL;
        case AllocType::CELL:
            return OP_GET_CELL;
    }
  }

  /**
   * Returns set opcode based on allocation type.
   */
  int getNameSetter(const std::string& name) {
    switch (allocInfo[name]) {
      case AllocType::GLOBAL:
          return OP_SET_GLOBAL;
      case AllocType::LOCAL:
          return OP_SET_LOCAL;
      case AllocType::CELL:
          return OP_SET_CELL;
    }
  }

  /**
   * Scope type.
   */
  ScopeType type;

  /**
   * Parent scope.
   */
  std::shared_ptr<Scope> parent;

  /**
   * Allocation info.
   */
  std::map<std::string, AllocType> allocInfo;

  /**
   * Set of free vars.
   */
  std::set<std::string> free;

  /**
   * Set of own cells.
   */
  std::set<std::string> cells;
};

#endif