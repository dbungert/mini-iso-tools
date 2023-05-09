#!/bin/bash
set -eux

IMAGE=$1
TESTER=u1

build_deps="build-essential $($(dirname $0)/build-depends.py)"

lxc launch $IMAGE $TESTER
lxc config device add $TESTER code disk source=`pwd` path=/src
lxc exec $TESTER -- sh -ec "
    cp -a /src ~
    cd ~/src
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y $build_deps
    dpkg-buildpackage -b -uc
"
