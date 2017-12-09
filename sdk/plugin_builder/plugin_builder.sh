#!/bin/sh
set -ue

run_node_gyp(){
  arch=$1
  cross_compile=$2

  CC=${cross_compile}gcc CXX=${cross_compile}g++ LINK=${cross_compile}g++ \
    node-gyp rebuild --loglevel=warn --release --arch $arch

  mkdir -p native/$arch
  mv build/Release/*.node native/$arch
  ${cross_compile}strip -s native/$arch/*.node
}

install_npm_dependencies() {
  arch=$1
  cross_compile=$2

  yarn --production --ignore-scripts
  CC=${cross_compile}gcc CXX=${cross_compile}g++ LINK=${cross_compile}g++ \
    npm rebuild --arch=$arch

  BUILD_DEPS="$(node -e "console.log((JSON.parse(fs.readFileSync('package.json')).buildDependencies || []).join(' '))")"
  for dep in $BUILD_DEPS; do
    rm -rf node_modules/$dep
  done

  mv node_modules deps/$arch
}

cp -r /plugin /build
cd /build
if [ -f package.json ]; then
  # The plugin directory is mounted on a volume. This causes a problem when you
  # build the plugin in Docker in Mac since node_modules may contains binary files
  # built for macOS.
  rm -rf node_modules native build

  yarn --ignore-scripts

  if [ -f tsconfig.json ]; then
    if [ ! -f makestack.d.ts ]; then
      # Plugins excluding runtime requires makestack.d.ts.
      mkdir -p node_modules/@types/makestack
      cp /makestack.d.ts node_modules/@types/makestack/index.d.ts
    fi

    yarn transpile
  fi;

  if [ -f binding.gyp ]; then
    run_node_gyp x64   ''
    run_node_gyp arm   arm-linux-gnueabihf-
    run_node_gyp arm64 aarch64-linux-gnu-
  fi

  mkdir deps
  install_npm_dependencies x64   ''
  install_npm_dependencies arm   arm-linux-gnueabihf-
  install_npm_dependencies arm64 aarch64-linux-gnu-
fi;

if [ -f .makestackignore ]; then
  rm -rf $(cat .makestackignore)
fi

if [ -f /dist/plugin.zip ]; then
  rm /dist/plugin.zip
fi

zip -FSr /dist/plugin.zip *
