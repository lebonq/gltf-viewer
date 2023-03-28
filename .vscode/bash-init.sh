clone_gltf_samples() {
  git clone https://github.com/KhronosGroup/glTF-Sample-Models gltf-sample-models
}

get_sponza_sample() {
  mkdir -p gltf-sample-models/2.0/ || true
  curl -L https://gltf-viewer-tutorial.gitlab.io/assets/Sponza.zip > gltf-sample-models/2.0/Sponza.zip
  unzip gltf-sample-models/2.0/Sponza.zip -d gltf-sample-models/2.0/
  rm gltf-sample-models/2.0/Sponza.zip
}

get_damaged_helmet_sample() {
  mkdir -p gltf-sample-models/2.0/ || true
  curl -L https://gltf-viewer-tutorial.gitlab.io/assets/DamagedHelmet.zip > gltf-sample-models/2.0/DamagedHelmet.zip
  unzip gltf-sample-models/2.0/DamagedHelmet.zip -d gltf-sample-models/2.0/
  rm gltf-sample-models/2.0/DamagedHelmet.zip
}

cmake_clean() {
  rm -rf build
  rm -rf dist
}

cmake_prepare() {
  # Note: $@ forwards argument so you can add additional arguments
  # e.g: cmake_prepare -DCMAKE_BUILD_TYPE=Release to configure the build solution in release mode with gcc
  cmake -S . -B build -DCMAKE_INSTALL_PREFIX=./dist $@
}

cmake_build() {
  # Note: $@ forwards argument so you can add additional arguments
  # e.g: cmake_build --config release to build in release mode with Visual Studio Compiler
  cmake --build build -j $@
}

cmake_install() {
  # Note: $@ forwards argument so you can add additional arguments
  # e.g: cmake_install --config release to build in release mode, then install, with Visual Studio Compiler
  cmake --build build -j --target install $@
}

view_sponza() {
  cmake_prepare
  cmake_install
  dist/gltf-viewer viewer gltf-sample-models/2.0/Sponza/glTF/Sponza.gltf \
    --lookat 0.926627,4.73407,0.16938,-0.0712768,4.70099,0.113776,-0.0330304,0.999454,-0.00183996
}

view_helmet() {
  cmake_prepare
  cmake_install
  dist/gltf-viewer viewer gltf-sample-models/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf
}

view_ABC(){
  cmake_prepare
  cmake_install
  dist/gltf-viewer viewer gltf-sample-models/2.0/ABeautifulGame/glTF/ABeautifulGame.gltf \
  --lookat 0.470957,0.26103,0.319238,-0.158447,-0.0374414,-0.132236,-0.29217,0.933121,-0.209575
}

view_avocado(){
  cmake_prepare
  cmake_install
  dist/gltf-viewer viewer gltf-sample-models/2.0/Avocado/glTF/Avocado.gltf \
  --lookat -0.00332791,0.0428359,0.0799262,0,0.0314002,-4.65661e-10,0.00588725,0.989936,-0.141394
}

view_suzanne(){
  cmake_prepare
  cmake_install
  dist/gltf-viewer  viewer gltf-sample-models/2.0/Suzanne/glTF/Suzanne.gltf
}

view_water_bottle(){
  cmake_prepare
  cmake_install
  dist/gltf-viewer viewer gltf-sample-models/2.0/WaterBottle/glTF/WaterBottle.gltf \
  --lookat 0.244619,-0.145272,0.0200841,-0.0500873,0.0288614,0.00669602,0.507791,0.861171,0.0230681
}

render_all() {
  render_sponza
  render_helmet
  render_ABC
  render_avocado
  render_suzanne
  render_water_bottle
}

render_sponza() {
  cmake_prepare
  cmake_install
  [ ! -d output-images ] && mkdir output-images
  dist/gltf-viewer viewer gltf-sample-models/2.0/Sponza/glTF/Sponza.gltf \
    --lookat 0.926627,4.73407,0.16938,-0.0712768,4.70099,0.113776,-0.0330304,0.999454,-0.00183996 \
    --output output-images/sponza.png --w 3840 --h 2160
}

render_helmet() {
  cmake_prepare
  cmake_install
  [ ! -d output-images ] && mkdir output-images
  dist/gltf-viewer viewer gltf-sample-models/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf \
    --output output-images/helmet.png --w 3840 --h 2160
}

render_ABC(){
  cmake_prepare
  cmake_install
  [ ! -d output-images ] && mkdir output-images
  dist/gltf-viewer viewer gltf-sample-models/2.0/ABeautifulGame/glTF/ABeautifulGame.gltf \
  --lookat 0.470957,0.26103,0.319238,-0.158447,-0.0374414,-0.132236,-0.29217,0.933121,-0.209575 \
  --output output-images/ABG.png --w 3840 --h 2160
}

render_avocado(){
  cmake_prepare
  cmake_install
  [ ! -d output-images ] && mkdir output-images
  dist/gltf-viewer viewer gltf-sample-models/2.0/Avocado/glTF/Avocado.gltf \
  --lookat -0.00332791,0.0428359,0.0799262,0,0.0314002,-4.65661e-10,0.00588725,0.989936,-0.141394 \
  --output output-images/avocado.png --w 3840 --h 2160
}

render_suzanne(){
  cmake_prepare
  cmake_install
  [ ! -d output-images ] && mkdir output-images
  dist/gltf-viewer viewer gltf-sample-models/2.0/Suzanne/glTF/Suzanne.gltf \
  --output output-images/suzanne.png --w 3840 --h 2160
}

render_water_bottle(){
  cmake_prepare
  cmake_install
  [ ! -d output-images ] && mkdir output-images
  dist/gltf-viewer viewer gltf-sample-models/2.0/WaterBottle/glTF/WaterBottle.gltf \
  --lookat 0.244619,-0.145272,0.0200841,-0.0500873,0.0288614,0.00669602,0.507791,0.861171,0.0230681 \
  --output output-images/water_bottle.png --w 3840 --h 2160
}
