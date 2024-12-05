class MonitorCommand(gdb.Command):
    """Enables and disables a set of breakpoints"""

    def __init__(self, cmd, breakpoint_classes):
        super().__init__(
            cmd, gdb.COMMAND_USER
        )

        self.enabled = False
        self.breakpoints = []
        self.breakpoint_classes = breakpoint_classes

    def complete(self, text, word):
        # We expect the argument passed to be a symbol so fallback to the
        # internal tab-completion handler for symbols
        result = []
        if 'start'.startswith(word):
            result += ['start']
        if 'stop'.startswith(word):
            result += ['stop']
        return result

    def invoke(self, args, from_tty):
        # We can pass args here and use Python CLI utilities like argparse
        # to do argument parsing
        print(f"Args Passed: {args}")

        if args != 'start' and args != 'stop':
            print(f"Invalid arguments")
            return
        
        if args == 'start' and self.enabled:
            print('Already started')
            return
        
        if args == 'stop' and not self.enabled:
            print('Already stopped')
            return
        
        if args == 'start':
            self.breakpoints = [c() for c in self.breakpoint_classes]
        else:
            for b in self.breakpoints:
                b.delete()
            self.breakpoints = []

        
        self.enabled = not self.enabled

class BreakPoint_RF_hwiHw(gdb.Breakpoint):
    def __init__(self):
        super().__init__(spec='*RF_hwiHw', type=gdb.BP_HARDWARE_BREAKPOINT, internal=False, temporary=False)

    def stop(self):
        print('Detected RF_hwiHw call, continuing')
        return False

class BreakPoint_RF_hwiCpe0PowerFsm(gdb.Breakpoint):
    def __init__(self):
        super().__init__(spec='*RF_hwiCpe0PowerFsm', type=gdb.BP_HARDWARE_BREAKPOINT, internal=False, temporary=False)

    def stop(self):
        print('Detected RF_hwiCpe0PowerFsm call, continuing')
        return False

class BreakPoint_RF_hwiCpe0Active(gdb.Breakpoint):
    def __init__(self):
        super().__init__(spec='*RF_hwiCpe0Active', type=gdb.BP_HARDWARE_BREAKPOINT, internal=False, temporary=False)

    def stop(self):
        print('Detected RF_hwiCpe0Active call, continuing')
        return False

class WatchPoint_RF_CMDR(gdb.Breakpoint):
    def __init__(self):
        super().__init__(spec='*0x40041000', type=gdb.BP_WATCHPOINT, internal=False, temporary=False, wp_class=gdb.WP_WRITE)

    def stop(self):
        print('Detected new radio command, continuing')
        return False



class MonitorRadio(MonitorCommand):
    def __init__(self):
        super().__init__("monitor_radio", [
            BreakPoint_RF_hwiHw,
            BreakPoint_RF_hwiCpe0PowerFsm,
            BreakPoint_RF_hwiCpe0Active,
            WatchPoint_RF_CMDR
            ])

class BreakPoint_pm_state_set(gdb.Breakpoint):
    def __init__(self):
        super().__init__(spec='*pm_state_set', type=gdb.BP_HARDWARE_BREAKPOINT, internal=False, temporary=False)

    def stop(self):
        print('Detected pm_state_set call, continuing')
        return False

class MonitorPower(MonitorCommand):
    def __init__(self):
        super().__init__("monitor_power", [
            BreakPoint_pm_state_set
            ])

MonitorRadio()
MonitorPower()