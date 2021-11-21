grapple
========
grapple is a graphical menu of plumbable lines.
When run, grapple captures all output from a pipe looking for plumbable text (pattern is: <path>(:<addr>)?). If the line matches the pattern it can then be sent to the plumber.
When an item is selected, it is sent to the plumber `edit` port. If the `-q` option is passed, grapple will exit once a line is plumbed.

Left-click an item to select it.  
Right-click to activate the selected item.

Keyboard shortcuts:
- Arrow up / down change selection
- Enter activate selection
- Page up / down scroll by one screen page
- Home go to first item in the list
- End go to last item in the list
- q or Del or Esc to exit grapple

Usage:
-------
Install with usual ``mk install``  
Run: ``... | grapple [-q]``

The provided `gg` (grapple g) and `gm` (grapple mk) shows how to use grapple. 

Bugs:
------
Indeed.
