# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "autocomplete",
    "banner",
    "execute",
    "language_id",
)

import os
import bpy

language_id = "shell"


def add_scrollback(text, text_type):
    for line in text.split("\n"):
        bpy.ops.console.scrollback_append(
            text=line.replace("\t", "    "),
            type=text_type,
        )


def shell_run(text):
    import subprocess
    val, output = subprocess.getstatusoutput(text)

    if not val:
        style = 'OUTPUT'
    else:
        style = 'ERROR'

    add_scrollback(output, style)


PROMPT = "$ "


def execute(context, _is_interactive):
    sc = context.space_data

    try:
        line = sc.history[-1].body
    except:
        return {'CANCELLED'}

    bpy.ops.console.scrollback_append(text=sc.prompt + line, type='INPUT')

    shell_run(line)

    # insert a new blank line
    bpy.ops.console.history_append(text="", current_character=0,
                                   remove_duplicates=True)

    sc.prompt = os.getcwd() + PROMPT
    return {'FINISHED'}


def autocomplete(context):
    import glob
    import os

    sc = context.space_data
    try:
        current_line = sc.history[-1]
    except Exception:
        return {'CANCELLED'}

    line = current_line.body
    before, sep, prefix = line.rpartition(" ")
    matches = glob.glob(prefix + "*")
    if not matches:
        return {'CANCELLED'}

    add_scrollback("  ".join(sorted(matches)), 'INFO')

    if len(matches) == 1:
        match = matches[0]
        if os.path.isdir(match):
            match += os.sep
        current_line.body = before + sep + match if sep else match
        current_line.current_character = len(current_line.body)

    return {'FINISHED'}


def banner(context):
    sc = context.space_data

    shell_run("bash --version")
    sc.prompt = os.getcwd() + PROMPT

    return {'FINISHED'}
