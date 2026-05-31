# Copyright (c) 2017 Linaro Limited.
#
# SPDX-License-Identifier: Apache-2.0
#
# pylint: disable=duplicate-code

'''Runner for 1KT heni.'''

import re
import socket
import subprocess
import time
import shlex
import shutil

from os import path
from pathlib import Path
from zephyr_ext_common import ZEPHYR_BASE

from runners.core import ZephyrBinaryRunner, RunnerCaps

class HeniRunner(ZephyrBinaryRunner):
    '''Runner front-end for 1KT Heni tool.'''

    def __init__(self, cfg,
                 tui=None, config=None, heni_cmd=None, dev_id=None,
                 gdb_init=None):
        super().__init__(cfg)

        if not path.exists(cfg.board_dir):
            # try to find the board support in-tree
            cfg_board_path = path.normpath(cfg.board_dir)
            _temp_path = cfg_board_path.split("boards/")[1]
            support = path.join(ZEPHYR_BASE, "boards", _temp_path, 'support')
        else:
            support = path.join(cfg.board_dir, 'support')


        if not config:
            default = path.join(support, 'openocd.cfg')
            if path.exists(default):
                config = default
        self.openocd_config = config

        def verify_cmd(args):
            if not args:
                return False
            try:
                return self.call(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0
            except FileNotFoundError:
                return False

        heni_cmd = shlex.split(heni_cmd) if heni_cmd else None
        heni_docker_cmd = self.get_docker_heni_cmd()
        candidates = [heni_cmd, ['heni'], heni_docker_cmd]
        self.heni_cmd = next(filter(verify_cmd, candidates))
        if self.heni_cmd is None:
            raise RuntimeError(f'Viable heni command not found')

        if not dev_id:
            raise RuntimeError('dev_id is required for Heni runner')
        self.dev_id = dev_id

        # openocd doesn't cope with Windows path names, so convert
        # them to POSIX style just to be sure.
        self.elf_name = Path(cfg.elf_file).as_posix() if cfg.elf_file else None
        self.gdb_cmd = [cfg.gdb] if cfg.gdb else None
        self.tui_arg = ['-tui'] if tui else []
        self.gdb_init = gdb_init

    @classmethod
    def name(cls):
        return 'heni'

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={'flash', 'debug', 'debugserver', 'attach'}, dev_id=True)

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument('--heni', help="Custom heni command")
        parser.add_argument('--config', help='''if given, override default config file;
                            may be given multiple times''')

        # Options for debugging:
        parser.add_argument('--tui', default=False, action='store_true',
                            help='if given, GDB uses -tui')
        parser.add_argument('--gdb-init', action='append',
                            help='if given, add GDB init commands')

    @classmethod
    def do_create(cls, cfg, args):
        return HeniRunner(cfg,
            tui=args.tui, config=args.config, heni_cmd=args.heni, dev_id=args.dev_id,
            gdb_init=args.gdb_init)
    
    def get_docker_heni_cmd(self):
        return ['docker', 'run',
            '--privileged',
            '--network', 'host',
            '--rm',
            '-it',
            '--user=1000',
            '-w=/home/lorak/studia/magisterka/zephyr/zephyr',
            '-v', '/home/lorak/.mim-dsg/heni:/home/heni/.heni',
            '-v', '/home/lorak/studia/magisterka/zephyr/zephyr:/home/lorak/studia/magisterka/zephyr/zephyr',
            '--mount', 'type=bind,source=/dev,target=/dev', 
            'ghcr.io/mimuw-distributed-systems-group/heni_client:heni',
            'heni']

    def do_run(self, command, **kwargs):
        if command == 'flash':
            self.do_flash(**kwargs)
        elif command in ('attach', 'debug'):
            self.do_attach_debug(command, **kwargs)
        elif command == 'debugserver':
            self.do_debugserver(**kwargs)
        else:
            raise RuntimeError('Unsupported command')

    def do_flash(self, **kwargs):
        self.ensure_output('bin')
        bin_name = self.cfg.bin_file + '.flash'
        shutil.copyfile(self.cfg.bin_file, bin_name)

        self.logger.info(f'Flashing file: {bin_name}')

        cmd = (self.heni_cmd + ['node', 'prog', 'cherry', '-d', f'dev:{self.dev_id}', bin_name])
        self.check_call(cmd)
    
    def push_openocd_config(self):
        self.check_call(self.heni_cmd + [
            'supervisor',
            'scp',
            '-d', f'dev:{self.dev_id}',
            self.openocd_config,
            '/usr/share/openocd/scripts/board/cherrymote.cfg'])
    
    def get_device_ip(self):
        stdout = self.check_output(self.heni_cmd + ['supervisor', 'effectiveip', self.dev_id, '--raw']).decode()
        ip = stdout.strip()
        self.logger.info(f"Obtained IP of the device: {ip}")
        return ip

    def do_attach_debug(self, command, **kwargs):
        if self.gdb_cmd is None:
            raise ValueError('Cannot debug; no gdb specified')
        if self.elf_name is None:
            raise ValueError('Cannot debug; no .elf specified')
        
        self.push_openocd_config()

        device_ip = self.get_device_ip()

        server_cmd = (self.heni_cmd + ['supervisor', 'ssh', self.dev_id, '--', '-t', '--'] +
                      ['openocd', '-f', '/usr/share/openocd/scripts/board/cherrymote.cfg'])
        gdb_cmd = (self.gdb_cmd + self.tui_arg +
                   ['-ex', f'target extended-remote {device_ip}:3333',
                    self.elf_name])
        if self.gdb_init is not None:
            for i in self.gdb_init:
                gdb_cmd.append("-ex")
                gdb_cmd.append(i)

        self.require(gdb_cmd[0])
        self.run_server_and_client(server_cmd, gdb_cmd)

    def do_debugserver(self, **kwargs):
        self.push_openocd_config()
        server_cmd = (self.heni_cmd + ['supervisor', 'ssh', self.dev_id, '--', '-t', '--'] +
                      ['openocd', '-f', '/usr/share/openocd/scripts/board/cherrymote.cfg'])
        self.check_call(server_cmd)
