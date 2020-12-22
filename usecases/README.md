# Before building any benchmark
To be run once each time the docker container is restarted:

`./prepare-build.sh`

# To set up the sysroot
## Make sure that libc6-dev is installed
`(repo root)/toolchain/_helpers/install-to-sysroot.sh libc6-dev`

Remove unwanted libc6-dev files: `(repo root)/toolchain/_helpers/remove-unwanted-from-sysroot.sh`

## Find package owning a specific file
https://packages.ubuntu.com/

or

`(repo root)/toolchain/_helpers/find-package.sh [file name]`

## Install package from remote into local sysroot
`(repo root)/toolchain/_helpers/install-to-sysroot.sh [package name]`


# To run Diablo
To improve the accuracy of Diablo's liveness analysis, we provide the tool with liveness information for the floating-point registers.
Diablo will try to load that information automatically when running, and will fail if no information is found for a dependency.

## Collect liveness information
`(repo root)/liveness/generate-liveness-info.sh [library name]`
