[[ -f .venv/Scripts/activate ]] && source .venv/Scripts/activate
[[ -f .venv/bin/activate ]] && source .venv/bin/activate

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
