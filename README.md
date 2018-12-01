# IRC chat client & server

To run example:

```console
ekzyis@ekzyis-MINT:~/Programming/chat$ gcc example/server.cpp -o out/server
ekzyis@ekzyis-MINT:~/Programming/chat$ gcc example/client.cpp -o out/client
ekzyis@ekzyis-MINT:~/Programming/chat$ ./out/server


```
Open new console since server is now waiting for client to connect:
```console
ekzyis@ekzyis-MINT:~/Programming/chat$ ./out/client
Hello message sent
Hello from server
```
Output of server should be:
```console
Hello from client
Hello message sent
```