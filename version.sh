#!/bin/bash
git rev-list HEAD | sort > config.git-hash
LOCALVER=`wc -l config.git-hash | awk '{print $1}'`
if [ $LOCALVER \> 1 ] ; then
    VER=`git rev-list origin/master | sort | join config.git-hash - | wc -l | awk '{print $1}'`
    if [ $VER != $LOCALVER ] ; then
        VER="$VER+$(($LOCALVER-$VER))"
    elif git status | grep -q "modified:" ; then
        VER="${VER}M"
    fi
    VER="$VER $(git rev-list HEAD -n 1 | head -c 7)"
    echo "#define XAVS_VERSION \" r$VER\"" >> config.h
else
    echo "#define XAVS_VERSION \"\"" >> config.h
    VER="x"
fi
rm -f config.git-hash
API=`grep '#define XAVS_BUILD' < xavs.h | sed -e 's/.* \([1-9][0-9]*\).*/\1/'`
echo "#define XAVS_POINTVER \"0.$API.$VER\"" >> config.h
