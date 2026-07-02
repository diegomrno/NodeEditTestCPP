mkdir -p ../lib/vortex/tests/project/.vx/modules
cp -r ../dist/* ../lib/vortex/tests/project/.vx/modules
cp -r ../../NodeEdit/dist/* ../lib/vortex/tests/project/.vx/modules

VERSION=$(cat ../lib/vortex/version.conf)
SCRIPT_DIR=$(dirname "$(realpath "$0")")
VORTEX_PATH="${SCRIPT_DIR}/../lib/vortex/build/dist/${VERSION}/bin/"
PROJECT_PATH="${SCRIPT_DIR}/../lib/vortex/tests/project"

bash "$VORTEX_PATH/vx.sh" \
  "$PROJECT_PATH"
