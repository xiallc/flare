#! /usr/bin/env python
# encoding: utf-8

APPNAME = 'flare'
VERSION = '0.1'

import buildcontrol
import buildver
import xilinx

from waflib import Context, Build, Errors, Logs, Scripting, Task, TaskGen, Utils

directories = ['bootloader']

sources = [
    'bootloader/fsbl-boot.c',
]


@TaskGen.feature('format')
class formatter(Build.BuildContext):
    '''format thesources'''
    cmd = 'format'
    fun = 'format'


def init(ctx):
    buildcontrol.recurse(ctx, directories)


def options(opt):
    opt.add_option_group('configure options')
    buildcontrol.recurse(opt, directories)
    buildcontrol.options(opt)
    xilinx.options(opt)


def configure(conf):
    buildcontrol.recurse(conf, directories)
    buildcontrol.configure(conf)
    buildver.configure(conf)
    xilinx.configure(conf)
    if len(conf.env.FLARE_BOARD) == 0:
        conf.fatal('board not found: ' + conf.options.flare_board)
    conf.msg('Board', conf.env.FLARE_BOARD)


def build(bld):
    xilinx.build(bld)
    buildver.build(bld)
    buildcontrol.recurse(bld, directories)
    bld.program(target='flare_fsbl',
                features='c cprogram',
                source=sources,
                use=['flare', 'flare_drivers', 'flare_version'],
                install_path='${PREFIX}/share/flare/${FLARE_BOARD}')


def format(bld):
    if 'C_FORMATTER' not in bld.env and 'PY_FORMATTER' not in bld.env:
        bld.fatal(
            'no formatters found; reconfigure with --formatter for C or install one'
        )
    buildcontrol.recurse(bld, directories)
    buildcontrol.format(bld, bld.path.ant_glob('*.py', dir=False, src=True))
    buildcontrol.format(bld,
                        bld.path.ant_glob('**/wscript', dir=False, src=True))
