#ifndef NETWORK_H
#define NETWORK_H

// Create a new network namespace
int create_network_namespace(void);

// Enable loopback interface inside namespace
int setup_loopback(void);

#endif
