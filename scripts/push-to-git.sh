#!/bin/bash

get_klee_dependencies()
{
	KLEE_BIN="$1"
	LIBS=$(get_external_library $KLEE_BIN libstdc++)
	LIBS="$LIBS $(get_external_library $KLEE_BIN tinfo)"
	LIBS="$LIBS $(get_external_library $KLEE_BIN libgomp)"
	# FIXME: remove once we build/download our z3
	LIBS="$LIBS $(get_any_library $KLEE_BIN libz3)"
	LIBS="$LIBS $(get_any_library $KLEE_BIN libstp)"

	echo $LIBS
}

get_nidhugg_dependencies()
{
	KLEE_BIN="$1"
	LIBS=$(get_external_library $KLEE_BIN libstdc++)
	LIBS="$LIBS $(get_external_library $KLEE_BIN libboost)"

	echo $LIBS
}

######################################################################
#  create distribution
######################################################################
# copy license
cp LICENSE.txt $PREFIX/
for license in licenses/*; do
	cp $license $PREFIX/
done

# copy readme
cp README.md $PREFIX/

# copy smoketests
cp -r smoketest $PREFIX/
cp smoketest.sh $PREFIX/
chmod +x $PREFIX/smoketest.sh

# copy the symbiotic python module
cp -r $SRCDIR/lib/symbioticpy $PREFIX/lib || exit 1

# copy dependencies
DEPENDENCIES=""

DEPS=`get_klee_dependencies $LLVM_PREFIX/witch-klee/bin/witch-klee`
for D in $DEPS; do
	DEST="$PREFIX/lib/$(basename $D)"
	cmp "$D" "$DEST" || cp -u "$D" "$DEST"
	DEPENDENCIES="$DEST $DEPENDENCIES"
done

cd $PREFIX || exitmsg "Whoot? prefix directory not found! This is a BUG, sir..."

BINARIES="$LLVM_PREFIX/bin/sbt-slicer \
	  $LLVM_PREFIX/bin/llvm-slicer \
	  $LLVM_PREFIX/bin/sbt-instr"

for B in $LLVM_TOOLS; do
	BINARIES="$LLVM_PREFIX/bin/${B} $BINARIES"
done

BINARIES="$BINARIES $LLVM_PREFIX/witch-klee/bin/witch-klee"

SCRIPTS=
	LIBRARIES="\
		$LLVM_PREFIX/lib/libdgllvmdg.so $LLVM_PREFIX/lib/libdgllvmpta.so \
		$LLVM_PREFIX/lib/libdgdda.so $LLVM_PREFIX/lib/libdganalysis.so \
		$LLVM_PREFIX/lib/libdgpta.so $LLVM_PREFIX/lib/libdgllvmdda.so \
		$LLVM_PREFIX/lib/libdgcda.so $LLVM_PREFIX/lib/libdgllvmcda.so \
		$LLVM_PREFIX/lib/libdgllvmthreadregions.so\
		$LLVM_PREFIX/lib/libdgllvmforkjoin.so\
		$LLVM_PREFIX/lib/libdgllvmpta.so\
		$LLVM_PREFIX/lib/libdgllvmcda.so \
		$LLVM_PREFIX/lib/libdgllvmslicer.so \
		$LLVM_PREFIX/lib/LLVMsbt.so \
		$LLVM_PREFIX/lib/libdgPointsToPlugin.so \
		$LLVM_PREFIX/lib/libPredatorPlugin.so \
		$LLVM_PREFIX/lib/libRangeAnalysisPlugin.so \
		$LLVM_PREFIX/lib/libCheckNSWPlugin.so \
		$LLVM_PREFIX/lib/libInfiniteLoopsPlugin.so \
		$LLVM_PREFIX/lib/libLLVMPointsToPlugin.so \
		$LLVM_PREFIX/lib/libValueRelationsPlugin.so"

BCFILES=""
BCFILES="${BCFILES} \
		$LLVM_PREFIX/witch-klee/lib/klee/runtime/*.bc* \
		$LLVM_PREFIX/witch-klee/lib32/klee/runtime/*.bc* \
		$LLVM_PREFIX/witch-klee/lib/*.bc* \
		$LLVM_PREFIX/witch-klee/lib32/*.bc*"
SCRIPTS=
	INSTR="$LLVM_PREFIX/share/sbt-instrumentation/"

if [ "$BUILD_Z3" = "yes" ]; then
	LIBRARIES="$LIBRARIES $PREFIX/lib/libz3*.so*"
fi

# strip binaries unless we are in a CI job, it will save us 500 MB
if [ -z "$CI" ]; then
	for B in $BINARIES $LIBRARIES; do
		echo "Stripping $B"
		test -w "$B" && strip "$B"
	done
fi

git init
git add \
	$BINARIES \
	$BCFILES \
	$SCRIPTS \
	$LIBRARIES \
	$DEPENDENCIES \
	$INSTR\
	bin/symbiotic \
	bin/kleetester.py \
	bin/gen-c \
	include/symbiotic.h \
	include/symbiotic-size_t.h \
        include/witch.h \
	$(find lib -name '*.c')\
	$(find . -name '*.bc')\
	properties/* \
	$(find lib/symbioticpy/symbiotic -name '*.py')\
	$(find lib/symbioticpy/clang -name '*.py')\
	$(find lib/symbioticpy/clang -name '*.so')\
	lib/symbioticpy/libclang-*dist-info \
	*LICENSE.txt README.md smoketest*
	#$LLVM_PREFIX/include/stddef.h \

git commit -m "Create Symbiotic distribution `date`" || true

# remove unnecessary files
# git clean -xdf

if [ "x$ARCHIVE" = "xyes" ]; then
	git archive --prefix "$ARCHIVE_PREFIX" -o witch.zip -9 --format zip HEAD
	mv witch.zip ..
fi
