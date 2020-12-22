FROM debian:buster

RUN apt-get update && \
    apt-get install -y sudo make gcc g++ gfortran wget firefox-esr vlc evince less vim git

# to build the use cases
RUN apt-get install -y zlib1g-dev libbz2-dev libffi-dev libssl-dev uuid-dev libsqlite3-dev libgdbm-dev liblzma-dev libncursesw5-dev libncursesw5 libtinfo-dev libreadline-dev patch qt5-default libqt5svg5-dev qttools5-dev libgpg-error-dev libgcrypt20-dev libargon2-dev libqrencode-dev libsodium-dev xclip
