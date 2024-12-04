#!/usr/bin/env -S bash ../.port_include.sh
port=dash
version=0.5.10.2
useconfigure=true
files=(
    "http://gondor.apana.org.au/~herbert/dash/files/dash-${version}.tar.gz#3c663919dc5c66ec991da14c7cf7e0be8ad00f3db73986a987c118862b5f6071"
)
launcher_name='Dash'
launcher_category='&Utilities'
launcher_command='/usr/local/bin/dash'
launcher_run_in_terminal='true'

configure() {
    host_env
    run autoupdate
    run autoconf
    run aclocal
    run automake --add-missing
    run mkdir -p host-build
    run sh -c "cd host-build && ../configure ${configopts[@]} CFLAGS=-I."
    target_env
    run mkdir -p target-build
    run sh -c "cd target-build && ../configure --host="${SERENITY_ARCH}-pc-serenity" --disable-helpers ${configopts[@]} CFLAGS=-I."
}

build() {
    host_env
    run sh -c "cd host-build && make ${makeopts[@]}"
    run cp host-build/src/{mkinit,mksyntax,mknodes,mksignames} src
    target_env
    run sh -c "cd target-build && make ${makeopts[@]}"
}

install() {
    run sh -c "cd target-build && make DESTDIR="${SERENITY_INSTALL_ROOT}" ${installopts[@]} install"
}

post_installi() {
    cat << 'EOF' > "${SERENITY_INSTALL_ROOT}/home/anon/.profile"
export PS1='[dash] \$ '
export PS2='> '
export PS3='+ '
export LANG='C.UTF-8'
export SHELL='/usr/local/bin/dash'
echo "SerenityOS [Version 1.0-dev]\n(c) the SerenityOS developers, 2018-2024\n"
EOF
}
