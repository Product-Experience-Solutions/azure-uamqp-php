FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
ARG PHUAMQP_PHP_MAJOR_VERSION=8.3
ARG PHUAMQP_PHP_CPP_VERSION=2.4.1
ENV DEBIAN_FRONTEND=${DEBIAN_FRONTEND}
ENV PHUAMQP_PHP_MAJOR_VERSION=${PHUAMQP_PHP_MAJOR_VERSION}
ENV PHUAMQP_PHP_CPP_VERSION=${PHUAMQP_PHP_CPP_VERSION}

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        curl \
        gnupg \
        lsb-release \
        netcat-openbsd \
        software-properties-common \
    && add-apt-repository -y ppa:ondrej/php \
    && apt-get update \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . /workspace
RUN chmod +x /workspace/setup.sh \
    && /bin/bash /workspace/setup.sh

CMD ["/bin/bash", "/workspace/scripts/run-e2e.sh"]

