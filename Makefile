CC:=gcc
exe:=encodebej
source:=cJSON.c decodebej.c encodebej.c
options:=ENCODE_TEST

all:
	$(CC) $(source) -o $(exe)

encode:
	$(CC) $(source) -o $(exe) -D $(options) -g
