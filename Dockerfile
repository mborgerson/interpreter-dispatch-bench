FROM ubuntu:24.10
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive \
    apt-get install -qq build-essential clang
