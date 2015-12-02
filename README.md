# psh
A shitty incomplete shell.

Please don't actually use this.

## Features
- Moving and editing the line
- Running commands with parameters
- History, up/down arrows to go back/forward
- Builtins: `cd`, `history`, `! n` (requires a space before the number)
- Background jobs (no actual job control though)
- Prompt shows cwd
- Pipes (limited to 16 commands)

## Incomplete/missing:
- `&` needs a space before it and has to be at the end of the line
- `<` only works with the first command of the line
- Line editing broken if line is too long (multiple lines)
- `!` is a dirty hack (well, like the whole program)
- Can't bring a job back to fg/bg after starting
- Probably implodes from any non-trivial errors
- Pretty much everything else
