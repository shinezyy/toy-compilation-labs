rsync -arzvL --update \
    --exclude=cmake-build-debug \
    --exclude=.idea \
    --exclude=.git \
    --exclude=include \
    --exclude=llvm-include \
    --no-perms --no-owner --no-group \
    -e 'ssh -p 22' \
    . \
    zyy@124.16.84.210:/home/zyy/clang-devel-clion/

