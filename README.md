# auto-assert
LLVM pass that adds assertions to check for undefined behavior

Place in the tools directory of an LLVM source tree to compile the passes.

Run with:

    opt -load <path-to-"autoassert.so"> -auto-assert

Then link with an assertion function implementation:

    llvm-link <input-llvm-ir> <path-to-"libassert.ll">

