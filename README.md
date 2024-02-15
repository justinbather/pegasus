A lightweight terminal-based text editor built in C with vim-motions in progress

Usage:
``` bash
git clone github.com/justinbather/pegasus.git
cd pegasus
make
./pegasus [filename]
```



Editor Commands:
Quit - CTRL+Q (In normal mode)
Save - CTRL+S (In normal mode)
  Upon successful save a message displayed at the bottom of the editor will indicate number of bytes written
  If the file has been modified since last save, the status bar will indicate [modified]

Motions:

User by default starts in normal mode. 

Normal Mode:
h - move left
l - move right
j - move down
k - move up
i - enter insert mode

Insert Mode: 
ESC - exit insert mode

