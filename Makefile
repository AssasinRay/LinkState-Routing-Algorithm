#all: vec_router ls_router manager_send
all:  ls_router manager_send
# vec_router: main.cpp monitor_neighbors.cpp
# 	g++ -pthread -std=c++11 -o vec_router main.cpp monitor_neighbors.cpp

ls_router: main.cpp monitor_neighbors.cpp LinkState.cpp LinkState.h
	g++ -pthread -g -std=c++11 -o ls_router main.cpp monitor_neighbors.cpp LinkState.cpp 

manager_send: manager_send.cpp
	g++ -pthread -g -std=c++11  -o manager_send manager_send.cpp

.PHONY: clean
clean:
	rm *.o  ls_router manager_send
