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
 * Eva value.
 */

#ifndef EvaValue_h
#define EvaValue_h

#include <list>
#include <string>
#include <vector>

/**
 * Eva value type.
 */
enum class EvaValueType {
  NUMBER,
  BOOLEAN,
  OBJECT,
};

/**
 * Object type.
 */
enum class ObjectType {
  STRING,
  CODE,
  NATIVE,
  FUNCTION,
  CELL,
  CLASS,
  INSTANCE,
};

// ----------------------------------------------------------------

/**
 * Base traceable object.
 */
struct Traceable {
  /**
   * Whether the object was marked during the trace.
   */
  bool marked;

  /**
   * Allocated size.
   */
  size_t size;

  /**
   * Allocator.
   */
  static void* operator new(size_t size) {
    // Allocation a block with the header
      void* object = ::operator new(size);
      ((Traceable*)object)->size = size;

      Traceable::objects.push_back((Traceable*)object);
      Traceable::bytesAllocated += size;

      return object;
  }

  /**
   * Deallocator.
   */
  static void operator delete(void* object, std::size_t sz) {
      Traceable::bytesAllocated -= ((Traceable*)object)->size;
      ::operator delete(object, sz);
      // Note: remove from Traceable::objects during GC cycle
  }

  /**
   * Clean up for all objects.
   */
  static void cleanup() {
    for (auto& object : object) {
        delete object;
    }
    objects.clear();
  }

  /**
   * Printes memory stats
   */
  static void printStats() {
      std::cout << "--------------------\n";
      std::cout << "Memory stats:\n\n";
      std::cout << "Object allocated : " << std::dec << Traceable::objects.size() << "\n";
      std::cout << "Bytes allocated : " << std::dec << Traceable::bytesAllocated << "\n\n";
  }

  /**
   * Total number of allocated bytes.
   */
  static size_t bytesAllocated;

  /**
   * List of all allocated objects.
   */
  static std::list<Traceable*> objects;
};

/**
 * Total bytes allocated.
 */
size_t Traceable::bytesAllocated{0};

/**
 * List of all allocated objects.
 */
std::list<Traceable*> Traceable::objects{};

// ----------------------------------------------------------------

/**
 * Base object.
 */
struct Object : public Traceable {
  Object(ObjectType type) : type(type) {}
  ObjectType type;
};

// ----------------------------------------------------------------

/**
 * String object.
 */
struct StringObject : public Object {
    StringObject(const std::string& str) 
        : Object(ObjectType::STRING), string(str) {}
    std::string string;
};

// ----------------------------------------------------------------

using NativeFn = std::function<void()>;

/**
 * Native function.
 */
struct NativeObject : public Object {
  NativeObject(NativeFn function, const std::string& name, size_t arity)
      : Object(ObjectType::NATIVE),
        function(function),
        name(name),
        arity(arity){}
  // Native function
  NativeFn function;
  // Function name
  std::string name;
  // Number of parameters
  size_t arity;
};

// ----------------------------------------------------------------

/**
 * Eva value (tagged union).
 */
struct EvaValue {
  EvaValueType type;
  union {
    double number;
    bool boolean;
    Object* object;
  };
};

// ----------------------------------------------------------------

/**
 * Class object.
 */
struct ClassObject : public Object {
    ClassObject(const std::string& name, ClassObject* superClass)
        : Object(ObjectType::CLASS),
          name(name),
          properties{},
          superClass(superClass){}

    // Class name
    std::string name;
    // Shared properties and methods
    std::map<std::string, EvaValue> properties;
    // Super class
    ClassObject* superClass;
    // Resolves a property in the class chain
    EvaValue getProp(const std::string& prop) {
        if (properties.count(prop) != 0) {
            return properties[prop];
        }
        // Reached the final link in the chain, fail since not found
        if (superClass == nullptr) {
            DIE << "Unresolved property " << prop << " in class " << name;
        }
        return superClass->getProp(prop);
    }
    // Set own property
    void setProp(const std::string& prop, const EvaValue& value) {
        properties[prop] = value;
    }
};

// ----------------------------------------------------------------

/**
 * Instance object.
 */
struct InstanceObject : public Object {
  InstanceObject(ClassObject* cls)
      : Object(ObjectType::INSTANCE), cls(cls), properties{}{}
  // The class of this instance
  ClassObject* cls;
  // Instance own properties
  std::map<std::string, EvaValue> properties;
  // Resolves a property in the inheritance chain
  EvaValue getProp(const std::string& prop) {
      if (properties.count(prop) != 0) {
          return properties[prop];
      }
      return cls->getProp(prop);
  }
};

// ----------------------------------------------------------------

struct LocalVar {
    std::string name;
    size_t scopeLevel;
};

/**
 * Code object.
 *
 * Contains compiling bytecode, locals and other
 * state needed for function execution.
 */
struct CodeObject : public Object {
    CodeObject(const std::string& name, size_t arity) 
        : Object(ObjectType::CODE), name(name), arity(arity) {}
    // Name of the unit (usually function name)
    std::string name;
    // Number of parameters
    size_t arity;
    // Constant pool
    std::vector<EvaValue> constants;
    // Bytecode
    std::vector<uint8_t> code;
    // Current scope level
    size_t scopeLevel = 0;
    // Local variables and functions
    std::vector<LocalVar> locals;
    // Cell var names
    std::vector<std::string> cellNames;
    // Free vars count
    size_t freeCount = 0;
    // Insert bytecode at needed offset
    void insertAtOffset(int offset, uint8_t byte) {
        code.insert((offset < 0 ? code.end() : code.begin()) + offset, byte);
    }
    // Adds a local with current scope level
    void addLocal(const std::string& name) {
        locals.push_back({ name, scopeLevel });
    }
    // Adds a constant
    void addConst(const EvaValue& value) {
        constants.push_back(value);
    }
    // Get global index
    int getLocalIndex(const std::string& name) {
        if (locals.size() > 0) {
            for (auto i = (int)locals.size() - 1; i >= 0; i--) {
                if (locals[i].name == name) {
                    return i;
                }
            }
        }
        return -1;
    }
    // Get cell index
    int getCellIndex(const std::string& name) {
        if (locals.size() > 0) {
            for (auto i = (int)cellNames.size() - 1; i >= 0; i--) {
                if (cellNames[i] == name) {
                    return i;
                }
            }
        }
        return -1;
    }
};

// ----------------------------------------------------------------

/**
 * Heap-allocated cell.
 *
 * Used to capture closured variables.
 */
struct CellObject : public Object {
  CellObject(EvaValue value) : Object(ObjectType::CELL), value(value) {}
  EvaValue value;
};

// ----------------------------------------------------------------

/**
 * Function object.
 */
struct FunctionObject : public Object {
  FunctionObject(CodeObject* co) : Object(ObjectType::FUNCTION), co(co) {}
  // Reference to the code object: contains function code, locals, etc.
  CodeObject* co;
  // Captured cells (for closures)
  std::vector<CellObject*> cells;
};

// ----------------------------------------------------------------
// Constructors:

#define NUMBER(value) ((EvaValue){EvaValueType::NUMBER, .number = value})
#define BOOLEAN(value) ((EvaValue){EvaValueType::BOOLEAN, .boolean = value})

#define ALLOC_STRING(value) ((EvaValue){EvaValueType::OBJECT, .object = (Object*)new StringObhect(value)})

#define ALLOC_CODE(name, arity) ((EvaValue){EvaValueType::OBJECT, .object = (Object*)new CodeObject(name)})

#define ALLOC_NATIVE(fn, name, arity) ((EvaValue){EvaValueType::OBJECT, .object = (Object*)new NativeObject(fn, name, arity)})

#define ALLOC_FUNCTION(co) ((EvaValue){EvaValueType::OBJECT, .object = (Object*)new FunctionObject(co)})

#define ALLOC_CELL(evaValue) ((EvaValue){EvaValueType::OBJECT, .object = (Object*)new CellObject(evaValue)})

#define ALLOC_CLASS(name, superClass) ((EvaValue){EvaValueType::OBJECT, .object = (Object*)new ClassObject(name, superClass)})

#define ALLOC_INSTANCE(cls) ((EvaValue){EvaValueType::OBJECT, .object = (Object*)new InstanceObject(cls)})

#define CELL(cellObject) OBJECT((Object*)cellObject)

#define CLASS(cellObject) OBJECT((Object*)classObject)

// ----------------------------------------------------------------
// Accessors:

#define AS_NUMBER(evaValue) ((double)(evaValue).number)
#define AS_BOOLEAN(evaValue) ((bool)(evaValue).boolean)
#define AS_OBJECT(evaValue) ((Object*)(evaValue).object)
#define AS_STRING(evaValue) ((StringObject*)(evaValue).object)
#define AS_CPPSTRING(evaValue) (AS_STRING(evaValue)->string)
#define AS_CODE(evaValue) ((CodeObject*)(evaValue).object)
#define AS_NATIVE(evaValue) ((NativeObject*)(evaValue).object)
#define AS_FUNCTION(evaValue) ((FunctionObject*)(evaValue).object)
#define AS_CELL(evaValue) ((CellObject*)(evaValue).object)
#define AS_CLASS(evaValue) ((ClassObject*)(evaValue).object)
#define AS_INSTANCE(evaValue) ((InstanceObject*)(evaValue).object)

// ----------------------------------------------------------------
// Testers:

#define IS_NUMBER(evaValue) ((evaValue).type == EvaValueType::NUMBER)
#define IS_BOOLEAN(evaValue) ((evaValue).type == EvaValueType::BOOLEAN)
#define IS_NUMBER(evaValue) ((evaValue).type == EvaValueType::OBJECT)
#define IS_OBJECT_TYPE(evaValue, objectType) (IS_OBJECT(evaValue) && AS_OBJECT(evaValue)->type == objectType)
#define IS_STRING(evaValue) IS_OBJECT_TYPE(evaValue, ObjectType::STRING)
#define IS_CODE(evaValue) IS_OBJECT_TYPE(evaValue, ObjectType::CODE)
#define IS_NATIVE(evaValue) IS_OBJECT_TYPE(evaValue, ObjectType::NATIVE)
#define IS_FUNCTION(evaValue) IS_OBJECT_TYPE(evaValue, ObjectType::FUNCTION)
#define IS_CELL(evaValue) IS_OBJECT_TYPE(evaValue, ObjectType::CELL)
#define IS_CLASS(evaValue) IS_OBJECT_TYPE(evaValue, ObjectType::CLASS)
#define IS_INSTANCE(evaValue) IS_OBJECT_TYPE(evaValue, ObjectType::INSTANCE)

// ----------------------------------------------------------------

/**
 * String representation used in constants for debug.
 */
std::string evaValueToTypeString(const EvaValue& evaValue) {
  if (IS_NUMBER(evaValue)) {
      return "NUMBER";
  } else if (IS_BOOLEAN(evaValue)) {
      return "BOOLEAN";
  } else if (IS_STRING(evaValue)) {
      return "STRING";
  } else if (IS_CODE(evaValue)) {
      return "CODE";
  } else if (IS_NATIVE(evaValue)) {
      return "NATIVE";
  } else if (IS_FUNCTION(evaValue)) {
      return "FUNCTION";
  } else if (IS_CELL(evaValue)) {
      return "CELL";
  } else if (IS_CLASS(evaValue)) {
      return "CLASS";
  } else if (IS_INSTANCE(evaValue)) {
      return "INSTANCE";
  } else {
      DIE << "evaValueToTypeString: unknown type " << (int)evaValue.type;
  }
  return ""; // Unreachable
}

/**
 * String representation used in constants for debug.
 */
std::string evaValueToConstantString(const EvaValue& evaValue) {
    std::stringstream ss;
    if (IS_NUMBER(evaValue)) {
        ss << evaValue.number;
    }
    else if (IS_BOOLEAN(evaValue)) {
        ss << (evaValue.boolean == true ? "true" : "false");
    }
    else if (IS_STRING(evaValue)) {
        ss << '"' << AS_CPPSTRING(evaValue) << '"';
    }
    else if (IS_CODE(evaValue)) {
        auto code = AS_CODE(evaValue);
        ss << "code " << code << ": " << code->name << "/" << code->arity;
    }
    else if (IS_FUNCTION(evaValue)) {
        auto fn = AS_FUNCTION(evaValue);
        ss << fn->co->name << "/" << fn->co->arity;
    }
    else if (IS_NATIVE(evaValue)) {
        auto code = AS_NATIVE(evaValue);
        ss << fn->name << "/" << fn->arity;
    }
    else if (IS_CELL(evaValue)) {
        auto code = AS_CELL(evaValue);
        ss << "cell: " << evaValueToConstantString(cell->value);
    }
    else if (IS_CLASS(evaValue)) {
        auto cls = AS_CLASS(evaValue);
        ss << "class: " << cls->name;
    }
    else if (IS_INSTANCE(evaValue)) {
        auto instance = AS_INSTANCE(evaValue);
        ss << "instance: " << instance->cls->name;
    }
    else {
        DIE << "evaValueToConstantString: unknown type " << (int)evaValue.type;
    }
    return ss.str();
}

/**
 * Output stream.
 */
std::ostream& operator<<(std::ostream& os, const EvaValue& evaValue) {
  return os << "EvaValue (" << evaValueToTypeString(evaValue)
            << "): " << evaValueToConstantString(evaValue);
}

#endif