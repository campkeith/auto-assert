# auto-assert
LLVM pass that adds assertions to check for undefined behavior

Five kinds of checks are supported:

* getelementptr bounds checking
* add/sub/mul signed wrap checking
* shl/ashr/shr shift bounds checking
* sdiv/srem overflow
* udiv/urem/sdiv/srem divide by zero

## Usage

Place in the tools directory of an LLVM source tree to compile the passes.

Run with:

    opt -load <path-to-"autoassert.so"> -auto-assert

Then link with an assertion function implementation:

    llvm-link <input-llvm-ir> <path-to-"libassert.ll">

