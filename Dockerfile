FROM nvidia/opengl:1.0-glvnd-runtime-ubuntu16.04 as nvidia
FROM ubuntu:20.04

COPY --from=nvidia /usr/local /usr/local
COPY --from=nvidia /etc/ld.so.conf.d/glvnd.conf /etc/ld.so.conf.d/glvnd.conf

ENV NVIDIA_VISIBLE_DEVICES=all NVIDIA_DRIVER_CAPABILITIES=all

ENV TZ=Europe/Paris
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  libglvnd-dev libgl1-mesa-dev libegl1-mesa-dev \
  cmake \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

ADD . .
RUN cmake -S . -B build -DCMAKE_INSTALL_PREFIX=./dist
RUN cmake --build build -j --target install

CMD [ \
  "dist/gltf-viewer", "viewer", ".local/gltf-sample-models/2.0/Sponza/glTF/Sponza.gltf", \
  "--lookat", "-5.26056,6.59932,0.85661,-4.40144,6.23486,0.497347,0.342113,0.931131,-0.126476", \
  "--output", "output-images/sponza.png" \
  ]
