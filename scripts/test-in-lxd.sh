#!/bin/bash
set -eux

TESTER=$1

lxc config device add $TESTER code disk source=`pwd` path=/src

lxc exec $TESTER -- sh -ec "
    cd ~
    cp -a /src .
    [ -d ~/src ]
    "

lxc exec $TESTER -- cloud-init status --wait

build_deps="build-essential $($(dirname $0)/build-depends.py)"

lxc exec $TESTER -- sh -ec "
    cd ~/src
    DEBIAN_FRONTEND=noninteractive apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y $build_deps
    dpkg-buildpackage -b -uc"
