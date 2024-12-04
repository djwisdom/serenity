#!/usr/bin/env -S bash ../.port_include.sh
port=zsh
version=5.9
files=(
    "https://sourceforge.net/projects/zsh/files/zsh/${version}/zsh-${version}.tar.xz#9b8d1ecedd5b5e81fbf1918e876752a7dd948e05c1a0dba10ab863842d45acd5"
)
launcher_name='Zsh'
launcher_category='&Utilities'
launcher_command='/usr/local/bin/zsh'
launcher_run_in_terminal='true'
useconfigure=true
use_fresh_config_sub=true

pre_configure() {
    run "./Util/preconfig"
}

post_configure() {
    run_replace_in_file "s/define HAVE_PRCTL 1/undef HAVE_PRCTL/" config.h
}

post_install() {
    cp "${PORT_META_DIR}/zshrc" "${SERENITY_INSTALL_ROOT}/etc/"
cat << 'EOF' > "${SERENITY_INSTALL_ROOT}/home/anon/.zshrc"
#!/usr/local/bin/zsh  
PS1=$'[\033[1;37mzsh\033[0m] %# '
export SHELL='/usr/local/bin/zsh'
echo "\033[1;37mSerenity\033[0mOS [Version 1.0-dev]\n(c) the SerenityOS developers, 2018-2024\n"
EOF
}
