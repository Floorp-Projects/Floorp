#! /usr/bin/python

commandRegistry = {}

def registerSlaveCommand(name, factory, version):
    """
    Register a slave command with the registry, making it available in slaves.

    @type  name:    string
    @param name:    name under which the slave command will be registered; used
                    for L{buildbot.slave.bot.SlaveBuilder.remote_startCommand}
                    
    @type  factory: L{buildbot.slave.commands.Command}
    @type  version: string
    @param version: version string of the factory code
    """
    assert not commandRegistry.has_key(name)
    commandRegistry[name] = (factory, version)
