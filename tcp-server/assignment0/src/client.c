#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <argp.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <time.h>

// function prototypes
int populate_buffer(int fd, char *buff, int buff_size);

struct client_arguments
{
   char ip_address[16]; /* You can store this as a string, but I probably wouldn't */
   int port;            /* is there already a structure you can store the address
	           * and port in instead of like this? */
   int hashnum;
   int smin;
   int smax;
   char *filename; /* you can store this as a string, but I probably wouldn't */
};

error_t client_parser(int key, char *arg, struct argp_state *state)
{
   struct client_arguments *args = state->input;
   error_t ret = 0;
   int len;
   switch (key)
   {
   case 'a':
      /* validate that address parameter makes sense */
      strncpy(args->ip_address, arg, 16);
      if (0 /* ip address is goofytown */)
      {
         argp_error(state, "Invalid address");
      }
      break;
   case 'p':
      /* Validate that port is correct and a number, etc!! */
      args->port = atoi(arg);
      if (0 /* port is invalid */){
         argp_error(state, "Invalid option for a port, must be a number");
      }
      break;
   case 'n':
      /* validate argument makes sense */
      args->hashnum = atoi(arg);
      break;
   case 300:
      /* validate arg */
      args->smin = atoi(arg);
      break;
   case 301:
      /* validate arg */
      args->smax = atoi(arg);
      break;
   case 'f':
      /* validate file */
      len = strlen(arg);
      args->filename = malloc(len + 1);
      strcpy(args->filename, arg);
      break;
   default:
      ret = ARGP_ERR_UNKNOWN;
      break;
   }
   return ret;
}
int populate_buffer(int fd, char buff[], int buff_size){
   
   ssize_t bytes_read =  read(fd, buff, buff_size);
   buff[bytes_read] = '\0';
   if (bytes_read < buff_size){
      return -1;
   }
   return 1;
}

int main(int argc, char * argv[]){
   struct argp_option options[] = {
       {"addr", 'a', "addr", 0, "The IP address the server is listening at", 0},
       {"port", 'p', "port", 0, "The port that is being used at the server", 0},
       {"hashreq", 'n', "hashreq", 0, "The number of hash requests to send to the server", 0},
       {"smin", 300, "minsize", 0, "The minimum size for the data payload in each hash request", 0},
       {"smax", 301, "maxsize", 0, "The maximum size for the data payload in each hash request", 0},
       {"file", 'f', "file", 0, "The file that the client reads data from for all hash requests", 0},
       {0}};

   struct argp argp_settings = {options, client_parser, 0, 0, 0, 0, 0};

   struct client_arguments args;
   bzero(&args, sizeof(args));

   if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0)
   {
      printf("Got error in parse\n");
      return EXIT_FAILURE;
   }

   /* If they don't pass in all required settings, you should detect
	 * this and return a non-zero value from main */
   // printf("Got %s on port %d with n=%d smin=%d smax=%d filename=%s\n",
   //        args.ip_address, args.port, args.hashnum, args.smin, args.smax, args.filename);


   uint16_t init_type = htons(1);
   uint32_t N = htonl(args.hashnum);
   //create a socket using TCP
   int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
   if (sock < 0){
      printf("Error creating socket\n");
      return EXIT_FAILURE;
   }
   // printf("debug1\n");

   //server address structure
   struct sockaddr_in servAddr;
   memset(&servAddr, 0, sizeof(servAddr)); 
   servAddr.sin_family = AF_INET; //IPV4 address, not 6

   //converts ip_address from string into network format
   int check = inet_pton(AF_INET, args.ip_address, &servAddr.sin_addr.s_addr);
   if (check == 0){
      printf("Error: invalid address\n");
      return EXIT_FAILURE;
   }else if(check < 0){
      printf("inet_pton() failed\n");
      return EXIT_FAILURE;
   }
   servAddr.sin_port = htons(args.port); //change port number to network byte order
   // printf("debug2\n");

   //connect to the server
   if (connect(sock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0){
      perror("connection failed");
      return EXIT_FAILURE;
   }

   //sending initialization msg to server
   ssize_t type_bytes = send(sock, &init_type, 2, 0);
   if (type_bytes < 0 || type_bytes != 2){
      printf("init: sending type failed\n");
      return EXIT_FAILURE;
   }
   ssize_t N_bytes = send(sock, &N, 4, 0);
   if (N_bytes < 0 || N_bytes != 4)
   {
      printf("init: sending num of hash requests failed\n");
      return EXIT_FAILURE;
   }

   ssize_t num_bytes;
   ssize_t tot_bytes_rec = 0;
   uint16_t ack_type;
   uint32_t len_total_responses;
   len_total_responses = htonl(len_total_responses);
   num_bytes = recv(sock, &ack_type, 2, 0);

   tot_bytes_rec += num_bytes;
   num_bytes = recv(sock, &len_total_responses, 4, 0);
   tot_bytes_rec += num_bytes;
   if (tot_bytes_rec != 6){
      printf("error: acknowledgment recieved incorrectly");
   }
   len_total_responses = ntohl(len_total_responses);
   // printf("num_responses: %d\n", len_total_responses);
   //opening file and determining size of each hash request
   int fd = open(args.filename, O_RDONLY);
   struct stat file_stats;
   if (fstat(fd, &file_stats) < 0)
      return 1;
   // printf("size of file: %ld\n", file_stats.st_size);

   srand(time(NULL));
   ssize_t bytes_sent = 0;
   uint32_t buf_size = args.smax;
   uint16_t type_num = htons(3);
   char buf[args.smax];
   // printf("hashNum: %d\n", args.hashnum);
   for (int i = 0; i < args.hashnum; i++)
   {
      //sending hashrequest
      if(args.smin == args.smax)
         buf_size = args.smin;
      else
         buf_size = args.smin + (rand() % (args.smax - args.smin));
      
      populate_buffer(fd, buf, buf_size);
      // printf("buf size: %d\n", buf_size);
      // printf("buf: %s\n", buf);
      
      buf_size = htonl(buf_size);
      bytes_sent += send(sock, &type_num, 2, 0);

      bytes_sent += send(sock, &buf_size, 4, 0); //4 byte integer. represents size of hashrequest
      buf_size = ntohl(buf_size);
      bytes_sent += send(sock, buf, buf_size, 0);

      // printf("total Bytes sent: %ld\n", bytes_sent);
      bytes_sent = 0;

      //recieving hash response
      uint16_t type4_num;
      u_int32_t index;
      uint8_t hash[32];
      ssize_t bytes_rcvd = recv(sock, &type4_num, 2, 0);
      bytes_rcvd += recv(sock, &index, 4, 0);
      bytes_rcvd += recv(sock, hash, 32, 0);
      index = ntohl(index);
      printf("%d: 0x", index);
      for (unsigned int j = 0; j < sizeof(hash); j++)
         printf("%02x", (unsigned int)hash[j]);
      printf("\n");
   }

   close(sock);
   free(args.filename);
   close(fd);
   return EXIT_SUCCESS;
}