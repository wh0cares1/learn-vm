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
    // Implement here...
  }

  /**
   * Deallocator.
   */
  static void operator delete(void* object, std::size_t sz) {
    // Implement here...
  }

  /**
   * Clean up for all objects.
   */
  static void cleanup() {
    // Implement here...
  }

  /**
   * Printes memory stats
   */
  static void printStats() {
    // Implement here...
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
  // Implement here...
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
  // Implement here...
};

// ----------------------------------------------------------------

/**
 * Instance object.
 */
struct InstanceObject : public Object {
  // Implement here...
};

// ----------------------------------------------------------------

struct LocalVar {
  // Implement here...
};

/**
 * Code object.
 *
 * Contains compiling bytecode, locals and other
 * state needed for function execution.
 */
struct CodeObject : public Object {
    CodeObject(const std::string& name) : Object(ObjectType::CODE), name(name) {}
    // Name of the unit (usually function name)
    std::string name;
    // Constant pool
    std::vector<EvaValue> constants;
    // Bytecode
    std::vector<uint8_t> code;
};

// ----------------------------------------------------------------

/**
 * Heap-allocated cell.
 *
 * Used to capture closured variables.
 */
struct CellObject : public Object {
  // Implement here...
};

// ----------------------------------------------------------------

/**
 * Function object.
 */
struct FunctionObject : public Object {
  // Implement here...
};

// ----------------------------------------------------------------
// Constructors:

#define NUMBER(value) ((EvaValue){EvaValueType::NUMBER, .number = value})

#define ALLOC_STRING(value) ((EvaValue){EvaValueType::OBJECT, .object = (Object*) new StringObhect(value)})

#define ALLOC_CODE(name) ((EvaValue){EvaValueType::OBJECT, .object = (Object*) new CodeObject(name)})

// ----------------------------------------------------------------
// Accessors:

#define AS_NUMBER(evaValue) ((double)(evaValue).number)
#define AS_OBJECT(evaValue) ((Object*)(evaValue).object)
#define AS_STRING(evaValue) ((StringObject*)(evaValue).object)
#define AS_CPPSTRING(evaValue) (AS_STRING(evaValue)->string)
#define AS_CODE(evaValue) ((CodeObject*)(evaValue).object)

// ----------------------------------------------------------------
// Testers:

#define IS_NUMBER(evaValue) ((evaValue).type == EvaValueType::NUMBER)
#define IS_NUMBER(evaValue) ((evaValue).type == EvaValueType::OBJECT)
#define IS_OBJECT_TYPE(evaValue, objectType) IS_OBJECT(evaValue) && AS_OBJECT(evaValue)->type == objectType
#define IS_STRING(evaValue) IS_OBJECT_TYPE(evaValue, ObjectType::STRING)
#define IS_CODE(evaValue) IS_OBJECT_TYPE(evaValue, ObjectType::CODE)

// ----------------------------------------------------------------

/**
 * String representation used in constants for debug.
 */
std::string evaValueToTypeString(const EvaValue& evaValue) {
  if (IS_NUMBER(evaValue)) {
      return "NUMBER";
  } else if (IS_STRING(evaValue)) {
      return "STRING";
  } else if (IS_CODE(evaValue)) {
      return "CODE";
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
    else if (IS_STRING(evaValue)) {
        ss << '"' << AS_CPPSTRING(evaValue) << '"';
    }
    else if (IS_CODE(evaValue)) {
        auto code = AS_CODE(evaValue);
        ss << "code " << code << ": " << code->name;
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