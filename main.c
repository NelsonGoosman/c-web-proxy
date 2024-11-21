#include <stdio.h>
#include "proxy.h"
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }
    printf("%s", user_agent_hdr);
    uint16_t port = (uint16_t)atoi(argv[1]);
    serve(port);
    return EXIT_SUCCESS;
}
