#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <strings.h>
#include <argp.h>
#include <time.h>
#include <arpa/inet.h>
//#include <pthread.h>
#define BUF_SIZE 160

struct server_arguments
{
   int port;
   int drop_percent;
};

struct thread_args{
   int sock;
   int drop_percent;
};

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

typedef struct node
{
   int max_seq;
   int port;
   clock_t time_elapsed;
   struct node *next;
} Node;

Node *head = NULL;
Node *current = NULL;

//insert node
void insertFirst(int port, int max_seq, clock_t time_elapsed)
{
   Node *link = malloc(sizeof(Node));

   link->port = port;
   link->max_seq = max_seq;
   link->time_elapsed = time_elapsed;
   link->next = head;
   head = link;
}
// find node and return it
Node * find(int port){
   Node *current = head;
   if (head == NULL){
      return NULL;
   }
   while (current->port != port){
      if (current->next == NULL){
         return NULL;
      }
      else{
         current = current->next;
      }
   }
   return current;
}

error_t server_parser(int key, char *arg, struct argp_state *state)
{
   struct server_arguments *args = state->input;
   error_t ret = 0;
   switch (key)
   {
   case 'p':
      /* Validate that port is correct and a number, etc!! */
      args->port = atoi(arg);
      if (0 /* port is invalid */)
      {
         argp_error(state, "Invalid option for a port, must be a number");
      }
      break;
   case 'd':
      args->drop_percent = atoi(arg);
      break;
   default:
      ret = ARGP_ERR_UNKNOWN;
      break;
   }
   return ret;
}

int willDrop(int drop_percent, struct timespec t1)
{
   srand(t1.tv_nsec * t1.tv_sec);
   float a = 1.0;
   float x = ((float)rand() / (float)(RAND_MAX)) * a;
   x *= 100;
   if (x < (float)drop_percent)
   {
      return 1;
   }
   return 0;
}

void * handle_client(void *arg){
   struct thread_args *args = (struct thread_args *) arg;
   ssize_t num_bytes = 0;
   clock_t time_elapsed;
   double time_taken = 0;
   char recv_buf[BUF_SIZE];
   strcpy(recv_buf, "");
   struct sockaddr_in client_addr;
   socklen_t client_addr_len = sizeof(client_addr);
   uint8_t version;
   uint16_t seq;
   uint64_t seconds, nseconds;
   num_bytes = 0;

   num_bytes = recvfrom(args->sock, &recv_buf, BUF_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
   if (num_bytes < 19){
      perror("recvfrom error");
      exit(EXIT_FAILURE);
   }
   sscanf(recv_buf, "%hhu %hu %ld %ld", &version, &seq, &seconds, &nseconds);
   seconds = be64toh(seconds);
   nseconds = be64toh(nseconds);
   seq = ntohs(seq);
   //printf("seq number: %d\n", seq);
   // get client ip address
  
   struct sockaddr_in *sin = (struct sockaddr_in *)&client_addr;
   unsigned char *ip = (unsigned char *)&sin->sin_addr.s_addr;
   char client_ip[16];
   sprintf(client_ip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
   //printf("Received message from IP: %s and port: %i\n", inet_ntoa(sin->sin_addr), ntohs(sin->sin_port));

   struct timespec req_time;
   if (clock_gettime(CLOCK_REALTIME, &req_time) == -1){
      perror("clock gettime");
      exit(EXIT_FAILURE);
   }
   // printf("CLIENT seconds: %ld  nseconds: %ld SERVER seconds: %ld  nseconds: %ld\n",
   //        seconds, nseconds, req_time.tv_sec, req_time.tv_nsec);
   int port = sin->sin_port;
   if (seq == 1){
      insertFirst(port, 1, clock());
   }

   char send_buf[BUF_SIZE];
   int isDropped = willDrop(args->drop_percent, req_time); //check if packet will be dropped
   if (isDropped == 0){ //not dropped
      strcpy(send_buf, "");
      if (seq != 1){
         Node *client = find(port);
         if (client != NULL){
            if (client->max_seq == -1){
               client->max_seq = seq;
               client->time_elapsed = clock();
            }else if (seq > client->max_seq){
               client->max_seq = seq;
               client->time_elapsed = clock();
            }else if (seq < client->max_seq){
               printf("%s:%d %d %d\n", client_ip, ntohs(sin->sin_port), seq, client->max_seq);
            }
            time_elapsed = clock() - client->time_elapsed;
            time_taken = ((double)time_elapsed) / CLOCKS_PER_SEC;
            if (time_taken > 120.0){ //take the next sequence number when more than 2 minutes has passed and max_seq hasnt changed
               client->max_seq = -1; // -1 indicates to take the next observed sequence
            }
         }
      }
      // construct and send time response
      uint64_t server_sec = htobe64(req_time.tv_sec);
      uint64_t server_nsec = htobe64(req_time.tv_nsec);
      seconds = htobe64(seconds);
      nseconds = htobe64(nseconds);
      seq = htons(seq);
      sprintf(send_buf, "%hhu %hu %ld %ld %ld %ld", version, seq, seconds, nseconds, server_sec, server_nsec);
      num_bytes = 0;
      num_bytes = sendto(args->sock, &send_buf, BUF_SIZE, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
      if (num_bytes < BUF_SIZE){
         perror("sendto() failed");
         exit(EXIT_FAILURE);
      }
   }
   
   return NULL;
}

int main(int argc, char *argv[]){
   
   struct server_arguments args;
   /* bzero ensures that "default" parameters are all zeroed out */
   bzero(&args, sizeof(args));
   struct argp_option options[] = {
       {"port", 'p', "port", 0, "The port to be used for the server", 0},
       {"drop_percent", 'd', "percent", 0, "Percentage chance that servers any UDP payload it received. Optional", 0},
       {0}};
   struct argp argp_settings = {options, server_parser, 0, 0, 0, 0, 0};
   if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0){
      printf("Got an error condition when parsing\n");
      return EXIT_FAILURE;
   }
   if (args.drop_percent < 0 || args.drop_percent > 100){
      printf("Error: supply -d option between 0 and 100 inclusive\n");
      return EXIT_FAILURE;
   }
   printf("Got port %d and percentage %d\n", args.port, args.drop_percent);
   char port[6];
   sprintf(port, "%d", args.port);

   struct addrinfo addrCriteria;
   memset(&addrCriteria, 0, sizeof(addrCriteria));
   addrCriteria.ai_family = AF_UNSPEC;
   addrCriteria.ai_flags = AI_PASSIVE;
   addrCriteria.ai_socktype = SOCK_DGRAM;
   addrCriteria.ai_protocol = IPPROTO_UDP;

   struct addrinfo *serv_addr;
   int rtnVal = getaddrinfo(NULL, port, &addrCriteria, &serv_addr);
   if (rtnVal != 0){
      perror("getaddrinfo() error");
      return EXIT_FAILURE;
   }
   int sock = socket(serv_addr->ai_family, serv_addr->ai_socktype, serv_addr->ai_protocol);
   if (sock < 0){
      perror("socket() error");
      return EXIT_FAILURE;
   }
   if (bind(sock, serv_addr->ai_addr, serv_addr->ai_addrlen) < 0){
      perror("bind() error");
      return EXIT_FAILURE;
   }
   freeaddrinfo(serv_addr);
   
   struct thread_args *my_args = malloc(sizeof(struct thread_args));
   my_args->drop_percent = args.drop_percent;
   my_args->sock = sock;

   while (1){
      handle_client(my_args);
   }

   free(my_args);
   return 0;
}