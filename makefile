CC=g++
CFLAGS=-std=c++14
DEPS = maincleaned.h
MAIN = speedtrack
LIBS = -l sqlite3
SRCS = maincleaned.cpp

default: speedtracker

speedtracker:
		${CC} ${CFLAGS} ${SRCS} -o ${MAIN} ${LIBS}


#g++ -std=c++14 main.cpp -o speedt -l sqlite3
