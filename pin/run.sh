#!/usr/bin/env bash
set -euo pipefail

# /data/pin-3.16-98275-ge0db48c31-gcc-linux.tar.gz
#  should have been extracted in docker/build/

container_name=pin

if ! docker image ls | grep $container_name; then
  # need to build the container
  echo "Building container '$container_name'"
  docker build . -t $container_name
fi

toplevel=$(git rev-parse --show-toplevel)

additional_volumes="-v $(git rev-parse --show-toplevel):/data/BOEK -v /home/jens/repositories/framework/projects:/projects"
# docker run --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --net=host -it --env="DISPLAY" -v ${HOME}/.Xauthority:/root/.Xauthority -v /tmp/.X11-unix:/tmp/.X11-unix -v ${PWD}/docker/build:/build $additional_volumes $container_name
$toplevel/repos/x11docker/x11docker --gpu --clipboard --pulseaudio --sudouser --interactive -- -v ${PWD}/docker/build:/build $additional_volumes -- pin
# password: x11docker
