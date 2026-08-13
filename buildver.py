import os
import shutil
import time

sources = ['bootloader/flare-build-id.c']

flare_build_ver_template = [
    '''/*
 * Copyright 2025 Contemporary Software
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

/*
 * Header file for generated file containing the git hash and unix time of
 * build as an ID.
 */

#include <flare-build-id.h>

const char* flare_build_id() {
    return "''', '''";
}

size_t flare_build_id_length() {
    return ''', ''';
}'''
]


def _set_build_id(bld):
    ''' Build id is `git_hash[-m]-hex_time` where
    `-m` means the repo is dirty'''
    import waflib
    bid = None
    try:
        cmd = 'git ls-files --modified'
        modified = bld.cmd_and_log(cmd, quiet=waflib.Context.BOTH).strip()
        cmd = 'git rev-parse --verify HEAD'
        out = bld.cmd_and_log(cmd, quiet=waflib.Context.BOTH).strip()
    except WafError:
        bid = 'no-repo'
    if out:
        bid = out[:8]
    elif bid is None:
        bid = 'no-repo'
    if modified:
        bid += '-m'
    bid += '-' + hex(int(time.time()))[2:]
    bld.env.BUILD_ID = bid


def configure(conf):
    pass


def build(bld):
    from waflib import Task

    def create_build_ver_c(task):
        output = str(task.outputs[0])
        with open(output, 'w') as bv:
            bv.write(flare_build_ver_template[0])
            bv.write(task.env.BUILD_ID)
            bv.write(flare_build_ver_template[1])
            bv.write(hex(len(task.env.BUILD_ID)))
            bv.write(flare_build_ver_template[2])

    class build_ver(Task.Task):
        color = 'CYAN'
        always_run = True
        run_str = [create_build_ver_c]

    _set_build_id(bld)

    outs = [bld.path.find_or_declare(file) for file in sources]

    build_ver_tsk = build_ver(env=bld.env)
    build_ver_tsk.set_outputs(outs)
    bld.add_to_group(build_ver_tsk)

    bld.objects(target='flare_version', features='c', source=sources)
