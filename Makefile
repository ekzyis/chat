chat: src/client.cpp src/server.cpp
	mkdir -p out/
	g++ -o out/client src/client.cpp -pthread
	g++ -o out/server src/server.cpp
