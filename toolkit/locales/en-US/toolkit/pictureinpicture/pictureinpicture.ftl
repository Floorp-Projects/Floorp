# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

pictureinpicture-player-title = Picture-in-Picture

## Note that this uses .tooltip rather than the standard '.title'
## or '.tooltiptext' -  but it has the same effect. Code in the
## picture-in-picture window will read and copy this to an in-document
## DOM node that then shows the tooltip.
##
## Variables:
##   $shortcut (String) - Keyboard shortcut to execute the command.

pictureinpicture-pause-btn =
  .aria-label = Pause
  .tooltip = Pause (Spacebar)
pictureinpicture-play-btn =
  .aria-label = Play
  .tooltip = Play (Spacebar)

pictureinpicture-mute-btn =
  .aria-label = Mute
  .tooltip = Mute ({ $shortcut })
pictureinpicture-unmute-btn =
  .aria-label = Unmute
  .tooltip = Unmute ({ $shortcut })

pictureinpicture-unpip-btn =
  .aria-label = Send back to tab
  .tooltip = Back to tab

pictureinpicture-close-btn =
  .aria-label = Close
  .tooltip = Close ({ $shortcut })

pictureinpicture-subtitles-btn =
  .aria-label = Subtitles
  .tooltip = Subtitles

pictureinpicture-fullscreen-btn2 =
  .aria-label = Fullscreen
  .tooltip = Fullscreen (double-click or { $shortcut })

pictureinpicture-exit-fullscreen-btn2 =
  .aria-label = Exit fullscreen
  .tooltip = Exit fullscreen (double-click or { $shortcut })

##

# Keyboard shortcut to toggle fullscreen mode when Picture-in-Picture is open.
pictureinpicture-toggle-fullscreen-shortcut =
  .key = F

## Note that this uses .tooltip rather than the standard '.title'
## or '.tooltiptext' -  but it has the same effect. Code in the
## picture-in-picture window will read and copy this to an in-document
## DOM node that then shows the tooltip.

pictureinpicture-seekbackward-btn =
  .aria-label = Backward
  .tooltip = Backward (←)

pictureinpicture-seekforward-btn =
  .aria-label = Forward
  .tooltip = Forward (→)

##

# This string is never displayed on the window. Is intended to be announced by
# a screen reader whenever a user opens the subtitles settings panel
# after selecting the subtitles button.
pictureinpicture-subtitles-panel-accessible = Subtitles settings

pictureinpicture-subtitles-label = Subtitles

pictureinpicture-font-size-label = Font size

pictureinpicture-font-size-small = Small

pictureinpicture-font-size-medium = Medium

pictureinpicture-font-size-large = Large
