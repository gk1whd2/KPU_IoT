#!/usr/bin/python
import sys
import RPi.GPIO as g

g.setmode(g.BCM)
g.setup(14, g.OUT)
p = g.PWM(14, 0.5)

def resetLines():
	global startLine, listenLine, endLine
	startLine = 0
	listenLine = 0
	endLine = 0

listen = False
resetLines()

while 1:
	line = sys.stdin.readline()
	line = line.split('\r')[0]
	print(line)
	if "###########################" in line:
		if startLine == 1:
			if listenLine == 1:
				endLine = 1
			else:
				resetLines()
		else:
			if listenLine == 0 and endLine == 0:
				startLine = 1
	elif startLine == 1: 
		if "Listening" in line:
			listenLine = 1
		else:
			if listen and line == '':
				continue
			else:
				resetLines()

	if startLine == 1 and listenLine == 1 and endLine == 1:
		listen = True
		p.start(0.1)
	else:
		listen = False
		p.stop()
