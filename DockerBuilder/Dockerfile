FROM debian:buster-slim

COPY sources.list /etc/apt/sources.list

RUN apt-get update -yq && apt-get -yq install \
                                 autoconf \
                                 automake \
                                 devscripts \
                                 gawk \
                                 g++ \
                                 git-core \
                                 'libjpeg-dev|libjpeg62-turbo-dev' \
                                 libncurses5-dev \
                                 'libtool-bin|libtool' \
                                 make \
                                 python-dev \
                                 gawk \
                                 pkg-config \
                                 libtiff5-dev \
                                 libperl-dev \
                                 libgdbm-dev \
                                 libdb-dev \
                                 gettext \
                                 libssl-dev \
                                 libcurl4-openssl-dev \
                                 libpcre3-dev \
                                 libspeex-dev \
                                 libspeexdsp-dev \
                                 libsqlite3-dev \
                                 libedit-dev \
                                 libldns-dev \
                                 libpq-dev \
                                 dpkg-dev \
                                 reprepro \
                                 s3cmd