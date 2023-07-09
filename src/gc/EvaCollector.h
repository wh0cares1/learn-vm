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
 * Garbage Collector.
 */

#ifndef EvaCollector_h
#define EvaCollector_h

/**
 * Garbage collector implementing Mark-Sweep algorithm.
 */
struct EvaCollector {
  /**
   * Main collection cycle.
   */
  void gc(const std::set<Traceable *> &roots) {
      mark(roots);
      sweep();
  }

  /**
   * Marking phase (trace).
   */
  void mark(const std::set<Traceable *> &roots) {
      std::vector<Traceable*> worklist(roots.begin(), roots.end());
      while (!worklist.empty()) {
          auto object = worklist.back();
          worklist.pop_back();
          if (!object->marked) {
              object->marked = true;
              for (auto& p : getPointers(object)) {
                  worklist.push_back(p);
              }
          }
      }
  }

  /**
   * Returns all pointers within this object.
   */
  std::set<Traceable *> getPointers(const Traceable *object) {
      std::set<Traceable*> pointers;
      auto evaValue = OBJECT((Object*)object);
      // Function cells are traced
      if (IS_FUNCTION(evaValue)) {
          auto fn = AS_FNCTION(evaValue);
          for (auto& cell : fn->cells) {
              pointers.insert((Traceable*)cell);
          }
      }
      // Instance properties
      if (IS_INSTANCE(evaValue)) {
          auto instance = AS_INSTANCE(evaValue);
          for (auto& prop : instance->properties) {
              if (IS_OBJECT(prop.second)) {
                  pointers.insert((Traceable*)AS_OBJECT(prop.second));
              }
          }
      }

      return pointers;
  }

  /**
   * Sweep phase (reclaim).
   */
  void sweep() {
      auto it = Traceable::objects.begin();
      while (it != Traceable::objects.end()) {
          auto object = (Traceable*)*it;
          if (object->marked) {
              // Alive object, reset the mark bit for future collection cycles
              object->marked = false;
              ++it;
          }
          else {
              it = Traceable::objects.erase(it);
              delete(object);
          }
      }
  }
};

#endif