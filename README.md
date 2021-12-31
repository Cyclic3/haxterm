# haxterm
_terminal emulator for designing and testing exploits_

## Usage:
`haxterm ((tcp <host> <port> | local <path>)`

Press `ESC` to enter the both escaped mode
| Key | Input escaped | Output escaped | Both escaped |
|-|-|-|-|
| i | No effect | Switches to input escaped| Switches to input escaped |
| o | No effect |  Switches to output escaped | Switches to output escaped |
| x | hexadecimal input | hexadecimal output | |
| a | assembly file input | | |
| s | standard input | standard output |  |
| f | file input | | flushes the buffer without newline |
| `ESC` | Unescapes | Unescapes | Unescapes |


## Why?
Actually performing binary exploitation generally requires an idiotic cat command like `cat - recon - stager payload -`, or a hacked together helper script that does this stuff for you, like my terrible [helper.py](https://github.com/Cyclic3/smashing.js/blob/master/helper.py). You also will need subtly different interactions for remote and local programs, such as making sure the TCP stream flushes for the geniuses who treat TCP as a message protocol.
