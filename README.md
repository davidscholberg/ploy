# Ploy

Ploy is a command line [Scheme](https://www.scheme.org/) interpreter implemented as a stack-based bytecode virtual machine and written in [C++23](https://en.wikipedia.org/wiki/C%2B%2B23).

### Why did I make this?

I like Scheme. I like C++. I wanted to learn more about programming language design and implementation, and since writing a C++ compiler in Scheme seemed like a reckless endeavor, I instead opted to write a Scheme interpreter in C++.

### What's implemented so far?

Uhhhh not a whole lot. You can check out the [test cases](src/tests/test_cases/) directory for an overview of what's been implemented so far. Some of the highlights featurewise include:

* Basic arithmetic (yay)
* `if` expressions
* Primitive `lambda` support with variable captures
* `define` and `set!` expressions
* Pairs and symbols
* `display` procedure
* Continuations (`call/cc`)

Error handling is absolutely bare-bones at the moment. Exceptions are thrown with almost no context that you'd normally want from a language interpreter.

Also, there is no [REPL](https://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop). I don't really like REPLs all that much so I'm not prioritizing it.

### Scheme standard?

Currently the plan is to target the [R7RS Scheme standard](https://standards.scheme.org/official/r7rs.pdf). I initially targeted R5RS for this project, and there's still remnants of that in the code (mostly in the tokenizer).

### Design choices

The main resource that helped get this project off the ground is the excellent book [Crafting Interpreters: A Bytecode Virtual Machine](https://craftinginterpreters.com/a-bytecode-virtual-machine.html). As such, my interpreter is a stack-based bytecode virtual machine like the one in the book. While my implementation does share some fundamental design choices with the interpreter in the book, there are some differences:

* My interpreter is written in C++ instead of C and heavily uses the [C++ Standard Library](https://en.cppreference.com/w/cpp/memory/shared_ptr). This has undoubtedly sped up the implementation of this interpreter (likely at the cost of some performance, but performance is something I can worry about later).
* The tokenizer completely finishes tokenizing before handing off the tokens to the bytecode compiler, instead of the tokenizer and compiler working in lockstep. The former approach seemed like it might be more performant and potentially more amenable to macro expansions.
* Captured variables and reference types are reference-counted instead of garbage collected. This is mostly because I figured it would be easier to implement up front since [`std::shared_ptr`](https://en.cppreference.com/w/cpp/memory/shared_ptr) already exists.
* Scheme values are represented as [`std::variant`](https://en.cppreference.com/w/cpp/utility/variant) types instead of raw unions. This allows us to use [`std::visit`](https://en.cppreference.com/w/cpp/utility/variant/visit2) and the [overload pattern](https://www.modernescpp.com/index.php/visiting-a-std-variant-with-the-overload-pattern/) to cleanly handle all the dynamic dispatching that needs to be done on scheme values based on their underlying type.

### Project direction

Moving forward, my main goals with this project are to keep implementing features from the R7RS standard (prioritizing ones that I find interesting) and to fix up error handling to be more end-user friendly.

Writing performant code is a secondary goal. Making implementation decisions for new features will be a balance between ease and performance, where ease is slightly more prioritized up front with the possibility that more performant implementations are revisited down the road.

## Building

### Requirements

To build ploy, you'll need [CMake](https://cmake.org/) and a C++ compiler that supports C++23. [GCC](https://gcc.gnu.org/), [MSVC](https://en.wikipedia.org/wiki/Microsoft_Visual_C%2B%2B), and [Clang](https://clang.llvm.org/) are all supported.

### Build on the cli

Note: for all of the following commands, replace `Debug` with `Release` for release builds.

Configure and build:

```bash
cmake -B build/Debug -D CMAKE_BUILD_TYPE=Debug
cmake --build build/Debug
```

Run ploy on a scheme program:

```bash
./build/Debug/src/ploycli/ploy /path/to/blah.scm
```

View bytecode disassembly of a scheme program:

```bash
./build/Debug/src/ploycli/ploy -d /path/to/blah.scm
```

Run tests:

```bash
ctest --output-on-failure --test-dir build/Debug/src/tests/
```
