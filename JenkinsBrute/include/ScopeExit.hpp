#pragma once

#include <functional>

namespace jcmp {

#define _SCOPE_CONCAT2(a, b) a ## b
#define _SCOPE_CONCAT(a, b) _SCOPE_CONCAT2(a, b)

#define SCOPE_EXIT_BASE(counter) \
 struct _SCOPE_CONCAT(ScopeExit, counter) \
  {  \
    _SCOPE_CONCAT(ScopeExit, counter)(std::function<void()> scopeExitFn) : scopeExitFn(scopeExitFn) {} \
    ~_SCOPE_CONCAT(ScopeExit, counter)() { scopeExitFn(); } \

#define SCOPE_EXIT \
    SCOPE_EXIT_BASE(__COUNTER__) \
    std::function<void()> scopeExitFn; \
  } _SCOPE_CONCAT(scopeExit, __COUNTER__)([&]()

#define SCOPE_EXIT_END \
  );

#define scope_exit(a) SCOPE_EXIT a SCOPE_EXIT_END
}