all: sleep_server clean

sleep_server: main.cpp globals.cpp
	g++ -std=c++17 -o sleep_server main.cpp globals.cpp -lpthread

clean:
	rm -rf sleep_server
