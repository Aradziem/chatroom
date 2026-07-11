This is a simple chatroom.  
You can build it using `make`.  
At startup, it reads commands from `~/.chatroomrc` and executes each nonempty line that doesn't start with a `'#'` as a command. Then, parses the arguments, and executes each `<command>` found in `-C '<command>'` arguments.

# AUTHORS
- hhsNel
- Aradziem - supposed to be main author but is actually PR merger.

# Commands

There are currently 3 commands:
1. `set` (alias: `se`)
    - `set nick <username>` sets the username
    - `set ip <ip>` sets the server IP address. Cannot be used at runtime
    - `set port <port>` sets the server port. Cannot be used at runtime
    - `set fetch_timeout <ms>` sets the refresh message interval in milliseconds. Cannot be used at runtime
2. `highlight` (alias: `hi`)  
    A highlight command has two sections: the highlight group and the color/styles.
    - `highlight msg_id <display>` sets the rendering for the message ID, printed before every message
    - `highlight msg_time <display>` sets the rendering for the message send time, printed before every message
    - `highlight msg_time_ms <display>` sets the rendering for the millisecond part of the message send time, printed before every message
    - `highlight msg_author <display>` sets the rendering for the message author, printed before every message
    - `highlight msg_text <display>` sets the rendering for the actual message
    - `highlight command <display>` sets the rendering for the command mode, and status when the command was successful
    - `highlight command_failure <display>` sets the rendering for the status when a command was unsuccessful  
    the `<display>` part in each of those commands consists of zero or more space-separated parts. Only the last color/style combination takes effect, the previous ones are ignored.
    - `default` sets the color to the terminals default
    - `c256=<id>` sets the color to the `<id>`th color (usually colors 0-15 are customizable in a terminal)
    - `rgb=<r>,<g>,<b>` sets the color to `rgb(<r>, <g>, <b>)`
    - `style=<flags>` sets style to a combination of flags. Each character in `<flags>` is interpreted as a singular style:
        - `b` is "bold"
        - `u` is "underline"
        - `h` is "hidden" (turns off rendering)
        - `H` is "explicitly not hidden" (turns rendering back on)  
    for example, `style=b` sets the rendering to "bold, not underlined, not hidden"
3. `source` (alias: `so`)
    - `source <file>` reads a file, then executes all nonempty, not beginning with `'#'` lines as commands. At program startup, `source ~/.chatroomrc` is automatically executed

