class EmulatorBattery(object):

    def __init__(self, emulator):
        self.emulator = emulator

    def get_state(self):
        status = {}
        state = {}

        response = self.emulator._run_telnet('power display')
        for line in response:
            if ':' in line:
                field, value = line.split(':')
                value = value.strip()
                if value == 'true':
                    value = True
                elif value == 'false':
                    value = False
                elif field == 'capacity':
                    value = float(value)
                status[field] = value

        state['level'] = status.get('capacity', 0.0) / 100
        if status.get('AC') == 'online':
            state['charging'] = True
        else:
            state['charging'] = False

        return state

    def get_charging(self):
        return self.get_state()['charging']

    def get_level(self):
        return self.get_state()['level']

    def set_level(self, level):
        self.emulator._run_telnet('power capacity %d' % (level * 100))

    def set_charging(self, charging):
        if charging:
            cmd = 'power ac on'
        else:
            cmd = 'power ac off'
        self.emulator._run_telnet(cmd)

    charging = property(get_charging, set_charging)
    level = property(get_level, set_level)

