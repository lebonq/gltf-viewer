[[ -f .venv/Scripts/activate ]] && source .venv/Scripts/activate
[[ -f .venv/bin/activate ]] && source .venv/bin/activate

clone_gltf_samples() {
  mkdir -p .local || true
  git clone https://github.com/KhronosGroup/glTF-Sample-Models .local/gltf-sample-models
}

get_sponza_sample() {
  mkdir -p .local/gltf-sample-models/2.0/ || true
  curl -L https://gltf-viewer-tutorial.gitlab.io/assets/Sponza.zip > .local/gltf-sample-models/2.0/Sponza.zip
  unzip .local/gltf-sample-models/2.0/Sponza.zip -d .local/gltf-sample-models/2.0/
  rm .local/gltf-sample-models/2.0/Sponza.zip
}

get_damaged_helmet_sample() {
  mkdir -p .local/gltf-sample-models/2.0/ || true
  curl -L https://gltf-viewer-tutorial.gitlab.io/assets/DamagedHelmet.zip > .local/gltf-sample-models/2.0/DamagedHelmet.zip
  unzip .local/gltf-sample-models/2.0/DamagedHelmet.zip -d .local/gltf-sample-models/2.0/
  rm .local/gltf-sample-models/2.0/DamagedHelmet.zip
}

# source: https://gist.github.com/judy2k/7656bfe3b322d669ef75364a46327836
# usage: export_envs FILE
export_envs() {
  local envFile=${1:-.env}
  while IFS='=' read -r key temp || [ -n "$key" ]; do
    local isComment='^[[:space:]]*#'
    local isBlank='^[[:space:]]*$'
    [[ $key =~ $isComment ]] && continue
    [[ $key =~ $isBlank ]] && continue
    value=$(eval echo "$temp")
    eval export "$key='$value'";
  done < $envFile
}

[ -f ".local/.env" ] && export_envs .local/.env
