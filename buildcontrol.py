#
# Flare Build Control
#

import os


def includes(bld, paths):
    if isinstance(paths, str):
        paths = paths.split(' ')
    return [str(bld.path.find_node(i)) for i in paths]


def recurse(ctx, directories):
    for d in directories:
        ctx.recurse(d)


def board_source(bld, board_sources, all=False):
    srcs = []
    if all:
        for b in board_sources:
            srcs += board_sources[b]
    else:
        if bld.env.FLARE_BOARD in board_sources:
            srcs += board_sources[bld.env.FLARE_BOARD]
    return list(set(srcs))


def format(bld, sources):
    if sources:

        def run_c(task):
            task.color = 'PINK'
            srcs = ' '.join([i.abspath() for i in task.inputs])
            cmd = task.env.C_FORMATTER[0] + ' -i ' + srcs
            return task.exec_command(cmd)

        def run_py(task):
            task.color = 'YELLOW'
            srcs = ' '.join([i.abspath() for i in task.inputs])
            cmd = task.env.PY_FORMATTER[0] + ' -i ' + srcs
            return task.exec_command(cmd)

        c_sources = [s for s in sources if str(s).endswith(('.c', '.h'))]
        if c_sources:
            bld(rule=run_c, source=c_sources, always=True, name='format c')

        py_sources = [
            s for s in sources if str(s).endswith(('.py', 'wscript'))
        ]
        if py_sources:
            bld(rule=run_py,
                source=py_sources,
                always=True,
                name='format python')


def options(opt):
    copts = opt.get_option_group('configure options')
    copts.add_option('--tools-path',
                     default=None,
                     dest='flare_tools_path',
                     help='Path to tools (installed prefix)')
    copts.add_option('--compiler-prefix',
                     default=None,
                     dest='flare_compiler_prefix',
                     help='Compiler prefix')
    copts.add_option('--board',
                     default=None,
                     dest='flare_board',
                     help='Compiler prefix')
    copts.add_option('--formatter',
                     default='clang-format',
                     dest='formatter',
                     help='Clang format command (default: %(default)s)')


def configure(conf):
    tools_prefix = conf.options.flare_compiler_prefix
    if tools_prefix == None:
        conf.fatal('No compiler prefix found')

    board = conf.options.flare_board
    if board == None:
        conf.fatal('No board specified')

    tool_path_list = []
    if conf.options.flare_tools_path == None:
        tool_path_list = os.environ['PATH']
    else:
        if os.path.exists(os.path.join(conf.options.flare_tools_path, 'bin')):
            tool_path_list = os.path.join(conf.options.flare_tools_path, 'bin')
        else:
            tool_path_list = conf.options.flare_tools_path

    conf.find_program(tools_prefix + 'gcc', path_list=tool_path_list, var="CC")
    conf.find_program(tools_prefix + 'g++',
                      path_list=tool_path_list,
                      var="CXX")
    conf.find_program(tools_prefix + 'gcc',
                      path_list=tool_path_list,
                      var="LINK_CC")
    conf.find_program(tools_prefix + 'g++',
                      path_list=tool_path_list,
                      var="LINK_CXX")
    conf.find_program(tools_prefix + 'gcc', path_list=tool_path_list, var="AS")
    conf.find_program(tools_prefix + 'ld', path_list=tool_path_list, var="LD")
    conf.find_program(tools_prefix + 'ar', path_list=tool_path_list, var="AR")

    conf.load('gcc')
    conf.load('g++')
    conf.load('gas')

    conf.env.FLARE_TOP_DIR = str(conf.path.find_node('.'))

    conf.env.DEFINES += ['FLARE=1', 'FLARE_DATASAFE_FORMAT=1']
    conf.env.INCLUDES += ['.'] + includes(conf, 'bootloader')
    conf.env.CFLAGS_NOWARNINGS = conf.env.CFLAGS + [
        '-ffreestanding', '-g', '-O2', '-fPIE'
    ]
    conf.env.CFLAGS_WARNINGS = ['-Wall', '-Wextra']
    conf.env.CFLAGS = conf.env.CFLAGS_NOWARNINGS + conf.env.CFLAGS_WARNINGS

    conf.find_program(conf.options.formatter,
                      var='C_FORMATTER',
                      mandatory=False)
    conf.find_program('yapf', var='PY_FORMATTER', mandatory=False)
