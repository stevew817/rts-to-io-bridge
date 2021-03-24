# RTS-to-IO Bridge
Bridge to control a Somfy io blind using an RTS transmitter

# Overview
This project aims to 'bridge' commands from the Somfy RTS protocol to the io protocol. Since
the io protocol is very much closed-off, the aim is to implement an RTS receiver based on
available information about the protocol, and connect the receiver GPIO to a modified io
remote control.

## Motivation
I have a bunch of blinds, and all but one of them are RTS. There's only one on io. Since I
wanted to integrate the blinds with Home-Assistant, and didn't fancy buying a 250 euro Somfy
bridge just to be able to control that last blind, the bridge idea was born.

Integrating RTS blinds with Home-Assistant can be done for around 50 euros (see rflink or
rfxtrx integrations).

## Implementation
Firmware for EFR32FG14 (supports 433MHz band and can be tuned to the 433.42 frequency for RTS)
is compiled by this project.

The RTS protocol consists of sending information about button presses. This means mapping onto
an actual remote is super-easy: just toggle the GPIO corresponding to the RTS-requested button
which will 'press' the button on the io remote.

Button presses recognized:
* Up
* Down
* 'My'

## Learning mode
To teach the bridge which remote control to listen to, put it in learning mode by pressing
BTN0. It will then latch on to whichever RTS remote's PROG button is pressed first. If that
remote was an already-attached remote, it will be removed from the bridge.

The bridge can bind to 4 RTS remotes.

# Status
Work-in-progress, not nearly ready yet.

# File tree
The root of this repository is a Simplicity Studio v5 project, and can be imported as such.
