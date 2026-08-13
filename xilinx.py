import os
import shutil

import buildcontrol

boards = ['versal', 'zynqmp', 'zynq7000']

config = {
    'arch': {
        'versal': 'aarch64',
        'zynqmp': 'aarch64',
        'zynq7000': 'arm'
    },
    'defines': {
        'versal': ['FLARE_VERSAL'],
        'zynqmp': ['FLARE_ZYNQMP'],
        'zynq7000': ['FLARE_ZYNQ7000']
    },
    'includes': {
        'versal': [],
        'zynqmp': [],
        'zynq7000': ['bootloader/board/zynq7000/']
    },
    'cflags': {
        'versal': [],
        'zynqmp': [],
        'zynq7000': []
    },
    'mflags': {
        'versal': [],
        'zynqmp': [
            '-mcpu=cortex-a53', '-mfix-cortex-a53-835769',
            '-mfix-cortex-a53-843419', '-mlittle-endian', '-DEL2=1',
            '-DEL1_NONSECURE=0'
            '-mabi=lp64'
        ],
        'zynq7000': [
            '-march=armv7-a',
            '-mthumb',
            '-mfpu=neon',
            '-mfloat-abi=hard',
            '-mtune=cortex-a9',
            '-mlittle-endian',
        ]
    },
    'linkflags': {
        'versal': [],
        'zynqmp': ['-T../bootloader/board/zynqmp/zynqmp-lscript.ld'],
        'zynq7000': ['-T../bootloader/board/zynq7000/zynq7000-lscript.ld']
    },
    'sources': {
        'versal': [],
        'zynqmp': [
            'xilinx/psu_init.c',
        ],
        'zynq7000': [
            'xilinx/ps7_init.c',
        ]
    }
}


def options(opt):
    copts = opt.get_option_group('configure options')
    copts.add_option('--xsa',
                     default=None,
                     dest='flare_xsa',
                     help='Path to XSA')
    copts.add_option('--ps-init',
                     default=None,
                     dest='flare_ps_init',
                     help='Path to PS initialisation file')


def configure(conf):
    board = conf.options.flare_board
    if board in boards:
        if conf.options.flare_xsa and conf.options.flare_ps_init:
            conf.fatal("Vivado XSA OR ps7_init is required for this board")
        if conf.options.flare_xsa:
            conf.env.FLARE_XSA = conf.options.flare_xsa
            conf.msg('Vivado XSA', conf.env.FLARE_XSA)
        elif conf.options.flare_ps_init:
            conf.env.FLARE_PS_INIT = conf.options.flare_ps_init
            conf.msg('Vivado PS Init', conf.env.FLARE_PS_INIT_SRC)
        else:
            conf.fatal("Vivado XSA OR ps7_init is required for this board")
        conf.env.FLARE_BOARD = board
        conf.env.FLARE_ARCH = config['arch'][board]
        conf.env.DEFINES += config['defines'][board]
        conf.env.INCLUDES += buildcontrol.includes(conf,
                                                   config['includes'][board])
        conf.env.ASFLAGS += config['mflags'][board]
        conf.env.CFLAGS += config['cflags'][board] + config['mflags'][board]
        conf.env.LINKFLAGS += config['linkflags'][board] + config['mflags'][
            board]
        conf.env.FLARE_USE = ['flare_xilinx']


def build(bld):
    board = bld.env.FLARE_BOARD
    if board in boards:
        from waflib import Task

        def unzip_xsa(task):
            board = task.env.FLARE_BOARD
            path = task.env.FLARE_XSA
            if board == 'zynq7000':
                if path:
                    if not os.path.exists(path):
                        task.fatal('XSA not found: {}'.format(path))
                    shutil.unpack_archive(path, 'build/xsa', 'zip')
                    task.env.FLARE_PS_INIT_SRC = 'build/xsa/ps7_init.c'
                else:
                    task.env.FLARE_PS_INIT_SRC = task.env.FLARE_PS_INIT
            elif board == 'zynqmp':
                if not os.path.exists(path):
                    task.fatal('XSA not found: {}'.format(path))
                shutil.unpack_archive(path, 'build/xsa', 'zip')
                task.env.FLARE_PS_INIT_SRC = 'build/xsa/psu_init.c'
                task.env.FLARE_PS_INIT_HEADER = 'build/xsa/psu_init.h'

        def copy_ps_init(task):
            board = task.env.FLARE_BOARD
            path = task.env.FLARE_PS_INIT_SRC
            if not os.path.exists('build/xilinx'):
                os.mkdir('build/xilinx')
            src = task.outputs[0]
            shutil.copyfile(path, src.abspath())
            path = task.env.FLARE_PS_INIT_HEADER
            if path:
                hdr = src.change_ext('.h')
                shutil.copyfile(path, hdr.abspath())

        class xilinx_init(Task.Task):
            color = 'CYAN'
            always_run = True
            run_str = (unzip_xsa, copy_ps_init)

        sources = config['sources'][board]
        outs = [bld.path.find_or_declare(file) for file in sources]

        xilinx_init_tsk = xilinx_init(env=bld.env)
        xilinx_init_tsk.set_outputs(outs)
        bld.add_to_group(xilinx_init_tsk)

        bld.objects(target='flare_xilinx',
                    features='c',
                    source=sources,
                    cflags=['-w'])
