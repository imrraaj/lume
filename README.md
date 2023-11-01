# Lume

## Description

A hobby programming language.

Lume... because it's simple and lightweight yet also very flammable if handled improperly 🔥

## Build Instructions

Linux only for now.

Dependencies:

- CMake
- NASM
- ld

```console
$ git clone https://github.com/imrraaj/lume
$ cd lume
$ cmake -S . -B build
$ cmake --build build
```

## Compiler Usage

```console
$ lume <program.lm>
$ ./program
```

The following files will be generated when compiled:

- `program` - The executable
