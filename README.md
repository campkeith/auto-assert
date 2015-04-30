# auto-assert
LLVM pass that adds assertions to check for undefined behavior

* getelementptr,load,store null pointer check
* getelementptr bounds checking
* add,sub,mul,shl unsigned/signed wrap checking
* udiv,sdiv,lshr,ashr exact result check
* shl,ashr,lshr shift bounds checking
* sdiv,srem overflow
* udiv,urem,sdiv,srem divide by zero

## Usage

Place in the tools directory of an LLVM source tree to compile the passes.

Run with:

    opt -load <path-to-"autoassert.so"> -auto-assert

Then link with an assertion function implementation:

    llvm-link <input-llvm-ir> <path-to-"libassert.ll">

