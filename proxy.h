#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

// Define buffer_t
typedef char buffer_t[0xFFF];
typedef char domain_t[0xFF];
typedef char port_t[6];




void serve(uint16_t port);
void handle_client(int client_fd);
int parse_request(buffer_t buffer, domain_t domain, port_t port);
int create_proxy_request(buffer_t buffer, const domain_t domain);
void forward_request(int client_fd, const domain_t domain, const port_t port, buffer_t buffer);