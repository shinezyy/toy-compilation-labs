rsync -arzvL --update \
    --exclude=cmake-build-debug \
    --exclude=.idea \
    --exclude=.git \
    --exclude=include \
    --no-perms --no-owner --no-group \
    -e 'ssh -p 11022' \
    . \
    zyy@124.16.85.152:/home/zyy/clang-devel-clion/

