#!/usr/bin/env bash

LLVMLITE_DIR=llvmlite
SLOWBEAST_DIR=slowbeast
MATHSAT5_DIR=mathsat5

if [ ! -d ${LLVMLITE_DIR} ];then
    git clone https://github.com/mchalupa/llvmlite
fi
if [ ! -d ${SLOWBEAST_DIR} ];then
    # git clone https://gitlab.com/mchalupa/slowbeast
    # git clone https://gitlab.fi.muni.cz/xkumor/slowbeastcse.git slowbeast
    git clone -b feature/sewpa-integration https://gitlab.com/JindraSe/slowbeast.git
fi
if [ ! -d ${MATHSAT5_DIR} ];then
    wget https://mathsat.fbk.eu/release/mathsat-5.6.12-linux-x86_64.tar.gz
    tar -xzf mathsat-5.6.12-linux-x86_64.tar.gz
    mv mathsat-5.6.12-linux-x86_64 ${MATHSAT5_DIR}
    rm mathsat-5.6.12-linux-x86_64.tar.gz
fi

if [ -e ${LLVMLITE_DIR}/build ];then rm -r ${LLVMLITE_DIR}/build ; fi
if [ -e ${SLOWBEAST_DIR}/build ];then rm -r ${SLOWBEAST_DIR}/build ; fi
if [ -e ${SLOWBEAST_DIR}/dist ];then rm -r ${SLOWBEAST_DIR}/dist ; fi
if [ -e ${MATHSAT5_DIR}/build ];then rm -r ${MATHSAT5_DIR}/build ; fi

pushd ${LLVMLITE_DIR}
python3 setup.py build
popd

pushd ${MATHSAT5_DIR}/python
python3 setup.py build
cp build/lib.linux-x86_64-3.10/_mathsat.cpython-310-x86_64-linux-gnu.so .
popd

pushd ${SLOWBEAST_DIR}
pyinstaller -p ../${LLVMLITE_DIR} -p ../${MATHSAT5_DIR}/python --collect-binaries z3 --collect-binaries mathsat --collect-binaries _mathsat sb
popd
