/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/LexicalPath.h>
#include <AK/String.h>
#include <AK/StringBuilder.h>
#include <LibCore/StandardPaths.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

namespace Core {

String StandardPaths::home_directory()
{
    if (auto* home_env = getenv("HOME"))
        return LexicalPath::canonicalized_path(home_env);

    auto* pwd = getpwuid(getuid());
    String path = pwd ? pwd->pw_dir : "/";
    endpwent();
    return LexicalPath::canonicalized_path(path);
}

String StandardPaths::desktop_directory()
{
    StringBuilder builder;
    builder.append(home_directory());
    builder.append("/Desktop");
    return LexicalPath::canonicalized_path(builder.to_string());
}

String StandardPaths::downloads_directory()
{
    StringBuilder builder;
    builder.append(home_directory());
    builder.append("/Downloads");
    return LexicalPath::canonicalized_path(builder.to_string());
}

String StandardPaths::documents_directory()
{
    StringBuilder builder;
    builder.append(home_directory());
    builder.append("/Documents");
    return LexicalPath::canonicalized_path(builder.to_string());
}

String StandardPaths::source_directory()
{
    StringBuilder builder;
    builder.append(home_directory());
    builder.append("/Source");
    return LexicalPath::canonicalized_path(builder.to_string());
}

String StandardPaths::music_directory()
{
    StringBuilder builder;
    builder.append(home_directory());
    builder.append("/Music");
    return LexicalPath::canonicalized_path(builder.to_string());
}

String StandardPaths::pictures_directory()
{
    StringBuilder builder;
    builder.append(home_directory());
    builder.append("/Pictures");
    return LexicalPath::canonicalized_path(builder.to_string());
}

String StandardPaths::videos_directory()
{
    StringBuilder builder;
    builder.append(home_directory());
    builder.append("/Videos");
    return LexicalPath::canonicalized_path(builder.to_string());
}

String StandardPaths::config_directory()
{
    StringBuilder builder;
    builder.append(home_directory());
    builder.append("/.config");
    return LexicalPath::canonicalized_path(builder.to_string());
}

String StandardPaths::tempfile_directory()
{
    return "/tmp";
}

}
