# p18clock: a libledmtx-based alarm clock
NOTE: this is a legacy repository. Sources may be outdated and/or not properly documented.

## Compiling
As a requirement, you should install libledmtx in the `libledmtx/` directory prior to building p18clock. 
Change to the directory where you extracted p18clock and type `$ make LANG=xxx`,
substituting xxx by one of: `EN`, `ES`.  LANG defaults to EN if not defined.

## State machine inputs
- PUSHB1	change display mode
- PUSHB2	enter/exit STATE_*_SET* states
- PUSHB3	increment value
- PUSHB4	decrement value

## State machine states
- STATE_TIME		display time
- STATE_DATE		display date
- STATE_TEMP		display temperature
- STATE_ALARM		display alarm time
- STATE_AUTO		auto change display mode
- STATE_TIME_SETHOUR	set time: hour
- STATE_TIME_SETMIN	set time: min
- STATE_DATE_SETMDAY	set date: mday
- STATE_DATE_SETMON	set date: mon
- STATE_DATE_SETYEAR	set date: year
- STATE_TEMP_SETVDD	manually set vdd if hlvd fails
- STATE_TEMP_SETOFF	set temperature offset
- STATE_ALARM_SETHOUR	set alarm: hour
- STATE_ALARM_SETMIN	set alarm: min

## p18clock circuit
```
                                                    +--+
                                                    |  |
                                                    | ___ 4.7uf
       VDD      VDD                                 | ___                   VDD
        |        |                                  |  |                     |
      _____      |                                  |  |  ICSP               |
1N4148  ^        _                               o  o  o  o  o               +---------------------------+--+--+--\
      /___\     | | 10k               __________/ VDD VSS | /                |                           |  |  |  |
        |       |_|                  /                    | |                |  +--------+               _  _  _  _
        |        |         ____      |  +--------------+  | |                +--|4   7VCC|-- VDD        | || || || |             __|__
        +--------+--------|____|-----+--|MCLR       RB7|--/ |                +--|5   4 EO|              |_||_||_||_| 22k /-------o   o----\
                 |          1k8         |RA0        RB6|----/                +--|6   H GS|-- RB0/INT0    |  |  |  |     /        __|__    |
                 |                   |--|RA1   p    RB5|                     \--|7   C  3|---------------+--|--|--|----/    /----o   o----+
           100nf___                  |--|RA2   1    RB4|                  VSS --|EI  1  2|------------------+  |--|--------/              |
                ___                  |--|RA3   8    RB3|------------------------|A2  4  1|---------------------+--|--------\     __|__    |
                 |                   |--|RA4   f    RB2|------------------------|A1  8  0|------------------------+----\    \----o   o----+
                 |      to r393c164  |--|RA5   2    RB1|--------------\   VSS --|GND   A0|---                           \        __|__    |
                VSS     hardware ---/   |VSS   5    RB0|               \        +--------+  /                            \-------o   o----+
                                        |OSC1  5    VDD|--------+-- VDD \                  /                                              |
                                        |OSC2  0    VSS|--\     |        \----------------/                                               |
                                        |T1OSO      RC7|  | | | |                                                                        VSS
                         BUZZER  +      |T1OSI      RC6|  +-| |-/
                      +--------O--------|CCP1       RC5|  | | |
                      |                 |VUSB       RC4|  |100nf
                      |                 +--------------+  |
                     VSS                                 VSS


      VDD
       |
       |
  +--------+     ____
  |  LM35  |----|____|---- RA0/AN0
  +--------+      2k
       |
       |
      VSS


   22pf| |
  +----| |----+-------- OSC1
  |    | |    |
  |         _____
  |         ===== XTAL 8mhz
  |         -----
  |22pf| |    |
  +----| |----+-------- OSC2
  |    | |
  |
 VSS


   12pf| |
  +----| |----+-------- T1OSO
  |    | |    |
  |         _____
  |         ===== XTAL 32.768khz
  |         -----
  |12pf| |    |
  +----| |----+-------- T1OSI
  |    | |
  |
 VSS
```