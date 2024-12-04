#!/usr/bin/env -S bash ../.port_include.sh
port='bash'
version='5.2.37'
useconfigure='true'
use_fresh_config_sub='true'
use_fresh_config_guess='true'
config_sub_paths=("support/config.sub")
config_guess_paths=("support/config.guess")
configopts=("--disable-nls" "--without-bash-malloc")
files=(
    "https://ftpmirror.gnu.org/gnu/bash/bash-${version}.tar.gz#9599b22ecd1d5787ad7d3b7bf0c59f312b3396d1e281175dd1f8a4014da621ff"
)
launcher_name='Bash'
launcher_category='&Utilities'
launcher_command='/usr/local/bin/bash'
launcher_run_in_terminal='true'

build() {
    run_replace_in_file "s/define GETCWD_BROKEN 1/undef GETCWD_BROKEN/" config.h
    run_replace_in_file "s/define CAN_REDEFINE_GETENV 1/undef CAN_REDEFINE_GETENV/" config.h
    run make "${makeopts[@]}"
}

post_install() {
    mkdir -p "${SERENITY_INSTALL_ROOT}/bin"
    ln -sf /usr/local/bin/bash "${SERENITY_INSTALL_ROOT}/bin/bash"
    cat << 'EOF' > "${SERENITY_INSTALL_ROOT}/home/anon/.bashrc"
#!/usr/local/bin/bash
export PS1='[\033[37;1mbash\033[0m] \$ '
export PS2='> '
export PS3='+ '
export LANG='C.UTF-8'
export SHELL='/usr/local/bin/bash'
echo -e "\e[37;1mSerenity\e[0mOS [Version 1.0-dev]\n(c) the SerenityOS developers, 2018-2024\n"
EOF
}
