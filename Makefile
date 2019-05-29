
CPPFLAGS  = -std=c++14
	
# Portul pe care asculta serverul (de completat)
#PORT = 1500

# Adresa IP a serverului (de completat)
#IP_SERVER = 127.0.0.1

all: server subscriber

# Compileaza server.c
server: server.cpp

# Compileaza subscriber.c
subscriber: subscriber.cpp

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza subscriberul
run_subscriber:
	./subscriber ${ID} ${IP_SERVER} ${PORT}

clean:
	rm -f server subscriber
