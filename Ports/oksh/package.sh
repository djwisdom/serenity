#!/usr/bin/env -S bash ../.port_include.sh
port='oksh'
version='7.1'
files=(
    "https://github.com/ibara/oksh/releases/download/oksh-${version}/oksh-${version}.tar.gz#9dc0b0578d9d64d10c834f9757ca11f526b562bc5454da64b2cb270122f52064"
)
launcher_name='Oksh'
launcher_category='&Utilities'
launcher_command='/usr/local/bin/oksh'
launcher_run_in_terminal='true'
useconfigure='true'
depends=("ncurses")

export LDFLAGS='-lncurses'

configure() {
    run ./configure --no-thanks
}

install() {
    run mkdir -p "${SERENITY_INSTALL_ROOT}/usr/local/bin"
    run cp oksh "${SERENITY_INSTALL_ROOT}/usr/local/bin"
}

post_install() {
    cat << 'EOF' > "${SERENITY_INSTALL_ROOT}/home/anon/.kshrc"
#!/usr/local/bin/oksh
export PS1='[\033[37;1moksh\033[0m] \$ '
export PS2='> '
export PS3='+ '
export LANG='C.UTF-8'
export SHELL='/usr/local/bin/oksh'
echo "\033[37;1mSerenity\033[0mOS [Version 1.0-dev]\n(c) the SerenityOS developers, 2018-2024\n"
EOF

    cat << 'EOF' >> "${SERENITY_INSTALL_ROOT}/home/anon/.profile"
export ENV='/home/anon/.kshrc'
EOF
}
