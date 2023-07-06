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
 * Global object.
 */

#ifndef Global_h
#define Global_h

/**
 * Global var.
 */
struct GlobalVar {
    std::string name;
    EvaValue value;
};

/**
 * Global object.
 */
struct Global {
  /**
   * Returns a global.
   */
  GlobalVar& get(size_t index) { return globals[index]; }

  /**
   * Sets a global.
   */
  void set(size_t index, const EvaValue& value) {
    if (index >= globals.size()) {
        DIE << "Global " << index << " doesn't exist.";
    }
    globals[index].value = value;
  }

  /**
   * Registers a global.
   */
  void define(const std::string& name) {
      auto index = getGlobalIndex(name);
      // Already defined
      if (index != -1) {
          return;
      }
      // Set to default number 0
      globals.push_back({ name, NUMBER(0) });
  }

  /**
   * Adds a native function.
   */
  void addNativeFunction(const std::string& name, std::function<void()> fn,
                         size_t arity) {
      if (exists(name)) {
          return;
      }
      globals.push_back({name, ALLOC_NATIVE(fn, name, arity)});
  }

  /**
   * Adds a global constant.
   */
  void addConst(const std::string& name, double value) {
      if (exists(name)) {
          return;
      }
      globals.push_back({ name, NUMBER(value) });
  }

  /**
   * Get global index.
   */
  int getGlobalIndex(const std::string& name) {
      if (globals.size() > 0) {
          for (auto i = (int)globals.size() - 1; i >= 0; i--) {
              if (globals[i].name == name) {
                  return i;
              }
          }
      }
      return -1;
  }

  /**
   * Whether a global variable exists.
   */
  bool exists(const std::string& name) { return getGlobalIndex(name) != -1; }

  /**
   * Global variables and functions.
   */
  std::vector<GlobalVar> globals;
};

#endif