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
A sketch of the wiring can be found [here](https://github.com/jalopezg-git/p18clock/blob/master/doc/hardware.txt).
For the LED dot matrix display, refer to the libledmtx [hardware description](https://github.com/jalopezg-git/libledmtx/#hardware).
