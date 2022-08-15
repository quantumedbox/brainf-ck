# Optimizations to make

## `+(+...)` : `-(-...)` : `<(<...)` : `>(>...)`
One of the most obvious way to optimize code, and one of the most used things in general
Instead of executing each instruction separately, you could just combine them

## `[-]` : `[+]`
This gadget does nothing, but zeroes the current cell.
It's also fully self contained, no outside branching could enter here

## `[->+>++<<]`
Linear loops that are self contained and operate on limited set of cells
