LStore Release Tools
==============================================

Structure
-----------------------------------------------
* scripts  - Build scripts
  * docker - Cached Dockerfiles
    * base - Bare images with only LStore dependencies and build tools installed
* source   - Source repositories
* build    - Location where all sources are built
* logs     - Build logs
* local    - Installation path for packages built with build-*.sh
* package  - Storage with built RPMs
* repo     - Default YUM/APT repositories

Build process
----------------------------------------------
The build is broken into 2 steps.

1. Building the external dependencies
2. Building local packages

If the external packages are already installed you can skip step 1 and 
proceed directly to step 2. 

Building the external packages
----------------------------------------------
All the external dependencies can be built using:
>    ./scripts/build-external.sh

These only include ACCRE-modified externals. You will need to bring your own
copies of:

* openssl-devel
* czmq-devel
* zmq-devel
* zlib-devel
* fuse-devel

In addition, LStore has build-time dependencies on

* C, C++ compiler
* cmake

For centos, at least, these dependencies can be installed with:

```
yum groupinstall "Development Tools"
yum install cmake openssl-devel czmq-devel zmq-devel zlib-devel fuse-devel
```

If the local CMake installation is too old, we install a local copy into build/

Building the local project packages
----------------------------------------------
All of the local dependencies can be built using:
>    ./scripts/build-local.sh

Packaging LStore
----------------------------------------------
LStore uses a docker-based system for packaging LStore for various linux
distributions. In general, the packaging scripts all accept a list of
distributions on the command line. By default, each distribution will be
attempted. These base images containing external dependencies and build tools
can be bootstrapped with:

>    ./scripts/build-docker-base.sh [distribution] [distribution] ...

For each supported distribution, a docker image named `lstore/builder:DIST`
will pe produced and tagged. For instance, a base Centos 7 image will be named
`lstore/builder:centos-7`. These images can be updated by executing
`build-docker-base.sh` again.

Once the base images are installed, the current source tree can be packaged
with:

>    ./scripts/package.sh [distribution] [distribution] ...

Once `package.sh` completes, the output binaries for each distribution will be
stored in `package/<distribution>/<package>/<revision>`. The revisions are
auto-generated by a heuristic that considers the number of git commits between
the working copy and the most recent release tag.
