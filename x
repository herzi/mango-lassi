#!/bin/sh

exec &> /dev/null

Xephyr -ac :1 &
disown

DISPLAY=:1 fluxbox &
disown

Xephyr -ac :2 &
disown

DISPLAY=:2 fluxbox &
disown

