/* This is an example program that parses the options provided on the
 * command line that are needed for assignment 3. You may copy all or
 * parts of this code in the assignment */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

struct resolve_arguments {
    struct sockaddr_in bootstrap_address;
    char *domain;
};

error_t resolve_parser(int key, char *arg, struct argp_state *state) {
	struct resolve_arguments *args = state->input;
	error_t ret = 0;
	switch(key) {
	case 'a':
		/* Validate that port is correct and a number, etc!! */
		if (!inet_pton(AF_INET, arg, &args->bootstrap_address.sin_addr)) {
			argp_error(state, "Invalid bootstrap address argument");
        } else {
            args->bootstrap_address.sin_family = AF_INET;
            args->bootstrap_address.sin_port = htons(53);
        }
		break;
	case 'd':
        args->domain = strdup(arg);
		break;
    case ARGP_KEY_END:
        if (!args->bootstrap_address.sin_family) {
            argp_error(state, "No bootstrap address specified");
        } else if (!args->domain) {
            argp_error(state, "No domain specified");
        }
        break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

void resolve_parseopt(int argc, char *argv[]) {
	struct resolve_arguments args;

	/* bzero ensures that "default" parameters are all zeroed out */
	bzero(&args, sizeof(args));

	struct argp_option options[] = {
		{ "address", 'a', "address", 0, "Nameserver to contact" ,0},
		{ "domain", 'd', "domain", 0, "Domain to resolve", 0},
		{0}
	};
	struct argp argp_settings = { options, resolve_parser, 0, 0, 0, 0, 0 };

	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
		printf("Got an error condition when parsing\n");
	} else {
        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &args.bootstrap_address.sin_addr, addr, sizeof(struct sockaddr_in));
        printf("Got address %s:%d and domain %s\n", addr, ntohs(args.bootstrap_address.sin_port), args.domain);
    }

	free(args.domain);
}

int main(int argc, char *argv[]) {
    resolve_parseopt(argc, argv);
}
