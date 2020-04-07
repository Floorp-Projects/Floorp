# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import


def frametree(debugger, command, result, dict):
    """Dumps the frame tree containing the given nsIFrame*."""
    debugger.HandleCommand('expr (' + command + ')->DumpFrameTree()')


def frametree_pixels(debugger, command, result, dict):
    """Dumps the frame tree containing the given nsIFrame* in CSS pixels."""
    debugger.HandleCommand('expr (' + command + ')->DumpFrameTreeInCSSPixels()')


def frametreelimited(debugger, command, result, dict):
    """Dumps the subtree of a frame tree rooted at the given nsIFrame*."""
    debugger.HandleCommand('expr (' + command + ')->DumpFrameTreeLimited()')


def frametreelimited_pixels(debugger, command, result, dict):
    """Dumps the subtree of a frame tree rooted at the given nsIFrame*
    in CSS pixels."""
    debugger.HandleCommand('expr (' + command + ')->DumpFrameTreeLimitedInCSSPixels()')


def pstate(debugger, command, result, dict):
    """Displays a frame's state bits symbolically."""
    debugger.HandleCommand('expr mozilla::PrintFrameState(' + command + ')')


def init(debugger):
    debugger.HandleCommand('command script add -f lldbutils.layout.frametree frametree')
    debugger.HandleCommand(
        'command script add -f lldbutils.layout.frametree_pixels frametree_pixels')
    debugger.HandleCommand(
        "command script add -f lldbutils.layout.frametreelimited frametreelimited"
    )
    debugger.HandleCommand(
        "command script add -f lldbutils.layout.frametreelimited_pixels frametreelimited_pixels"
    )
    debugger.HandleCommand('command alias ft frametree')
    debugger.HandleCommand('command alias ftp frametree_pixels')
    debugger.HandleCommand('command alias ftl frametreelimited')
    debugger.HandleCommand('command alias ftlp frametreelimited_pixels')
    debugger.HandleCommand('command script add -f lldbutils.layout.pstate pstate')
