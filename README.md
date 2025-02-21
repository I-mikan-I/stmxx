# STMXX
## A Modern C++ Software Transactional Memory Implementation

### Features

- Multiple self-contained transaction *types*.
- Transparent Memory Isolation of arbitrary concurrent transactions of the same type.
- Lazy conflict detection during commit.
- Transparent automatic retries on conflict.
- Support for arbitrarily large data types shared between competing transactions.
- Efficient transactional reading of individual data members.


### Planned

- [ ] Merged transaction types that subsume both types and combine their atomic execution semantics.
- [ ] Quick abort utilizing stack unwinding for legacy/non-transactional code bases.

## Usage

### Requirements

- C++23 CMake supported toolchain.
- Build system supporting C++ modules, e.g. Ninja.
- [Clang for running unit tests under sanitizers.]\(optional\)