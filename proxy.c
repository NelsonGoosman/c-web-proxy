#include "proxy.h"
#define MAX_IN_SERVER_QUEUE 10
#define MAX_BUFFER_SIZE 0xFFF
#define DEFAULT_PORT "80"

// Function to start the server and listen for incoming connections on the specified port
// It sets up the socket, binds it to the port, and listens for incoming connections
void serve(uint16_t port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port),
    };

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_IN_SERVER_QUEUE) < 0)
    {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);

    while (1)
    {
        // Accept an incoming connection from a client
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            // If accept fails, log the error and continue to accept other connections
            perror("accept failed");
            continue; // Log error but continue serving
        }
        handle_client(client_fd);
    }
    // Close the server socket when done
    close(server_fd);
}

// Function to handle communication with a connected client
// It reads the client's request, parses it, and forwards it to the appropriate server
void handle_client(int client_fd)
{
    buffer_t buffer;
    // Read the client's request into the buffer
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
    if (bytes_read < 0)
    {
        // If read fails, log the error and close the client connection
        perror("read failed");
        close(client_fd);
        return;
    }

    buffer[bytes_read] = '\0';

    // Parse the client's request to extract the domain and port
    domain_t domain;
    port_t port;
    if (parse_request(buffer, domain, port))
    {
        // If parsing is successful, forward the request to the remote server
        forward_request(client_fd, domain, port, buffer);
    }

    // Close the client connection after handling the request
    close(client_fd);
}

// Function to parse the client's HTTP request and extract the domain and port
// It checks if the request is a valid HTTP GET request and extracts the domain and port from the URL
int parse_request(buffer_t buffer, domain_t domain, port_t port)
{
    // Check if the request starts with "GET http://"
    if (strncmp(buffer, "GET http://", 11) != 0)
    {
        return 0;
    }

    // Extract the URL from the request
    char *url_start = buffer + 11;
    char *url_end = strchr(url_start, ' ');
    if (!url_end)
        return 0;

    *url_end = '\0';

    // Extract the path and port from the URL
    char *path_start = strchr(url_start, '/');
    if (path_start)
        *path_start = '\0';

    char *port_start = strchr(url_start, ':');
    if (port_start)
    {
        *port_start = '\0';
        strncpy(port, port_start + 1, sizeof(port_t) - 1);
    }
    else
    {
        // If no port is specified, use the default port (80)
        strncpy(port, DEFAULT_PORT, sizeof(port_t));
    }

    // Copy the extracted domain and port to the provided buffers
    strncpy(domain, url_start, sizeof(domain_t) - 1);
    return 1;
}

// Function to create a proxy request to be sent to the remote server
// It formats the client's request to be compatible with the remote server
int create_proxy_request(buffer_t buffer, const domain_t domain)
{
    char *path_start = strchr(buffer + 11, '/');
    if (!path_start)
        path_start = "/";

    // Format the proxy request with the appropriate headers
    return snprintf(buffer, MAX_BUFFER_SIZE,
                    "GET %s HTTP/1.0\r\n"
                    "Host: %s\r\n"
                    "User-Agent: Mozilla/5.0\r\n"
                    "Connection: close\r\n"
                    "Proxy-Connection: close\r\n\r\n",
                    path_start, domain);
}

// Function to forward the client's request to the remote server and relay the response back to the client
// It establishes a connection to the remote server, sends the client's request, and forwards the server's response back to the client
void forward_request(int client_fd, const domain_t domain, const port_t port, buffer_t buffer)
{
    // Resolve the domain name to an IP address
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM}, *res;
    if (getaddrinfo(domain, port, &hints, &res) != 0)
    {
        perror("getaddrinfo failed");
        return;
    }

    // Create a socket and connect to the remote server
    int server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (server_fd < 0 || connect(server_fd, res->ai_addr, res->ai_addrlen) < 0)
    {
        // If connection fails, log the error and return
        perror("connection to remote server failed");
        freeaddrinfo(res);
        return;
    }
    freeaddrinfo(res);

    // Create the proxy request and send it to the remote server
    int request_length = create_proxy_request(buffer, domain);
    if (write(server_fd, buffer, request_length) < 0)
    {
        // If sending fails, log the error and close the server socket
        perror("write to remote server failed");
        close(server_fd);
        return;
    }

    // Read the response from the remote server and forward it to the client
    ssize_t bytes;
    while ((bytes = read(server_fd, buffer, MAX_BUFFER_SIZE)) > 0)
    {
        if (write(client_fd, buffer, bytes) < 0)
        {
            // If writing to the client fails, log the error and break the loop
            perror("write to client failed");
            break;
        }
    }

    // Close the server socket after forwarding the response
    close(server_fd);
}
