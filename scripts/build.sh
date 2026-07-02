#!/bin/sh

rm -rf ../dist
rm -rf ../build

rm -rf ../lib/vortex/tests/project/.vx/modules/
mkdir -p ../build
cd ../build

cmake ..
make

MODULE_JSON_PATH="../module.json"
NAME=$(jq -r .name $MODULE_JSON_PATH)
VERSION=$(jq -r .version $MODULE_JSON_PATH)
FOLDER_NAME="$NAME-$VERSION-linux"

mkdir -p "../dist/$FOLDER_NAME"

cp -r ../build/* ../dist/$FOLDER_NAME/
cp -r ../lib ../dist/$FOLDER_NAME/ 2>/dev/null || true
cp -r ../main/assets/* ../dist/$FOLDER_NAME/ 2>/dev/null || true
cp ../module.json ../dist/$FOLDER_NAME/
cp ../LICENSE ../dist/$FOLDER_NAME/
cp ../CREDITS ../dist/$FOLDER_NAME/
cp -r ../docs ../dist/$FOLDER_NAME/
cp ../README.md ../dist/$FOLDER_NAME/

rm -rf "../dist/$FOLDER_NAME/CMakeFiles"
rm -rf "../dist/$FOLDER_NAME/scripts"
rm -rf "../dist/$FOLDER_NAME/.git"
rm -rf "../dist/$FOLDER_NAME/.vscode"
rm -rf "../dist/$FOLDER_NAME/lib"
rm "../dist/$FOLDER_NAME/remove_resources_if_exists.cmake" 2>/dev/null || true
rm "../dist/$FOLDER_NAME/cmake_install.cmake" 2>/dev/null || true
rm "../dist/$FOLDER_NAME/CMakeCache.txt" 2>/dev/null || true
rm "../dist/$FOLDER_NAME/dist.tar.gz" 2>/dev/null || true
rm "../dist/$FOLDER_NAME/Makefile" 2>/dev/null || true
rm "../dist/$FOLDER_NAME/.gitmodules" 2>/dev/null || true
rm "../dist/$FOLDER_NAME/CMakeLists.txt" 2>/dev/null || true

TAR_NAME="../build/$FOLDER_NAME.tar.gz"
tar -czf "$TAR_NAME" -C ../dist "$FOLDER_NAME"

echo "Archive created : $TAR_NAME"