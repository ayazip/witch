
[![Build - Linux CI](https://github.com/staticafi/symbiotic/actions/workflows/linux.yml/badge.svg)](https://github.com/staticafi/symbiotic/actions/workflows/linux.yml)

Witch is a violation witness validator based on the 
[Symbiotic](https://github.com/staticafi/symbiotic) framework. It uses the general
infrastructure of Symbiotic as well as the symbolic executor [JetKlee](https://github.com/staticafi/JetKlee),
extending both to handle and validate witnesses in the format 2.0. 

Symbiotic is an open-source framework for program analysis integrating
instrumentation, static program slicing and various program analysis tools.
It is highly modular and most of its components are self-standing
programs or LLVM passes that have their own repositories at
https://github.com/staticafi.

## Getting started

### Building Witch from sources

First of all you must clone the repository:

```
$ git clone https://github.com/ayazip/witch
```

Run `build.sh` or `system-build.sh` script to compile Symbiotic:

```
$ cd symbiotic
$ ./build.sh -j2
```
The difference betwee `build.sh` and `system-build.sh` is that
`system-build.sh` will try to build only the components of Witch, using the
system's packages.  `build.sh`, on the other hand, tries to build also the most
of the missing dependencies, including LLVM, z3, etc.

The scripts should complain about missing dependencies if any. You can try using
`scripts/install-system-dependencies.sh` script to install the main
dependencies (or at least check the names of packages). If the build script
continues to complain, you must install the dependencies manually.

Possible options for the `build.sh` script include:
  - `build-type=TYPE` (TYPE one of `Release`, `Debug`)
  - `llvm-version=VERSION` (the default `VERSION` is `14.0.0`,
     other versions are rather experimental)
  - `with-llvm=`, `with-llvm-src=`, `with-llvm-dir=`
     This set of options orders the script to use already built external LLVM
     (the build script will build LLVM otherwise if it has not been built
     already in this folder)
  - `no-llvm` Do not try building LLVM

There are many other options, but they are not properly documented (check the
script). Actually, the whole build script should be rather a guidance of what
is needed and how to build the components, but is not guaranteed to work on any
system.

As you can see from the example, you can pass also arguments for make, e.g.
`-j2`, to the build script.  If you need to specify paths to header files or
libraries, you can do it by passing `CFLAGS`, `CPPFLAGS`, and/or `LDFLAGS`
environment variables either by exporting them beforehand, or by passing them
on the command line similarly to make options (e.g. ./build.sh `CFLAGS='-g'`)

If everything goes well, Witch and Symbiotic components are built and should be usable
right from the build directories (see the next section for more details).
Also, the components are installed to the `install/` directory that can be
packed or copied wherever you need (you can use ./build.sh `archive` to create
a .zip file or `full-archive` to create .zip file including system libraries
like libc with the build script).
The `install/` directory is under `git` control, so you can see the differences
between versions or manually create an archive using `git archive` command.

When building on mac, you may need to build LLVM with shared libraries
(modify the build script) or use `with-llvm-*` switch with your LLVM build.

### Running Witch

You can run Witch directly from the root directory:
```
scripts/symbiotic --witness-check witness.yml [--prp <property>] [--32 | --64] <OPTIONS> file.c
```
If you run witch from the `scripts/` directory, it uses the components
directly from the build directories, any changes to the components should
take effect in this mode.

Alternatively, you can run Witch also from the `install/` directory:
```
$ install/bin/symbiotic --witness-check witness.yml [--prp <property>] [--32 | --64] file.c
```

In this mode, Witch uses the components from the `install/` directory.

The options `--prp <property>` and `--32` or `--64` options specify the
considered property and architecture, the default being assertion safety and a LP64.
To view other possible options, run Witch either from the scripts or install
directory with the option `--help`.

### Symbiotic Components

Components of Symbiotic and Witch can be found at https://github.com/staticafi with the
only exception of `dg` library that is currently at https://github.com/mchalupa/dg.
All software used in Symbiotic are open-source projects and are licensed under various
open-source licenses (mostly MIT license, and University of Illinois Open Source license)

## Contact

For more information send an e-mail to <statica@fi.muni.cz>.
