#/usr/bin/bash
set -e
mkdir input
pushd input
git clone git://github.com/martine/ninja.git
pushd ninja
git checkout release
./configure.py --bootstrap
popd
curl -O https://raw.githubusercontent.com/martine/ninja/master/misc/ninja_syntax.py
curl -O ftp://ftp.simplesystems.org/pub/libpng/png/src/libpng12/libpng-1.2.53.tar.gz
tar -xzf libpng-1.2.53.tar.gz
ln -s libpng-1.2.53 libpng
pushd libpng
make -f scripts/makefile.darwin
popd
git clone --depth=1 git@github.com:madler/zlib
pushd zlib
./configure --static
make
popd