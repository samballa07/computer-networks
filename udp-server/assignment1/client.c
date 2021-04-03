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
#include <math.h>
#include <pthread.h>

#define BUF_SIZE 160

double calc_theta(struct timespec T0, struct timespec T1, struct timespec T2);
int timespec_subtract(struct timespec *result, struct timespec *x, struct timespec *y);
double calc_delta(struct timespec T0, struct timespec T2);
void *send_requests(void *arg);

struct client_arguments
{
   char ip_address[16];
   int port;
   int num_timeouts;
   int timeout;
};

// struct for arguments for thread to send time requests
struct request_info
{
   int num_timeouts;
   struct addrinfo *serv_addr;
   int sock;
};

error_t client_parser(int key, char *arg, struct argp_state *state)
{
   struct client_arguments *args = state->input;
   error_t ret = 0;
   switch (key)
   {
   case 'a':
      strncpy(args->ip_address, arg, 16);
      if (0 /* ip address is goofytown */)
      {
         argp_error(state, "Invalid address");
      }
      break;
   case 'p':
      /* Validate that port is correct and a number, etc!! */
      args->port = atoi(arg);
      break;
   case 'n':
      /* validate argument makes sense */
      args->num_timeouts = atoi(arg);
      break;
   case 't':
      args->timeout = atoi(arg);
      break;
   default:
      ret = ARGP_ERR_UNKNOWN;
      break;
   }
   return ret;
}

int timespec_subtract(struct timespec *result, struct timespec *x, struct timespec *y){
   result->tv_sec = x->tv_sec - y->tv_sec;
   result->tv_nsec = x->tv_nsec - y->tv_nsec;
   return x->tv_sec < y->tv_sec; /* Return 1 if result is negative. */
}
double calc_theta(struct timespec T0, struct timespec T1, struct timespec T2){
   struct timespec first, second;
   struct timespec temp1 = {T1.tv_sec, T1.tv_nsec};
   timespec_subtract(&first, &temp1, &T0);
   timespec_subtract(&second, &T1, &T2);
   // printf("first -  seconds: %ld  nseconds: %ld\n", first.tv_sec, first.tv_nsec);
   // printf("second -  seconds: %ld  nseconds: %ld\n", second.tv_sec, second.tv_nsec);
   int64_t seconds = first.tv_sec;
   int64_t nseconds = first.tv_nsec;
   int n_digits = floor(log10(abs(nseconds))) + 1;
   double temp = pow(10.0, (double)n_digits);
   double decimal = (double)nseconds / temp;
   double first_ans = (double)seconds + decimal;
   // printf("first: %f\n", first_ans);
   seconds = second.tv_sec;
   nseconds = second.tv_nsec;
   n_digits = floor(log10(abs(nseconds))) + 1;
   temp = pow(10.0, (double)n_digits);
   decimal = (double)nseconds / temp;
   double second_ans = (double)seconds + decimal;
   // printf("second: %f\n", second_ans);
   return (first_ans + second_ans) / 2.0;
}
double calc_delta(struct timespec T0, struct timespec T2){
   struct timespec result;
   timespec_subtract(&result, &T2, &T0);
   int64_t nseconds = result.tv_nsec;
   //int n_digits = floor(log10(abs(nseconds))) + 1;
   //double temp = pow(10.0, (double)n_digits);
   double temp = 1000000000;
   double decimal = (double)nseconds / temp;
   return (double)result.tv_sec + decimal;
}

void *send_requests(void *args)
{
   struct request_info *info = (struct request_info *)args;
   uint16_t seq = htons(1);
   uint8_t version = 1;
   uint64_t seconds;
   uint64_t nseconds;
   ssize_t num_bytes = 0;
   struct timespec req_time;
   char send_buf[BUF_SIZE] = "";
   for (int i = 0; i < info->num_timeouts; i++){
      if (clock_gettime(CLOCK_REALTIME, &req_time) == -1){ //getting current time
         perror("clock gettime");
         exit(EXIT_FAILURE);
      }
      seconds = htobe64(req_time.tv_sec);
      nseconds = htobe64(req_time.tv_nsec);
      sprintf(send_buf, "%hhu %hu %ld %ld", version, seq, seconds, nseconds);
      num_bytes = sendto(info->sock, &send_buf, BUF_SIZE, 0, info->serv_addr->ai_addr, info->serv_addr->ai_addrlen);
      usleep(150);

      if (num_bytes < BUF_SIZE){
         perror("sendto() failure");
         exit(EXIT_FAILURE);
      }
      seq = ntohs(seq);
      seq++;
      seq = htons(seq);
      strcpy(send_buf, "");
   }
   return NULL;
}

int main(int argc, char *argv[]){

   struct argp_option options[] = {
       {"addr", 'a', "addr", 0, "The IP address the server is listening at", 0},
       {"port", 'p', "port", 0, "The port that is being used at the server", 0},
       {"num_timeouts", 'n', "hashreq", 0, "The number of hash requests to send to the server", 0},
       {"timeout", 't', "timeout", 0, "The timeout in seconds that the client will wait", 0},
       {0}};
   struct argp argp_settings = {options, client_parser, 0, 0, 0, 0, 0};
   struct client_arguments args;
   bzero(&args, sizeof(args));
   if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0)
   {
      printf("Got error in parse\n");
      return EXIT_FAILURE;
   }
   //printf("Got %s on port %d with n=%d timeout=%d\n", args.ip_address, args.port, args.num_timeouts, args.timeout);

   struct addrinfo addrCriteria;
   memset(&addrCriteria, 0, sizeof(addrCriteria));
   addrCriteria.ai_family = AF_UNSPEC;
   addrCriteria.ai_socktype = SOCK_DGRAM; // Only datagram sockets
   addrCriteria.ai_protocol = IPPROTO_UDP;

   char port[6];
   sprintf(port, "%d", args.port);
   struct addrinfo *serv_addr;
   int rtnVal = getaddrinfo(args.ip_address, port, &addrCriteria, &serv_addr);
   if (rtnVal != 0)
   {
      perror("Server addr error");
      return EXIT_FAILURE;
   }
   //creating a datagram socket
   int sock = socket(serv_addr->ai_family, serv_addr->ai_socktype, serv_addr->ai_protocol);
   if (sock < 0)
   {
      perror("socket() failure");
      return EXIT_FAILURE;
   }

   struct request_info *thread_args = malloc(sizeof(struct request_info));
   thread_args->num_timeouts = args.num_timeouts;
   thread_args->serv_addr = serv_addr;
   thread_args->sock = sock;
   pthread_t mythread;
   pthread_create(&mythread, NULL, send_requests, thread_args);

   uint16_t seq;
   uint8_t version = 1;
   uint64_t seconds, nseconds, server_sec, server_nsec;
   ssize_t num_bytes = 0;
   fd_set readfds, og_sock;
   FD_ZERO(&readfds); //clear the socket set
   FD_ZERO(&og_sock);
   FD_SET(sock, &og_sock);
   FD_SET(sock, &readfds);
   int count = 1;
   char recv_buf[BUF_SIZE];
   while (count <= args.num_timeouts){

      struct sockaddr_in from_addr;
      socklen_t from_addr_len = sizeof(from_addr);
      readfds = og_sock;
      struct timeval tv = {args.timeout, 0};

      int ret = select(sock + 1, &readfds, NULL, NULL, &tv);
      if (ret == -1){
         perror("select() error");
         return EXIT_FAILURE;
      }else if (ret){

         strcpy(recv_buf, "");
         num_bytes = recvfrom(sock, &recv_buf, BUF_SIZE, 0, (struct sockaddr *)&from_addr, &from_addr_len);
         sscanf(recv_buf, "%hhu %hu %ld %ld %ld %ld", &version, &seq, &seconds, &nseconds, &server_sec, &server_nsec);
         if (num_bytes < 35)
         {
            perror("recvfrom() error");
            return EXIT_FAILURE;
         }
         //printf("recv_buf: %s\n", recv_buf);
         seq = ntohs(seq);
         seconds = be64toh(seconds);
         nseconds = be64toh(nseconds);
         server_nsec = be64toh(server_nsec);
         server_sec = be64toh(server_sec);
         // printf("seq: %d, CLIENT seconds: %ld,  nsecs: %ld, SERVER seconds: %ld,  nsecs: %ld\n",
         //        seq, seconds, nseconds, server_sec, server_nsec);

         struct timespec T0 = {seconds, nseconds};
         struct timespec T1 = {server_sec, server_nsec};
         struct timespec T2;
         if (clock_gettime(CLOCK_REALTIME, &T2) == -1){
            perror("clock gettime");
            return EXIT_FAILURE;
         }
         //printf("T2: seconds-%ld  nsecs-%ld\n", T2.tv_sec, T2.tv_nsec);
         double theta;
         double delta;
         if (seq > count){
            while (seq > count)
               printf("%d: Dropped\n", count++);

            theta = calc_theta(T0, T1, T2);
            delta = calc_delta(T0, T2);
            printf("%d: %0.4f %0.4f\n", count, theta, delta);
         }
         else if (seq == count){
            theta = calc_theta(T0, T1, T2);
            delta = calc_delta(T0, T2);
            printf("%d: %0.4f %0.4f\n", count, theta, delta);
         }
      }
      else{
         while (count <= args.num_timeouts)
         {
            printf("%d: Dropped\n", count++);
         }
         return EXIT_SUCCESS;
      }
      count++;
      FD_CLR(sock, &readfds);
   }

   pthread_join(mythread, NULL);
   free(thread_args);
   close(sock);
   return 0;
}