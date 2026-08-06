# Portability test image: fresh, minimal Ubuntu 20.04 (Intel/amd64), no
# pre-installed build tools. Everything needed is installed by
# scripts/test_ubuntu20.sh, which is the actual test - this Dockerfile is
# just a disposable, reproducible wrapper around it (the script itself works
# the same way outside Docker too, e.g. in a bare VM or CI runner).
#
# Build (from the repo root, i.e. this file's own directory):
#   docker build --platform=linux/amd64 -t cdx-ubuntu20-test .
#
# Run:
#   docker run --rm --platform=linux/amd64 cdx-ubuntu20-test
#
# --platform=linux/amd64 forces a real Intel/amd64 image + build even when
# building/running from an Apple Silicon (arm64) host, via QEMU emulation.

FROM --platform=linux/amd64 ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

# Only what's needed to clone the repo; scripts/test_ubuntu20.sh installs
# every real build dependency itself, so the image starts genuinely bare.
RUN apt-get update -qq && apt-get install -y -qq git ca-certificates

ARG REPO_URL=https://github.com/JolanBoucher/cdx.git
ARG REPO_REF=main

WORKDIR /workspace
RUN git clone --branch "${REPO_REF}" --depth 1 "${REPO_URL}" cdx

WORKDIR /workspace/cdx
RUN chmod +x scripts/test_ubuntu20.sh

CMD ["./scripts/test_ubuntu20.sh"]
